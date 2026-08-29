#include "dose_controller/storage_nvs.h"

#include <stddef.h>
#include <string.h>

#include "esp_crc.h"

enum { DOSE_STORAGE_SCHEMA = 1 };

typedef struct {
    uint32_t schema;
    dose_record_t record;
    uint32_t crc32;
} stored_dose_t;

static uint32_t stored_crc(const stored_dose_t *stored)
{
    return esp_crc32_le(UINT32_MAX, (const uint8_t *)stored,
                        offsetof(stored_dose_t, crc32));
}

esp_err_t dose_nvs_storage_init(dose_nvs_storage_t *storage)
{
    if (storage == NULL) { return ESP_ERR_INVALID_ARG; }
    return nvs_open("dose_state", NVS_READWRITE, &storage->handle);
}

void dose_nvs_storage_close(dose_nvs_storage_t *storage)
{
    if (storage != NULL && storage->handle != 0) {
        nvs_close(storage->handle);
        storage->handle = 0;
    }
}

bool dose_nvs_load_latest(dose_nvs_storage_t *storage,
                          dose_record_t *record, bool *found)
{
    if (storage == NULL || record == NULL || found == NULL) { return false; }
    *found = false;

    stored_dose_t stored = {0};
    size_t size = sizeof(stored);
    esp_err_t error = nvs_get_blob(storage->handle, "latest", &stored, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) { return true; }
    if (error != ESP_OK || size != sizeof(stored) ||
        stored.schema != DOSE_STORAGE_SCHEMA || stored.crc32 != stored_crc(&stored)) {
        return false;
    }
    *record = stored.record;
    *found = true;
    return true;
}

bool dose_nvs_load(void *context, const char *occurrence_id,
                   dose_record_t *record, bool *found)
{
    if (occurrence_id == NULL) { return false; }
    if (!dose_nvs_load_latest((dose_nvs_storage_t *)context, record, found)) {
        return false;
    }
    if (*found && strcmp(record->occurrence_id, occurrence_id) != 0) {
        *found = false;
    }
    return true;
}

bool dose_nvs_store(void *context, const dose_record_t *record)
{
    dose_nvs_storage_t *storage = (dose_nvs_storage_t *)context;
    if (storage == NULL || record == NULL) { return false; }

    stored_dose_t stored = {
        .schema = DOSE_STORAGE_SCHEMA,
        .record = *record,
    };
    stored.crc32 = stored_crc(&stored);
    return nvs_set_blob(storage->handle, "latest", &stored, sizeof(stored)) == ESP_OK &&
           nvs_commit(storage->handle) == ESP_OK;
}
