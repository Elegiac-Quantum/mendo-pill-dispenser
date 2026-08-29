#ifndef PILL_DOMAIN_SCHEDULE_H
#define PILL_DOMAIN_SCHEDULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PILL_SCHEDULE_ID_MAX 40
#define PILL_MEDICATION_NAME_MAX 65
#define PILL_SCHEDULE_TIMES_MAX 8
#define PILL_OCCURRENCE_ID_MAX 64

typedef struct {
    char id[PILL_SCHEDULE_ID_MAX];
    char medication[PILL_MEDICATION_NAME_MAX];
    bool enabled;
    uint8_t time_count;
    uint16_t times[PILL_SCHEDULE_TIMES_MAX];
    uint8_t reminder_minutes;
    uint8_t snooze_minutes;
} pill_schedule_t;

typedef enum {
    PILL_SCHEDULE_OK = 0,
    PILL_SCHEDULE_NULL,
    PILL_SCHEDULE_ID_INVALID,
    PILL_SCHEDULE_MEDICATION_INVALID,
    PILL_SCHEDULE_TIME_COUNT_INVALID,
    PILL_SCHEDULE_TIME_OUT_OF_RANGE,
    PILL_SCHEDULE_TIMES_NOT_STRICTLY_INCREASING,
    PILL_SCHEDULE_REMINDER_OUT_OF_RANGE,
    PILL_SCHEDULE_SNOOZE_OUT_OF_RANGE,
} pill_schedule_result_t;

typedef enum {
    PILL_DUE_TIME_INVALID = 0,
    PILL_DUE_PENDING,
    PILL_DUE_NOW,
    PILL_DUE_MISSED,
    PILL_DUE_ALREADY_RECORDED,
} pill_due_decision_t;

pill_schedule_result_t pill_schedule_validate(const pill_schedule_t *schedule);

bool pill_occurrence_id(const char *schedule_id,
                        int year,
                        int month,
                        int day,
                        uint16_t minute_of_day,
                        char *output,
                        size_t output_size);

pill_due_decision_t pill_due_decide(bool clock_valid,
                                    int64_t now_utc,
                                    int64_t intended_utc,
                                    bool occurrence_recorded);

#endif

