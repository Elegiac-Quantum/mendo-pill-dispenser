#include "schedule_store/storage_nvs.h"

static const char *slot_key(unsigned slot)
{
    return slot == 0 ? "slot_a" : "slot_b";
}

static bool read_slot(void *context, unsigned slot, void *data, size_t size)
{
    schedule_nvs_t *storage = context;
    if (storage == NULL || slot > 1) return false;
    size_t stored_size = size;
    return nvs_get_blob(storage->handle, slot_key(slot), data, &stored_size) == ESP_OK &&
           stored_size == size;
}

static bool write_slot(void *context, unsigned slot, const void *data, size_t size)
{
    schedule_nvs_t *storage = context;
    if (storage == NULL || slot > 1) return false;
    return nvs_set_blob(storage->handle, slot_key(slot), data, size) == ESP_OK &&
           nvs_commit(storage->handle) == ESP_OK;
}

static bool read_active(void *context, uint32_t *generation)
{
    schedule_nvs_t *storage = context;
    return storage != NULL && generation != NULL &&
           nvs_get_u32(storage->handle, "active", generation) == ESP_OK;
}

static bool write_active(void *context, uint32_t generation)
{
    schedule_nvs_t *storage = context;
    return storage != NULL && nvs_set_u32(storage->handle, "active", generation) == ESP_OK &&
           nvs_commit(storage->handle) == ESP_OK;
}

esp_err_t schedule_nvs_open(schedule_nvs_t *storage)
{
    if (storage == NULL) return ESP_ERR_INVALID_ARG;
    storage->handle = 0;
    return nvs_open("schedule_draft", NVS_READWRITE, &storage->handle);
}

void schedule_nvs_close(schedule_nvs_t *storage)
{
    if (storage != NULL && storage->handle != 0) {
        nvs_close(storage->handle);
        storage->handle = 0;
    }
}

schedule_store_backend_t schedule_nvs_backend(schedule_nvs_t *storage)
{
    schedule_store_backend_t backend = {
        .context = storage,
        .read_slot = read_slot,
        .write_slot = write_slot,
        .read_active_generation = read_active,
        .write_active_generation = write_active,
    };
    return backend;
}

bool schedule_nvs_has_data(schedule_nvs_t *storage)
{
    if (storage == NULL) return false;
    uint32_t active;
    size_t size = 0;
    return nvs_get_u32(storage->handle, "active", &active) == ESP_OK ||
           nvs_get_blob(storage->handle, "slot_a", NULL, &size) == ESP_OK ||
           nvs_get_blob(storage->handle, "slot_b", NULL, &size) == ESP_OK;
}
