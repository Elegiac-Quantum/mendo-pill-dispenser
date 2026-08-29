#include "pill_ai_transport/transport.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "board_support/audio.h"
#include "board_support/board.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_websocket_client.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "pill_ai_transport/config_nvs.h"
#include "pill_ai_transport/policy.h"
#include "pill_ai_transport/wire_protocol.h"
#include "pill_audio/player.h"
#include "opus.h"

#define CONNECTED_BIT BIT0
#define HELLO_BIT BIT1
#define TTS_STOP_BIT BIT2
#define FAILED_BIT BIT3

static const char *TAG = "pill_ai_ws";
static volatile pill_ai_runtime_state_t s_state = PILL_AI_RUNTIME_IDLE;
static const char *s_last_error = "none";
static EventGroupHandle_t s_events;
static OpusDecoder *s_decoder;
static char s_session_id[80];
static uint8_t s_volume = 60;
static bool s_volume_loaded;
static uint16_t s_vad_threshold = PILL_AI_VAD_RECOMMENDED;
static bool s_vad_loaded;
static volatile uint32_t s_vad_minimum;
static volatile uint32_t s_vad_maximum;
static volatile bool s_listening_permitted = true;
static volatile bool s_connection_ready;
static char s_activation_code[16];
static char s_service_url[256] = "wss://api.tenclass.net/xiaozhi/v1/";
static char s_service_token[256] = "test-token";
static const char *s_client_id = "689b7f66-96b9-46d7-8bc5-98d45ceaf7ec";

typedef struct {
    char data[4096];
    size_t length;
} activation_response_t;

static esp_err_t activation_http_event(esp_http_client_event_t *event)
{
    activation_response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_DATA && response != NULL &&
        event->data_len > 0 &&
        response->length + (size_t)event->data_len < sizeof(response->data)) {
        memcpy(response->data + response->length, event->data,
               (size_t)event->data_len);
        response->length += (size_t)event->data_len;
        response->data[response->length] = '\0';
    }
    return ESP_OK;
}

static bool copy_json_string(cJSON *object, const char *name,
                             char *destination, size_t capacity)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsString(value) || value->valuestring[0] == '\0' ||
        strlcpy(destination, value->valuestring, capacity) >= capacity) {
        return false;
    }
    return true;
}

static bool refresh_service_config(void)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[18];
    snprintf(device_id, sizeof(device_id), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    activation_response_t response = {0};
    const esp_http_client_config_t config = {
        .url = "https://api.tenclass.net/xiaozhi/ota/",
        .method = HTTP_METHOD_POST,
        .event_handler = activation_http_event,
        .user_data = &response,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return false;
    esp_http_client_set_header(client, "Activation-Version", "1");
    esp_http_client_set_header(client, "Device-Id", device_id);
    esp_http_client_set_header(client, "Client-Id", s_client_id);
    esp_http_client_set_header(client, "User-Agent",
                               "smart-pill-dispenser/1.0 esp32s3");
    esp_http_client_set_header(client, "Accept-Language", "zh-CN");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, "{}", 2);
    esp_err_t result = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK || status != 200 || response.length == 0) {
        ESP_LOGW(TAG, "Activation check failed: %s HTTP %d",
                 esp_err_to_name(result), status);
        return false;
    }
    cJSON *root = cJSON_Parse(response.data);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON *activation = cJSON_GetObjectItemCaseSensitive(root, "activation");
    cJSON *websocket = cJSON_GetObjectItemCaseSensitive(root, "websocket");
    bool ready = false;
    if (cJSON_IsObject(activation) &&
        copy_json_string(activation, "code", s_activation_code,
                         sizeof(s_activation_code))) {
        ESP_LOGW(TAG, "Xiaozhi activation code: %s", s_activation_code);
        const board_display_view_t view = {
            .time_text = "BIND",
            .icon = BOARD_DISPLAY_ICON_WARNING,
            .accent_rgb565 = 0xffe0,
        };
        board_display_view_t code_view = view;
        strlcpy(code_view.status_text, s_activation_code,
                sizeof(code_view.status_text));
        (void)board_display_show(&code_view);
    } else if (cJSON_IsObject(websocket) &&
               copy_json_string(websocket, "url", s_service_url,
                                sizeof(s_service_url)) &&
               copy_json_string(websocket, "token", s_service_token,
                                sizeof(s_service_token))) {
        s_activation_code[0] = '\0';
        ready = true;
    }
    cJSON_Delete(root);
    return ready;
}

const char *pill_ai_activation_code(void) { return s_activation_code; }

static void activation_check_task(void *unused)
{
    (void)unused;
    vTaskDelay(pdMS_TO_TICKS(8000));
    if (!refresh_service_config()) {
        s_last_error = s_activation_code[0] != '\0'
                           ? "activation_required" : "activation_check";
    } else if (strcmp(s_last_error, "activation_required") == 0 ||
               strcmp(s_last_error, "activation_check") == 0) {
        s_last_error = "none";
    }
    vTaskDelete(NULL);
}

void pill_ai_check_activation_async(void)
{
    if (xTaskCreate(activation_check_task, "ai_activation", 8192, NULL, 2,
                    NULL) != pdPASS) {
        ESP_LOGW(TAG, "Activation check task unavailable");
    }
}

uint8_t pill_ai_volume(void)
{
    if (!s_volume_loaded) {
        nvs_handle_t handle = 0;
        uint8_t saved = 100;
        uint8_t loudness_default_applied = 0;
        if (nvs_open("pill_ai", NVS_READWRITE, &handle) == ESP_OK) {
            if (nvs_get_u8(handle, "loud_default_v2",
                           &loudness_default_applied) == ESP_OK &&
                loudness_default_applied != 0 &&
                nvs_get_u8(handle, "volume", &saved) == ESP_OK &&
                saved <= 100) {
                s_volume = saved;
            } else {
                /* Existing units silently carried the old 60% default.  New
                 * and upgraded medication dispensers start at full volume;
                 * caregivers can still lower it from the web control. */
                s_volume = 100;
                (void)nvs_set_u8(handle, "volume", s_volume);
                (void)nvs_set_u8(handle, "loud_default_v2", 1);
                (void)nvs_commit(handle);
            }
            nvs_close(handle);
        }
        s_volume_loaded = true;
    }
    return s_volume;
}

esp_err_t pill_ai_set_volume(uint8_t volume)
{
    if (volume > 100) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open("pill_ai", NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_u8(handle, "volume", volume);
    if (result == ESP_OK) result = nvs_set_u8(handle, "loud_default_v2", 1);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result == ESP_OK) {
        s_volume = volume;
        s_volume_loaded = true;
    }
    return result;
}

uint16_t pill_ai_vad_threshold(void)
{
    if (!s_vad_loaded) {
        nvs_handle_t handle = 0;
        uint16_t saved = PILL_AI_VAD_RECOMMENDED;
        if (nvs_open("pill_ai", NVS_READONLY, &handle) == ESP_OK) {
            if (nvs_get_u16(handle, "vad", &saved) == ESP_OK &&
                saved >= 10 && saved <= 500) {
                s_vad_threshold = saved;
            }
            nvs_close(handle);
        }
        s_vad_loaded = true;
    }
    return s_vad_threshold;
}

esp_err_t pill_ai_set_vad_threshold(uint16_t threshold)
{
    if (threshold < 10 || threshold > 500) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open("pill_ai", NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_u16(handle, "vad", threshold);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result == ESP_OK) {
        s_vad_threshold = threshold;
        s_vad_loaded = true;
    }
    return result;
}

uint32_t pill_ai_vad_minimum(void) { return s_vad_minimum; }
uint32_t pill_ai_vad_maximum(void) { return s_vad_maximum; }

static void ws_event(void *argument, esp_event_base_t base, int32_t event_id,
                     void *event_data)
{
    (void)base;
    esp_websocket_client_handle_t client = argument;
    esp_websocket_event_data_t *data = event_data;
    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        xEventGroupSetBits(s_events, CONNECTED_BIT);
        char hello[256];
        if (pill_ai_make_hello(hello, sizeof(hello))) {
            esp_websocket_client_send_text(client, hello, strlen(hello),
                                           pdMS_TO_TICKS(1000));
        }
    } else if (event_id == WEBSOCKET_EVENT_DISCONNECTED ||
               event_id == WEBSOCKET_EVENT_ERROR) {
        xEventGroupSetBits(s_events, FAILED_BIT);
    } else if (event_id == WEBSOCKET_EVENT_DATA && data->op_code == 0x1 &&
               data->payload_len < 256 &&
               data->payload_offset + data->data_len == data->payload_len) {
        char json[256];
        memcpy(json, data->data_ptr, data->data_len);
        json[data->data_len] = '\0';
        pill_ai_wire_event_t event = pill_ai_wire_event(json);
        if (event == PILL_AI_WIRE_HELLO) {
            pill_ai_read_session_id(json, s_session_id, sizeof(s_session_id));
            xEventGroupSetBits(s_events, HELLO_BIT);
        } else if (event == PILL_AI_WIRE_TTS_START) {
            if (board_audio_set_volume(pill_ai_volume()) == ESP_OK) {
                s_state = PILL_AI_RUNTIME_SPEAKING;
            } else {
                s_last_error = "speaker_start";
                xEventGroupSetBits(s_events, FAILED_BIT);
            }
        } else if (event == PILL_AI_WIRE_TTS_STOP) {
            board_audio_stop();
            xEventGroupSetBits(s_events, TTS_STOP_BIT);
        } else if (event == PILL_AI_WIRE_ERROR) {
            xEventGroupSetBits(s_events, FAILED_BIT);
        }
    } else if (event_id == WEBSOCKET_EVENT_DATA && data->op_code == 0x2 &&
               data->data_len > 0 &&
               data->payload_offset + data->data_len == data->payload_len &&
               s_decoder != NULL && pill_audio_state() == PILL_AUDIO_IDLE) {
        int16_t pcm[960];
        int frames = opus_decode(s_decoder, (const unsigned char *)data->data_ptr,
                                 data->data_len, pcm, 960, 0);
        if (frames > 0) {
            /* The onboard speaker is quiet even at the codec's 100% setting.
             * Add 6 dB in PCM while saturating to avoid integer wraparound. */
            for (int index = 0; index < frames; ++index) {
                int32_t amplified = (int32_t)pcm[index] * 2;
                if (amplified > INT16_MAX) amplified = INT16_MAX;
                if (amplified < INT16_MIN) amplified = INT16_MIN;
                pcm[index] = (int16_t)amplified;
            }
            for (size_t offset = 0; offset < (size_t)frames;) {
                size_t chunk = (size_t)frames - offset;
                if (chunk > 256) chunk = 256;
                size_t written = 0;
                if (board_audio_write(pcm + offset, chunk, &written) != ESP_OK ||
                    written != chunk) {
                    break;
                }
                offset += chunk;
            }
        } else {
            ESP_LOGW(TAG, "Opus decode failed: %d", frames);
        }
    }
}

static bool send_text(esp_websocket_client_handle_t client, bool start)
{
    char message[96];
    return pill_ai_make_listen_session(message, sizeof(message), s_session_id, start) &&
           esp_websocket_client_send_text(client, message, strlen(message),
                                          pdMS_TO_TICKS(1000)) > 0;
}

static uint32_t speech_activity(const int16_t *samples, size_t count)
{
    int16_t minimum = INT16_MAX;
    int16_t maximum = INT16_MIN;
    for (size_t index = 0; index < count; ++index) {
        if (samples[index] < minimum) minimum = samples[index];
        if (samples[index] > maximum) maximum = samples[index];
    }
    return (uint32_t)((int32_t)maximum - (int32_t)minimum);
}

static void conversation_task(void *argument)
{
    (void)argument;
    pill_ai_saved_config_t saved = {0};
    esp_websocket_client_handle_t client = NULL;
    OpusEncoder *encoder = NULL;
    int opus_error = OPUS_OK;
    if (pill_ai_config_load(&saved) != ESP_OK ||
        (s_listening_permitted &&
         pill_audio_state() != PILL_AUDIO_IDLE)) {
        s_last_error = "configuration_or_audio_busy";
        goto failed;
    }
    if (!refresh_service_config()) {
        s_last_error = s_activation_code[0] != '\0'
                           ? "activation_required" : "activation_check";
        goto failed;
    }
    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        s_last_error = "event_group";
        goto failed;
    }
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char headers[256];
    int header_length = snprintf(headers, sizeof(headers),
                                 "Authorization: Bearer %s\r\n"
                                 "Protocol-Version: 1\r\n"
                                 "Device-Id: %02x:%02x:%02x:%02x:%02x:%02x\r\n"
                                 "Client-Id: %s\r\n",
                                 s_service_token,
                                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                                 s_client_id);
    if (header_length <= 0 || (size_t)header_length >= sizeof(headers)) {
        s_last_error = "auth_header";
        goto failed;
    }
    encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &opus_error);
    s_decoder = opus_decoder_create(16000, 1, &opus_error);
    if (encoder == NULL || s_decoder == NULL || opus_error != OPUS_OK) {
        s_last_error = "opus_init";
        goto failed;
    }
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(0));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(16000));
    memset(s_session_id, 0, sizeof(s_session_id));
    esp_websocket_client_config_t config = {
        .uri = s_service_url,
        .headers = headers,
        .buffer_size = 1024,
        .task_stack = (int)pill_ai_websocket_task_stack_bytes(),
        .network_timeout_ms = 10000,
        .reconnect_timeout_ms = 5000,
        .disable_auto_reconnect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    client = esp_websocket_client_init(&config);
    if (client == NULL ||
        esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_event,
                                      client) != ESP_OK ||
        esp_websocket_client_start(client) != ESP_OK) {
        s_last_error = "websocket_start";
        goto failed;
    }
    EventBits_t bits = xEventGroupWaitBits(
        s_events, HELLO_BIT | FAILED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if (!(bits & HELLO_BIT) || (bits & FAILED_BIT)) {
        s_last_error = "handshake";
        goto failed;
    }
    s_connection_ready = true;
    unsigned gate_wait_ms = 0;
    while (!s_listening_permitted && gate_wait_ms < 10000) {
        vTaskDelay(pdMS_TO_TICKS(10));
        gate_wait_ms += 10;
    }
    if (!s_listening_permitted) {
        s_last_error = "listen_gate";
        goto failed;
    }
    if (!send_text(client, true)) {
        s_last_error = "listen_start";
        goto failed;
    }
    s_state = PILL_AI_RUNTIME_LISTENING;
    enum {
        MAX_RECORDING_PACKETS = 500,
        OPUS_FRAME_SAMPLES = 960,
        /* 25 x 60 ms = 1.5 s.  Xiaozhi auto VAD normally ends first; this is
         * only a local fallback when the server does not. */
        END_SILENCE_PACKETS = 25,
    };
    uint32_t end_threshold = pill_ai_vad_threshold();
    uint32_t start_threshold = end_threshold + end_threshold / 2;
    int16_t pcm[OPUS_FRAME_SAMPLES];
    unsigned char opus_packet[1000];
    bool speech_started = false;
    unsigned quiet_packets = 0;
    unsigned loud_packets = 0;
    s_vad_minimum = UINT32_MAX;
    s_vad_maximum = 0;
    for (unsigned packet_index = 0;
         packet_index < MAX_RECORDING_PACKETS; ++packet_index) {
        if (s_state == PILL_AI_RUNTIME_SPEAKING) break;
        if (pill_audio_state() != PILL_AUDIO_IDLE) {
            s_last_error = "audio_busy";
            goto failed;
        }
        for (size_t offset = 0; offset < OPUS_FRAME_SAMPLES; offset += 320) {
            size_t frames = 0;
            if (board_audio_read(pcm + offset, 320, &frames) != ESP_OK ||
                frames != 320) {
                s_last_error = "microphone_read";
                goto failed;
            }
        }
        for (size_t index = 0; index < OPUS_FRAME_SAMPLES; ++index) {
            pcm[index] /= 4;
        }
        int opus_bytes = opus_encode(encoder, pcm, OPUS_FRAME_SAMPLES,
                                    opus_packet, sizeof(opus_packet));
        if (opus_bytes <= 0) {
            s_last_error = "opus_encode";
            goto failed;
        }
        if (esp_websocket_client_send_bin(
                client, (const char *)opus_packet, opus_bytes,
                pdMS_TO_TICKS(3000)) <= 0) {
            s_last_error = "audio_send";
            goto failed;
        }
        /* In Xiaozhi auto mode the server may start TTS as soon as its VAD
         * decides the utterance is complete.  Stop reading the microphone
         * immediately instead of waiting for the local fallback timer. */
        if (s_state == PILL_AI_RUNTIME_SPEAKING) break;
        uint32_t activity = speech_activity(pcm, OPUS_FRAME_SAMPLES);
        if (activity < s_vad_minimum) s_vad_minimum = activity;
        if (activity > s_vad_maximum) s_vad_maximum = activity;
        if (activity >= start_threshold) {
            speech_started = true;
            if (++loud_packets >= 3) {
                quiet_packets = 0;
                loud_packets = 0;
            }
        } else if (speech_started && activity <= end_threshold) {
            loud_packets = 0;
            quiet_packets++;
            if (quiet_packets >= END_SILENCE_PACKETS) break;
        } else {
            loud_packets = 0;
        }
    }
    if (s_state != PILL_AI_RUNTIME_SPEAKING) {
        if (!send_text(client, false)) {
            s_last_error = "listen_stop";
            goto failed;
        }
        s_state = PILL_AI_RUNTIME_THINKING;
    }
    bits = xEventGroupWaitBits(s_events, TTS_STOP_BIT | FAILED_BIT, pdFALSE,
                               pdFALSE, pdMS_TO_TICKS(90000));
    if (!(bits & TTS_STOP_BIT) || (bits & FAILED_BIT)) {
        if (strcmp(s_last_error, "none") == 0) s_last_error = "response";
        goto failed;
    }
    s_last_error = "none";
    s_state = PILL_AI_RUNTIME_IDLE;
    goto cleanup;

failed:
    s_state = PILL_AI_RUNTIME_FAILED;
    ESP_LOGW(TAG, "AI conversation ended without a complete response");

cleanup:
    s_connection_ready = false;
    /* A deferred connection does not own audio until the wake acknowledgement
       has finished and listening is permitted. Do not truncate that prompt. */
    if (s_listening_permitted) board_audio_stop();
    if (client != NULL) {
        esp_websocket_client_stop(client);
        esp_websocket_client_destroy(client);
    }
    if (encoder != NULL) {
        opus_encoder_destroy(encoder);
    }
    if (s_decoder != NULL) {
        opus_decoder_destroy(s_decoder);
        s_decoder = NULL;
    }
    if (s_events != NULL) {
        vEventGroupDelete(s_events);
        s_events = NULL;
    }
    if (s_state == PILL_AI_RUNTIME_FAILED) {
        vTaskDelay(pdMS_TO_TICKS(1500));
        s_state = PILL_AI_RUNTIME_IDLE;
    }
    s_listening_permitted = true;
    vTaskDelete(NULL);
}

static bool start_conversation(bool deferred)
{
    if (s_state != PILL_AI_RUNTIME_IDLE) {
        return false;
    }
    if (pill_audio_state() == PILL_AUDIO_FAULT) {
        pill_audio_stop();
    }
    if (pill_audio_state() != PILL_AUDIO_IDLE) {
        return false;
    }
    s_last_error = "none";
    s_connection_ready = false;
    s_listening_permitted = !deferred;
    s_state = PILL_AI_RUNTIME_CONNECTING;
    /* Keep the TLS/WebSocket task on internal RAM.  32 KB could not be
     * allocated after AFE, while the measured local frame usage fits safely
     * in 16 KB.  External-PSRAM stacks can fault during network operations. */
    if (xTaskCreate(conversation_task, "pill_ai", 16384, NULL, 4, NULL) !=
        pdPASS) {
        s_state = PILL_AI_RUNTIME_IDLE;
        s_listening_permitted = true;
        return false;
    }
    return true;
}

bool pill_ai_start_conversation(void)
{
    return start_conversation(false);
}

bool pill_ai_start_conversation_deferred(void)
{
    return start_conversation(true);
}

bool pill_ai_connection_ready(void)
{
    return s_connection_ready;
}

void pill_ai_begin_listening(void)
{
    s_listening_permitted = true;
}

pill_ai_runtime_state_t pill_ai_runtime_state(void)
{
    return s_state;
}

const char *pill_ai_last_error(void)
{
    return s_last_error;
}
