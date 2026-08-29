#include "board_support/board.h"

#include <string.h>

#include "board_support/board_contract.h"
#include "board_support/audio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

enum {
    BOARD_I2C_PORT = 0,
    BOARD_RTC_I2C_PORT = 1,
    XL9555_ADDRESS = 0x20,
    ES8388_ADDRESS = 0x10,
    DS3231_ADDRESS = 0x68,
};

static const char *TAG = "board";
static board_health_t s_health;
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_bus_handle_t s_rtc_i2c_bus;
static int64_t s_last_tf_retry_us;

extern esp_err_t board_xl9555_init(void);
extern esp_err_t board_display_init(void);

static bool probe(i2c_master_bus_handle_t bus, uint8_t address)
{
    return bus != NULL && i2c_master_probe(bus, address, 100) == ESP_OK;
}

esp_err_t board_init(board_health_t *health)
{
    memset(&s_health, 0, sizeof(s_health));

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t error = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed: %s", esp_err_to_name(error));
        if (health != NULL) { *health = s_health; }
        return error;
    }

    s_health.i2c = true;
    s_health.io_expander = probe(s_i2c_bus, XL9555_ADDRESS);
    s_health.audio_codec = probe(s_i2c_bus, ES8388_ADDRESS);
    const i2c_master_bus_config_t rtc_bus_config = {
        .i2c_port = BOARD_RTC_I2C_PORT,
        .sda_io_num = BOARD_RTC_I2C_SDA_GPIO,
        .scl_io_num = BOARD_RTC_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    error = i2c_new_master_bus(&rtc_bus_config, &s_rtc_i2c_bus);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "RTC I2C initialization failed: %s", esp_err_to_name(error));
    }
    s_health.rtc = probe(s_rtc_i2c_bus, DS3231_ADDRESS);
    s_health.mt6701 = board_mt6701_init() == ESP_OK;
    if (s_health.io_expander && board_xl9555_init() == ESP_OK) {
        s_health.display = board_display_init() == ESP_OK;
    }
    if (s_health.audio_codec) {
        s_health.audio_codec = board_audio_init() == ESP_OK;
    }
    uint64_t tf_capacity_bytes = 0;
    esp_err_t tf_error = board_tf_card_init(&tf_capacity_bytes);
    s_health.tf_card = tf_error == ESP_OK;
    if (s_health.tf_card) {
        ESP_LOGI(TAG, "TF card mounted read-only probe: %llu bytes",
                 (unsigned long long)tf_capacity_bytes);
    } else {
        ESP_LOGW(TAG, "TF card unavailable: %s", esp_err_to_name(tf_error));
    }
    s_health.servo_ready = board_servo_init() == ESP_OK;
    if (board_patient_button_init() != ESP_OK) {
        ESP_LOGW(TAG, "Patient button unavailable");
    }

    uint16_t mt_raw = 0;
    float mt_degrees = 0.0f;
    if (s_health.mt6701 && board_mt6701_read(&mt_raw, &mt_degrees) == ESP_OK) {
        board_servo_capture_startup_zero(mt_degrees);
        ESP_LOGI(TAG, "MT6701 ready: raw=%u angle=%.2f deg", mt_raw, mt_degrees);
    }
    ESP_LOGI(TAG, "XL9555=%s display=%s ES8388=%s DS3231=%s MT6701=%s servo=%s",
             s_health.io_expander ? "ready" : "missing",
             s_health.display ? "ready" : "missing",
             s_health.audio_codec ? "ready" : "missing",
             s_health.rtc ? "ready" : "missing",
             s_health.mt6701 ? "ready" : "missing",
             s_health.servo_ready ? "ready" : "fault");

    if (health != NULL) { *health = s_health; }
    return ESP_OK;
}

const board_health_t *board_health(void)
{
    return &s_health;
}

void board_tf_card_retry(void)
{
    if (s_health.tf_card) return;
    int64_t now = esp_timer_get_time();
    if (s_last_tf_retry_us != 0 && now - s_last_tf_retry_us < 5000000) return;
    s_last_tf_retry_us = now;
    uint64_t capacity_bytes = 0;
    esp_err_t error = board_tf_card_init(&capacity_bytes);
    if (error == ESP_OK) {
        s_health.tf_card = true;
        ESP_LOGI(TAG, "TF card mounted after retry: %llu bytes",
                 (unsigned long long)capacity_bytes);
    } else {
        ESP_LOGW(TAG, "TF card retry failed: %s", esp_err_to_name(error));
    }
}

i2c_master_bus_handle_t board_i2c_bus(void)
{
    return s_i2c_bus;
}

i2c_master_bus_handle_t board_rtc_i2c_bus(void)
{
    return s_rtc_i2c_bus;
}
