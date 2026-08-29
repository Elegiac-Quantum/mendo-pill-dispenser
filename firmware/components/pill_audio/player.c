#include "pill_audio/player.h"

#include <stdint.h>
#include <string.h>

bool pill_audio_path_valid(const char *path)
{
    static const char prefix[] = "/sdcard/smartpill/audio/";
    if (path == NULL || strncmp(path, prefix, sizeof(prefix) - 1) != 0) return false;
    const char *name = path + sizeof(prefix) - 1;
    size_t length = strlen(path);
    size_t name_length = strlen(name);
    if (length >= 192 || name_length <= 4 || strstr(name, "..") != NULL ||
        strchr(name, '/') != NULL || strchr(name, '\\') != NULL) return false;
    return strcmp(name + name_length - 4, ".wav") == 0;
}

bool pill_audio_test_policy_valid(unsigned volume, unsigned duration_ms,
                                  unsigned peak_amplitude)
{
    return volume <= 35 && duration_ms <= 500 && peak_amplitude <= 8192;
}

#ifdef ESP_PLATFORM
#include <stdio.h>
#include "board_support/audio.h"
#include "board_support/board_contract.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pill_audio/wav_reader.h"

static volatile pill_audio_state_t s_state = PILL_AUDIO_IDLE;
static volatile bool s_cancel;
static char s_file_path[192];
static uint8_t s_file_volume;

static const int16_t sine_lut[32] = {
       0, 1600, 3136, 4552, 5792, 6808, 7568, 8032,
    8192, 8032, 7568, 6808, 5792, 4552, 3136, 1600,
       0,-1600,-3136,-4552,-5792,-6808,-7568,-8032,
   -8192,-8032,-7568,-6808,-5792,-4552,-3136,-1600,
};

static void tone_task(void *unused)
{
    (void)unused;
    int16_t samples[128];
    uint32_t phase = 0;
    const uint32_t increment = (uint32_t)(((uint64_t)440 << 32) / 16000);
    bool ok = board_audio_set_volume(BOARD_AUDIO_FIRST_TEST_VOLUME) == ESP_OK;
    for (unsigned produced = 0; ok && produced < 8000 && !s_cancel; produced += 128) {
        for (size_t i = 0; i < 128; ++i) {
            samples[i] = sine_lut[phase >> 27];
            phase += increment;
        }
        size_t written = 0;
        ok = board_audio_write(samples, 128, &written) == ESP_OK && written == 128;
    }
    board_audio_stop();
    s_state = ok ? PILL_AUDIO_IDLE : PILL_AUDIO_FAULT;
    vTaskDelete(NULL);
}

static void file_task(void *unused)
{
    (void)unused;
    bool ok = false;
    FILE *file = fopen(s_file_path, "rb");
    uint8_t header[4096];
    pill_wav_info_t info;
    if (file != NULL) {
        size_t header_bytes = fread(header, 1, sizeof(header), file);
        ok = pill_wav_parse(header, header_bytes, &info) &&
             fseek(file, (long)info.data_offset, SEEK_SET) == 0 &&
             board_audio_set_volume(s_file_volume) == ESP_OK;
    }
    uint8_t bytes[512];
    uint32_t remaining = ok ? info.data_bytes : 0;
    while (ok && remaining > 0 && !s_cancel) {
        size_t requested = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
        size_t received = fread(bytes, 1, requested, file);
        if (received != requested || (received & 1u) != 0) {
            ok = false;
            break;
        }
        int16_t samples[256];
        for (size_t i = 0; i < received / 2; ++i)
            samples[i] = (int16_t)((uint16_t)bytes[i * 2] |
                                   ((uint16_t)bytes[i * 2 + 1] << 8));
        size_t written = 0;
        ok = board_audio_write(samples, received / 2, &written) == ESP_OK &&
             written == received / 2;
        remaining -= (uint32_t)received;
    }
    if (file != NULL) fclose(file);
    board_audio_stop();
    s_state = ok ? PILL_AUDIO_IDLE : PILL_AUDIO_FAULT;
    vTaskDelete(NULL);
}

bool pill_audio_test_tone(void)
{
    if (!board_audio_ready() || s_state != PILL_AUDIO_IDLE ||
        !pill_audio_test_policy_valid(BOARD_AUDIO_FIRST_TEST_VOLUME, 500, 8192)) return false;
    s_cancel = false;
    s_state = PILL_AUDIO_PLAYING;
    if (xTaskCreate(tone_task, "safe_tone", 3072, NULL, 4, NULL) != pdPASS) {
        s_state = PILL_AUDIO_FAULT;
        board_audio_stop();
        return false;
    }
    return true;
}

bool pill_audio_play_file(const char *path)
{
    return pill_audio_play_file_at_volume(path,
                                          BOARD_AUDIO_FIRST_TEST_VOLUME);
}

bool pill_audio_play_file_at_volume(const char *path, unsigned volume)
{
    if (!pill_audio_path_valid(path) || !board_audio_ready() ||
        s_state != PILL_AUDIO_IDLE || volume > 100) return false;
    memcpy(s_file_path, path, strlen(path) + 1);
    s_file_volume = (uint8_t)volume;
    s_cancel = false;
    s_state = PILL_AUDIO_PLAYING;
    /* WAV parsing keeps a 4 KiB header plus I/O buffers on this task's stack.
     * Leave enough headroom for FATFS/newlib calls at maximum-volume playback. */
    if (xTaskCreate(file_task, "wav_player", 10240, NULL, 4, NULL) != pdPASS) {
        s_state = PILL_AUDIO_FAULT;
        board_audio_stop();
        return false;
    }
    return true;
}

void pill_audio_stop(void)
{
    if (s_state == PILL_AUDIO_PLAYING) {
        s_state = PILL_AUDIO_STOPPING;
        s_cancel = true;
    } else {
        board_audio_stop();
        /* A failed playback task has already exited, so FAULT must be
         * recoverable.  Keeping it latched permanently blocks wake word,
         * button and web-started AI conversations until a power cycle. */
        s_state = PILL_AUDIO_IDLE;
    }
}

pill_audio_state_t pill_audio_state(void) { return s_state; }
#else
bool pill_audio_test_tone(void) { return false; }
bool pill_audio_play_file(const char *path) { (void)path; return false; }
bool pill_audio_play_file_at_volume(const char *path, unsigned volume)
{
    (void)path;
    (void)volume;
    return false;
}
void pill_audio_stop(void) {}
pill_audio_state_t pill_audio_state(void) { return PILL_AUDIO_IDLE; }
#endif
