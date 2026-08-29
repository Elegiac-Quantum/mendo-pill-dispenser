#ifndef PILL_AI_TRANSPORT_CONFIG_NVS_H
#define PILL_AI_TRANSPORT_CONFIG_NVS_H

#include <stdbool.h>

#include "esp_err.h"
#include "pill_ai_transport/config_validation.h"

typedef struct {
    pill_ai_wifi_config_t wifi;
    pill_ai_remote_config_t remote;
    bool wifi_configured;
    bool remote_configured;
} pill_ai_saved_config_t;

esp_err_t pill_ai_config_load(pill_ai_saved_config_t *config);
esp_err_t pill_ai_config_store(const pill_ai_saved_config_t *config);

#endif
