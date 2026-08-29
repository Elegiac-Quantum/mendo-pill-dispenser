#include "board_support/display.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t codepoint;
    uint16_t rows[16];
} chinese_glyph_t;

#include "chinese_glyphs.inc"

static const uint8_t FONT[59][7] = {
    ['-' - 32] = {0, 0, 0, 31, 0, 0, 0},
    ['0' - 32] = {14, 17, 19, 21, 25, 17, 14},
    ['1' - 32] = {4, 12, 4, 4, 4, 4, 14},
    ['2' - 32] = {14, 17, 1, 2, 4, 8, 31},
    ['3' - 32] = {30, 1, 1, 14, 1, 1, 30},
    ['4' - 32] = {2, 6, 10, 18, 31, 2, 2},
    ['5' - 32] = {31, 16, 16, 30, 1, 1, 30},
    ['6' - 32] = {14, 16, 16, 30, 17, 17, 14},
    ['7' - 32] = {31, 1, 2, 4, 8, 8, 8},
    ['8' - 32] = {14, 17, 17, 14, 17, 17, 14},
    ['9' - 32] = {14, 17, 17, 15, 1, 1, 14},
    [':' - 32] = {0, 4, 4, 0, 4, 4, 0},
    ['A' - 32] = {14, 17, 17, 31, 17, 17, 17},
    ['B' - 32] = {30, 17, 17, 30, 17, 17, 30},
    ['C' - 32] = {14, 17, 16, 16, 16, 17, 14},
    ['D' - 32] = {30, 17, 17, 17, 17, 17, 30},
    ['E' - 32] = {31, 16, 16, 30, 16, 16, 31},
    ['F' - 32] = {31, 16, 16, 30, 16, 16, 16},
    ['G' - 32] = {14, 17, 16, 23, 17, 17, 15},
    ['H' - 32] = {17, 17, 17, 31, 17, 17, 17},
    ['I' - 32] = {14, 4, 4, 4, 4, 4, 14},
    ['J' - 32] = {7, 2, 2, 2, 18, 18, 12},
    ['K' - 32] = {17, 18, 20, 24, 20, 18, 17},
    ['L' - 32] = {16, 16, 16, 16, 16, 16, 31},
    ['M' - 32] = {17, 27, 21, 21, 17, 17, 17},
    ['N' - 32] = {17, 25, 21, 19, 17, 17, 17},
    ['O' - 32] = {14, 17, 17, 17, 17, 17, 14},
    ['P' - 32] = {30, 17, 17, 30, 16, 16, 16},
    ['Q' - 32] = {14, 17, 17, 17, 21, 18, 13},
    ['R' - 32] = {30, 17, 17, 30, 20, 18, 17},
    ['S' - 32] = {15, 16, 16, 14, 1, 1, 30},
    ['T' - 32] = {31, 4, 4, 4, 4, 4, 4},
    ['U' - 32] = {17, 17, 17, 17, 17, 17, 14},
    ['V' - 32] = {17, 17, 17, 17, 17, 10, 4},
    ['W' - 32] = {17, 17, 17, 21, 21, 21, 10},
    ['X' - 32] = {17, 17, 10, 4, 10, 17, 17},
    ['Y' - 32] = {17, 17, 10, 4, 4, 4, 4},
    ['Z' - 32] = {31, 1, 2, 4, 8, 16, 31},
};

void board_canvas_fill_rect(board_canvas_t *canvas,
                            int x, int y, int width, int height,
                            uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL || width <= 0 || height <= 0) {
        return;
    }
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = x + width > canvas->width ? canvas->width : x + width;
    int bottom = y + height > canvas->height ? canvas->height : y + height;
    for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
            canvas->pixels[row * canvas->width + column] = color;
        }
    }
}

void board_canvas_line(board_canvas_t *canvas,
                       int x0, int y0, int x1, int y1,
                       uint16_t color, int thickness)
{
    if (thickness <= 0) {
        return;
    }
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        board_canvas_fill_rect(canvas, x0 - thickness / 2, y0 - thickness / 2,
                               thickness, thickness, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void board_canvas_text_5x7(board_canvas_t *canvas,
                           int x, int y, const char *text,
                           int scale, uint16_t color)
{
    if (text == NULL || scale <= 0) {
        return;
    }
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != 0; ++cursor) {
        const uint8_t *glyph = (*cursor >= 32 && *cursor <= 90) ? FONT[*cursor - 32] : FONT[0];
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1U << (4 - column))) != 0) {
                    board_canvas_fill_rect(canvas, x + column * scale, y + row * scale,
                                           scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

static int centered_text_x(const board_canvas_t *canvas, const char *text, int scale)
{
    int width = (int)strlen(text) * 6 * scale - scale;
    return ((int)canvas->width - width) / 2;
}

static void fill_ring(board_canvas_t *canvas, int cx, int cy,
                      int outer_radius, int thickness, uint16_t color)
{
    int outer2 = outer_radius * outer_radius;
    int inner = outer_radius - thickness;
    int inner2 = inner * inner;
    for (int y = -outer_radius; y <= outer_radius; ++y) {
        for (int x = -outer_radius; x <= outer_radius; ++x) {
            int distance2 = x * x + y * y;
            if (distance2 <= outer2 && distance2 >= inner2) {
                board_canvas_fill_rect(canvas, cx + x, cy + y, 1, 1, color);
            }
        }
    }
}

static void draw_icon(board_canvas_t *canvas, board_display_icon_t icon, uint16_t color)
{
    const int cx = canvas->width / 2;
    const int cy = 142;
    switch (icon) {
    case BOARD_DISPLAY_ICON_RING:
        fill_ring(canvas, cx, cy, 34, 8, color);
        break;
    case BOARD_DISPLAY_ICON_CHECK:
        board_canvas_line(canvas, cx - 34, cy, cx - 10, cy + 25, color, 9);
        board_canvas_line(canvas, cx - 10, cy + 25, cx + 38, cy - 29, color, 9);
        break;
    case BOARD_DISPLAY_ICON_CLOCK_WARNING:
        fill_ring(canvas, cx - 8, cy, 32, 6, color);
        board_canvas_line(canvas, cx - 8, cy, cx - 8, cy - 19, color, 6);
        board_canvas_line(canvas, cx - 8, cy, cx + 8, cy + 10, color, 6);
        board_canvas_line(canvas, cx + 36, cy - 20, cx + 36, cy + 9, color, 7);
        board_canvas_fill_rect(canvas, cx + 33, cy + 18, 7, 7, color);
        break;
    case BOARD_DISPLAY_ICON_PILL:
        board_canvas_line(canvas, cx - 30, cy + 25, cx + 30, cy - 25, color, 28);
        board_canvas_line(canvas, cx - 5, cy + 4, cx + 11, cy + 23, 0x4a08, 5);
        break;
    case BOARD_DISPLAY_ICON_WARNING:
        board_canvas_line(canvas, cx, cy - 38, cx - 39, cy + 30, color, 7);
        board_canvas_line(canvas, cx - 39, cy + 30, cx + 39, cy + 30, color, 7);
        board_canvas_line(canvas, cx + 39, cy + 30, cx, cy - 38, color, 7);
        board_canvas_line(canvas, cx, cy - 13, cx, cy + 9, color, 7);
        board_canvas_fill_rect(canvas, cx - 3, cy + 17, 7, 7, color);
        break;
    case BOARD_DISPLAY_ICON_CROSS:
        board_canvas_line(canvas, cx - 31, cy - 31, cx + 31, cy + 31, color, 10);
        board_canvas_line(canvas, cx + 31, cy - 31, cx - 31, cy + 31, color, 10);
        break;
    }
}

static uint16_t decode_utf8_3(const uint8_t *text)
{
    return (uint16_t)(((uint16_t)(text[0] & 0x0f) << 12) |
                      ((uint16_t)(text[1] & 0x3f) << 6) |
                      (uint16_t)(text[2] & 0x3f));
}

static const chinese_glyph_t *find_chinese_glyph(uint16_t codepoint)
{
    for (size_t index = 0;
         index < sizeof(CHINESE_GLYPHS) / sizeof(CHINESE_GLYPHS[0]); ++index) {
        if (CHINESE_GLYPHS[index].codepoint == codepoint) {
            return &CHINESE_GLYPHS[index];
        }
    }
    return NULL;
}

static void draw_chinese_status(board_canvas_t *canvas, const char *text,
                                uint16_t color)
{
    size_t bytes = strlen(text);
    size_t count = bytes / 3;
    int x = ((int)canvas->width - (int)(count * 19 - 3)) / 2;
    for (size_t offset = 0; offset + 2 < bytes; offset += 3, x += 19) {
        const chinese_glyph_t *glyph =
            find_chinese_glyph(decode_utf8_3((const uint8_t *)text + offset));
        if (glyph == NULL) continue;
        for (int row = 0; row < 16; ++row) {
            for (int column = 0; column < 16; ++column) {
                if (glyph->rows[row] & (uint16_t)(1u << (15 - column))) {
                    board_canvas_fill_rect(canvas, x + column, 210 + row, 1, 1,
                                           color);
                }
            }
        }
    }
}

void board_display_render(board_canvas_t *canvas, const board_display_view_t *view)
{
    if (canvas == NULL || canvas->pixels == NULL || view == NULL) {
        return;
    }
    board_canvas_fill_rect(canvas, 0, 0, canvas->width, canvas->height, 0x4a08);
    board_canvas_text_5x7(canvas, centered_text_x(canvas, view->time_text, 7), 18,
                          view->time_text, 7, 0xffff);
    draw_icon(canvas, view->icon, view->accent_rgb565);
    if (((const uint8_t *)view->status_text)[0] >= 0x80) {
        draw_chinese_status(canvas, view->status_text, 0xffff);
    } else {
        board_canvas_text_5x7(canvas, centered_text_x(canvas, view->status_text, 2),
                             211, view->status_text, 2, 0xffff);
    }
}
