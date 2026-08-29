#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dose_controller/dose_controller.h"

typedef struct {
    bool found;
    dose_record_t record;
    unsigned store_count;
    unsigned servo_count;
    bool servo_result;
    bool dispensing_was_stored_before_servo;
} fixture_t;

static bool load_record(void *context,
                        const char *occurrence_id,
                        dose_record_t *record,
                        bool *found)
{
    fixture_t *fixture = context;
    (void)occurrence_id;
    *found = fixture->found;
    if (*found) {
        *record = fixture->record;
    }
    return true;
}

static bool store_record(void *context, const dose_record_t *record)
{
    fixture_t *fixture = context;
    fixture->record = *record;
    fixture->found = true;
    fixture->store_count++;
    return true;
}

static bool servo_cycle(void *context)
{
    fixture_t *fixture = context;
    fixture->servo_count++;
    fixture->dispensing_was_stored_before_servo =
        fixture->found && fixture->record.state == PILL_DOSE_DISPENSING;
    return fixture->servo_result;
}

static dose_controller_t controller_for(fixture_t *fixture)
{
    dose_controller_t controller = {
        .context = fixture,
        .load = load_record,
        .store = store_record,
        .servo_cycle = servo_cycle,
    };
    return controller;
}

static void test_success_is_persisted_before_and_after_servo(void)
{
    fixture_t fixture = {.servo_result = true};
    dose_controller_t controller = controller_for(&fixture);

    dose_controller_result_t result = dose_controller_process_due(
        &controller, "med:20260720:0800", 1000, 1000, true);

    assert(result == DOSE_CONTROLLER_DISPENSED);
    assert(fixture.servo_count == 1);
    assert(fixture.dispensing_was_stored_before_servo);
    assert(fixture.store_count == 2);
    assert(fixture.record.state == PILL_DOSE_DISPENSED);
}

static void test_repeat_never_moves_twice(void)
{
    fixture_t fixture = {.servo_result = true};
    dose_controller_t controller = controller_for(&fixture);

    assert(dose_controller_process_due(&controller, "same", 1000, 1000, true) ==
           DOSE_CONTROLLER_DISPENSED);
    assert(dose_controller_process_due(&controller, "same", 1000, 1001, true) ==
           DOSE_CONTROLLER_ALREADY_RECORDED);
    assert(fixture.servo_count == 1);
}

static void test_servo_failure_becomes_fault(void)
{
    fixture_t fixture = {.servo_result = false};
    dose_controller_t controller = controller_for(&fixture);

    assert(dose_controller_process_due(&controller, "failed", 1000, 1000, true) ==
           DOSE_CONTROLLER_FAULT);
    assert(fixture.servo_count == 1);
    assert(fixture.record.state == PILL_DOSE_FAULT);
}

static void test_recovery_from_dispensing_never_moves(void)
{
    fixture_t fixture = {.found = true, .servo_result = true};
    strcpy(fixture.record.occurrence_id, "recover");
    fixture.record.state = PILL_DOSE_DISPENSING;
    dose_controller_t controller = controller_for(&fixture);

    assert(dose_controller_recover(&controller, "recover") == DOSE_CONTROLLER_FAULT);
    assert(fixture.servo_count == 0);
    assert(fixture.record.state == PILL_DOSE_FAULT);
}

static void test_invalid_and_stale_time_do_not_move(void)
{
    fixture_t fixture = {.servo_result = true};
    dose_controller_t controller = controller_for(&fixture);

    assert(dose_controller_process_due(&controller, "invalid", 1000, 1000, false) ==
           DOSE_CONTROLLER_TIME_INVALID);
    assert(fixture.servo_count == 0);

    assert(dose_controller_process_due(&controller, "late", 1000, 2801, true) ==
           DOSE_CONTROLLER_MISSED);
    assert(fixture.servo_count == 0);
    assert(fixture.record.state == PILL_DOSE_MISSED);
}

int main(void)
{
    test_success_is_persisted_before_and_after_servo();
    test_repeat_never_moves_twice();
    test_servo_failure_becomes_fault();
    test_recovery_from_dispensing_never_moves();
    test_invalid_and_stale_time_do_not_move();
    puts("dose controller tests passed");
    return 0;
}
