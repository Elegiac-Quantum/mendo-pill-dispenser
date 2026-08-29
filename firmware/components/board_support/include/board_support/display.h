#pragma once

#include <stdint.h>

typedef struct {
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
} board_canvas_t;

typedef enum {
    BOARD_DISPLAY_ICON_RING,
    BOARD_DISPLAY_ICON_CHECK,
    BOARD_DISPLAY_ICON_CLOCK_WARNING,
    BOARD_DISPLAY_ICON_PILL,
    BOARD_DISPLAY_ICON_WARNING,
    BOARD_DISPLAY_ICON_CROSS,
} board_display_icon_t;

typedef struct {
    char time_text[6];
    char status_text[17];
    board_display_icon_t icon;
    uint16_t accent_rgb565;
} board_display_view_t;

void board_canvas_fill_rect(board_canvas_t *canvas,
                            int x, int y, int width, int height,
                            uint16_t color);
void board_canvas_line(board_canvas_t *canvas,
                       int x0, int y0, int x1, int y1,
                       uint16_t color, int thickness);
void board_canvas_text_5x7(board_canvas_t *canvas,
                           int x, int y, const char *text,
                           int scale, uint16_t color);
void board_display_render(board_canvas_t *canvas, const board_display_view_t *view);
