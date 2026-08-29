#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pill_app/display_model.h"

static void test_valid_no_schedule(void)
{
    board_display_view_t view = {0};
    assert(pill_display_build(PILL_DISPLAY_NO_SCHEDULE, true, 8, 5, &view));
    assert(strcmp(view.time_text, "08:05") == 0);
    assert(strcmp(view.status_text, "NO SCHEDULE") == 0);
    assert(view.icon == BOARD_DISPLAY_ICON_CHECK);
}

static void test_invalid_clock(void)
{
    board_display_view_t view = {0};
    assert(pill_display_build(PILL_DISPLAY_NO_SCHEDULE, false, 99, 99, &view));
    assert(strcmp(view.time_text, "--:--") == 0);
    assert(strcmp(view.status_text, "SET TIME") == 0);
    assert(view.icon == BOARD_DISPLAY_ICON_CLOCK_WARNING);
}

static void test_rejects_invalid_arguments(void)
{
    board_display_view_t view = {0};
    assert(!pill_display_build(PILL_DISPLAY_NO_SCHEDULE, true, 24, 0, &view));
    assert(!pill_display_build(PILL_DISPLAY_NO_SCHEDULE, true, 12, 60, &view));
    assert(!pill_display_build(PILL_DISPLAY_NO_SCHEDULE, true, 12, 0, NULL));
}

static void test_fault_is_unambiguous(void)
{
    board_display_view_t view = {0};
    assert(pill_display_build(PILL_DISPLAY_FAULT, true, 9, 30, &view));
    assert(strcmp(view.status_text, "CALL CAREGIVER") == 0);
    assert(view.icon == BOARD_DISPLAY_ICON_CROSS);
}

static void test_startup_selection_fails_closed(void)
{
    board_display_view_t view = {0};
    assert(pill_display_build_startup(true, true, 7, 45, &view));
    assert(strcmp(view.time_text, "07:45") == 0);
    assert(strcmp(view.status_text, "NO SCHEDULE") == 0);

    assert(pill_display_build_startup(false, true, 7, 45, &view));
    assert(strcmp(view.time_text, "--:--") == 0);
    assert(strcmp(view.status_text, "RTC ERROR") == 0);
    assert(view.icon == BOARD_DISPLAY_ICON_CROSS);
}

int main(void)
{
    test_valid_no_schedule();
    test_invalid_clock();
    test_rejects_invalid_arguments();
    test_fault_is_unambiguous();
    test_startup_selection_fails_closed();
    puts("display model tests passed");
    return 0;
}
