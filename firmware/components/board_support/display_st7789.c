#include "board_support/board_contract.h"
#include "board_support/display.h"

#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#include <string.h>

enum {
    LCD_WIDTH = 320,
    LCD_HEIGHT = 240,
    LCD_DRAW_LINES = 20,
};

static esp_lcd_panel_handle_t s_panel;
static uint16_t s_line_buffer[LCD_WIDTH * LCD_DRAW_LINES];
static uint16_t *s_framebuffer;
static SemaphoreHandle_t s_transfer_done;
static bool s_rotate_180 = true;

esp_err_t board_display_set_rotation(bool rotate_180)
{
    ESP_RETURN_ON_FALSE(s_panel != NULL, ESP_ERR_INVALID_STATE,
                        "display", "not ready");
    /* After landscape swap, toggling both mirror axes is a 180 degree turn. */
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_mirror(s_panel, rotate_180, !rotate_180),
        "display", "rotation");
    s_rotate_180 = rotate_180;
    return ESP_OK;
}

bool board_display_rotation(void)
{
    return s_rotate_180;
}

static bool transfer_done(esp_lcd_panel_io_handle_t panel_io,
                          esp_lcd_panel_io_event_data_t *event_data,
                          void *user_context)
{
    (void)panel_io;
    (void)event_data;
    (void)user_context;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_transfer_done, &task_woken);
    return task_woken == pdTRUE;
}

esp_err_t board_display_show(const board_display_view_t *view)
{
    ESP_RETURN_ON_FALSE(view != NULL && s_panel != NULL && s_framebuffer != NULL,
                        ESP_ERR_INVALID_STATE, "display", "not ready");
    board_canvas_t canvas = {
        .pixels = s_framebuffer,
        .width = LCD_WIDTH,
        .height = LCD_HEIGHT,
    };
    board_display_render(&canvas, view);
    for (int y = 0; y < LCD_HEIGHT; y += LCD_DRAW_LINES) {
        memcpy(s_line_buffer, &s_framebuffer[y * LCD_WIDTH], sizeof(s_line_buffer));
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_WIDTH,
                                                      y + LCD_DRAW_LINES, s_line_buffer),
                            "display", "draw");
        ESP_RETURN_ON_FALSE(xSemaphoreTake(s_transfer_done, pdMS_TO_TICKS(1000)) == pdTRUE,
                            ESP_ERR_TIMEOUT, "display", "transfer timeout");
    }
    return ESP_OK;
}

esp_err_t board_display_init(void)
{
    s_framebuffer = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(*s_framebuffer),
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_framebuffer != NULL, ESP_ERR_NO_MEM, "display", "framebuffer");
    s_transfer_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_transfer_done != NULL, ESP_ERR_NO_MEM, "display", "semaphore");
    const spi_bus_config_t bus = {
        .mosi_io_num = BOARD_SPI_MOSI_GPIO,
        .miso_io_num = BOARD_SPI_MISO_GPIO,
        .sclk_io_num = BOARD_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(s_line_buffer),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO),
                        "display", "SPI bus");

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_DC_GPIO,
        .cs_gpio_num = BOARD_LCD_CS_GPIO,
        .pclk_hz = 20000000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                                 &io_config, &io),
                        "display", "panel I/O");
    const esp_lcd_panel_io_callbacks_t callbacks = {
        .on_color_trans_done = transfer_done,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(io, &callbacks, NULL),
                        "display", "callbacks");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = BOARD_LCD_BGR_ORDER ? LCD_RGB_ELEMENT_ORDER_BGR
                                               : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &panel_config, &s_panel),
                        "display", "ST7789");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), "display", "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), "display", "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), "display", "landscape");
    nvs_handle_t preferences = 0;
    uint8_t rotate_180 = 1;
    if (nvs_open("ui_prefs", NVS_READONLY, &preferences) == ESP_OK) {
        (void)nvs_get_u8(preferences, "rotate180", &rotate_180);
        nvs_close(preferences);
    }
    ESP_RETURN_ON_ERROR(board_display_set_rotation(rotate_180 != 0),
                        "display", "saved rotation");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), "display", "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), "display", "display on");

    const board_display_view_t starting = {
        .time_text = "--:--",
        .status_text = "STARTING",
        .icon = BOARD_DISPLAY_ICON_RING,
        .accent_rgb565 = 0xffff,
    };
    return board_display_show(&starting);
}
