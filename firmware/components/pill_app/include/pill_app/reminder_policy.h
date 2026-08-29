#ifndef PILL_APP_REMINDER_POLICY_H
#define PILL_APP_REMINDER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PILL_REMINDER_NONE = 0,
    PILL_REMINDER_PLAY,
} pill_reminder_action_t;

typedef struct {
    bool active;
    bool snoozed;
    int64_t next_play_utc;
    int64_t snooze_until_utc;
    uint32_t repeat_seconds;
} pill_reminder_t;

void pill_reminder_start(pill_reminder_t *reminder,
                         int64_t now_utc,
                         uint32_t repeat_seconds);
bool pill_reminder_snooze(pill_reminder_t *reminder,
                          int64_t now_utc,
                          uint32_t snooze_seconds);
void pill_reminder_acknowledge(pill_reminder_t *reminder);
pill_reminder_action_t pill_reminder_tick(pill_reminder_t *reminder, int64_t now_utc);
const char *pill_reminder_default_audio_path(void);
const char *pill_reminder_audio_path(bool remote_ai_available);

#endif
