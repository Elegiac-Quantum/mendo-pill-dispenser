#include "pill_app/app.h"
#include "pill_app/display_model.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "board_support/board.h"
#include "board_support/audio.h"
#include "dose_controller/dose_controller.h"
#include "dose_controller/storage_nvs.h"
#include "esp_check.h"
#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_path.h"
#include "nvs.h"
#include "pill_ai_transport/transport.h"
#include "pill_audio/player.h"
#include "pill_audio/wav_reader.h"
#include "schedule_store/schedule_store.h"
#include "schedule_store/storage_nvs.h"

static const char *TAG = "pill_app";
static dose_nvs_storage_t s_storage;
static dose_controller_t s_controller;
static volatile int s_reminder_display_minute = -1;
static volatile bool s_ui_chinese;
static volatile bool s_button_ai_starting;
static void start_ai_with_prompts(void);

#define BUTTON_AI_LONG_PRESS_MS 1000
#define WAKE_PROMPT_VOLUME 100
#define WAKE_PROMPT_GAIN_NUMERATOR 1
#define WAKE_PROMPT_GAIN_DENOMINATOR 1

static void button_ai_task(void *unused)
{
    (void)unused;
    start_ai_with_prompts();
    s_button_ai_starting = false;
    vTaskDelete(NULL);
}

static void start_ai_from_button(void)
{
    if (s_button_ai_starting ||
        pill_ai_runtime_state() != PILL_AI_RUNTIME_IDLE) {
        return;
    }
    s_button_ai_starting = true;
    if (xTaskCreate(button_ai_task, "button_ai", 8192, NULL, 3, NULL) !=
        pdPASS) {
        s_button_ai_starting = false;
        ESP_LOGW(TAG, "Button AI task unavailable");
    }
}
static bool selected_servo_cycle(void *unused)
{
    (void)unused;
    return board_servo_cycle_channel(0);
}

static void patient_button_task(void *unused)
{
    (void)unused;
    nvs_handle_t runtime = 0;
    if (nvs_open("reminder_active", NVS_READWRITE, &runtime) != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }
    bool was_pressed = false;
    bool reminder_claimed_press = false;
    bool long_press_handled = false;
    TickType_t pressed_at = 0;
    for (;;) {
        bool pressed = board_patient_button_pressed();
        if (pressed && !was_pressed) {
            uint8_t due = 0;
            reminder_claimed_press =
                nvs_get_u8(runtime, "due_active", &due) == ESP_OK && due;
            long_press_handled = false;
            pressed_at = xTaskGetTickCount();
            if (reminder_claimed_press &&
                nvs_set_u8(runtime, "action", 1) == ESP_OK) {
                (void)nvs_commit(runtime);
                pill_audio_stop();
            }
        } else if (pressed && !long_press_handled &&
                   !reminder_claimed_press &&
                   xTaskGetTickCount() - pressed_at >=
                       pdMS_TO_TICKS(BUTTON_AI_LONG_PRESS_MS)) {
            long_press_handled = true;
            uint8_t due = 0;
            if (nvs_get_u8(runtime, "due_active", &due) == ESP_OK && due) {
                reminder_claimed_press = true;
                if (nvs_set_u8(runtime, "action", 1) == ESP_OK) {
                    (void)nvs_commit(runtime);
                }
            }
        } else if (!pressed && was_pressed) {
            reminder_claimed_press = false;
            long_press_handled = false;
        }
        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

extern const uint8_t wake_ack_wav_start[] asm("_binary_wake_ack_wav_start");
extern const uint8_t wake_ack_wav_end[] asm("_binary_wake_ack_wav_end");
extern const uint8_t medicine_reminder_wav_start[]
    asm("_binary_medicine_reminder_wav_start");
extern const uint8_t medicine_reminder_wav_end[]
    asm("_binary_medicine_reminder_wav_end");
extern const uint8_t alarm_bell_wav_start[] asm("_binary_alarm_bell_wav_start");
extern const uint8_t alarm_bell_wav_end[] asm("_binary_alarm_bell_wav_end");

static bool play_embedded_wav(const uint8_t *start, const uint8_t *end)
{
    size_t wav_bytes = (size_t)(end - start);
    pill_wav_info_t info = {0};
    if (!pill_wav_parse(start, wav_bytes, &info) ||
        info.data_offset > wav_bytes ||
        info.data_bytes > wav_bytes - info.data_offset ||
        board_audio_set_volume(pill_ai_volume()) != ESP_OK) {
        return false;
    }

    const uint8_t *pcm = start + info.data_offset;
    size_t remaining = info.data_bytes;
    int16_t samples[256];
    bool ok = true;
    while (remaining > 0) {
        size_t bytes = remaining < sizeof(samples) ? remaining : sizeof(samples);
        memcpy(samples, pcm, bytes);
        /* Improve the perceived loudness of the built-in medication prompt.
         * The final board driver saturates safely, so quiet speech is lifted
         * while peaks cannot wrap into noise. */
        for (size_t i = 0; i < bytes / sizeof(samples[0]); ++i) {
            int32_t lifted = (int32_t)samples[i] * 2;
            if (lifted > INT16_MAX) lifted = INT16_MAX;
            if (lifted < INT16_MIN) lifted = INT16_MIN;
            samples[i] = (int16_t)lifted;
        }
        size_t written = 0;
        if (board_audio_write(samples, bytes / sizeof(samples[0]), &written) != ESP_OK ||
            written != bytes / sizeof(samples[0])) {
            ok = false;
            break;
        }
        pcm += bytes;
        remaining -= bytes;
    }
    board_audio_stop();
    return ok;
}

static bool play_embedded_prompt(const uint8_t *wav_start, const uint8_t *wav_end)
{
    size_t wav_bytes = (size_t)(wav_end - wav_start);
    pill_wav_info_t info = {0};
    if (!pill_wav_parse(wav_start, wav_bytes, &info) ||
        info.data_offset > wav_bytes ||
        info.data_bytes > wav_bytes - info.data_offset ||
        board_audio_set_volume(WAKE_PROMPT_VOLUME) != ESP_OK) {
        return false;
    }

    const int16_t *input =
        (const int16_t *)(wav_start + info.data_offset);
    size_t count = info.data_bytes / sizeof(input[0]);
    size_t first = 0;
    while (first < count && abs(input[first]) < 300) ++first;

    int16_t output[256];
    size_t position = first;
    bool ok = true;
    while (position < count) {
        size_t output_count = count - position;
        if (output_count > 256) output_count = 256;
        for (size_t index = 0; index < output_count; ++index) {
            int32_t amplified =
                (int32_t)input[position + index] * WAKE_PROMPT_GAIN_NUMERATOR /
                WAKE_PROMPT_GAIN_DENOMINATOR;
            if (amplified > INT16_MAX) amplified = INT16_MAX;
            if (amplified < INT16_MIN) amplified = INT16_MIN;
            output[index] = (int16_t)amplified;
        }
        position += output_count;
        size_t written = 0;
        if (board_audio_write(output, output_count, &written) != ESP_OK ||
            written != output_count) {
            ok = false;
            break;
        }
    }
    board_audio_stop();
    return ok;
}

static const char *CUSTOM_SPOKEN_PATH =
    "/sdcard/smartpill/audio/spoken.wav";
static const char *CUSTOM_ALARM_PATH =
    "/sdcard/smartpill/audio/reminder.wav";
static const char *DOSE_LOG_PATH =
    "/sdcard/smartpill/dose-history.csv";

static void append_dose_log(const struct tm *when, const char *event,
                            const pill_schedule_t *schedule)
{
    if (when == NULL || event == NULL || schedule == NULL) return;
    (void)mkdir("/sdcard/smartpill", 0775);
    bool new_file = false;
    struct stat info;
    if (stat(DOSE_LOG_PATH, &info) != 0 || info.st_size == 0) new_file = true;
    FILE *file = fopen(DOSE_LOG_PATH, "a");
    if (file == NULL) {
        ESP_LOGW(TAG, "Dose history unavailable");
        return;
    }
    if (new_file) {
        fputs("time,event,schedule_id,medicine\n", file);
    }
    char timestamp[24] = "time unavailable";
    (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M", when);
    fprintf(file, "%s,%s,%s,\"", timestamp, event, schedule->id);
    for (const char *cursor = schedule->medication; *cursor != '\0'; ++cursor) {
        if (*cursor == '"') fputc('"', file);
        fputc(*cursor, file);
    }
    fputs("\"\n", file);
    fflush(file);
    fclose(file);
}

static bool custom_audio_valid(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    uint8_t header[4096];
    size_t header_bytes = fread(header, 1, sizeof(header), file);
    pill_wav_info_t info = {0};
    bool valid = pill_wav_parse(header, header_bytes, &info) &&
                 fseek(file, 0, SEEK_END) == 0;
    long file_bytes = valid ? ftell(file) : -1;
    fclose(file);
    return valid && file_bytes >= 0 &&
           info.data_offset + info.data_bytes == (uint32_t)file_bytes;
}

static void play_due_sound(nvs_handle_t runtime)
{
    uint8_t custom_spoken = 0;
    bool spoken_played =
        nvs_get_u8(runtime, "custom_spoken", &custom_spoken) == ESP_OK &&
        custom_spoken != 0 && custom_audio_valid(CUSTOM_SPOKEN_PATH) &&
        pill_audio_play_file_at_volume(CUSTOM_SPOKEN_PATH, pill_ai_volume());
    if (spoken_played) {
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(31000);
        while (pill_audio_state() != PILL_AUDIO_IDLE &&
               xTaskGetTickCount() < deadline) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (pill_audio_state() != PILL_AUDIO_IDLE) pill_audio_stop();
    } else {
        play_embedded_wav(medicine_reminder_wav_start,
                          medicine_reminder_wav_end);
    }

    /* The codec has just been muted by the spoken prompt.  Give its output
     * path time to settle before a file-player task unmutes it again. */
    vTaskDelay(pdMS_TO_TICKS(250));

    /* Never keep the schedule task inside an alarm loop.  A reminder is
     * replayed by repeat_at after three minutes, while a later scheduled dose
     * must always get a chance to rotate the tray. */
    uint8_t custom = 0;
    bool custom_selected =
        nvs_get_u8(runtime, "custom_sound", &custom) == ESP_OK &&
        custom != 0 && custom_audio_valid(CUSTOM_ALARM_PATH);
    bool started = false;
    for (unsigned attempt = 0; custom_selected && attempt < 3 && !started;
         ++attempt) {
        if (pill_audio_state() == PILL_AUDIO_FAULT) pill_audio_stop();
        started = pill_audio_play_file_at_volume(CUSTOM_ALARM_PATH,
                                                 pill_ai_volume());
        if (!started) vTaskDelay(pdMS_TO_TICKS(120));
    }
    if (!started) {
        (void)play_embedded_wav(alarm_bell_wav_start, alarm_bell_wav_end);
    }
}

static bool schedule_id_list_contains(const char *list, const char *id)
{
    size_t id_length = strlen(id);
    const char *cursor = list;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == id_length && strncmp(cursor, id, length) == 0) return true;
        if (end == NULL) break;
        cursor = end + 1;
    }
    return false;
}

static void show_due_reminder(nvs_handle_t runtime,
                              const pill_schedule_t *schedule,
                              const struct tm *now)
{
    struct tm copy = *now;
    int64_t repeat_at = (int64_t)mktime(&copy) + 180;
    if (nvs_set_u8(runtime, "due_active", 1) != ESP_OK ||
        nvs_set_str(runtime, "due_med", schedule->medication) != ESP_OK ||
        nvs_set_str(runtime, "due_id", schedule->id) != ESP_OK ||
        nvs_set_u8(runtime, "action", 0) != ESP_OK ||
        nvs_set_i64(runtime, "repeat_at", repeat_at) != ESP_OK ||
        nvs_commit(runtime) != ESP_OK) {
        return;
    }
    board_display_view_t view = {0};
    if (pill_display_build(PILL_DISPLAY_DOSE_DUE, true, now->tm_hour,
                           now->tm_min, &view)) {
        if (s_ui_chinese) {
            snprintf(view.status_text, sizeof(view.status_text), "该吃药");
        }
        s_reminder_display_minute = now->tm_hour * 60 + now->tm_min;
        board_display_show(&view);
    }
    play_due_sound(runtime);
}

static void reminder_task(void *unused)
{
    (void)unused;
    nvs_handle_t runtime = 0;
    schedule_nvs_t schedule_nvs = {0};
    if (nvs_open("reminder_active", NVS_READWRITE, &runtime) != ESP_OK ||
        schedule_nvs_open(&schedule_nvs) != ESP_OK) {
        ESP_LOGW(TAG, "RTC reminder storage unavailable");
        vTaskDelete(NULL);
        return;
    }
    schedule_store_backend_t backend = schedule_nvs_backend(&schedule_nvs);
    for (;;) {
        char active_ids[SCHEDULE_STORE_MAX_DRAFTS * PILL_SCHEDULE_ID_MAX + 1] = {0};
        size_t id_length = sizeof(active_ids);
        if (nvs_get_str(runtime, "active_ids", active_ids, &id_length) == ESP_OK) {
            schedule_store_t store;
            if (schedule_store_load(&store, &backend) == SCHEDULE_STORE_OK) {
                struct tm now = {0};
                bool valid = false;
                if (board_rtc_read(&now, &valid) == ESP_OK && valid) {
                    uint16_t minute = (uint16_t)(now.tm_hour * 60 + now.tm_min);
                    for (size_t index = 0; index < store.count; ++index) {
                        const pill_schedule_t *schedule =
                            &store.drafts[index].schedule;
                        if (!schedule->enabled ||
                            !schedule_id_list_contains(active_ids,
                                                       schedule->id)) {
                            continue;
                        }
                        uint8_t due_active = 0;
                        uint8_t action = 0;
                        char due_id[PILL_SCHEDULE_ID_MAX] = {0};
                        size_t due_id_length = sizeof(due_id);
                        (void)nvs_get_u8(runtime, "due_active", &due_active);
                        (void)nvs_get_u8(runtime, "action", &action);
                        (void)nvs_get_str(runtime, "due_id", due_id,
                                          &due_id_length);
                        bool owns_due = strcmp(due_id, schedule->id) == 0;
                        if (due_active && owns_due && action != 0) {
                            char event_time[24];
                            if (strftime(event_time, sizeof(event_time),
                                         "%Y-%m-%d %H:%M", &now) == 0) {
                                strlcpy(event_time, "time unavailable",
                                        sizeof(event_time));
                            }
                            nvs_set_u8(runtime, "due_active", 0);
                            nvs_set_u8(runtime, "action", 0);
                            nvs_set_str(runtime, "last_event",
                                        action == 1 ? "taken" : "snoozed");
                            nvs_set_str(runtime, "last_time", event_time);
                            if (action == 2) {
                                time_t current = mktime(&now);
                                nvs_set_i64(
                                    runtime, "snooze_at",
                                    (int64_t)current +
                                        schedule->snooze_minutes * 60);
                            } else {
                                nvs_erase_key(runtime, "snooze_at");
                            }
                            nvs_erase_key(runtime, "repeat_at");
                            nvs_commit(runtime);
                            append_dose_log(&now,
                                            action == 1 ? "taken" : "snoozed",
                                            schedule);
                            s_reminder_display_minute = -1;
                        }

                        int64_t snooze_at = 0;
                        time_t current = mktime(&now);
                        if (!due_active && owns_due &&
                            nvs_get_i64(runtime, "snooze_at", &snooze_at) ==
                                ESP_OK &&
                            snooze_at <= (int64_t)current &&
                            pill_ai_runtime_state() == PILL_AI_RUNTIME_IDLE &&
                            pill_audio_state() == PILL_AUDIO_IDLE) {
                            nvs_erase_key(runtime, "snooze_at");
                            nvs_commit(runtime);
                            show_due_reminder(runtime, schedule, &now);
                            due_active = 1;
                        }
                        int64_t repeat_at = 0;
                        if (due_active && owns_due && action == 0 &&
                            nvs_get_i64(runtime, "repeat_at", &repeat_at) ==
                                ESP_OK &&
                            repeat_at <= (int64_t)current &&
                            pill_ai_runtime_state() == PILL_AI_RUNTIME_IDLE &&
                            pill_audio_state() == PILL_AUDIO_IDLE) {
                            show_due_reminder(runtime, schedule, &now);
                        }
                        for (uint8_t time = 0; time < schedule->time_count; ++time) {
                            if (schedule->times[time] != minute) continue;
                            char occurrence[PILL_OCCURRENCE_ID_MAX];
                            char last_occurrence[PILL_OCCURRENCE_ID_MAX] = {0};
                            char last_key[12];
                            snprintf(last_key, sizeof(last_key), "last_%u",
                                     (unsigned)index);
                            size_t last_length = sizeof(last_occurrence);
                            (void)nvs_get_str(runtime, last_key,
                                              last_occurrence, &last_length);
                            if (!pill_occurrence_id(
                                    schedule->id, now.tm_year + 1900,
                                    now.tm_mon + 1, now.tm_mday, minute,
                                    occurrence, sizeof(occurrence)) ||
                                strcmp(occurrence, last_occurrence) == 0) {
                                continue;
                            }
                            /* A missed confirmation must not lock out a new
                             * scheduled dose.  Stop the old reminder and
                             * dispense the new occurrence immediately. */
                            if (pill_audio_state() != PILL_AUDIO_IDLE) {
                                pill_audio_stop();
                            }
                            struct tm intended = now;
                            intended.tm_sec = 0;
                            dose_controller_result_t dispense =
                                dose_controller_process_due(
                                    &s_controller, occurrence,
                                    (int64_t)mktime(&intended),
                                    (int64_t)current, true);
                            if (dispense != DOSE_CONTROLLER_DISPENSED &&
                                dispense != DOSE_CONTROLLER_ALREADY_RECORDED &&
                                dispense != DOSE_CONTROLLER_FAULT) {
                                ESP_LOGW(TAG,
                                         "Automatic dispense deferred: %d",
                                         (int)dispense);
                                continue;
                            }
                            if (dispense == DOSE_CONTROLLER_FAULT) {
                                ESP_LOGE(TAG,
                                         "Automatic dispense failed; reminder continues");
                                nvs_set_str(runtime, "last_event",
                                            "dispense_fault");
                            }
                            append_dose_log(
                                &now,
                                dispense == DOSE_CONTROLLER_FAULT
                                    ? "dispense_fault"
                                    : "dispensed",
                                schedule);
                            if (nvs_set_str(runtime, last_key, occurrence) ==
                                    ESP_OK &&
                                nvs_commit(runtime) == ESP_OK) {
                                show_due_reminder(runtime, schedule, &now);
                                due_active = 1;
                            }
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void start_ai_with_prompts(void)
{
    /* Xiaozhi has one audio owner.  Wake detection is already stopped and
     * destroyed before this transition, so also finish any stale output
     * before opening the conversation channel. */
    if (pill_audio_state() != PILL_AUDIO_IDLE) {
        pill_audio_stop();
        for (unsigned waited_ms = 0;
             pill_audio_state() != PILL_AUDIO_IDLE && waited_ms < 1500;
             waited_ms += 20) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    bool conversation_started = pill_ai_start_conversation_deferred();
    if (!conversation_started) {
        ESP_LOGW(TAG, "AI start rejected: ai=%d audio=%d",
                 (int)pill_ai_runtime_state(), (int)pill_audio_state());
        return;
    }
    unsigned ready_wait_ms = 0;
    while (!pill_ai_connection_ready() &&
           pill_ai_runtime_state() == PILL_AI_RUNTIME_CONNECTING &&
           ready_wait_ms < 12000) {
        vTaskDelay(pdMS_TO_TICKS(20));
        ready_wait_ms += 20;
    }
    if (pill_ai_connection_ready()) {
        /* Match Xiaozhi's interaction: a short popup sound means the
         * microphone is about to become live.  Do not capture the popup on
         * hardware without acoustic echo cancellation. */
        if (!play_embedded_prompt(wake_ack_wav_start, wake_ack_wav_end)) {
            ESP_LOGW(TAG, "Wake popup playback failed");
        }
        pill_ai_begin_listening();
    } else {
        ESP_LOGW(TAG, "AI connection was not ready after wake wait");
        pill_ai_begin_listening();
    }
    while (pill_ai_runtime_state() != PILL_AI_RUNTIME_IDLE) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void wake_word_task(void *unused)
{
    (void)unused;
    for (;;) {
        srmodel_list_t *models = esp_srmodel_init("model");
        afe_config_t *config = models
                                   ? afe_config_init("M", models, AFE_TYPE_SR,
                                                     AFE_MODE_HIGH_PERF)
                                   : NULL;
        if (config != NULL) {
            /* Match Xiaozhi's official AFE path.  The board has one real
             * microphone and no playback-reference input. */
            config->aec_init = false;
            config->se_init = false;
            config->wakenet_mode = DET_MODE_90;
            config->afe_perferred_core = 1;
            config->afe_perferred_priority = 1;
            config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
        }
        esp_afe_sr_iface_t *afe =
            config ? esp_afe_handle_from_config(config) : NULL;
        esp_afe_sr_data_t *afe_data =
            afe ? afe->create_from_config(config) : NULL;
        if (afe_data == NULL || afe->get_samp_rate(afe_data) != 16000) {
            ESP_LOGW(TAG, "Offline wake word unavailable; web Talk remains available");
            if (afe_data != NULL) afe->destroy(afe_data);
            if (config != NULL) afe_config_free(config);
            if (models != NULL) esp_srmodel_deinit(models);
            vTaskDelete(NULL);
            return;
        }
        int samples_per_chunk = afe->get_feed_chunksize(afe_data);
        int16_t *samples = malloc((size_t)samples_per_chunk * sizeof(int16_t));
        if (samples == NULL) {
            afe->destroy(afe_data);
            afe_config_free(config);
            esp_srmodel_deinit(models);
            vTaskDelete(NULL);
            return;
        }
        ESP_LOGI(TAG, "Official AFE wake word ready: feed=%d fetch=%d",
                 samples_per_chunk, afe->get_fetch_chunksize(afe_data));
        afe->print_pipeline(afe_data);

        bool detected = false;
        bool was_paused = false;
        while (!detected) {
            if (s_button_ai_starting ||
                pill_ai_runtime_state() != PILL_AI_RUNTIME_IDLE) {
                was_paused = true;
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            if (was_paused) {
                afe->reset_buffer(afe_data);
                was_paused = false;
            }
            bool captured = true;
            for (size_t offset = 0; offset < (size_t)samples_per_chunk;) {
                size_t request = (size_t)samples_per_chunk - offset;
                if (request > 320) request = 320;
                size_t frames = 0;
                if (board_audio_read(samples + offset, request, &frames) != ESP_OK ||
                    frames != request) {
                    captured = false;
                    break;
                }
                offset += frames;
            }
            if (!captured) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            afe->feed(afe_data, samples);
            afe_fetch_result_t *result =
                afe->fetch_with_delay(afe_data, pdMS_TO_TICKS(1000));
            detected = result != NULL && result->ret_value != ESP_FAIL &&
                       result->wakeup_state == WAKENET_DETECTED;
        }

        ESP_LOGI(TAG, "AFE wake word detected; releasing AFE for conversation");
        free(samples);
        afe->destroy(afe_data);
        afe_config_free(config);
        esp_srmodel_deinit(models);

        /* Let AFE's worker finish returning its internal buffers before the
         * TLS/WebSocket conversation task allocates its stack. */
        vTaskDelay(pdMS_TO_TICKS(100));

        start_ai_with_prompts();
        /* Keep WakeNet deaf briefly so the dispenser cannot wake on its own
         * prompt or the tail of the assistant's loudspeaker output. */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void clock_refresh_task(void *unused)
{
    (void)unused;
    nvs_handle_t ui_nvs = 0;
    (void)nvs_open("ui_prefs", NVS_READONLY, &ui_nvs);
    int displayed_hour = -1;
    int displayed_minute = -1;
    pill_ai_runtime_state_t displayed_ai_state = PILL_AI_RUNTIME_FAILED;
    bool was_reminder_visible = false;
    for (;;) {
        if (ui_nvs == 0) {
            (void)nvs_open("ui_prefs", NVS_READONLY, &ui_nvs);
        }
        struct tm rtc_time = {0};
        bool time_valid = false;
        esp_err_t rtc_error = board_rtc_read(&rtc_time, &time_valid);
        pill_ai_runtime_state_t ai_state = pill_ai_runtime_state();
        uint8_t chinese = 0;
        if (ui_nvs != 0 &&
            nvs_get_u8(ui_nvs, "chinese", &chinese) == ESP_OK &&
            s_ui_chinese != (chinese != 0)) {
            s_ui_chinese = chinese != 0;
            displayed_ai_state = PILL_AI_RUNTIME_FAILED;
        }
        int minute_of_day = rtc_time.tm_hour * 60 + rtc_time.tm_min;
        bool reminder_visible =
            ai_state == PILL_AI_RUNTIME_IDLE &&
            s_reminder_display_minute == minute_of_day;
        if (s_reminder_display_minute >= 0 &&
            s_reminder_display_minute != minute_of_day) {
            s_reminder_display_minute = -1;
        }
        if (!reminder_visible &&
            (rtc_time.tm_hour != displayed_hour || rtc_time.tm_min != displayed_minute ||
             ai_state != displayed_ai_state || was_reminder_visible)) {
            board_display_view_t view = {0};
            bool view_ready = true;
            if (rtc_error == ESP_OK && time_valid) {
                (void)snprintf(view.time_text, sizeof(view.time_text), "%02d:%02d",
                               rtc_time.tm_hour, rtc_time.tm_min);
            } else {
                strlcpy(view.time_text, "--:--", sizeof(view.time_text));
            }
            switch (ai_state) {
                case PILL_AI_RUNTIME_CONNECTING:
                    snprintf(view.status_text, sizeof(view.status_text), "%s",
                             s_ui_chinese ? "连接中" : "CONNECTING");
                    view.icon = BOARD_DISPLAY_ICON_RING;
                    view.accent_rgb565 = 0x07ff;
                    break;
                case PILL_AI_RUNTIME_LISTENING:
                    snprintf(view.status_text, sizeof(view.status_text), "%s",
                             s_ui_chinese ? "请说话" : "LISTENING");
                    view.icon = BOARD_DISPLAY_ICON_RING;
                    view.accent_rgb565 = 0x07e0;
                    break;
                case PILL_AI_RUNTIME_THINKING:
                    snprintf(view.status_text, sizeof(view.status_text), "%s",
                             s_ui_chinese ? "思考中" : "THINKING");
                    view.icon = BOARD_DISPLAY_ICON_CLOCK_WARNING;
                    view.accent_rgb565 = 0xffe0;
                    break;
                case PILL_AI_RUNTIME_SPEAKING:
                    snprintf(view.status_text, sizeof(view.status_text), "%s",
                             s_ui_chinese ? "回答中" : "SPEAKING");
                    view.icon = BOARD_DISPLAY_ICON_CHECK;
                    view.accent_rgb565 = 0x07ff;
                    break;
                case PILL_AI_RUNTIME_FAILED:
                    snprintf(view.status_text, sizeof(view.status_text), "%s",
                             s_ui_chinese ? "出错了" : "AI ERROR");
                    view.icon = BOARD_DISPLAY_ICON_CROSS;
                    view.accent_rgb565 = 0xf800;
                    break;
                default:
                    view_ready = pill_display_build_startup(
                        rtc_error == ESP_OK, time_valid, rtc_time.tm_hour,
                        rtc_time.tm_min, &view);
                    if (view_ready && s_ui_chinese) {
                        const char *status = "待机";
                        if (rtc_error != ESP_OK) {
                            status = "时钟故障";
                        } else if (!time_valid) {
                            status = "请设时间";
                        }
                        snprintf(view.status_text, sizeof(view.status_text), "%s",
                                 status);
                    }
                    break;
            }
            if (view_ready && board_display_show(&view) == ESP_OK) {
                displayed_hour = rtc_time.tm_hour;
                displayed_minute = rtc_time.tm_min;
                displayed_ai_state = ai_state;
            }
        }
        was_reminder_visible = reminder_visible;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

esp_err_t pill_app_start(void)
{
    ESP_RETURN_ON_ERROR(dose_nvs_storage_init(&s_storage), TAG, "dose storage");
    s_controller = (dose_controller_t){
        .context = &s_storage,
        .load = dose_nvs_load,
        .store = dose_nvs_store,
        .servo_cycle = selected_servo_cycle,
    };

    dose_record_t last = {0};
    bool found = false;
    if (!dose_nvs_load_latest(&s_storage, &last, &found)) {
        ESP_LOGE(TAG, "Dose history failed integrity validation; dispensing locked");
        return ESP_ERR_INVALID_CRC;
    }
    if (found && last.state == PILL_DOSE_DISPENSING) {
        ESP_LOGE(TAG, "Interrupted movement detected; forcing FAULT without moving servo");
        if (dose_controller_recover(&s_controller, last.occurrence_id) !=
            DOSE_CONTROLLER_FAULT) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    struct tm rtc_time = {0};
    bool time_valid = false;
    esp_err_t rtc_error = board_rtc_read(&rtc_time, &time_valid);
    if (rtc_error != ESP_OK || !time_valid) {
        ESP_LOGW(TAG, "RTC unavailable or invalid; automatic dispensing remains locked");
    } else {
        ESP_LOGI(TAG, "RTC valid: %04d-%02d-%02d %02d:%02d:%02d",
                 rtc_time.tm_year + 1900, rtc_time.tm_mon + 1, rtc_time.tm_mday,
                 rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
    }

    board_display_view_t view = {0};
    if (pill_display_build_startup(rtc_error == ESP_OK, time_valid,
                                   rtc_time.tm_hour, rtc_time.tm_min, &view)) {
        esp_err_t display_error = board_display_show(&view);
        if (display_error != ESP_OK) {
            ESP_LOGW(TAG, "Patient display update failed: %s", esp_err_to_name(display_error));
        }
    }
    if (xTaskCreate(clock_refresh_task, "clock_refresh", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Clock refresh task unavailable; RTC remains valid");
    }
    /* Field-stable basic build: leave offline AI wake disabled so display,
     * reminders, RTC, TF and dispensing keep all available memory. */
    ESP_LOGI(TAG, "Offline AI wake disabled in basic-function build");
    if (xTaskCreate(reminder_task, "reminder", 6144, NULL, 2, NULL) != pdPASS) {
        ESP_LOGW(TAG, "RTC reminder task unavailable; dispensing remains locked");
    }
    if (xTaskCreate(patient_button_task, "patient_button", 4096, NULL, 2,
                    NULL) != pdPASS) {
        ESP_LOGW(TAG, "Patient button task unavailable");
    }

    ESP_LOGI(TAG, "Safety controller ready; no schedules are installed by default");
    return ESP_OK;
}
