#include "pill_domain/dose.h"

#include <stddef.h>

bool pill_dose_transition(pill_dose_state_t current,
                          pill_dose_event_t event,
                          pill_dose_state_t *next)
{
    if (next == NULL) {
        return false;
    }

    pill_dose_state_t result;
    switch (current) {
    case PILL_DOSE_PENDING:
        if (event == PILL_DOSE_REQUEST_DISPENSE) {
            result = PILL_DOSE_DISPENSING;
        } else if (event == PILL_DOSE_MARK_MISSED) {
            result = PILL_DOSE_MISSED;
        } else {
            return false;
        }
        break;
    case PILL_DOSE_DISPENSING:
        if (event == PILL_DOSE_SERVO_SUCCEEDED) {
            result = PILL_DOSE_DISPENSED;
        } else if (event == PILL_DOSE_SERVO_FAILED || event == PILL_DOSE_RECOVER) {
            result = PILL_DOSE_FAULT;
        } else {
            return false;
        }
        break;
    case PILL_DOSE_DISPENSED:
        if (event != PILL_DOSE_START_REMINDER) {
            return false;
        }
        result = PILL_DOSE_REMINDING;
        break;
    case PILL_DOSE_REMINDING:
        if (event == PILL_DOSE_SNOOZE) {
            result = PILL_DOSE_SNOOZED;
        } else if (event == PILL_DOSE_ACKNOWLEDGE) {
            result = PILL_DOSE_ACKNOWLEDGED;
        } else {
            return false;
        }
        break;
    case PILL_DOSE_SNOOZED:
        if (event == PILL_DOSE_SNOOZE_EXPIRED) {
            result = PILL_DOSE_REMINDING;
        } else if (event == PILL_DOSE_ACKNOWLEDGE) {
            result = PILL_DOSE_ACKNOWLEDGED;
        } else {
            return false;
        }
        break;
    case PILL_DOSE_ACKNOWLEDGED:
    case PILL_DOSE_MISSED:
    case PILL_DOSE_FAULT:
    default:
        return false;
    }

    *next = result;
    return true;
}
