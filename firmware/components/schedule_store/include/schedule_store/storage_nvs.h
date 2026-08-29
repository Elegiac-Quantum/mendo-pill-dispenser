#ifndef SCHEDULE_STORE_STORAGE_NVS_H
#define SCHEDULE_STORE_STORAGE_NVS_H

#include "nvs.h"
#include "schedule_store/schedule_store.h"

typedef struct {
    nvs_handle_t handle;
} schedule_nvs_t;

esp_err_t schedule_nvs_open(schedule_nvs_t *storage);
void schedule_nvs_close(schedule_nvs_t *storage);
schedule_store_backend_t schedule_nvs_backend(schedule_nvs_t *storage);
bool schedule_nvs_has_data(schedule_nvs_t *storage);

#endif
