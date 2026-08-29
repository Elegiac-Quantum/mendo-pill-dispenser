#include "schedule_store/schedule_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORE_MAGIC 0x53445246u
#define STORE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t next_id;
    uint32_t count;
    schedule_draft_t drafts[SCHEDULE_STORE_MAX_DRAFTS];
    uint32_t crc32;
} stored_document_t;

static uint32_t crc32_bytes(const void *data, size_t size)
{
    const unsigned char *bytes = data;
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static bool backend_valid(const schedule_store_backend_t *backend)
{
    return backend != NULL && backend->read_slot != NULL && backend->write_slot != NULL &&
           backend->read_active_generation != NULL && backend->write_active_generation != NULL;
}

static bool document_valid(const stored_document_t *document)
{
    if (document->magic != STORE_MAGIC || document->version != STORE_VERSION ||
        document->count > SCHEDULE_STORE_MAX_DRAFTS || document->next_id == 0 ||
        document->crc32 != crc32_bytes(document, offsetof(stored_document_t, crc32))) {
        return false;
    }
    for (uint32_t index = 0; index < document->count; ++index) {
        const schedule_draft_t *draft = &document->drafts[index];
        if (pill_schedule_validate(&draft->schedule) != PILL_SCHEDULE_OK ||
            memchr(draft->dose_instruction, '\0', sizeof(draft->dose_instruction)) == NULL ||
            draft->dose_instruction[0] == '\0') {
            return false;
        }
    }
    return true;
}

static void store_from_document(schedule_store_t *store, const stored_document_t *document)
{
    memset(store, 0, sizeof(*store));
    store->generation = document->generation;
    store->next_id = document->next_id;
    store->count = document->count;
    memcpy(store->drafts, document->drafts, sizeof(store->drafts));
}

static bool valid_visible_utf8(const unsigned char *text, size_t length)
{
    size_t index = 0;
    while (index < length) {
        unsigned char first = text[index++];
        if (first < 0x80) {
            if (first < 0x20 || first == 0x7f) return false;
            continue;
        }
        unsigned continuation = 0;
        unsigned char second_min = 0x80, second_max = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) continuation = 1;
        else if (first >= 0xe0 && first <= 0xef) {
            continuation = 2;
            if (first == 0xe0) second_min = 0xa0;
            if (first == 0xed) second_max = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation = 3;
            if (first == 0xf0) second_min = 0x90;
            if (first == 0xf4) second_max = 0x8f;
        } else return false;
        if (index + continuation > length || text[index] < second_min ||
            text[index] > second_max) return false;
        ++index;
        for (unsigned remaining = 1; remaining < continuation; ++remaining, ++index) {
            if (text[index] < 0x80 || text[index] > 0xbf) return false;
        }
    }
    return true;
}

static bool copy_trimmed(char *destination, size_t capacity, const char *source)
{
    if (destination == NULL || capacity == 0 || source == NULL) {
        return false;
    }
    while (*source == ' ' || *source == '\t' || *source == '\r' || *source == '\n') {
        ++source;
    }
    size_t length = strnlen(source, capacity);
    if (length == capacity) {
        return false;
    }
    while (length > 0 && (source[length - 1] == ' ' || source[length - 1] == '\t' ||
                          source[length - 1] == '\r' || source[length - 1] == '\n')) {
        --length;
    }
    if (length == 0) {
        return false;
    }
    if (!valid_visible_utf8((const unsigned char *)source, length)) return false;
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

void schedule_store_init_empty(schedule_store_t *store)
{
    if (store != NULL) {
        memset(store, 0, sizeof(*store));
        store->next_id = 1;
    }
}

size_t schedule_store_count(const schedule_store_t *store)
{
    return store == NULL ? 0 : store->count;
}

static schedule_store_result_t prepare_draft(const schedule_draft_input_t *input,
                                             const char *id,
                                             schedule_draft_t *draft)
{
    if (input == NULL || id == NULL || draft == NULL || input->time_count == 0 ||
        input->time_count > PILL_SCHEDULE_TIMES_MAX) {
        return SCHEDULE_STORE_INVALID;
    }
    memset(draft, 0, sizeof(*draft));
    if (!copy_trimmed(draft->schedule.medication, sizeof(draft->schedule.medication),
                      input->medication) ||
        !copy_trimmed(draft->dose_instruction, sizeof(draft->dose_instruction),
                      input->dose_instruction)) {
        return SCHEDULE_STORE_INVALID;
    }
    draft->schedule.time_count = input->time_count;
    memcpy(draft->schedule.times, input->times,
           input->time_count * sizeof(input->times[0]));
    for (uint8_t outer = 1; outer < input->time_count; ++outer) {
        uint16_t value = draft->schedule.times[outer];
        uint8_t inner = outer;
        while (inner > 0 && draft->schedule.times[inner - 1] > value) {
            draft->schedule.times[inner] = draft->schedule.times[inner - 1];
            --inner;
        }
        draft->schedule.times[inner] = value;
    }
    draft->schedule.reminder_minutes = input->reminder_minutes;
    draft->schedule.snooze_minutes = input->snooze_minutes;
    draft->schedule.enabled = true;
    snprintf(draft->schedule.id, sizeof(draft->schedule.id), "%s", id);
    if (pill_schedule_validate(&draft->schedule) != PILL_SCHEDULE_OK) {
        return SCHEDULE_STORE_INVALID;
    }
    return SCHEDULE_STORE_OK;
}

schedule_store_result_t schedule_store_create(schedule_store_t *store,
                                              const schedule_draft_input_t *input,
                                              schedule_draft_t *created)
{
    if (store == NULL) return SCHEDULE_STORE_INVALID;
    if (store->count >= SCHEDULE_STORE_MAX_DRAFTS) return SCHEDULE_STORE_FULL;
    char id[PILL_SCHEDULE_ID_MAX];
    snprintf(id, sizeof(id), "draft-%08lx", (unsigned long)store->next_id);
    schedule_draft_t draft;
    schedule_store_result_t result = prepare_draft(input, id, &draft);
    if (result != SCHEDULE_STORE_OK) return result;

    store->drafts[store->count++] = draft;
    ++store->next_id;
    if (created != NULL) {
        *created = draft;
    }
    return SCHEDULE_STORE_OK;
}

schedule_store_result_t schedule_store_update(schedule_store_t *store,
                                              const char *id,
                                              const schedule_draft_input_t *input,
                                              schedule_draft_t *updated)
{
    if (store == NULL || id == NULL) return SCHEDULE_STORE_INVALID;
    for (size_t index = 0; index < store->count; ++index) {
        if (strcmp(store->drafts[index].schedule.id, id) == 0) {
            schedule_draft_t draft;
            schedule_store_result_t result = prepare_draft(input, id, &draft);
            if (result != SCHEDULE_STORE_OK) return result;
            store->drafts[index] = draft;
            if (updated != NULL) *updated = draft;
            return SCHEDULE_STORE_OK;
        }
    }
    return SCHEDULE_STORE_NOT_FOUND;
}

schedule_store_result_t schedule_store_delete(schedule_store_t *store, const char *id)
{
    if (store == NULL || id == NULL) return SCHEDULE_STORE_INVALID;
    for (size_t index = 0; index < store->count; ++index) {
        if (strcmp(store->drafts[index].schedule.id, id) == 0) {
            memmove(&store->drafts[index], &store->drafts[index + 1],
                    (store->count - index - 1) * sizeof(store->drafts[0]));
            --store->count;
            memset(&store->drafts[store->count], 0, sizeof(store->drafts[0]));
            return SCHEDULE_STORE_OK;
        }
    }
    return SCHEDULE_STORE_NOT_FOUND;
}

schedule_store_result_t schedule_store_commit(schedule_store_t *store,
                                              const schedule_store_backend_t *backend)
{
    if (store == NULL || !backend_valid(backend) || store->count > SCHEDULE_STORE_MAX_DRAFTS) {
        return SCHEDULE_STORE_INVALID;
    }
    stored_document_t *document = calloc(1, sizeof(*document));
    if (document == NULL) return SCHEDULE_STORE_IO_ERROR;
    *document = (stored_document_t){
        .magic = STORE_MAGIC,
        .version = STORE_VERSION,
        .generation = store->generation + 1,
        .next_id = store->next_id,
        .count = (uint32_t)store->count,
    };
    memcpy(document->drafts, store->drafts, sizeof(document->drafts));
    document->crc32 = crc32_bytes(document, offsetof(stored_document_t, crc32));
    unsigned slot = document->generation & 1u;
    uint32_t generation = document->generation;
    uint32_t expected_crc = document->crc32;
    if (!backend->write_slot(backend->context, slot, document, sizeof(*document))) {
        free(document);
        return SCHEDULE_STORE_IO_ERROR;
    }
    memset(document, 0, sizeof(*document));
    if (!backend->read_slot(backend->context, slot, document, sizeof(*document)) ||
        document->generation != generation || document->crc32 != expected_crc ||
        !document_valid(document)) {
        free(document);
        return SCHEDULE_STORE_IO_ERROR;
    }
    if (!backend->write_active_generation(backend->context, generation)) {
        free(document);
        return SCHEDULE_STORE_IO_ERROR;
    }
    store->generation = generation;
    free(document);
    return SCHEDULE_STORE_OK;
}

schedule_store_result_t schedule_store_load(schedule_store_t *store,
                                            const schedule_store_backend_t *backend)
{
    if (store == NULL || !backend_valid(backend)) {
        return SCHEDULE_STORE_INVALID;
    }
    schedule_store_init_empty(store);
    stored_document_t *documents = calloc(2, sizeof(*documents));
    if (documents == NULL) return SCHEDULE_STORE_IO_ERROR;
    bool valid[2];
    for (unsigned slot = 0; slot < 2; ++slot) {
        valid[slot] = backend->read_slot(backend->context, slot, &documents[slot],
                                         sizeof(documents[slot])) &&
                      document_valid(&documents[slot]);
    }
    uint32_t active;
    if (backend->read_active_generation(backend->context, &active)) {
        unsigned slot = active & 1u;
        if (valid[slot] && documents[slot].generation == active) {
            store_from_document(store, &documents[slot]);
            free(documents);
            return SCHEDULE_STORE_OK;
        }
    }
    int selected = -1;
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (valid[slot] && (selected < 0 ||
            documents[slot].generation > documents[selected].generation)) {
            selected = (int)slot;
        }
    }
    if (selected < 0) {
        free(documents);
        return SCHEDULE_STORE_CORRUPT;
    }
    store_from_document(store, &documents[selected]);
    free(documents);
    return SCHEDULE_STORE_OK;
}
