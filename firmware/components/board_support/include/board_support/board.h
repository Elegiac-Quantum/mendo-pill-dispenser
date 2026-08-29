#ifndef BOARD_SUPPORT_BOARD_H
#define BOARD_SUPPORT_BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "board_support/display.h"

typedef struct {
    bool i2c;
    bool io_expander;
    bool audio_codec;
    bool rtc;
    bool display;
    bool tf_card;
    bool servo_ready;
    bool mt6701;
} board_health_t;

esp_err_t board_init(board_health_t *health);
const board_health_t *board_health(void);
void board_tf_card_retry(void);

esp_err_t board_servo_init(void);
void board_servo_capture_startup_zero(float degrees);
esp_err_t board_servo_set_current_zero(float *degrees);
bool board_servo_cycle(void *unused);
bool board_servo_cycle_channel(uint8_t channel);
void board_servo_last_motion(float *start, float *target, float *final,
                             float *error, const char **reason);
esp_err_t board_patient_button_init(void);
bool board_patient_button_pressed(void);
uint32_t board_patient_button_press_count(void);

esp_err_t board_rtc_read(struct tm *local_time, bool *time_valid);
esp_err_t board_rtc_write(const struct tm *local_time);
esp_err_t board_display_show(const board_display_view_t *view);
esp_err_t board_display_set_rotation(bool rotate_180);
bool board_display_rotation(void);
esp_err_t board_tf_card_init(uint64_t *capacity_bytes);
i2c_master_bus_handle_t board_i2c_bus(void);
i2c_master_bus_handle_t board_rtc_i2c_bus(void);
esp_err_t board_mt6701_init(void);
esp_err_t board_mt6701_read(uint16_t *raw_angle, float *degrees);
void board_mt6701_diagnose(int *sda_level, int *scl_level,
                           int *acknowledged_address);
bool board_mt6701_diagnose_timing(int *half_period_us);
bool board_mt6701_scan_safe_p1(int *sda_gpio, int *scl_gpio);
bool board_mt6701_scan_known_buses(int *sda_gpio, int *scl_gpio);

#endif
