#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pill_ai_transport/config_validation.h"

static void test_wifi_configuration_bounds(void)
{
    pill_ai_wifi_config_t config = {0};
    strcpy(config.ssid, "Hospital-Guest");
    strcpy(config.password, "safe-password");
    assert(pill_ai_wifi_config_valid(&config));

    config.ssid[0] = '\0';
    assert(!pill_ai_wifi_config_valid(&config));
    strcpy(config.ssid, "Hospital-Guest");
    strcpy(config.password, "short");
    assert(!pill_ai_wifi_config_valid(&config));
}

static void test_remote_configuration_bounds(void)
{
    pill_ai_remote_config_t config = {0};
    strcpy(config.server_url, "wss://voice.example.test/device");
    memset(config.device_secret, 'a', PILL_AI_CONFIG_SECRET_BYTES);
    config.device_secret[PILL_AI_CONFIG_SECRET_BYTES] = '\0';
    assert(pill_ai_remote_config_valid(&config));

    strcpy(config.server_url, "ws://voice.example.test/device");
    assert(!pill_ai_remote_config_valid(&config));
    strcpy(config.server_url, "wss://voice.example.test/device");
    config.device_secret[PILL_AI_CONFIG_SECRET_BYTES - 1] = '\0';
    assert(!pill_ai_remote_config_valid(&config));
}

int main(void)
{
    test_wifi_configuration_bounds();
    test_remote_configuration_bounds();
    puts("ai config validation tests passed");
    return 0;
}
