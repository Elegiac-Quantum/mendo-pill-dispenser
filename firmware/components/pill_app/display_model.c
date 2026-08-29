#include "pill_app/display_model.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *text;
    board_display_icon_t icon;
    uint16_t color;
} state_style_t;

static const state_style_t STYLES[] = {
    [PILL_DISPLAY_STARTING] = {"STARTING", BOARD_DISPLAY_ICON_RING, 0xffff},
    [PILL_DISPLAY_NO_SCHEDULE] = {"NO SCHEDULE", BOARD_DISPLAY_ICON_CHECK, 0x07e0},
    [PILL_DISPLAY_DOSE_DUE] = {"TAKE MEDICINE", BOARD_DISPLAY_ICON_PILL, 0xffe0},
    [PILL_DISPLAY_READY] = {"MEDICINE READY", BOARD_DISPLAY_ICON_CHECK, 0x07e0},
    [PILL_DISPLAY_MISSED] = {"DOSE MISSED", BOARD_DISPLAY_ICON_WARNING, 0xfd20},
    [PILL_DISPLAY_FAULT] = {"CALL CAREGIVER", BOARD_DISPLAY_ICON_CROSS, 0xf800},
};

bool pill_display_build(pill_display_state_t state,
                        bool rtc_valid,
                        int hour,
                        int minute,
                        board_display_view_t *view)
{
    if (view == NULL || state < PILL_DISPLAY_STARTING || state > PILL_DISPLAY_FAULT) {
        return false;
    }
    if (rtc_valid && (hour < 0 || hour > 23 || minute < 0 || minute > 59)) {
        return false;
    }

    const state_style_t *style = &STYLES[state];
    if (!rtc_valid && state != PILL_DISPLAY_STARTING) {
        *view = (board_display_view_t){
            .icon = BOARD_DISPLAY_ICON_CLOCK_WARNING,
            .accent_rgb565 = 0xffe0,
        };
        memcpy(view->time_text, "--:--", sizeof(view->time_text));
        memcpy(view->status_text, "SET TIME", sizeof("SET TIME"));
        return true;
    }

    *view = (board_display_view_t){
        .icon = style->icon,
        .accent_rgb565 = style->color,
    };
    if (rtc_valid) {
        (void)snprintf(view->time_text, sizeof(view->time_text), "%02d:%02d", hour, minute);
    } else {
        memcpy(view->time_text, "--:--", sizeof(view->time_text));
    }
    (void)snprintf(view->status_text, sizeof(view->status_text), "%s", style->text);
    return true;
}

bool pill_display_build_startup(bool rtc_read_ok,
                                bool rtc_valid,
                                int hour,
                                int minute,
                                board_display_view_t *view)
{
    if (!rtc_read_ok) {
        if (view == NULL) {
            return false;
        }
        *view = (board_display_view_t){
            .time_text = "--:--",
            .status_text = "RTC ERROR",
            .icon = BOARD_DISPLAY_ICON_CROSS,
            .accent_rgb565 = 0xf800,
        };
        return true;
    }
    return pill_display_build(PILL_DISPLAY_NO_SCHEDULE,
                              rtc_valid,
                              hour,
                              minute,
                              view);
}
