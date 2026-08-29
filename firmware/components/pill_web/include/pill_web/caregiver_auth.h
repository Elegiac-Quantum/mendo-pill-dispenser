#ifndef PILL_WEB_CAREGIVER_AUTH_H
#define PILL_WEB_CAREGIVER_AUTH_H

#include <stdbool.h>

#include "esp_err.h"

esp_err_t caregiver_auth_init(void);
bool caregiver_auth_enabled(void);
bool caregiver_auth_pin_valid(const char *pin);
bool caregiver_auth_verify(const char *pin);
esp_err_t caregiver_auth_set_pin(const char *pin);
esp_err_t caregiver_auth_disable(void);

#endif
