#include "pill_ai_transport/config_validation.h"

#include <stddef.h>
#include <string.h>

static bool bounded_string(const char *value, size_t capacity, size_t minimum)
{
    if (value == NULL) {
        return false;
    }
    size_t length = strnlen(value, capacity);
    return length >= minimum && length < capacity;
}

bool pill_ai_wifi_config_valid(const pill_ai_wifi_config_t *config)
{
    return config != NULL &&
           bounded_string(config->ssid, sizeof(config->ssid), 1) &&
           bounded_string(config->password, sizeof(config->password), 8);
}

bool pill_ai_remote_config_valid(const pill_ai_remote_config_t *config)
{
    static const char prefix[] = "wss://";
    return config != NULL &&
           bounded_string(config->server_url, sizeof(config->server_url),
                          sizeof(prefix)) &&
           strncmp(config->server_url, prefix, sizeof(prefix) - 1) == 0 &&
           bounded_string(config->device_secret, sizeof(config->device_secret),
                          PILL_AI_CONFIG_SECRET_BYTES);
}
