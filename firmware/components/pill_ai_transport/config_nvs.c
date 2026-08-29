#include "pill_ai_transport/config_nvs.h"

#include <string.h>

#include "nvs.h"

#define PILL_AI_CONFIG_VERSION 1u

typedef struct {
    uint32_t version;
    pill_ai_saved_config_t config;
} stored_config_t;

esp_err_t pill_ai_config_load(pill_ai_saved_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(config, 0, sizeof(*config));
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open("pill_ai", NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    stored_config_t stored = {0};
    size_t bytes = sizeof(stored);
    result = nvs_get_blob(handle, "config", &stored, &bytes);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK || bytes != sizeof(stored) ||
        stored.version != PILL_AI_CONFIG_VERSION) {
        return ESP_ERR_INVALID_STATE;
    }
    if ((stored.config.wifi_configured &&
         !pill_ai_wifi_config_valid(&stored.config.wifi)) ||
        (stored.config.remote_configured &&
         !pill_ai_remote_config_valid(&stored.config.remote))) {
        return ESP_ERR_INVALID_STATE;
    }
    *config = stored.config;
    return ESP_OK;
}

esp_err_t pill_ai_config_store(const pill_ai_saved_config_t *config)
{
    if (config == NULL ||
        (config->wifi_configured && !pill_ai_wifi_config_valid(&config->wifi)) ||
        (config->remote_configured && !pill_ai_remote_config_valid(&config->remote))) {
        return ESP_ERR_INVALID_ARG;
    }
    stored_config_t stored = {
        .version = PILL_AI_CONFIG_VERSION,
        .config = *config,
    };
    nvs_handle_t handle;
    esp_err_t result = nvs_open("pill_ai", NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        result = nvs_set_blob(handle, "config", &stored, sizeof(stored));
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    return result;
}
