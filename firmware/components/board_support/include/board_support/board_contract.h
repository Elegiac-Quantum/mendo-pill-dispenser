#ifndef BOARD_SUPPORT_BOARD_CONTRACT_H
#define BOARD_SUPPORT_BOARD_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

enum {
    BOARD_XL9555_PORT0_SAFE_OUTPUT = 1U << 3,
    BOARD_XL9555_PORT0_BOOT_OUTPUT = 0x08,
    BOARD_XL9555_PORT1_BOOT_OUTPUT = 0x0d,
    BOARD_XL9555_PORT0_BOOT_CONFIG = 0x03,
    BOARD_XL9555_PORT1_BOOT_CONFIG = 0xf0,
    BOARD_LCD_BGR_ORDER = 1,
    BOARD_TF_FORMAT_IF_MOUNT_FAILED = 0,
    BOARD_I2C_SDA_GPIO = 41,
    BOARD_I2C_SCL_GPIO = 42,
    BOARD_RTC_I2C_SDA_GPIO = 17,
    BOARD_RTC_I2C_SCL_GPIO = 18,
    BOARD_SPI_MOSI_GPIO = 11,
    BOARD_SPI_SCLK_GPIO = 12,
    BOARD_SPI_MISO_GPIO = 13,
    BOARD_LCD_CS_GPIO = 21,
    BOARD_LCD_DC_GPIO = 40,
    BOARD_TF_CS_GPIO = 2,
    BOARD_AUDIO_MCLK_GPIO = 3,
    BOARD_AUDIO_BCLK_GPIO = 46,
    BOARD_AUDIO_LRCLK_GPIO = 9,
    BOARD_AUDIO_DATA_OUT_GPIO = 10,
    BOARD_AUDIO_DATA_IN_GPIO = 14,
    BOARD_AUDIO_SAMPLE_RATE = 16000,
    BOARD_AUDIO_BITS = 16,
    BOARD_AUDIO_CHANNELS = 1,
    BOARD_AUDIO_FIRST_TEST_VOLUME = 35,
    BOARD_PCA9685_SDA_GPIO = 47,
    BOARD_PCA9685_SCL_GPIO = 48,
    BOARD_PCA9685_ADDRESS = 0x40,
    BOARD_PATIENT_BUTTON_GPIO = 4,
    BOARD_MT6701_SDA_GPIO = 39,
    BOARD_MT6701_SCL_GPIO = 38,
    BOARD_MT6701_ADDRESS = 0x06,
    BOARD_USB_D_MINUS_GPIO = 19,
    BOARD_USB_D_PLUS_GPIO = 20,
};

bool board_servo_config_valid(uint16_t minimum_us,
                              uint16_t maximum_us,
                              uint16_t closed_us,
                              uint16_t release_us);
uint16_t board_pca9685_pulse_count(uint16_t pulse_us);
bool board_audio_config_valid(uint32_t sample_rate,
                              uint8_t bits,
                              uint8_t channels,
                              uint8_t volume);
bool board_rotary_alignment_acceptable(float signed_error_degrees,
                                        float tolerance_degrees);

#endif
