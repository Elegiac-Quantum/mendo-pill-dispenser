#include "pill_app/reminder_policy.h"

#include <stddef.h>

static const char DEFAULT_REMINDER_AUDIO[] =
    "/sdcard/smartpill/audio/default-reminder.wav";

const char *pill_reminder_default_audio_path(void)
{
    return DEFAULT_REMINDER_AUDIO;
}

const char *pill_reminder_audio_path(bool remote_ai_available)
{
    (void)remote_ai_available;
    return DEFAULT_REMINDER_AUDIO;
}

void pill_reminder_start(pill_reminder_t *reminder,
                         int64_t now_utc,
                         uint32_t repeat_seconds)
{
    if (reminder == NULL) {
        return;
    }
    reminder->active = repeat_seconds > 0;
    reminder->snoozed = false;
    reminder->next_play_utc = now_utc;
    reminder->snooze_until_utc = 0;
    reminder->repeat_seconds = repeat_seconds;
}

bool pill_reminder_snooze(pill_reminder_t *reminder,
                          int64_t now_utc,
                          uint32_t snooze_seconds)
{
    if (reminder == NULL || !reminder->active || snooze_seconds == 0) {
        return false;
    }
    reminder->snoozed = true;
    reminder->snooze_until_utc = now_utc + snooze_seconds;
    return true;
}

void pill_reminder_acknowledge(pill_reminder_t *reminder)
{
    if (reminder != NULL) {
        reminder->active = false;
        reminder->snoozed = false;
    }
}

pill_reminder_action_t pill_reminder_tick(pill_reminder_t *reminder, int64_t now_utc)
{
    if (reminder == NULL || !reminder->active) {
        return PILL_REMINDER_NONE;
    }
    if (reminder->snoozed) {
        if (now_utc < reminder->snooze_until_utc) {
            return PILL_REMINDER_NONE;
        }
        reminder->snoozed = false;
        reminder->next_play_utc = now_utc;
    }
    if (now_utc < reminder->next_play_utc) {
        return PILL_REMINDER_NONE;
    }
    reminder->next_play_utc = now_utc + reminder->repeat_seconds;
    return PILL_REMINDER_PLAY;
}
