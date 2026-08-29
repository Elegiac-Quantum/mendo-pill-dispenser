#include <inttypes.h>

#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "board_support/board.h"
#include "pill_ai_transport/transport.h"
#include "pill_app/app.h"
#include "pill_web/web.h"

static const char *TAG = "smartpill";

static void diagnose_mt6701_output(void)
{
    adc_oneshot_unit_handle_t adc = NULL;
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t error = adc_oneshot_new_unit(&unit_config, &adc);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "MT_OUTPUT error=%s", esp_err_to_name(error));
        return;
    }
    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    error = adc_oneshot_config_channel(adc, ADC_CHANNEL_4, &channel_config);
    int minimum = 4095, maximum = 0;
    int64_t total = 0;
    int valid = 0;
    if (error == ESP_OK) {
        for (int sample = 0; sample < 1000; ++sample) {
            int raw = 0;
            if (adc_oneshot_read(adc, ADC_CHANNEL_4, &raw) == ESP_OK) {
                if (raw < minimum) minimum = raw;
                if (raw > maximum) maximum = raw;
                total += raw;
                valid++;
            }
            esp_rom_delay_us(100);
        }
    }
    adc_oneshot_del_unit(adc);
    ESP_LOGW(TAG, "MT_OUTPUT gpio=5 samples=%d min=%d max=%d avg=%d span=%d error=%s",
             valid, valid ? minimum : -1, valid ? maximum : -1,
             valid ? (int)(total / valid) : -1,
             valid ? maximum - minimum : -1, esp_err_to_name(error));
}

static void calibration_console(void *unused)
{
    (void)unused;
    ESP_LOGW(TAG, "CAL console ready: send D for MT6701 diagnostics, A for analog output, S to scan safe P1 pins, or T for one empty-tray move");
    for (;;) {
        uint8_t command = 0;
        if (uart_read_bytes(UART_NUM_0, &command, 1, pdMS_TO_TICKS(200)) != 1) continue;
        if (command == 'D') {
            int sda = -1, scl = -1, address = -1;
            uint16_t raw = 0;
            float degrees = 0.0f;
            board_mt6701_diagnose(&sda, &scl, &address);
            esp_err_t mt_error = board_mt6701_read(&raw, &degrees);
            ESP_LOGW(TAG,
                     "MT_DIAG ok=%d sda=%d scl=%d address=%d raw=%u angle=%.2f error=%s",
                     mt_error == ESP_OK, sda, scl, address, raw, degrees,
                     esp_err_to_name(mt_error));
            int half_period_us = -1;
            bool timing_ok = board_mt6701_diagnose_timing(&half_period_us);
            ESP_LOGW(TAG, "MT_TIMING ok=%d half_us=%d approx_khz=%d",
                     timing_ok, half_period_us,
                     timing_ok ? 500 / half_period_us : 0);
            continue;
        }
        if (command == 'S') {
            int sda_gpio = -1, scl_gpio = -1;
            bool found = board_mt6701_scan_safe_p1(&sda_gpio, &scl_gpio);
            ESP_LOGW(TAG, "MT_SCAN found=%d sda_gpio=%d scl_gpio=%d",
                     found, sda_gpio, scl_gpio);
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
        }
        if (command == 'A') {
            diagnose_mt6701_output();
            continue;
        }
        if (command == 'E') {
            int sda_gpio = -1, scl_gpio = -1;
            bool found = board_mt6701_scan_known_buses(&sda_gpio, &scl_gpio);
            ESP_LOGW(TAG, "MT_EXT_SCAN found=%d sda_gpio=%d scl_gpio=%d",
                     found, sda_gpio, scl_gpio);
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
        }
        if (command != 'T') continue;
        bool success = board_servo_cycle_channel(0);
        float start = 0.0f, target = 0.0f, final = 0.0f, error = 0.0f;
        const char *reason = "unknown";
        board_servo_last_motion(&start, &target, &final, &error, &reason);
        ESP_LOGW(TAG,
                 "CAL_RESULT success=%d start=%.2f target=%.2f final=%.2f error=%.2f reason=%s",
                 success, start, target, final, error, reason);
    }
}

void app_main(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;

    esp_chip_info(&chip_info);
    ESP_ERROR_CHECK(esp_flash_get_size(NULL, &flash_size));

    ESP_LOGI(TAG, "Smart Pill Dispenser Prototype 2 rotary");
    ESP_LOGI(TAG, "ESP32-S3 cores=%u revision=%u", chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG, "Flash=%" PRIu32 " bytes PSRAM=%u bytes",
             flash_size,
             heap_caps_get_total_size(MALLOC_CAP_SPIRAM));

    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    board_health_t health;
    ESP_ERROR_CHECK(board_init(&health));

    esp_err_t console_error = uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    if (console_error != ESP_OK && console_error != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "COM8 calibration input unavailable: %s",
                 esp_err_to_name(console_error));
    }

    if (xTaskCreate(calibration_console, "cal_console", 3072, NULL, 3, NULL) !=
        pdPASS) {
        ESP_LOGW(TAG, "Calibration console unavailable");
    }

    error = pill_app_start();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Safety controller failed: %s; movement remains locked",
                 esp_err_to_name(error));
    }

    error = pill_web_start();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "Caregiver web UI unavailable: %s", esp_err_to_name(error));
    }
    pill_ai_check_activation_async();
}
