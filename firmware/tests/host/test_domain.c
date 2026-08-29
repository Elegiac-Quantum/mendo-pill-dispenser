#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pill_domain/dose.h"
#include "pill_domain/schedule.h"

static pill_schedule_t valid_schedule(void)
{
    pill_schedule_t schedule = {0};
    strcpy(schedule.id, "morning-med");
    strcpy(schedule.medication, "Medicine A");
    schedule.enabled = true;
    schedule.time_count = 2;
    schedule.times[0] = 8 * 60;
    schedule.times[1] = 20 * 60;
    schedule.reminder_minutes = 5;
    schedule.snooze_minutes = 10;
    return schedule;
}

static void test_schedule_validation(void)
{
    pill_schedule_t schedule = valid_schedule();
    assert(pill_schedule_validate(&schedule) == PILL_SCHEDULE_OK);

    schedule.times[1] = schedule.times[0];
    assert(pill_schedule_validate(&schedule) == PILL_SCHEDULE_TIMES_NOT_STRICTLY_INCREASING);

    schedule = valid_schedule();
    schedule.reminder_minutes = 31;
    assert(pill_schedule_validate(&schedule) == PILL_SCHEDULE_REMINDER_OUT_OF_RANGE);
}

static void test_occurrence_id_is_stable(void)
{
    char first[PILL_OCCURRENCE_ID_MAX];
    char second[PILL_OCCURRENCE_ID_MAX];
    assert(pill_occurrence_id("morning-med", 2026, 7, 20, 8 * 60, first, sizeof(first)));
    assert(pill_occurrence_id("morning-med", 2026, 7, 20, 8 * 60, second, sizeof(second)));
    assert(strcmp(first, "morning-med:20260720:0800") == 0);
    assert(strcmp(first, second) == 0);
}

static void test_due_policy(void)
{
    assert(pill_due_decide(false, 1000, 1000, false) == PILL_DUE_TIME_INVALID);
    assert(pill_due_decide(true, 999, 1000, false) == PILL_DUE_PENDING);
    assert(pill_due_decide(true, 1000, 1000, false) == PILL_DUE_NOW);
    assert(pill_due_decide(true, 2799, 1000, false) == PILL_DUE_NOW);
    assert(pill_due_decide(true, 2801, 1000, false) == PILL_DUE_MISSED);
    assert(pill_due_decide(true, 1000, 1000, true) == PILL_DUE_ALREADY_RECORDED);
}

static void test_dose_transitions(void)
{
    pill_dose_state_t next;
    assert(pill_dose_transition(PILL_DOSE_PENDING, PILL_DOSE_REQUEST_DISPENSE, &next));
    assert(next == PILL_DOSE_DISPENSING);
    assert(pill_dose_transition(next, PILL_DOSE_SERVO_SUCCEEDED, &next));
    assert(next == PILL_DOSE_DISPENSED);
    assert(pill_dose_transition(next, PILL_DOSE_START_REMINDER, &next));
    assert(next == PILL_DOSE_REMINDING);
    assert(pill_dose_transition(next, PILL_DOSE_SNOOZE, &next));
    assert(next == PILL_DOSE_SNOOZED);
    assert(pill_dose_transition(next, PILL_DOSE_SNOOZE_EXPIRED, &next));
    assert(next == PILL_DOSE_REMINDING);
    assert(pill_dose_transition(next, PILL_DOSE_ACKNOWLEDGE, &next));
    assert(next == PILL_DOSE_ACKNOWLEDGED);
    assert(!pill_dose_transition(next, PILL_DOSE_REQUEST_DISPENSE, &next));
}

static void test_ambiguous_recovery_faults(void)
{
    pill_dose_state_t next;
    assert(pill_dose_transition(PILL_DOSE_DISPENSING, PILL_DOSE_RECOVER, &next));
    assert(next == PILL_DOSE_FAULT);
    assert(!pill_dose_transition(PILL_DOSE_FAULT, PILL_DOSE_REQUEST_DISPENSE, &next));
}

int main(void)
{
    test_schedule_validation();
    test_occurrence_id_is_stable();
    test_due_policy();
    test_dose_transitions();
    test_ambiguous_recovery_faults();
    puts("domain tests passed");
    return 0;
}
