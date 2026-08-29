#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board_support/display.h"

enum { WIDTH = 12, HEIGHT = 12, GUARD = 0xa55a };

static void test_clipped_rectangle(void)
{
    uint16_t guarded[WIDTH * HEIGHT + 2] = {0};
    guarded[0] = GUARD;
    guarded[WIDTH * HEIGHT + 1] = GUARD;
    board_canvas_t canvas = {&guarded[1], WIDTH, HEIGHT};

    board_canvas_fill_rect(&canvas, -3, -2, 5, 4, 0x1234);
    assert(canvas.pixels[0] == 0x1234);
    assert(canvas.pixels[1] == 0x1234);
    assert(canvas.pixels[2 * WIDTH] == 0);
    assert(guarded[0] == GUARD);
    assert(guarded[WIDTH * HEIGHT + 1] == GUARD);
}

static void test_line_endpoints(void)
{
    uint16_t pixels[WIDTH * HEIGHT] = {0};
    board_canvas_t canvas = {pixels, WIDTH, HEIGHT};
    board_canvas_line(&canvas, 1, 2, 10, 9, 0xbeef, 1);
    assert(pixels[2 * WIDTH + 1] == 0xbeef);
    assert(pixels[9 * WIDTH + 10] == 0xbeef);
}

static void test_scaled_text_respects_guards(void)
{
    uint16_t guarded[WIDTH * HEIGHT + 2] = {0};
    guarded[0] = GUARD;
    guarded[WIDTH * HEIGHT + 1] = GUARD;
    board_canvas_t canvas = {&guarded[1], WIDTH, HEIGHT};
    board_canvas_text_5x7(&canvas, 8, 8, "A?", 3, 0xffff);
    assert(guarded[0] == GUARD);
    assert(guarded[WIDTH * HEIGHT + 1] == GUARD);
}

static uint32_t render_hash(board_display_icon_t icon)
{
    static uint16_t pixels[320 * 240];
    memset(pixels, 0, sizeof(pixels));
    board_canvas_t canvas = {pixels, 320, 240};
    board_display_view_t view = {
        .time_text = "08:05",
        .status_text = "NO SCHEDULE",
        .icon = icon,
        .accent_rgb565 = 0x07e0,
    };
    board_display_render(&canvas, &view);
    assert(pixels[0] == 0x4a08);
    assert(pixels[319] == 0x4a08);
    assert(pixels[239 * 320] == 0x4a08);
    assert(pixels[239 * 320 + 319] == 0x4a08);

    uint32_t hash = 2166136261u;
    for (size_t index = 0; index < 320U * 240U; ++index) {
        hash = (hash ^ pixels[index]) * 16777619u;
    }
    return hash;
}

static void test_icons_are_visually_distinct(void)
{
    uint32_t hashes[6];
    for (int icon = BOARD_DISPLAY_ICON_RING; icon <= BOARD_DISPLAY_ICON_CROSS; ++icon) {
        hashes[icon] = render_hash((board_display_icon_t)icon);
        for (int earlier = 0; earlier < icon; ++earlier) {
            assert(hashes[icon] != hashes[earlier]);
        }
    }
}

int main(void)
{
    test_clipped_rectangle();
    test_line_endpoints();
    test_scaled_text_respects_guards();
    test_icons_are_visually_distinct();
    puts("display renderer tests passed");
    return 0;
}
