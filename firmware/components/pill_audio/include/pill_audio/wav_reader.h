#ifndef PILL_AUDIO_WAV_READER_H
#define PILL_AUDIO_WAV_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t data_offset;
    uint32_t data_bytes;
    uint32_t sample_rate;
    uint32_t duration_ms;
} pill_wav_info_t;

bool pill_wav_parse(const uint8_t *header, size_t size, pill_wav_info_t *info);

#endif
