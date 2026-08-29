#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pill_audio/wav_reader.h"

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void valid_header(uint8_t wav[44], uint32_t data_bytes)
{
    memset(wav, 0, 44);
    memcpy(wav, "RIFF", 4);
    put32(wav + 4, 36 + data_bytes);
    memcpy(wav + 8, "WAVEfmt ", 8);
    put32(wav + 16, 16);
    put16(wav + 20, 1);
    put16(wav + 22, 1);
    put32(wav + 24, 16000);
    put32(wav + 28, 32000);
    put16(wav + 32, 2);
    put16(wav + 34, 16);
    memcpy(wav + 36, "data", 4);
    put32(wav + 40, data_bytes);
}

static void test_accepts_safe_pcm(void)
{
    uint8_t wav[44];
    valid_header(wav, 16000);
    pill_wav_info_t info;
    assert(pill_wav_parse(wav, sizeof(wav), &info));
    assert(info.data_offset == 44);
    assert(info.data_bytes == 16000);
    assert(info.sample_rate == 16000);
    assert(info.duration_ms == 500);
}

static void test_rejects_wrong_format(void)
{
    uint8_t wav[44];
    valid_header(wav, 16000);
    wav[0] = 'X';
    assert(!pill_wav_parse(wav, sizeof(wav), NULL));
    valid_header(wav, 16000);
    put16(wav + 22, 2);
    assert(!pill_wav_parse(wav, sizeof(wav), NULL));
    valid_header(wav, 16000);
    put32(wav + 24, 44100);
    assert(!pill_wav_parse(wav, sizeof(wav), NULL));
}

static void test_rejects_inconsistent_and_oversized_audio(void)
{
    uint8_t wav[44];
    valid_header(wav, 16000);
    put32(wav + 28, 1234);
    assert(!pill_wav_parse(wav, sizeof(wav), NULL));
    valid_header(wav, 1600002);
    assert(!pill_wav_parse(wav, sizeof(wav), NULL));
    assert(!pill_wav_parse(wav, 20, NULL));
}

int main(void)
{
    test_accepts_safe_pcm();
    test_rejects_wrong_format();
    test_rejects_inconsistent_and_oversized_audio();
    puts("wav reader tests passed");
    return 0;
}
