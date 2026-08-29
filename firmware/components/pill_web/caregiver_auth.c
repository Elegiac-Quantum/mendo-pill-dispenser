#include "pill_web/caregiver_auth.h"

#include <stdint.h>
#include <string.h>

#include "esp_random.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

enum {
    PIN_SALT_BYTES = 16,
    PIN_HASH_BYTES = 32,
};

static nvs_handle_t s_nvs;
static bool s_enabled;

static bool load_blob(const char *key, void *value, size_t bytes)
{
    size_t length = bytes;
    return s_nvs != 0 &&
           nvs_get_blob(s_nvs, key, value, &length) == ESP_OK &&
           length == bytes;
}

static bool pin_hash(const uint8_t salt[PIN_SALT_BYTES], const char *pin,
                     uint8_t output[PIN_HASH_BYTES])
{
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    int result = mbedtls_sha256_starts(&context, false);
    if (result == 0) {
        result = mbedtls_sha256_update(&context, salt, PIN_SALT_BYTES);
    }
    if (result == 0) {
        result = mbedtls_sha256_update(
            &context, (const unsigned char *)pin, strlen(pin));
    }
    if (result == 0) result = mbedtls_sha256_finish(&context, output);
    mbedtls_sha256_free(&context);
    return result == 0;
}

esp_err_t caregiver_auth_init(void)
{
    esp_err_t result = nvs_open("caregiver", NVS_READWRITE, &s_nvs);
    if (result != ESP_OK) return result;
    uint8_t salt[PIN_SALT_BYTES];
    uint8_t hash[PIN_HASH_BYTES];
    s_enabled = load_blob("salt", salt, sizeof(salt)) &&
                load_blob("hash", hash, sizeof(hash));
    return ESP_OK;
}

bool caregiver_auth_enabled(void)
{
    return s_enabled;
}

bool caregiver_auth_pin_valid(const char *pin)
{
    if (pin == NULL) return false;
    size_t length = strlen(pin);
    if (length < 4 || length > 8) return false;
    for (size_t index = 0; index < length; ++index) {
        if (pin[index] < '0' || pin[index] > '9') return false;
    }
    return true;
}

bool caregiver_auth_verify(const char *pin)
{
    if (!s_enabled || !caregiver_auth_pin_valid(pin)) return false;
    uint8_t salt[PIN_SALT_BYTES];
    uint8_t expected[PIN_HASH_BYTES];
    uint8_t actual[PIN_HASH_BYTES];
    if (!load_blob("salt", salt, sizeof(salt)) ||
        !load_blob("hash", expected, sizeof(expected)) ||
        !pin_hash(salt, pin, actual)) {
        return false;
    }
    uint8_t difference = 0;
    for (size_t index = 0; index < sizeof(actual); ++index) {
        difference |= actual[index] ^ expected[index];
    }
    return difference == 0;
}

esp_err_t caregiver_auth_set_pin(const char *pin)
{
    if (!caregiver_auth_pin_valid(pin)) return ESP_ERR_INVALID_ARG;
    uint8_t salt[PIN_SALT_BYTES];
    uint8_t hash[PIN_HASH_BYTES];
    esp_fill_random(salt, sizeof(salt));
    if (!pin_hash(salt, pin, hash)) return ESP_FAIL;
    esp_err_t result = nvs_set_blob(s_nvs, "salt", salt, sizeof(salt));
    if (result == ESP_OK) {
        result = nvs_set_blob(s_nvs, "hash", hash, sizeof(hash));
    }
    if (result == ESP_OK) result = nvs_commit(s_nvs);
    if (result == ESP_OK) s_enabled = true;
    return result;
}

esp_err_t caregiver_auth_disable(void)
{
    esp_err_t result = nvs_erase_key(s_nvs, "salt");
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    esp_err_t hash_result = nvs_erase_key(s_nvs, "hash");
    if (hash_result == ESP_ERR_NVS_NOT_FOUND) hash_result = ESP_OK;
    if (result == ESP_OK) result = hash_result;
    if (result == ESP_OK) result = nvs_commit(s_nvs);
    if (result == ESP_OK) s_enabled = false;
    return result;
}
