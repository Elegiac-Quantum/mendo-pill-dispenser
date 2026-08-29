#ifndef BOARD_SUPPORT_AUDIO_H
#define BOARD_SUPPORT_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint32_t write_calls;
    uint32_t frames_submitted;
    int last_write_result;
    int reg_chip_power;
    int reg_dac_power;
    int reg_dac_format;
    int reg_dac_rate;
    int reg_left_volume;
    int reg_right_volume;
    int reg_mute;
    int reg_left_mixer;
    int reg_right_mixer;
    int reg_speaker_left;
    int reg_speaker_right;
} board_audio_diagnostics_t;

esp_err_t board_audio_init(void);
esp_err_t board_audio_set_volume(uint8_t percent);
esp_err_t board_audio_write(const int16_t *mono, size_t frames, size_t *written);
esp_err_t board_audio_read(int16_t *mono, size_t frames, size_t *read_frames);
esp_err_t board_audio_read_stereo(int16_t *left, int16_t *right,
                                  size_t frames, size_t *read_frames);
esp_err_t board_audio_stop(void);
bool board_audio_ready(void);
esp_err_t board_audio_get_diagnostics(board_audio_diagnostics_t *diagnostics);

#endif
