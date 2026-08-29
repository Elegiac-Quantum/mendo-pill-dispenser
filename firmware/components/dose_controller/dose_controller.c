#include "dose_controller/dose_controller.h"

#include <string.h>

static bool controller_valid(const dose_controller_t *controller)
{
    return controller != NULL && controller->load != NULL && controller->store != NULL &&
           controller->servo_cycle != NULL;
}

static bool set_state(dose_record_t *record,
                      pill_dose_event_t event,
                      int64_t updated_utc)
{
    pill_dose_state_t next;
    if (!pill_dose_transition(record->state, event, &next)) {
        return false;
    }
    record->state = next;
    record->updated_utc = updated_utc;
    return true;
}

dose_controller_result_t dose_controller_process_due(dose_controller_t *controller,
                                                      const char *occurrence_id,
                                                      int64_t intended_utc,
                                                      int64_t now_utc,
                                                      bool clock_valid)
{
    if (!controller_valid(controller) || occurrence_id == NULL || occurrence_id[0] == '\0' ||
        strlen(occurrence_id) >= PILL_OCCURRENCE_ID_MAX) {
        return DOSE_CONTROLLER_INVALID_ARGUMENT;
    }

    dose_record_t record = {0};
    bool found = false;
    if (!controller->load(controller->context, occurrence_id, &record, &found)) {
        return DOSE_CONTROLLER_STORAGE_ERROR;
    }
    if (found) {
        return DOSE_CONTROLLER_ALREADY_RECORDED;
    }

    pill_due_decision_t decision =
        pill_due_decide(clock_valid, now_utc, intended_utc, false);
    if (decision == PILL_DUE_TIME_INVALID) {
        return DOSE_CONTROLLER_TIME_INVALID;
    }
    if (decision == PILL_DUE_PENDING) {
        return DOSE_CONTROLLER_NOT_DUE;
    }

    strcpy(record.occurrence_id, occurrence_id);
    record.intended_utc = intended_utc;
    record.updated_utc = now_utc;
    record.state = PILL_DOSE_PENDING;

    if (decision == PILL_DUE_MISSED) {
        if (!set_state(&record, PILL_DOSE_MARK_MISSED, now_utc) ||
            !controller->store(controller->context, &record)) {
            return DOSE_CONTROLLER_STORAGE_ERROR;
        }
        return DOSE_CONTROLLER_MISSED;
    }

    if (!set_state(&record, PILL_DOSE_REQUEST_DISPENSE, now_utc) ||
        !controller->store(controller->context, &record)) {
        return DOSE_CONTROLLER_STORAGE_ERROR;
    }

    pill_dose_event_t servo_event = controller->servo_cycle(controller->context)
                                              ? PILL_DOSE_SERVO_SUCCEEDED
                                              : PILL_DOSE_SERVO_FAILED;
    if (!set_state(&record, servo_event, now_utc) ||
        !controller->store(controller->context, &record)) {
        return DOSE_CONTROLLER_STORAGE_ERROR;
    }

    return record.state == PILL_DOSE_DISPENSED ? DOSE_CONTROLLER_DISPENSED
                                                : DOSE_CONTROLLER_FAULT;
}

dose_controller_result_t dose_controller_recover(dose_controller_t *controller,
                                                  const char *occurrence_id)
{
    if (!controller_valid(controller) || occurrence_id == NULL || occurrence_id[0] == '\0') {
        return DOSE_CONTROLLER_INVALID_ARGUMENT;
    }

    dose_record_t record = {0};
    bool found = false;
    if (!controller->load(controller->context, occurrence_id, &record, &found)) {
        return DOSE_CONTROLLER_STORAGE_ERROR;
    }
    if (!found) {
        return DOSE_CONTROLLER_NOT_DUE;
    }
    if (record.state != PILL_DOSE_DISPENSING) {
        return DOSE_CONTROLLER_ALREADY_RECORDED;
    }
    if (!set_state(&record, PILL_DOSE_RECOVER, record.updated_utc) ||
        !controller->store(controller->context, &record)) {
        return DOSE_CONTROLLER_STORAGE_ERROR;
    }
    return DOSE_CONTROLLER_FAULT;
}
