#include <assert.h>
#include <stdio.h>

#include "board_support/board_contract.h"

_Static_assert(BOARD_LCD_CS_GPIO != BOARD_TF_CS_GPIO, "SPI devices need unique CS pins");
_Static_assert(BOARD_TF_CS_GPIO == 2, "onboard TF card select must use GPIO2");
_Static_assert(BOARD_RTC_I2C_SDA_GPIO == 17 && BOARD_RTC_I2C_SCL_GPIO == 18,
               "external RTC must use the verified P1 pins");
_Static_assert(BOARD_RTC_I2C_SDA_GPIO != BOARD_I2C_SDA_GPIO &&
                   BOARD_RTC_I2C_SCL_GPIO != BOARD_I2C_SCL_GPIO,
               "external RTC must not disturb the onboard I2C bus");
_Static_assert(BOARD_SPI_MOSI_GPIO == 11 && BOARD_SPI_SCLK_GPIO == 12 &&
                   BOARD_SPI_MISO_GPIO == 13,
               "LCD and TF card must share the verified SPI2 pins");
_Static_assert(BOARD_TF_FORMAT_IF_MOUNT_FAILED == 0,
               "TF bring-up must never format user media");
_Static_assert(BOARD_LCD_BGR_ORDER == 1, "verified LCD module requires BGR color order");
_Static_assert(BOARD_PCA9685_SDA_GPIO == 47 && BOARD_PCA9685_SCL_GPIO == 48,
               "PCA9685 must use its dedicated verified P1 pins");
_Static_assert(BOARD_PATIENT_BUTTON_GPIO == 4,
               "patient button must use P1-1 GPIO4");
_Static_assert(BOARD_MT6701_SDA_GPIO == 39 && BOARD_MT6701_SCL_GPIO == 38,
               "MT6701 must use its independent P1 software-I2C pins");
_Static_assert(BOARD_AUDIO_DATA_OUT_GPIO == 10, "ESP-to-codec data pin mismatch");
_Static_assert(BOARD_AUDIO_DATA_IN_GPIO == 14, "codec-to-ESP data pin mismatch");
_Static_assert(BOARD_AUDIO_SAMPLE_RATE == 16000, "safe playback rate mismatch");
_Static_assert(BOARD_AUDIO_BITS == 16, "safe playback width mismatch");
_Static_assert(BOARD_AUDIO_CHANNELS == 1, "safe playback channel mismatch");
_Static_assert(BOARD_AUDIO_FIRST_TEST_VOLUME == 35, "diagnostic volume must stay conservative");
_Static_assert((BOARD_XL9555_PORT0_SAFE_OUTPUT & (1U << 3)) != 0,
               "active-low buzzer must boot muted");
_Static_assert(BOARD_XL9555_PORT0_BOOT_OUTPUT == 0x08,
               "board boot must keep the active-low buzzer muted");
_Static_assert(BOARD_XL9555_PORT1_BOOT_OUTPUT == 0x0d,
               "board boot must enable LCD control and backlight on bits 2 and 3");
_Static_assert(BOARD_XL9555_PORT0_BOOT_CONFIG == 0x03,
               "board boot must preserve port 0 input directions");
_Static_assert(BOARD_XL9555_PORT1_BOOT_CONFIG == 0xf0,
               "board boot must preserve key input directions");

int main(void)
{
    assert(board_servo_config_valid(500, 2500, 900, 2100));
    assert(!board_servo_config_valid(499, 2500, 900, 2100));
    assert(!board_servo_config_valid(500, 2501, 900, 2100));
    assert(!board_servo_config_valid(500, 2500, 499, 2100));
    assert(!board_servo_config_valid(500, 2500, 900, 2501));
    assert(!board_servo_config_valid(1000, 1000, 1000, 1000));
    assert(board_pca9685_pulse_count(1500) == 307);
    assert(board_pca9685_pulse_count(2500) == 512);
    assert(board_audio_config_valid(16000, 16, 1, 0));
    assert(board_audio_config_valid(16000, 16, 1, 35));
    assert(board_audio_config_valid(16000, 16, 1, 100));
    assert(!board_audio_config_valid(44100, 16, 1, 8));
    assert(!board_audio_config_valid(16000, 24, 1, 8));
    assert(!board_audio_config_valid(16000, 16, 2, 8));
    assert(board_rotary_alignment_acceptable(0.0f, 1.5f));
    assert(board_rotary_alignment_acceptable(1.5f, 1.5f));
    assert(board_rotary_alignment_acceptable(-1.5f, 1.5f));
    assert(!board_rotary_alignment_acceptable(1.51f, 1.5f));
    assert(!board_rotary_alignment_acceptable(-1.51f, 1.5f));
    puts("board contract tests passed");
    return 0;
}
