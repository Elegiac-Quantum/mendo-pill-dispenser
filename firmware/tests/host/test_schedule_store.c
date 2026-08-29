#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "schedule_store/schedule_store.h"

typedef struct {
    unsigned char slots[2][4096];
    size_t sizes[2];
    uint32_t active_generation;
    bool has_active;
    bool fail_write;
    bool fail_active;
} memory_backend_t;

static bool memory_read_slot(void *context, unsigned slot, void *data, size_t size)
{
    memory_backend_t *memory = context;
    if (slot > 1 || memory->sizes[slot] != size) return false;
    memcpy(data, memory->slots[slot], size);
    return true;
}

static bool memory_write_slot(void *context, unsigned slot, const void *data, size_t size)
{
    memory_backend_t *memory = context;
    if (memory->fail_write || slot > 1 || size > sizeof(memory->slots[slot])) return false;
    memcpy(memory->slots[slot], data, size);
    memory->sizes[slot] = size;
    return true;
}

static bool memory_read_active(void *context, uint32_t *generation)
{
    memory_backend_t *memory = context;
    if (!memory->has_active) return false;
    *generation = memory->active_generation;
    return true;
}

static bool memory_write_active(void *context, uint32_t generation)
{
    memory_backend_t *memory = context;
    if (memory->fail_active) return false;
    memory->active_generation = generation;
    memory->has_active = true;
    return true;
}

static schedule_store_backend_t backend_for(memory_backend_t *memory)
{
    schedule_store_backend_t backend = {
        .context = memory,
        .read_slot = memory_read_slot,
        .write_slot = memory_write_slot,
        .read_active_generation = memory_read_active,
        .write_active_generation = memory_write_active,
    };
    return backend;
}

static schedule_draft_input_t input_for(const char *medication,
                                        const char *dose,
                                        uint16_t first,
                                        uint16_t second)
{
    schedule_draft_input_t input = {0};
    input.medication = medication;
    input.dose_instruction = dose;
    input.time_count = 2;
    input.times[0] = first;
    input.times[1] = second;
    input.reminder_minutes = 5;
    input.snooze_minutes = 10;
    return input;
}

static void test_normalizes_times(void)
{
    schedule_store_t store;
    schedule_store_init_empty(&store);
    schedule_draft_input_t input = input_for("Vitamin D", "1 tablet", 20 * 60, 8 * 60);

    schedule_draft_t created;
    assert(schedule_store_create(&store, &input, &created) == SCHEDULE_STORE_OK);
    assert(created.schedule.times[0] == 8 * 60);
    assert(created.schedule.times[1] == 20 * 60);
    assert(strcmp(created.schedule.id, "draft-00000001") == 0);
}

static void test_rejects_duplicate_time_and_blank_dose(void)
{
    schedule_store_t store;
    schedule_store_init_empty(&store);
    schedule_draft_input_t duplicate = input_for("Vitamin D", "1 tablet", 8 * 60, 8 * 60);
    schedule_draft_input_t blank = input_for("Vitamin D", "   ", 8 * 60, 20 * 60);

    assert(schedule_store_create(&store, &duplicate, NULL) == SCHEDULE_STORE_INVALID);
    assert(schedule_store_create(&store, &blank, NULL) == SCHEDULE_STORE_INVALID);
    assert(schedule_store_count(&store) == 0);
}

static void test_rejects_malformed_utf8(void)
{
    schedule_store_t store;
    schedule_store_init_empty(&store);
    const char malformed[] = {(char)0xc0, (char)0xaf, '\0'};
    schedule_draft_input_t input = input_for(malformed, "1 tablet", 480, 1200);
    assert(schedule_store_create(&store, &input, NULL) == SCHEDULE_STORE_INVALID);
}

static void test_limits_store_to_eight(void)
{
    schedule_store_t store;
    schedule_store_init_empty(&store);
    schedule_draft_input_t input = input_for("Medicine", "1 tablet", 8 * 60, 20 * 60);

    for (int index = 0; index < SCHEDULE_STORE_MAX_DRAFTS; ++index) {
        assert(schedule_store_create(&store, &input, NULL) == SCHEDULE_STORE_OK);
    }
    assert(schedule_store_create(&store, &input, NULL) == SCHEDULE_STORE_FULL);
    assert(schedule_store_count(&store) == SCHEDULE_STORE_MAX_DRAFTS);
}

static void test_failed_active_switch_preserves_committed_generation(void)
{
    memory_backend_t memory = {0};
    schedule_store_backend_t backend = backend_for(&memory);
    schedule_store_t store;
    schedule_store_init_empty(&store);
    schedule_draft_input_t first = input_for("First", "1 tablet", 480, 1200);
    schedule_draft_input_t second = input_for("Second", "2 tablets", 540, 1260);

    assert(schedule_store_create(&store, &first, NULL) == SCHEDULE_STORE_OK);
    assert(schedule_store_commit(&store, &backend) == SCHEDULE_STORE_OK);
    assert(schedule_store_create(&store, &second, NULL) == SCHEDULE_STORE_OK);
    memory.fail_active = true;
    assert(schedule_store_commit(&store, &backend) == SCHEDULE_STORE_IO_ERROR);

    schedule_store_t loaded;
    assert(schedule_store_load(&loaded, &backend) == SCHEDULE_STORE_OK);
    assert(schedule_store_count(&loaded) == 1);
    assert(strcmp(loaded.drafts[0].schedule.medication, "First") == 0);
}

static void test_corrupt_active_recovers_newest_valid_slot(void)
{
    memory_backend_t memory = {0};
    schedule_store_backend_t backend = backend_for(&memory);
    schedule_store_t store;
    schedule_store_init_empty(&store);
    schedule_draft_input_t input = input_for("Medicine", "1 tablet", 480, 1200);
    assert(schedule_store_create(&store, &input, NULL) == SCHEDULE_STORE_OK);
    assert(schedule_store_commit(&store, &backend) == SCHEDULE_STORE_OK);
    memory.active_generation = 99;

    schedule_store_t loaded;
    assert(schedule_store_load(&loaded, &backend) == SCHEDULE_STORE_OK);
    assert(schedule_store_count(&loaded) == 1);
}

static void test_two_corrupt_slots_fail_closed(void)
{
    memory_backend_t memory = {0};
    memory.sizes[0] = 16;
    memory.sizes[1] = 16;
    schedule_store_backend_t backend = backend_for(&memory);
    schedule_store_t loaded;
    assert(schedule_store_load(&loaded, &backend) == SCHEDULE_STORE_CORRUPT);
    assert(schedule_store_count(&loaded) == 0);
}

static void test_update_and_delete_require_existing_device_id(void)
{
    schedule_store_t store;
    schedule_store_init_empty(&store);
    schedule_draft_input_t input = input_for("Original", "1 tablet", 480, 1200);
    schedule_draft_t created;
    assert(schedule_store_create(&store, &input, &created) == SCHEDULE_STORE_OK);

    schedule_draft_input_t changed = input_for("Changed", "2 tablets", 540, 1260);
    assert(schedule_store_update(&store, "draft-99999999", &changed, NULL) ==
           SCHEDULE_STORE_NOT_FOUND);
    assert(schedule_store_update(&store, created.schedule.id, &changed, NULL) ==
           SCHEDULE_STORE_OK);
    assert(strcmp(store.drafts[0].schedule.id, created.schedule.id) == 0);
    assert(strcmp(store.drafts[0].schedule.medication, "Changed") == 0);
    assert(schedule_store_delete(&store, "draft-99999999") == SCHEDULE_STORE_NOT_FOUND);
    assert(schedule_store_delete(&store, created.schedule.id) == SCHEDULE_STORE_OK);
    assert(schedule_store_count(&store) == 0);
}

int main(void)
{
    test_normalizes_times();
    test_rejects_duplicate_time_and_blank_dose();
    test_rejects_malformed_utf8();
    test_limits_store_to_eight();
    test_failed_active_switch_preserves_committed_generation();
    test_corrupt_active_recovers_newest_valid_slot();
    test_two_corrupt_slots_fail_closed();
    test_update_and_delete_require_existing_device_id();
    puts("schedule store tests passed");
    return 0;
}
