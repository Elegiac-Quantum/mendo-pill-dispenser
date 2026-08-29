#include "board_support/board.h"

#include <math.h>
#include <string.h>

#include "board_support/board_contract.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

enum {
    SERVO_MINIMUM_US = 500,
    SERVO_MAXIMUM_US = 2500,
    SERVO_HOME_US = 1500,
    SERVO_RETRACT_US = 2500,
    ROTARY_FORWARD_US = 2000,
    ROTARY_FINE_FORWARD_US = 1800,
    ROTARY_FINE_BREAKAWAY_US = 1850,
    ROTARY_SLOT_COUNT = 15,
    ROTARY_TOLERANCE_TENTHS = 10,
    ROTARY_SLOT_RECOGNITION_TENTHS = 30,
    ROTARY_CORRECTION_MS = 25,
    ROTARY_CORRECTION_ATTEMPTS = 80,
    ROTARY_TIMEOUT_MS = 8000,
    PCA9685_MODE1 = 0x00,
    PCA9685_MODE2 = 0x01,
    PCA9685_LED0_ON_L = 0x06,
    PCA9685_ALL_LED_OFF_H = 0xfd,
    PCA9685_PRESCALE = 0xfe,
};

static float clockwise_distance(float target, float current)
{
    float distance = target - current;
    if (distance < 0.0f) distance += 360.0f;
    return distance;
}

static const char *TAG = "servo_pca9685";
static bool s_initialized;
static SemaphoreHandle_t s_motion_mutex;
static float s_motion_start;
static float s_motion_target;
static float s_motion_final;
static float s_motion_error;
static const char *s_motion_reason = "not_run";
static float s_startup_zero;
static bool s_startup_zero_valid;

static float normalize_degrees(float degrees)
{
    while (degrees < 0.0f) degrees += 360.0f;
    while (degrees >= 360.0f) degrees -= 360.0f;
    return degrees;
}

void board_servo_capture_startup_zero(float degrees)
{
    nvs_handle_t nvs = 0;
    float saved = 0.0f;
    size_t size = sizeof(saved);
    bool restored = nvs_open("rotary_cal", NVS_READONLY, &nvs) == ESP_OK &&
                    nvs_get_blob(nvs, "zero_deg", &saved, &size) == ESP_OK &&
                    size == sizeof(saved) && isfinite(saved) && saved >= 0.0f &&
                    saved < 360.0f;
    if (nvs != 0) nvs_close(nvs);
    s_startup_zero = restored ? saved : normalize_degrees(degrees);
    s_startup_zero_valid = true;
    ESP_LOGI(TAG, "tray zero %s at %.2f deg (%d slots, %.2f deg/slot)",
             restored ? "restored" : "captured", s_startup_zero, ROTARY_SLOT_COUNT,
             360.0f / (float)ROTARY_SLOT_COUNT);
}

esp_err_t board_servo_set_current_zero(float *degrees)
{
    if (!s_initialized || s_motion_mutex == NULL ||
        xSemaphoreTake(s_motion_mutex, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t raw = 0;
    float current = 0.0f;
    esp_err_t result = board_mt6701_read(&raw, &current);
    if (result == ESP_OK) {
        current = normalize_degrees(current);
        nvs_handle_t nvs = 0;
        result = nvs_open("rotary_cal", NVS_READWRITE, &nvs);
        if (result == ESP_OK) result = nvs_set_blob(nvs, "zero_deg", &current, sizeof(current));
        if (result == ESP_OK) result = nvs_commit(nvs);
        if (nvs != 0) nvs_close(nvs);
        if (result == ESP_OK) {
            s_startup_zero = current;
            s_startup_zero_valid = true;
            if (degrees != NULL) *degrees = current;
            ESP_LOGI(TAG, "caregiver set tray zero at %.2f deg raw=%u", current, raw);
        }
    }
    xSemaphoreGive(s_motion_mutex);
    return result;
}

void board_servo_last_motion(float *start, float *target, float *final,
                             float *error, const char **reason)
{
    if (start != NULL) *start = s_motion_start;
    if (target != NULL) *target = s_motion_target;
    if (final != NULL) *final = s_motion_final;
    if (error != NULL) *error = s_motion_error;
    if (reason != NULL) *reason = s_motion_reason;
}

static void i2c_delay(void) { esp_rom_delay_us(5); }
static void sda(bool high) { gpio_set_level(BOARD_PCA9685_SDA_GPIO, high); }
static void scl(bool high) { gpio_set_level(BOARD_PCA9685_SCL_GPIO, high); }

static void i2c_start(void)
{
    sda(true); scl(true); i2c_delay();
    sda(false); i2c_delay(); scl(false);
}

static void i2c_stop(void)
{
    sda(false); i2c_delay(); scl(true); i2c_delay(); sda(true); i2c_delay();
}

static bool i2c_write_byte(uint8_t value)
{
    for (int bit = 7; bit >= 0; --bit) {
        sda((value & (1U << bit)) != 0);
        i2c_delay(); scl(true); i2c_delay(); scl(false);
    }
    sda(true);
    i2c_delay(); scl(true); i2c_delay();
    bool acknowledged = gpio_get_level(BOARD_PCA9685_SDA_GPIO) == 0;
    scl(false);
    return acknowledged;
}

static esp_err_t write_registers(uint8_t reg, const uint8_t *data, size_t length)
{
    i2c_start();
    bool ok = i2c_write_byte((uint8_t)(BOARD_PCA9685_ADDRESS << 1)) &&
              i2c_write_byte(reg);
    for (size_t i = 0; ok && i < length; ++i) {
        ok = i2c_write_byte(data[i]);
    }
    i2c_stop();
    return ok ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    return write_registers(reg, &value, 1);
}

static esp_err_t set_pulse(uint8_t channel, uint16_t pulse_us)
{
    if (channel >= 16) return ESP_ERR_INVALID_ARG;
    uint16_t count = board_pca9685_pulse_count(pulse_us);
    const uint8_t pwm[] = {0, 0, (uint8_t)count, (uint8_t)(count >> 8)};
    return write_registers((uint8_t)(PCA9685_LED0_ON_L + channel * 4),
                           pwm, sizeof(pwm));
}

static esp_err_t disable_channel(uint8_t channel)
{
    if (channel >= 16) return ESP_ERR_INVALID_ARG;
    const uint8_t off[] = {0, 0, 0, 0x10};
    return write_registers((uint8_t)(PCA9685_LED0_ON_L + channel * 4),
                           off, sizeof(off));
}

esp_err_t board_servo_init(void)
{
    if (!board_servo_config_valid(SERVO_MINIMUM_US, SERVO_MAXIMUM_US,
                                  SERVO_HOME_US, SERVO_RETRACT_US)) {
        return ESP_ERR_INVALID_ARG;
    }

    const gpio_config_t pins = {
        .pin_bit_mask = (1ULL << BOARD_PCA9685_SDA_GPIO) |
                        (1ULL << BOARD_PCA9685_SCL_GPIO),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pins), TAG, "configure software I2C");
    sda(true); scl(true);

    /* Sleep, set 50 Hz, then restart with register auto-increment enabled. */
    ESP_RETURN_ON_ERROR(write_register(PCA9685_MODE1, 0x10), TAG, "PCA9685 not detected");
    ESP_RETURN_ON_ERROR(write_register(PCA9685_ALL_LED_OFF_H, 0x10), TAG,
                        "keep outputs off");
    ESP_RETURN_ON_ERROR(write_register(PCA9685_PRESCALE, 121), TAG, "set frequency");
    ESP_RETURN_ON_ERROR(write_register(PCA9685_MODE2, 0x04), TAG, "set output mode");
    ESP_RETURN_ON_ERROR(write_register(PCA9685_MODE1, 0xa1), TAG, "start PWM");
    vTaskDelay(pdMS_TO_TICKS(2));
    ESP_RETURN_ON_ERROR(write_register(PCA9685_ALL_LED_OFF_H, 0x10), TAG, "outputs off");

    s_motion_mutex = xSemaphoreCreateMutex();
    if (s_motion_mutex == NULL) { return ESP_ERR_NO_MEM; }
    s_initialized = true;
    return ESP_OK;
}

bool board_servo_cycle(void *unused)
{
    (void)unused;
    return board_servo_cycle_channel(0);
}

bool board_servo_cycle_channel(uint8_t channel)
{
    if (channel != 0) return false;
    if (!s_initialized || s_motion_mutex == NULL ||
        xSemaphoreTake(s_motion_mutex, 0) != pdTRUE) {
        return false;
    }

    bool success = false;
    s_motion_reason = "starting";
    float previous = 0.0f;
    float current = 0.0f;
    uint16_t raw = 0;
    if (!s_startup_zero_valid) {
        s_motion_reason = "startup_zero_missing";
        goto done;
    }
    if (board_mt6701_read(&raw, &previous) != ESP_OK) goto done;

    current = previous;
    s_motion_start = previous;
    const float slot_degrees = 360.0f / (float)ROTARY_SLOT_COUNT;
    float landing_zero = s_startup_zero;
    float from_zero = clockwise_distance(previous, landing_zero);
    float slot_position = from_zero / slot_degrees;
    int lower_slot = (int)slot_position;
    float fraction = slot_position - (float)lower_slot;
    float recognition_fraction =
        (ROTARY_SLOT_RECOGNITION_TENTHS / 10.0f) / slot_degrees;
    bool aligned = fraction <= recognition_fraction ||
                   fraction >= 1.0f - recognition_fraction;
    int target_slot = aligned
                          ? ((int)(slot_position + 0.5f) + 1) % ROTARY_SLOT_COUNT
                          : (lower_slot + 1) % ROTARY_SLOT_COUNT;
    float target = landing_zero + (float)target_slot * slot_degrees;
    if (target < 0.0f) target += 360.0f;
    if (target >= 360.0f) target -= 360.0f;
    s_motion_target = target;
    float previous_error = clockwise_distance(target, current);
    if (previous_error > 36.0f) goto done;
    int stagnant_attempts = 0;
    for (int attempt = 0; attempt <= ROTARY_CORRECTION_ATTEMPTS; ++attempt) {
        float forward_error = clockwise_distance(target, current);
        float signed_error = forward_error > 180.0f ? forward_error - 360.0f
                                                    : forward_error;
        if (fabsf(signed_error) * 10.0f <= ROTARY_TOLERANCE_TENTHS) {
            /* Confirm the tray remains aligned after all gearbox coast ends. */
            vTaskDelay(pdMS_TO_TICKS(700));
            if (board_mt6701_read(&raw, &current) != ESP_OK) goto done;
            float settled = clockwise_distance(target, current);
            if (settled > 180.0f) settled -= 360.0f;
            if (fabsf(settled) * 10.0f <= ROTARY_TOLERANCE_TENTHS) {
                success = true;
                s_motion_reason = "settled_at_target";
                break;
            }
            continue;
        }
        /* Never reverse: gearbox backlash makes the physical tray lose alignment. */
        if (signed_error < 0.0f) {
            /* Overshoot cannot be corrected by this forward-only mechanism.
             * Stop safely, but report a positioning fault unless the settled
             * error is still inside the same tolerance used above. */
            success = board_rotary_alignment_acceptable(
                signed_error, ROTARY_TOLERANCE_TENTHS / 10.0f);
            s_motion_reason = success ? "settled_at_target" : "overshot_target";
            break;
        }
        if (signed_error > 36.0f || attempt == ROTARY_CORRECTION_ATTEMPTS) {
            s_motion_reason = signed_error > 36.0f ? "position_lost"
                                                  : "attempt_limit";
            break;
        }
        uint16_t drive = ROTARY_FORWARD_US;
        if (signed_error <= 5.0f) {
            drive = stagnant_attempts >= 3 ? ROTARY_FINE_BREAKAWAY_US
                                           : ROTARY_FINE_FORWARD_US;
        }
        if (set_pulse(channel, drive) != ESP_OK) goto done;
        vTaskDelay(pdMS_TO_TICKS(ROTARY_CORRECTION_MS));
        (void)disable_channel(channel);
        /* MG996R coasts after PWM stops. Judge only after the tray is still. */
        vTaskDelay(pdMS_TO_TICKS(550));
        if (board_mt6701_read(&raw, &current) != ESP_OK) goto done;
        float new_error = clockwise_distance(target, current);
        float new_signed = new_error > 180.0f ? new_error - 360.0f : new_error;
        if (fabsf(signed_error) - fabsf(new_signed) < 0.08f) ++stagnant_attempts;
        else stagnant_attempts = 0;
        if (stagnant_attempts >= 8) {
            s_motion_reason = "stalled";
            break;
        }
        previous_error = new_error;
    }

done:
    (void)disable_channel(channel);
    if (board_mt6701_read(&raw, &current) == ESP_OK) {
        s_motion_final = current;
        s_motion_error = clockwise_distance(s_motion_target, current);
        if (s_motion_error > 180.0f) s_motion_error -= 360.0f;
    }
    if (!success && strcmp(s_motion_reason, "starting") == 0) {
        s_motion_reason = "read_or_drive_error";
    }
    xSemaphoreGive(s_motion_mutex);
    return success;
}
