#ifndef PILL_AI_TRANSPORT_CONFIG_VALIDATION_H
#define PILL_AI_TRANSPORT_CONFIG_VALIDATION_H

#include <stdbool.h>

#define PILL_AI_WIFI_SSID_BYTES 33
#define PILL_AI_WIFI_PASSWORD_BYTES 65
#define PILL_AI_CONFIG_URL_BYTES 192
#define PILL_AI_CONFIG_SECRET_BYTES 32
#define PILL_AI_CONFIG_SECRET_STORAGE_BYTES 65

typedef struct {
    char ssid[PILL_AI_WIFI_SSID_BYTES];
    char password[PILL_AI_WIFI_PASSWORD_BYTES];
} pill_ai_wifi_config_t;

typedef struct {
    char server_url[PILL_AI_CONFIG_URL_BYTES];
    char device_secret[PILL_AI_CONFIG_SECRET_STORAGE_BYTES];
} pill_ai_remote_config_t;

bool pill_ai_wifi_config_valid(const pill_ai_wifi_config_t *config);
bool pill_ai_remote_config_valid(const pill_ai_remote_config_t *config);

#endif
