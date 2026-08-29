#pragma once

#include <stdbool.h>

#include "board_support/display.h"

typedef enum {
    PILL_DISPLAY_STARTING,
    PILL_DISPLAY_NO_SCHEDULE,
    PILL_DISPLAY_DOSE_DUE,
    PILL_DISPLAY_READY,
    PILL_DISPLAY_MISSED,
    PILL_DISPLAY_FAULT,
} pill_display_state_t;

bool pill_display_build(pill_display_state_t state,
                        bool rtc_valid,
                        int hour,
                        int minute,
                        board_display_view_t *view);
bool pill_display_build_startup(bool rtc_read_ok,
                                bool rtc_valid,
                                int hour,
                                int minute,
                                board_display_view_t *view);
