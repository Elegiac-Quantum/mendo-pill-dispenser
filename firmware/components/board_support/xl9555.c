#include "esp_check.h"
#include "driver/i2c_master.h"
#include "board_support/board_contract.h"

enum {
    XL9555_ADDRESS = 0x20,
    XL9555_OUTPUT_PORT0 = 0x02,
    XL9555_OUTPUT_PORT1 = 0x03,
    XL9555_CONFIG_PORT0 = 0x06,
    XL9555_CONFIG_PORT1 = 0x07,
};

extern i2c_master_bus_handle_t board_i2c_bus(void);

static esp_err_t write_register(i2c_master_dev_handle_t device,
                                uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), 100);
}

esp_err_t board_xl9555_init(void)
{
    i2c_master_dev_handle_t device = NULL;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = XL9555_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(board_i2c_bus(), &config, &device),
                        "xl9555", "attach");

    esp_err_t error = write_register(device, XL9555_OUTPUT_PORT0,
                                     BOARD_XL9555_PORT0_BOOT_OUTPUT);
    if (error == ESP_OK) {
        error = write_register(device, XL9555_OUTPUT_PORT1,
                               BOARD_XL9555_PORT1_BOOT_OUTPUT);
    }
    if (error == ESP_OK) {
        error = write_register(device, XL9555_CONFIG_PORT0,
                               BOARD_XL9555_PORT0_BOOT_CONFIG);
    }
    if (error == ESP_OK) {
        error = write_register(device, XL9555_CONFIG_PORT1,
                               BOARD_XL9555_PORT1_BOOT_CONFIG);
    }
    i2c_master_bus_rm_device(device);
    return error;
}
