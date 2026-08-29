#pragma once

#include <stdbool.h>

typedef enum {
    PILL_AUDIO_IDLE,
    PILL_AUDIO_PLAYING,
    PILL_AUDIO_STOPPING,
    PILL_AUDIO_FAULT,
} pill_audio_state_t;

bool pill_audio_test_policy_valid(unsigned volume, unsigned duration_ms,
                                  unsigned peak_amplitude);
bool pill_audio_path_valid(const char *path);
bool pill_audio_play_file(const char *path);
bool pill_audio_play_file_at_volume(const char *path, unsigned volume);
bool pill_audio_test_tone(void);
void pill_audio_stop(void);
pill_audio_state_t pill_audio_state(void);
