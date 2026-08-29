#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pill_app/clock_policy.h"
#include "pill_app/reminder_policy.h"

static void test_clock_quality(void)
{
    pill_clock_evidence_t evidence = {
        .rtc_present = true,
        .rtc_oscillator_stopped = false,
        .year = 2026,
        .ntp_synchronized = true,
        .seconds_since_sync = 0,
        .backward_jump_seconds = 0,
    };
    assert(pill_clock_quality(&evidence) == PILL_CLOCK_VALID);

    evidence.ntp_synchronized = false;
    evidence.seconds_since_sync = 8 * 24 * 60 * 60;
    assert(pill_clock_quality(&evidence) == PILL_CLOCK_DEGRADED);

    evidence.rtc_oscillator_stopped = true;
    assert(pill_clock_quality(&evidence) == PILL_CLOCK_INVALID);

    evidence.rtc_oscillator_stopped = false;
    evidence.backward_jump_seconds = 301;
    assert(pill_clock_quality(&evidence) == PILL_CLOCK_INVALID);
}

static void test_reminder_repeat_and_snooze(void)
{
    pill_reminder_t reminder;
    pill_reminder_start(&reminder, 1000, 300);
    assert(pill_reminder_tick(&reminder, 1000) == PILL_REMINDER_PLAY);
    assert(pill_reminder_tick(&reminder, 1299) == PILL_REMINDER_NONE);
    assert(pill_reminder_tick(&reminder, 1300) == PILL_REMINDER_PLAY);

    assert(pill_reminder_snooze(&reminder, 1300, 600));
    assert(pill_reminder_tick(&reminder, 1899) == PILL_REMINDER_NONE);
    assert(pill_reminder_tick(&reminder, 1900) == PILL_REMINDER_PLAY);

    pill_reminder_acknowledge(&reminder);
    assert(pill_reminder_tick(&reminder, 9999) == PILL_REMINDER_NONE);
}

int main(void)
{
    const char *default_audio = pill_reminder_default_audio_path();
    assert(default_audio != NULL);
    assert(strcmp(default_audio, "/sdcard/smartpill/audio/default-reminder.wav") == 0);
    assert(strcmp(pill_reminder_audio_path(false), default_audio) == 0);
    assert(strcmp(pill_reminder_audio_path(true), default_audio) == 0);

    test_clock_quality();
    test_reminder_repeat_and_snooze();
    puts("app policy tests passed");
    return 0;
}
