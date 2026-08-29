#include "board_support/board_contract.h"

#include <math.h>

bool board_servo_config_valid(uint16_t minimum_us,
                              uint16_t maximum_us,
                              uint16_t closed_us,
                              uint16_t release_us)
{
    return minimum_us >= 500 && maximum_us <= 2500 && minimum_us < maximum_us &&
           closed_us >= minimum_us && closed_us <= maximum_us && release_us >= minimum_us &&
           release_us <= maximum_us && closed_us != release_us;
}

uint16_t board_pca9685_pulse_count(uint16_t pulse_us)
{
    return (uint16_t)(((uint32_t)pulse_us * 4096U + 10000U) / 20000U);
}

bool board_audio_config_valid(uint32_t sample_rate,
                              uint8_t bits,
                              uint8_t channels,
                              uint8_t volume)
{
    return sample_rate == BOARD_AUDIO_SAMPLE_RATE && bits == BOARD_AUDIO_BITS &&
           channels == BOARD_AUDIO_CHANNELS && volume <= 100;
}

bool board_rotary_alignment_acceptable(float signed_error_degrees,
                                        float tolerance_degrees)
{
    return isfinite(signed_error_degrees) && isfinite(tolerance_degrees) &&
           tolerance_degrees >= 0.0f &&
           fabsf(signed_error_degrees) <= tolerance_degrees;
}
