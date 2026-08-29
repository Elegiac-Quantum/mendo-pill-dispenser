#include "board_support/board.h"

#include "board_support/board_contract.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum { I2C_HALF_US = 5, MT6701_ANGLE_REGISTER = 0x03 };
static int s_i2c_half_us = I2C_HALF_US;
static adc_oneshot_unit_handle_t s_analog_adc;

static void delay_half(void) { esp_rom_delay_us((uint32_t)s_i2c_half_us); }
static void sda(bool high) { gpio_set_level(BOARD_MT6701_SDA_GPIO, high); }
static void scl(bool high) { gpio_set_level(BOARD_MT6701_SCL_GPIO, high); }

static void start_condition(void)
{
    sda(true); scl(true); delay_half();
    sda(false); delay_half(); scl(false);
}

static void stop_condition(void)
{
    sda(false); delay_half(); scl(true); delay_half(); sda(true); delay_half();
}

static bool write_byte(uint8_t value)
{
    for (int bit = 7; bit >= 0; --bit) {
        sda((value >> bit) & 1U); delay_half();
        scl(true); delay_half(); scl(false);
    }
    sda(true); delay_half(); scl(true); delay_half();
    bool acknowledged = gpio_get_level(BOARD_MT6701_SDA_GPIO) == 0;
    scl(false); return acknowledged;
}

static uint8_t read_byte(bool acknowledge)
{
    uint8_t value = 0;
    sda(true);
    for (int bit = 7; bit >= 0; --bit) {
        delay_half(); scl(true); delay_half();
        value |= (uint8_t)gpio_get_level(BOARD_MT6701_SDA_GPIO) << bit;
        scl(false);
    }
    sda(!acknowledge); delay_half(); scl(true); delay_half(); scl(false); sda(true);
    return value;
}

static bool address_acknowledges(uint8_t address)
{
    start_condition();
    bool acknowledged = write_byte((uint8_t)(address << 1));
    stop_condition();
    return acknowledged;
}

static void recover_bus(void)
{
    sda(true);
    scl(true);
    delay_half();
    for (int pulse = 0; pulse < 9; ++pulse) {
        scl(false);
        delay_half();
        scl(true);
        delay_half();
    }
    stop_condition();
}

bool board_mt6701_diagnose_timing(int *half_period_us)
{
    static const int timings_us[] = {5, 10, 25, 50};
    for (size_t index = 0; index < sizeof(timings_us) / sizeof(timings_us[0]);
         ++index) {
        s_i2c_half_us = timings_us[index];
        recover_bus();
        if (address_acknowledges(BOARD_MT6701_ADDRESS)) {
            if (half_period_us != NULL) *half_period_us = s_i2c_half_us;
            return true;
        }
    }
    s_i2c_half_us = I2C_HALF_US;
    if (half_period_us != NULL) *half_period_us = -1;
    return false;
}

static bool probe_pins(int sda_gpio, int scl_gpio)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << sda_gpio) | (1ULL << scl_gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&config) != ESP_OK) return false;
    gpio_set_level(sda_gpio, 1);
    gpio_set_level(scl_gpio, 1);
    delay_half();
    if (gpio_get_level(sda_gpio) == 0 || gpio_get_level(scl_gpio) == 0) {
        gpio_reset_pin(sda_gpio);
        gpio_reset_pin(scl_gpio);
        return false;
    }
    gpio_set_level(sda_gpio, 0);
    delay_half();
    gpio_set_level(scl_gpio, 0);
    uint8_t value = BOARD_MT6701_ADDRESS << 1;
    for (int bit = 7; bit >= 0; --bit) {
        gpio_set_level(sda_gpio, (value >> bit) & 1U);
        delay_half();
        gpio_set_level(scl_gpio, 1);
        delay_half();
        gpio_set_level(scl_gpio, 0);
    }
    gpio_set_level(sda_gpio, 1);
    delay_half();
    gpio_set_level(scl_gpio, 1);
    delay_half();
    bool acknowledged = gpio_get_level(sda_gpio) == 0;
    gpio_set_level(scl_gpio, 0);
    gpio_set_level(sda_gpio, 0);
    delay_half();
    gpio_set_level(scl_gpio, 1);
    delay_half();
    gpio_set_level(sda_gpio, 1);
    gpio_reset_pin(sda_gpio);
    gpio_reset_pin(scl_gpio);
    return acknowledged;
}

bool board_mt6701_scan_safe_p1(int *sda_gpio, int *scl_gpio)
{
    static const int candidates[] = {4, 5, 7, 16, 17, 18, 38, 39, 45, 47, 48};
    for (size_t sda_index = 0; sda_index < sizeof(candidates) / sizeof(candidates[0]);
         ++sda_index) {
        for (size_t scl_index = 0;
             scl_index < sizeof(candidates) / sizeof(candidates[0]); ++scl_index) {
            if (sda_index == scl_index) continue;
            if (probe_pins(candidates[sda_index], candidates[scl_index])) {
                if (sda_gpio != NULL) *sda_gpio = candidates[sda_index];
                if (scl_gpio != NULL) *scl_gpio = candidates[scl_index];
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    return false;
}

bool board_mt6701_scan_known_buses(int *sda_gpio, int *scl_gpio)
{
    static const int pairs[][2] = {
        {17, 18}, {18, 17}, {47, 48}, {48, 47}, {45, 48}, {48, 45},
    };
    for (size_t index = 0; index < sizeof(pairs) / sizeof(pairs[0]); ++index) {
        if (probe_pins(pairs[index][0], pairs[index][1])) {
            if (sda_gpio != NULL) *sda_gpio = pairs[index][0];
            if (scl_gpio != NULL) *scl_gpio = pairs[index][1];
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
}

void board_mt6701_diagnose(int *sda_level, int *scl_level,
                           int *acknowledged_address)
{
    sda(true);
    scl(true);
    delay_half();
    if (sda_level != NULL) *sda_level = gpio_get_level(BOARD_MT6701_SDA_GPIO);
    if (scl_level != NULL) *scl_level = gpio_get_level(BOARD_MT6701_SCL_GPIO);
    if (acknowledged_address == NULL) return;
    *acknowledged_address = -1;
    for (uint8_t address = 1; address < 0x7f; ++address) {
        if (address_acknowledges(address)) {
            *acknowledged_address = address;
            break;
        }
    }
}

esp_err_t board_mt6701_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << BOARD_MT6701_SDA_GPIO) |
                        (1ULL << BOARD_MT6701_SCL_GPIO),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), "mt6701", "configure pins");
    sda(true); scl(true);
    esp_rom_delay_us(10000);
    uint16_t raw = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        esp_err_t error = board_mt6701_read(&raw, NULL);
        if (error == ESP_OK) return ESP_OK;
        esp_rom_delay_us(5000);
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t read_i2c(uint16_t *raw_angle, float *degrees)
{
    uint8_t values[2];
    for (uint8_t index = 0; index < 2; ++index) {
        start_condition();
        if (!write_byte((BOARD_MT6701_ADDRESS << 1) | 0U) ||
            !write_byte((uint8_t)(MT6701_ANGLE_REGISTER + index))) {
            stop_condition();
            return ESP_ERR_NOT_FOUND;
        }
        start_condition();
        if (!write_byte((BOARD_MT6701_ADDRESS << 1) | 1U)) {
            stop_condition();
            return ESP_ERR_NOT_FOUND;
        }
        values[index] = read_byte(false);
        stop_condition();
    }
    uint16_t raw = ((uint16_t)values[0] << 6) | (values[1] & 0x3fU);
    *raw_angle = raw;
    if (degrees != NULL) *degrees = (float)raw * (360.0f / 16384.0f);
    return ESP_OK;
}

static esp_err_t ensure_analog_adc(void)
{
    if (s_analog_adc != NULL) return ESP_OK;
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &s_analog_adc),
                        "mt6701", "create analog ADC");
    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(s_analog_adc, ADC_CHANNEL_4,
                                      &channel_config);
}

static esp_err_t read_analog(uint16_t *raw_angle, float *degrees)
{
    ESP_RETURN_ON_ERROR(ensure_analog_adc(), "mt6701", "analog ADC");
    int64_t total = 0;
    for (int sample = 0; sample < 64; ++sample) {
        int raw = 0;
        ESP_RETURN_ON_ERROR(adc_oneshot_read(s_analog_adc, ADC_CHANNEL_4, &raw),
                            "mt6701", "analog sample");
        total += raw;
    }
    int average = (int)(total / 64);
    uint16_t raw = (uint16_t)(((uint32_t)average * 16383U + 2047U) / 4095U);
    *raw_angle = raw;
    if (degrees != NULL) *degrees = (float)raw * (360.0f / 16384.0f);
    return ESP_OK;
}

esp_err_t board_mt6701_read(uint16_t *raw_angle, float *degrees)
{
    if (raw_angle == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t error = read_i2c(raw_angle, degrees);
    if (error == ESP_OK) return ESP_OK;
    return read_analog(raw_angle, degrees);
}
