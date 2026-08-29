#ifndef DOSE_CONTROLLER_STORAGE_NVS_H
#define DOSE_CONTROLLER_STORAGE_NVS_H

#include <stdbool.h>

#include "dose_controller/dose_controller.h"
#include "esp_err.h"
#include "nvs.h"

typedef struct {
    nvs_handle_t handle;
} dose_nvs_storage_t;

esp_err_t dose_nvs_storage_init(dose_nvs_storage_t *storage);
void dose_nvs_storage_close(dose_nvs_storage_t *storage);
bool dose_nvs_load(void *context, const char *occurrence_id,
                   dose_record_t *record, bool *found);
bool dose_nvs_store(void *context, const dose_record_t *record);
bool dose_nvs_load_latest(dose_nvs_storage_t *storage,
                          dose_record_t *record, bool *found);

#endif
