#include "board_support/board.h"

#include "board_support/board_contract.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static bool s_ready;
static bool s_stable_pressed;
static uint32_t s_press_count;
static SemaphoreHandle_t s_lock;

esp_err_t board_patient_button_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_PATIENT_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) return result;
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) return ESP_ERR_NO_MEM;
    vTaskDelay(pdMS_TO_TICKS(50));
    /* The installed button module specifies high while pressed and low while
       released. Do not infer polarity at boot, which fails if it is touched
       or the signal is still settling during startup. */
    s_stable_pressed = gpio_get_level(BOARD_PATIENT_BUTTON_GPIO) != 0;
    s_ready = true;
    return ESP_OK;
}

bool board_patient_button_pressed(void)
{
    if (!s_ready) return false;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return s_stable_pressed;
    }
    bool sample = gpio_get_level(BOARD_PATIENT_BUTTON_GPIO) != 0;
    if (sample != s_stable_pressed) {
        vTaskDelay(pdMS_TO_TICKS(25));
        sample = gpio_get_level(BOARD_PATIENT_BUTTON_GPIO) != 0;
        if (sample != s_stable_pressed) {
            s_stable_pressed = sample;
            if (sample) ++s_press_count;
        }
    }
    bool pressed = s_stable_pressed;
    xSemaphoreGive(s_lock);
    return pressed;
}

uint32_t board_patient_button_press_count(void)
{
    (void)board_patient_button_pressed();
    return s_press_count;
}
