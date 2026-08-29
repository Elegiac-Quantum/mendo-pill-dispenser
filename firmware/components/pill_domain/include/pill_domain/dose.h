#ifndef PILL_DOMAIN_DOSE_H
#define PILL_DOMAIN_DOSE_H

#include <stdbool.h>

typedef enum {
    PILL_DOSE_PENDING = 0,
    PILL_DOSE_DISPENSING,
    PILL_DOSE_DISPENSED,
    PILL_DOSE_REMINDING,
    PILL_DOSE_SNOOZED,
    PILL_DOSE_ACKNOWLEDGED,
    PILL_DOSE_MISSED,
    PILL_DOSE_FAULT,
} pill_dose_state_t;

typedef enum {
    PILL_DOSE_REQUEST_DISPENSE = 0,
    PILL_DOSE_SERVO_SUCCEEDED,
    PILL_DOSE_SERVO_FAILED,
    PILL_DOSE_START_REMINDER,
    PILL_DOSE_SNOOZE,
    PILL_DOSE_SNOOZE_EXPIRED,
    PILL_DOSE_ACKNOWLEDGE,
    PILL_DOSE_MARK_MISSED,
    PILL_DOSE_RECOVER,
} pill_dose_event_t;

bool pill_dose_transition(pill_dose_state_t current,
                          pill_dose_event_t event,
                          pill_dose_state_t *next);

#endif

