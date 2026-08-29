#include "board_support/board.h"
#include "board_support/rtc_decode.h"

#include "driver/i2c_master.h"
#include "esp_check.h"

enum { DS3231_ADDRESS = 0x68 };

esp_err_t board_rtc_read(struct tm *local_time, bool *time_valid)
{
    ESP_RETURN_ON_FALSE(local_time != NULL && time_valid != NULL,
                        ESP_ERR_INVALID_ARG, "rtc", "invalid output");
    *time_valid = false;

    i2c_master_dev_handle_t device = NULL;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_FALSE(board_rtc_i2c_bus() != NULL, ESP_ERR_INVALID_STATE,
                        "rtc", "RTC I2C bus unavailable");
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_rtc_i2c_bus(), &config, &device),
                        "rtc", "attach DS3231");

    uint8_t start = 0;
    uint8_t registers[7] = {0};
    esp_err_t error = i2c_master_transmit_receive(device, &start, 1, registers,
                                                  sizeof(registers), 100);
    i2c_master_bus_rm_device(device);
    if (error != ESP_OK) { return error; }

    *time_valid = board_rtc_decode(registers, local_time);
    return ESP_OK;
}

esp_err_t board_rtc_write(const struct tm *local_time)
{
    uint8_t payload[8] = {0};
    ESP_RETURN_ON_FALSE(board_rtc_encode(local_time, &payload[1]),
                        ESP_ERR_INVALID_ARG, "rtc", "invalid time");
    ESP_RETURN_ON_FALSE(board_rtc_i2c_bus() != NULL, ESP_ERR_INVALID_STATE,
                        "rtc", "RTC I2C bus unavailable");

    i2c_master_dev_handle_t device = NULL;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_rtc_i2c_bus(), &config, &device),
                        "rtc", "attach DS3231");
    esp_err_t error = i2c_master_transmit(device, payload, sizeof(payload), 100);
    if (error == ESP_OK) {
        uint8_t status_register = 0x0f;
        uint8_t status = 0;
        error = i2c_master_transmit_receive(device, &status_register, 1, &status, 1, 100);
        if (error == ESP_OK) {
            uint8_t clear_osf[2] = {0x0f, (uint8_t)(status & 0x7fU)};
            error = i2c_master_transmit(device, clear_osf, sizeof(clear_osf), 100);
        }
    }
    i2c_master_bus_rm_device(device);
    return error;
}
