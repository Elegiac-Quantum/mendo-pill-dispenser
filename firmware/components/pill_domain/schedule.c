#include "pill_domain/schedule.h"

#include <stdio.h>
#include <string.h>

static bool terminated_within(const char *text, size_t capacity)
{
    return text != NULL && memchr(text, '\0', capacity) != NULL;
}

pill_schedule_result_t pill_schedule_validate(const pill_schedule_t *schedule)
{
    if (schedule == NULL) {
        return PILL_SCHEDULE_NULL;
    }
    if (!terminated_within(schedule->id, sizeof(schedule->id)) || schedule->id[0] == '\0') {
        return PILL_SCHEDULE_ID_INVALID;
    }
    if (!terminated_within(schedule->medication, sizeof(schedule->medication)) ||
        schedule->medication[0] == '\0') {
        return PILL_SCHEDULE_MEDICATION_INVALID;
    }
    if (schedule->time_count == 0 || schedule->time_count > PILL_SCHEDULE_TIMES_MAX) {
        return PILL_SCHEDULE_TIME_COUNT_INVALID;
    }
    for (uint8_t index = 0; index < schedule->time_count; ++index) {
        if (schedule->times[index] >= 24U * 60U) {
            return PILL_SCHEDULE_TIME_OUT_OF_RANGE;
        }
        if (index > 0 && schedule->times[index] <= schedule->times[index - 1]) {
            return PILL_SCHEDULE_TIMES_NOT_STRICTLY_INCREASING;
        }
    }
    if (schedule->reminder_minutes < 1 || schedule->reminder_minutes > 30) {
        return PILL_SCHEDULE_REMINDER_OUT_OF_RANGE;
    }
    if (schedule->snooze_minutes < 1 || schedule->snooze_minutes > 30) {
        return PILL_SCHEDULE_SNOOZE_OUT_OF_RANGE;
    }
    return PILL_SCHEDULE_OK;
}

bool pill_occurrence_id(const char *schedule_id,
                        int year,
                        int month,
                        int day,
                        uint16_t minute_of_day,
                        char *output,
                        size_t output_size)
{
    if (schedule_id == NULL || schedule_id[0] == '\0' || output == NULL || output_size == 0 ||
        year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
        minute_of_day >= 24U * 60U) {
        return false;
    }

    int written = snprintf(output,
                           output_size,
                           "%s:%04d%02d%02d:%02u%02u",
                           schedule_id,
                           year,
                           month,
                           day,
                           minute_of_day / 60U,
                           minute_of_day % 60U);
    return written > 0 && (size_t)written < output_size;
}

pill_due_decision_t pill_due_decide(bool clock_valid,
                                    int64_t now_utc,
                                    int64_t intended_utc,
                                    bool occurrence_recorded)
{
    if (!clock_valid) {
        return PILL_DUE_TIME_INVALID;
    }
    if (occurrence_recorded) {
        return PILL_DUE_ALREADY_RECORDED;
    }
    if (now_utc < intended_utc) {
        return PILL_DUE_PENDING;
    }
    if (now_utc - intended_utc > 30 * 60) {
        return PILL_DUE_MISSED;
    }
    return PILL_DUE_NOW;
}

