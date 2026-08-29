#include "pill_web/web.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "board_support/board.h"
#include "board_support/audio.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif_ip_addr.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "cJSON.h"
#include "pill_web/schedule_json.h"
#include "pill_web/web_validation.h"
#include "pill_web/caregiver_auth.h"
#include "pill_audio/player.h"
#include "pill_audio/wav_reader.h"
#include "pill_ai_transport/policy.h"
#include "pill_ai_transport/config_nvs.h"
#include "pill_ai_transport/transport.h"
#include "schedule_store/schedule_store.h"
#include "schedule_store/storage_nvs.h"

static const char *TAG = "pill_web";
static httpd_handle_t s_server;
static schedule_nvs_t s_schedule_nvs;
static schedule_store_backend_t s_schedule_backend;
static schedule_store_t s_schedule_store;
static bool s_schedule_writable;
static nvs_handle_t s_reminder_nvs;
static char s_active_schedule_ids[SCHEDULE_STORE_MAX_DRAFTS *
                                  PILL_SCHEDULE_ID_MAX + 1];
static nvs_handle_t s_ui_nvs;
static bool s_ui_chinese;
static bool s_servo_test_running;
static char s_setup_ap_ssid[33];
static char s_caregiver_session[33];
static char s_fleet_internal_secret[33];
static nvs_handle_t s_fleet_nvs;
typedef struct {
    bool enabled;
    char url[160];
    char device_id[41];
    char token[96];
} fleet_config_t;
static fleet_config_t s_fleet;
static const char *ai_runtime_name(void);


static void create_caregiver_session(void)
{
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        snprintf(s_caregiver_session + index * 2, 3, "%02x", bytes[index]);
    }
}

static bool caregiver_session_valid(httpd_req_t *request)
{
    if (!caregiver_auth_enabled()) return true;
    size_t length = httpd_req_get_hdr_value_len(request, "Cookie");
    if (length == 0 || length > 511 || s_caregiver_session[0] == '\0') {
        return false;
    }
    char cookie[512];
    if (httpd_req_get_hdr_value_str(request, "Cookie", cookie,
                                    sizeof(cookie)) != ESP_OK) {
        return false;
    }
    char expected[64];
    snprintf(expected, sizeof(expected), "caregiver_session=%s",
             s_caregiver_session);
    return strstr(cookie, expected) != NULL;
}

static bool require_caregiver(httpd_req_t *request)
{
    if (caregiver_session_valid(request)) return true;
    size_t fleet_length = httpd_req_get_hdr_value_len(request, "X-Mendo-Fleet-Internal");
    if (fleet_length == 32 && s_fleet_internal_secret[0] != '\0') {
        char supplied[33];
        if (httpd_req_get_hdr_value_str(request, "X-Mendo-Fleet-Internal", supplied,
                                        sizeof(supplied)) == ESP_OK &&
            strcmp(supplied, s_fleet_internal_secret) == 0) return true;
    }
    httpd_resp_set_status(request, "401 Unauthorized");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr(request, "{\"error\":\"caregiver_pin_required\"}");
    return false;
}

static void fleet_load(void)
{
    if (nvs_open("fleet_cfg", NVS_READWRITE, &s_fleet_nvs) != ESP_OK) return;
    uint8_t enabled = 0;
    size_t size = sizeof(s_fleet.url);
    nvs_get_u8(s_fleet_nvs, "enabled", &enabled);
    nvs_get_str(s_fleet_nvs, "url", s_fleet.url, &size);
    size = sizeof(s_fleet.device_id);
    nvs_get_str(s_fleet_nvs, "device_id", s_fleet.device_id, &size);
    size = sizeof(s_fleet.token);
    nvs_get_str(s_fleet_nvs, "token", s_fleet.token, &size);
    s_fleet.enabled = enabled != 0;
}

static esp_err_t fleet_configuration(httpd_req_t *request)
{
    if (request->method == HTTP_GET) {
        char json[300];
        int length = snprintf(json, sizeof(json),
            "{\"enabled\":%s,\"url\":\"%s\",\"device_id\":\"%s\",\"token_saved\":%s}",
            s_fleet.enabled ? "true" : "false", s_fleet.url, s_fleet.device_id,
            s_fleet.token[0] ? "true" : "false");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, json, length);
    }
    if (!require_caregiver(request)) return ESP_OK;
    char body[420] = {0};
    if (request->content_len <= 0 || request->content_len >= (int)sizeof(body) ||
        httpd_req_recv(request, body, request->content_len) != request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid remote configuration");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *enabled = root ? cJSON_GetObjectItemCaseSensitive(root, "enabled") : NULL;
    cJSON *url = root ? cJSON_GetObjectItemCaseSensitive(root, "url") : NULL;
    cJSON *device_id = root ? cJSON_GetObjectItemCaseSensitive(root, "device_id") : NULL;
    cJSON *token = root ? cJSON_GetObjectItemCaseSensitive(root, "token") : NULL;
    bool valid = cJSON_IsBool(enabled) && cJSON_IsString(url) && cJSON_IsString(device_id) &&
                 (cJSON_IsString(token) || (s_fleet.token[0] && cJSON_IsNull(token))) &&
                 strlen(url->valuestring) < sizeof(s_fleet.url) &&
                 strlen(device_id->valuestring) >= 3 && strlen(device_id->valuestring) < sizeof(s_fleet.device_id) &&
                 strncmp(url->valuestring, "https://", 8) == 0;
    if (cJSON_IsString(token)) valid = valid && strlen(token->valuestring) >= 24 && strlen(token->valuestring) < sizeof(s_fleet.token);
    if (!valid) {
        cJSON_Delete(root);
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "HTTPS, device ID and token required");
    }
    s_fleet.enabled = cJSON_IsTrue(enabled);
    strlcpy(s_fleet.url, url->valuestring, sizeof(s_fleet.url));
    while (strlen(s_fleet.url) && s_fleet.url[strlen(s_fleet.url) - 1] == '/') s_fleet.url[strlen(s_fleet.url) - 1] = '\0';
    strlcpy(s_fleet.device_id, device_id->valuestring, sizeof(s_fleet.device_id));
    if (cJSON_IsString(token)) strlcpy(s_fleet.token, token->valuestring, sizeof(s_fleet.token));
    cJSON_Delete(root);
    esp_err_t result = nvs_set_u8(s_fleet_nvs, "enabled", s_fleet.enabled ? 1 : 0);
    if (result == ESP_OK) result = nvs_set_str(s_fleet_nvs, "url", s_fleet.url);
    if (result == ESP_OK) result = nvs_set_str(s_fleet_nvs, "device_id", s_fleet.device_id);
    if (result == ESP_OK) result = nvs_set_str(s_fleet_nvs, "token", s_fleet.token);
    if (result == ESP_OK) result = nvs_commit(s_fleet_nvs);
    if (result != ESP_OK) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    return httpd_resp_sendstr(request, "{\"saved\":true}");
}

static esp_err_t caregiver_access(httpd_req_t *request)
{
    if (request->method == HTTP_GET) {
        char json[96];
        snprintf(json, sizeof(json),
                 "{\"enabled\":%s,\"unlocked\":%s}",
                 caregiver_auth_enabled() ? "true" : "false",
                 caregiver_session_valid(request) ? "true" : "false");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, json);
    }
    enum { BODY_BYTES = 160 };
    if (request->content_len <= 0 || request->content_len >= BODY_BYTES) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid caregiver request");
    }
    char body[BODY_BYTES] = {0};
    int received = httpd_req_recv(request, body,
                                  (size_t)request->content_len);
    if (received != request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "incomplete caregiver request");
    }
    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    cJSON *action =
        root ? cJSON_GetObjectItemCaseSensitive(root, "action") : NULL;
    cJSON *pin = root ? cJSON_GetObjectItemCaseSensitive(root, "pin") : NULL;
    const char *action_text =
        cJSON_IsString(action) ? action->valuestring : "";
    const char *pin_text = cJSON_IsString(pin) ? pin->valuestring : "";
    esp_err_t result = ESP_OK;
    if (strcmp(action_text, "unlock") == 0) {
        if (!caregiver_auth_verify(pin_text)) result = ESP_ERR_INVALID_STATE;
        if (result == ESP_OK) create_caregiver_session();
    } else if (strcmp(action_text, "set") == 0) {
        if (caregiver_auth_enabled() && !caregiver_session_valid(request)) {
            result = ESP_ERR_INVALID_STATE;
        } else {
            result = caregiver_auth_set_pin(pin_text);
            if (result == ESP_OK) create_caregiver_session();
        }
    } else if (strcmp(action_text, "disable") == 0) {
        if (!caregiver_session_valid(request)) {
            result = ESP_ERR_INVALID_STATE;
        } else {
            result = caregiver_auth_disable();
            s_caregiver_session[0] = '\0';
        }
    } else if (strcmp(action_text, "logout") == 0) {
        s_caregiver_session[0] = '\0';
    } else {
        result = ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    if (result != ESP_OK) {
        return httpd_resp_send_err(
            request,
            result == ESP_ERR_INVALID_STATE ? HTTPD_401_UNAUTHORIZED
                                            : HTTPD_400_BAD_REQUEST,
            result == ESP_ERR_INVALID_STATE ? "incorrect PIN or locked"
                                            : "PIN must be 4 to 8 digits");
    }
    if (s_caregiver_session[0] != '\0') {
        char cookie[96];
        snprintf(cookie, sizeof(cookie),
                 "caregiver_session=%s; Path=/; HttpOnly; SameSite=Strict",
                 s_caregiver_session);
        httpd_resp_set_hdr(request, "Set-Cookie", cookie);
    } else {
        httpd_resp_set_hdr(
            request, "Set-Cookie",
            "caregiver_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"ok\":true}");
}

static bool active_schedule_contains(const char *id)
{
    size_t id_length = strlen(id);
    const char *cursor = s_active_schedule_ids;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == id_length && strncmp(cursor, id, length) == 0) return true;
        if (end == NULL) break;
        cursor = end + 1;
    }
    return false;
}

static esp_err_t remove_active_schedule(const char *id)
{
    if (id == NULL || id[0] == '\0' || !active_schedule_contains(id)) {
        return ESP_OK;
    }
    char updated[sizeof(s_active_schedule_ids)] = {0};
    const char *cursor = s_active_schedule_ids;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (!(length == strlen(id) && strncmp(cursor, id, length) == 0)) {
            if (updated[0] != '\0') strlcat(updated, ",", sizeof(updated));
            size_t used = strlen(updated);
            size_t available = sizeof(updated) - used - 1;
            if (length > available) return ESP_ERR_INVALID_SIZE;
            memcpy(updated + used, cursor, length);
            updated[used + length] = '\0';
        }
        if (end == NULL) break;
        cursor = end + 1;
    }
    esp_err_t result = updated[0] == '\0'
                           ? nvs_erase_key(s_reminder_nvs, "active_ids")
                           : nvs_set_str(s_reminder_nvs, "active_ids", updated);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(s_reminder_nvs);
    if (result == ESP_OK) {
        strlcpy(s_active_schedule_ids, updated,
                sizeof(s_active_schedule_ids));
    }
    return result;
}

static uint32_t s_audio_nonce;
static pill_ai_saved_config_t s_ai_config;
static bool s_sta_connected;
static bool s_sta_connecting;

static void restart_after_clock_sync(void *unused)
{
    (void)unused;
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
}

static esp_err_t setup_ap_password(httpd_req_t *request)
{
    if (request->method == HTTP_GET) {
        char json[96];
        snprintf(json, sizeof(json), "{\"ssid\":\"%s\"}", s_setup_ap_ssid);
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, json);
    }
    if (!require_caregiver(request)) return ESP_OK;
    enum { BODY_BYTES = 128 };
    if (request->content_len <= 0 || request->content_len >= BODY_BYTES) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid hotspot password body");
    }
    char body[BODY_BYTES] = {0};
    int received = httpd_req_recv(request, body,
                                  (size_t)request->content_len);
    if (received != request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "incomplete hotspot password body");
    }
    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    cJSON *password =
        root ? cJSON_GetObjectItemCaseSensitive(root, "password") : NULL;
    size_t length = cJSON_IsString(password)
                        ? strlen(password->valuestring)
                        : 0;
    bool valid = length >= 8 && length <= 63;
    esp_err_t saved = valid
                          ? nvs_set_str(s_ui_nvs, "ap_password",
                                        password->valuestring)
                          : ESP_ERR_INVALID_ARG;
    if (saved == ESP_OK) saved = nvs_commit(s_ui_nvs);
    cJSON_Delete(root);
    if (!valid) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "password must be 8 to 63 characters");
    }
    if (saved != ESP_OK) {
        return httpd_resp_send_err(request,
                                   HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "hotspot password save failed");
    }
    if (xTaskCreate(restart_after_clock_sync, "ap_restart", 2048, NULL, 3,
                    NULL) != pdPASS) {
        return httpd_resp_send_err(request,
                                   HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "password saved but restart failed");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"saved\":true,\"restarting\":true}");
}

static esp_err_t download_dose_history(httpd_req_t *request)
{
    FILE *file = fopen("/sdcard/smartpill/dose-history.csv", "rb");
    if (file == NULL) {
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND,
                                   "no dose history yet");
    }
    httpd_resp_set_type(request, "text/csv; charset=utf-8");
    httpd_resp_set_hdr(
        request, "Content-Disposition",
        "attachment; filename=\"smartpill-dose-history.csv\"");
    char chunk[1024];
    esp_err_t result = ESP_OK;
    size_t bytes;
    while ((bytes = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (httpd_resp_send_chunk(request, chunk, bytes) != ESP_OK) {
            result = ESP_FAIL;
            break;
        }
    }
    fclose(file);
    if (result == ESP_OK) result = httpd_resp_send_chunk(request, NULL, 0);
    return result;
}

static esp_err_t sync_clock(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    enum { BODY_BYTES = 192 };
    if (request->content_len <= 0 || request->content_len >= BODY_BYTES) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid clock body");
    }
    char body[BODY_BYTES] = {0};
    int received = httpd_req_recv(request, body, (size_t)request->content_len);
    if (received != request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "incomplete clock body");
    }
    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    const char *fields[] = {"year", "month", "day", "hour", "minute", "second", "weekday"};
    int values[7] = {0};
    bool valid = root != NULL;
    for (size_t i = 0; i < 7 && valid; ++i) {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(root, fields[i]);
        valid = cJSON_IsNumber(item);
        if (valid) values[i] = item->valueint;
    }
    cJSON_Delete(root);
    struct tm local_time = {
        .tm_year = values[0] - 1900, .tm_mon = values[1] - 1,
        .tm_mday = values[2], .tm_hour = values[3], .tm_min = values[4],
        .tm_sec = values[5], .tm_wday = values[6], .tm_isdst = -1,
    };
    if (!valid || board_rtc_write(&local_time) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid time or RTC write failed");
    }
    if (xTaskCreate(restart_after_clock_sync, "clock_restart", 2048, NULL, 3, NULL) != pdPASS) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "time saved but restart failed");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"synchronized\":true}");
}

static void wifi_event_handler(void *argument, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_data;
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START &&
        s_ai_config.wifi_configured) {
        s_sta_connecting = true;
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        if (s_ai_config.wifi_configured) {
            s_sta_connecting = true;
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_sta_connected = true;
        s_sta_connecting = false;
    }
}

static const char *audio_state_name(void)
{
    switch (pill_audio_state()) {
        case PILL_AUDIO_PLAYING: return "playing";
        case PILL_AUDIO_STOPPING: return "stopping";
        case PILL_AUDIO_FAULT: return "fault";
        default: return "idle";
    }
}

static esp_err_t send_audio_status(httpd_req_t *request)
{
    s_audio_nonce = esp_random();
    board_audio_diagnostics_t d = {0};
    bool diagnostic_ok = board_audio_get_diagnostics(&d) == ESP_OK;
    char json[512];
    int length = snprintf(json, sizeof(json),
        "{\"state\":\"%s\",\"nonce\":\"%08lx\",\"diagnostic_ok\":%s,"
        "\"writes\":%lu,\"frames\":%lu,\"write_result\":%d,"
        "\"registers\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]}",
        audio_state_name(), (unsigned long)s_audio_nonce,
        diagnostic_ok ? "true" : "false", (unsigned long)d.write_calls,
        (unsigned long)d.frames_submitted, d.last_write_result,
        d.reg_chip_power, d.reg_dac_power, d.reg_dac_format, d.reg_dac_rate,
        d.reg_left_volume, d.reg_right_volume, d.reg_mute, d.reg_left_mixer,
        d.reg_right_mixer, d.reg_speaker_left, d.reg_speaker_right);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t start_audio_test(httpd_req_t *request)
{
    char supplied[9] = {0};
    char expected[9];
    snprintf(expected, sizeof(expected), "%08lx", (unsigned long)s_audio_nonce);
    s_audio_nonce = esp_random();
    if (httpd_req_get_hdr_value_str(request, "X-Audio-Nonce", supplied,
                                    sizeof(supplied)) != ESP_OK ||
        strcmp(supplied, expected) != 0) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN,
                                   "fresh confirmation required");
    }
    if (!pill_audio_test_tone()) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "audio unavailable or busy");
    }
    return httpd_resp_sendstr(request, "{\"state\":\"playing\"}");
}

static esp_err_t stop_audio_test(httpd_req_t *request)
{
    pill_audio_stop();
    return httpd_resp_sendstr(request, "{\"state\":\"stopping\"}");
}

static esp_err_t microphone_level_test(httpd_req_t *request)
{
    char supplied[9] = {0};
    char expected[9];
    snprintf(expected, sizeof(expected), "%08lx", (unsigned long)s_audio_nonce);
    s_audio_nonce = esp_random();
    if (httpd_req_get_hdr_value_str(request, "X-Audio-Nonce", supplied,
                                    sizeof(supplied)) != ESP_OK ||
        strcmp(supplied, expected) != 0) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN,
                                   "fresh confirmation required");
    }
    if (pill_audio_state() != PILL_AUDIO_IDLE || !board_audio_ready()) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "audio unavailable or busy");
    }
    uint32_t peak_left = 0;
    uint32_t peak_right = 0;
    int16_t *channels = malloc(PILL_AI_PCM_FRAME_BYTES * 2);
    if (channels == NULL) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "microphone buffer unavailable");
    }
    int16_t *left = channels;
    int16_t *right = channels + PILL_AI_PCM_FRAME_SAMPLES;
    for (unsigned index = 0; index < 150; ++index) {
        size_t frames = 0;
        if (board_audio_read_stereo(left, right, PILL_AI_PCM_FRAME_SAMPLES,
                                    &frames) != ESP_OK ||
            frames != PILL_AI_PCM_FRAME_SAMPLES) {
            free(channels);
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "microphone capture failed");
        }
        uint32_t left_peak = pill_ai_pcm_peak(left, frames);
        uint32_t right_peak = pill_ai_pcm_peak(right, frames);
        if (left_peak > peak_left) peak_left = left_peak;
        if (right_peak > peak_right) peak_right = right_peak;
    }
    free(channels);
    char json[96];
    int length = snprintf(json, sizeof(json),
                          "{\"captured_ms\":3000,\"peak_left\":%lu,"
                          "\"peak_right\":%lu,\"stored\":false}",
                          (unsigned long)peak_left, (unsigned long)peak_right);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static bool ensure_audio_directories(void)
{
    return (mkdir("/sdcard/smartpill", 0775) == 0 || errno == EEXIST) &&
           (mkdir("/sdcard/smartpill/audio", 0775) == 0 || errno == EEXIST);
}

static bool request_targets_spoken(httpd_req_t *request)
{
    char query[48] = {0};
    char target[16] = {0};
    return httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK &&
           httpd_query_key_value(query, "target", target, sizeof(target)) == ESP_OK &&
           strcmp(target, "spoken") == 0;
}

static esp_err_t upload_preview(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    enum {
        MAX_SPOKEN_WAV_BYTES = 960044,
        MAX_ALARM_WAV_BYTES = 19200044,
    };
    bool spoken = request_targets_spoken(request);
    int max_wav_bytes = spoken ? MAX_SPOKEN_WAV_BYTES : MAX_ALARM_WAV_BYTES;
    if (pill_audio_state() != PILL_AUDIO_IDLE) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "stop playback before uploading");
    }
    if (!board_health()->tf_card || request->content_len < 44 ||
        request->content_len > max_wav_bytes || !ensure_audio_directories()) {
        httpd_resp_set_status(request, "413 Payload Too Large");
        return httpd_resp_sendstr(request, "invalid size or TF unavailable");
    }
    const char *temporary = spoken ? "/sdcard/smartpill/audio/spoktmp.wav"
                                   : "/sdcard/smartpill/audio/remtmp.wav";
    const char *active = spoken ? "/sdcard/smartpill/audio/spoken.wav"
                                : "/sdcard/smartpill/audio/reminder.wav";
    const char *backup = spoken ? "/sdcard/smartpill/audio/spokbak.wav"
                                : "/sdcard/smartpill/audio/rembak.wav";
    FILE *file = fopen(temporary, "wb");
    if (file == NULL)
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "cannot create staging file");
    uint8_t buffer[1024];
    int remaining = request->content_len;
    bool ok = true;
    unsigned timeouts = 0;
    while (remaining > 0 && ok) {
        int requested = remaining < (int)sizeof(buffer) ? remaining : (int)sizeof(buffer);
        int received = httpd_req_recv(request, (char *)buffer, requested);
        if (received == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 5) continue;
        ok = received > 0 && fwrite(buffer, 1, (size_t)received, file) == (size_t)received;
        if (received > 0) remaining -= received;
    }
    ok = fclose(file) == 0 && ok;
    file = ok ? fopen(temporary, "rb") : NULL;
    uint8_t *header = file ? malloc(4096) : NULL;
    pill_wav_info_t info;
    size_t header_bytes = header ? fread(header, 1, 4096, file) : 0;
    if (file) fclose(file);
    ok = ok && header != NULL && pill_wav_parse(header, header_bytes, &info) &&
         info.data_offset + info.data_bytes == (uint32_t)request->content_len;
    free(header);
    if (!ok) {
        remove(temporary);
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "WAV must be 16kHz mono 16-bit PCM, max 50 seconds");
    }
    remove(backup);
    bool had_active = rename(active, backup) == 0;
    if (rename(temporary, active) != 0) {
        if (had_active) rename(backup, active);
        remove(temporary);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "activation failed; previous preview kept");
    }
    remove(backup);
    return httpd_resp_sendstr(request, "{\"stored\":true}");
}

static esp_err_t play_preview(httpd_req_t *request)
{
    const char *path = request_targets_spoken(request)
                           ? "/sdcard/smartpill/audio/spoken.wav"
                           : "/sdcard/smartpill/audio/reminder.wav";
    if (!pill_audio_play_file_at_volume(
            path, pill_ai_volume())) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "preview unavailable or busy");
    }
    return httpd_resp_sendstr(request, "{\"state\":\"playing\"}");
}

static esp_err_t reminder_sound(httpd_req_t *request)
{
    if (request->method == HTTP_POST && !require_caregiver(request)) {
        return ESP_OK;
    }
    struct stat file_info = {0};
    bool alarm_available =
        stat("/sdcard/smartpill/audio/reminder.wav", &file_info) == 0 &&
        file_info.st_size >= 44;
    bool spoken_available =
        stat("/sdcard/smartpill/audio/spoken.wav", &file_info) == 0 &&
        file_info.st_size >= 44;
    if (request->method == HTTP_POST) {
        char body[80] = {0};
        if (request->content_len <= 0 ||
            request->content_len >= (int)sizeof(body) ||
            httpd_req_recv(request, body, (size_t)request->content_len) !=
                request->content_len) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "invalid reminder sound");
        }
        cJSON *root = cJSON_Parse(body);
        cJSON *mode =
            root ? cJSON_GetObjectItemCaseSensitive(root, "mode") : NULL;
        cJSON *target =
            root ? cJSON_GetObjectItemCaseSensitive(root, "target") : NULL;
        bool use_custom = cJSON_IsString(mode) &&
                          strcmp(mode->valuestring, "custom") == 0;
        bool use_default = cJSON_IsString(mode) &&
                           strcmp(mode->valuestring, "default") == 0;
        bool spoken_target = cJSON_IsString(target) &&
                             strcmp(target->valuestring, "spoken") == 0;
        bool available = spoken_target ? spoken_available : alarm_available;
        const char *key = spoken_target ? "custom_spoken" : "custom_sound";
        cJSON_Delete(root);
        if ((!use_custom && !use_default) ||
            (use_custom && !available) ||
            nvs_set_u8(s_reminder_nvs, key,
                       use_custom ? 1 : 0) != ESP_OK ||
            nvs_commit(s_reminder_nvs) != ESP_OK) {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(
                request, "upload a valid custom WAV before selecting it");
        }
    }
    uint8_t custom_alarm = 0;
    uint8_t custom_spoken = 0;
    (void)nvs_get_u8(s_reminder_nvs, "custom_sound", &custom_alarm);
    (void)nvs_get_u8(s_reminder_nvs, "custom_spoken", &custom_spoken);
    char json[192];
    int length = snprintf(
        json, sizeof(json),
        "{\"spoken_mode\":\"%s\",\"spoken_available\":%s,"
        "\"alarm_mode\":\"%s\",\"alarm_available\":%s}",
        custom_spoken != 0 && spoken_available ? "custom" : "default",
        spoken_available ? "true" : "false",
        custom_alarm != 0 && alarm_available ? "custom" : "default",
        alarm_available ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t styles_css_start[] asm("_binary_styles_css_start");
extern const uint8_t styles_css_end[] asm("_binary_styles_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

typedef struct {
    const char *content_type;
    const uint8_t *start;
    const uint8_t *end;
} asset_t;

static esp_err_t send_asset(httpd_req_t *request)
{
    const asset_t *asset = (const asset_t *)request->user_ctx;
    httpd_resp_set_type(request, asset->content_type);
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, (const char *)asset->start,
                           asset->end - asset->start);
}

static bool automatic_dispensing_enabled(void)
{
    struct tm rtc_time = {0};
    bool rtc_valid = false;
    return s_active_schedule_ids[0] != '\0' &&
           board_health()->servo_ready &&
           board_rtc_read(&rtc_time, &rtc_valid) == ESP_OK &&
           rtc_valid;
}

static esp_err_t send_health(httpd_req_t *request)
{
    board_tf_card_retry();
    const board_health_t *health = board_health();
    uint16_t mt_raw = 0;
    float mt_degrees = 0.0f;
    int mt_sda = -1;
    int mt_scl = -1;
    int mt_address = -1;
    board_mt6701_diagnose(&mt_sda, &mt_scl, &mt_address);
    bool mt_read_ok = board_mt6701_read(&mt_raw, &mt_degrees) == ESP_OK;
    float motion_start = 0.0f, motion_target = 0.0f, motion_final = 0.0f,
          motion_error = 0.0f;
    const char *motion_reason = "unknown";
    board_servo_last_motion(&motion_start, &motion_target, &motion_final,
                            &motion_error, &motion_reason);
    char json[560];
    int length = snprintf(json, sizeof(json),
                          "{\"i2c\":%s,\"io_expander\":%s,\"audio\":%s,"
                          "\"rtc\":%s,\"display\":%s,\"tf_card\":%s,"
                          "\"servo\":%s,\"mt6701\":%s,\"mt6701_raw\":%u,"
                          "\"mt6701_degrees\":%.2f,\"mt6701_sda\":%d,"
                          "\"mt6701_scl\":%d,\"mt6701_address\":%d,"
                          "\"motion_start\":%.2f,\"motion_target\":%.2f,"
                          "\"motion_final\":%.2f,\"motion_error\":%.2f,"
                          "\"motion_reason\":\"%s\","
                          "\"automatic_dispensing\":%s}",
                          health->i2c ? "true" : "false",
                          health->io_expander ? "true" : "false",
                          health->audio_codec ? "true" : "false",
                          health->rtc ? "true" : "false",
                          health->display ? "true" : "false",
                          health->tf_card ? "true" : "false",
                          health->servo_ready ? "true" : "false",
                          mt_read_ok ? "true" : "false", mt_raw, mt_degrees,
                          mt_sda, mt_scl, mt_address,
                          motion_start, motion_target, motion_final,
                          motion_error, motion_reason,
                          automatic_dispensing_enabled() ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t send_schedule_list(httpd_req_t *request)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "response allocation failed");
    }
    cJSON *drafts = cJSON_AddArrayToObject(root, "drafts");
    if (drafts == NULL) {
        cJSON_Delete(root);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "response allocation failed");
    }
    cJSON_AddStringToObject(root, "storage", s_schedule_writable ? "ready" : "fault");
    cJSON_AddBoolToObject(root, "automatic_dispensing",
                          automatic_dispensing_enabled());
    for (size_t index = 0; index < s_schedule_store.count; ++index) {
        const schedule_draft_t *draft = &s_schedule_store.drafts[index];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", draft->schedule.id);
        cJSON_AddStringToObject(item, "medication", draft->schedule.medication);
        cJSON_AddStringToObject(item, "dose_instruction", draft->dose_instruction);
        cJSON *times = cJSON_AddArrayToObject(item, "times");
        for (uint8_t time = 0; time < draft->schedule.time_count; ++time) {
            cJSON_AddItemToArray(times, cJSON_CreateNumber(draft->schedule.times[time]));
        }
        bool active = active_schedule_contains(draft->schedule.id);
        cJSON_AddBoolToObject(item, "active", active);
        cJSON_AddStringToObject(item, "status", active ? "reminder_active"
                                                       : "saved_draft");
        cJSON_AddItemToArray(drafts, item);
    }
    char *json = malloc(4096);
    if (json == NULL) {
        cJSON_Delete(root);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "response allocation failed");
    }
    bool printed = cJSON_PrintPreallocated(root, json, 4096, false);
    cJSON_Delete(root);
    if (!printed) {
        free(json);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "response too large");
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    esp_err_t result = httpd_resp_sendstr(request, json);
    free(json);
    return result;
}

static bool receive_schedule(httpd_req_t *request, schedule_draft_input_t *input,
                             schedule_json_request_t *parsed)
{
    if (request->content_len <= 0 || request->content_len > SCHEDULE_JSON_BODY_MAX) return false;
    char body[SCHEDULE_JSON_BODY_MAX];
    size_t received = 0;
    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(request, body + received,
                                    (size_t)request->content_len - received);
        if (result <= 0) return false;
        received += (size_t)result;
    }
    if (!schedule_json_parse(body, received, parsed)) return false;
    *input = (schedule_draft_input_t){
        .medication = parsed->medication,
        .dose_instruction = parsed->dose_instruction,
        .time_count = parsed->time_count,
        .reminder_minutes = 5,
        .snooze_minutes = 10,
    };
    memcpy(input->times, parsed->times, sizeof(input->times));
    return true;
}

static esp_err_t send_store_result(httpd_req_t *request, schedule_store_result_t result)
{
    if (result == SCHEDULE_STORE_OK) return send_schedule_list(request);
    if (result == SCHEDULE_STORE_NOT_FOUND)
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "draft not found");
    if (result == SCHEDULE_STORE_FULL) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "draft limit reached");
    }
    if (result == SCHEDULE_STORE_INVALID)
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid draft");
    return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "storage failed");
}

static esp_err_t mutate_schedule(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    schedule_store_t *candidate = malloc(sizeof(*candidate));
    if (candidate == NULL)
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "request allocation failed");
    *candidate = s_schedule_store;
    schedule_store_result_t result;
    const char *prefix = "/api/schedules/";
    const char *id = strncmp(request->uri, prefix, strlen(prefix)) == 0
                         ? request->uri + strlen(prefix) : NULL;
    if (request->method == HTTP_DELETE) {
        result = id != NULL && id[0] != '\0' ? schedule_store_delete(candidate, id)
                                               : SCHEDULE_STORE_INVALID;
    } else {
        schedule_json_request_t parsed;
        schedule_draft_input_t input;
        if (!receive_schedule(request, &input, &parsed))
        {
            free(candidate);
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid JSON");
        }
        if (request->method == HTTP_POST) {
            schedule_draft_t created;
            result = schedule_store_create(candidate, &input, &created);
        } else {
            result = id != NULL && id[0] != '\0'
                         ? schedule_store_update(candidate, id, &input, NULL)
                         : SCHEDULE_STORE_INVALID;
        }
    }
    if (result == SCHEDULE_STORE_OK) result = schedule_store_commit(candidate, &s_schedule_backend);
    if (result == SCHEDULE_STORE_OK) {
        s_schedule_store = *candidate;
        if (id != NULL && id[0] != '\0' &&
            remove_active_schedule(id) != ESP_OK) {
            result = SCHEDULE_STORE_IO_ERROR;
        }
    }
    free(candidate);
    return send_store_result(request, result);
}

static void initialize_schedule_storage(void)
{
    s_schedule_writable = false;
    schedule_store_init_empty(&s_schedule_store);
    if (schedule_nvs_open(&s_schedule_nvs) != ESP_OK) return;
    if (nvs_open("reminder_active", NVS_READWRITE, &s_reminder_nvs) == ESP_OK) {
        size_t length = sizeof(s_active_schedule_ids);
        if (nvs_get_str(s_reminder_nvs, "active_ids", s_active_schedule_ids,
                        &length) != ESP_OK) {
            char legacy[PILL_SCHEDULE_ID_MAX] = {0};
            length = sizeof(legacy);
            if (nvs_get_str(s_reminder_nvs, "schedule_id", legacy, &length) ==
                ESP_OK) {
                strlcpy(s_active_schedule_ids, legacy,
                        sizeof(s_active_schedule_ids));
                nvs_set_str(s_reminder_nvs, "active_ids",
                            s_active_schedule_ids);
                nvs_erase_key(s_reminder_nvs, "schedule_id");
                nvs_commit(s_reminder_nvs);
            }
        }
    }
    s_schedule_backend = schedule_nvs_backend(&s_schedule_nvs);
    schedule_store_result_t result = schedule_store_load(&s_schedule_store, &s_schedule_backend);
    if (result == SCHEDULE_STORE_OK) {
        s_schedule_writable = true;
    } else if (!schedule_nvs_has_data(&s_schedule_nvs)) {
        schedule_store_init_empty(&s_schedule_store);
        s_schedule_writable = schedule_store_commit(&s_schedule_store, &s_schedule_backend) ==
                              SCHEDULE_STORE_OK;
    }
}

static esp_err_t set_active_reminder(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    if (request->method == HTTP_DELETE) {
        if (nvs_erase_key(s_reminder_nvs, "active_ids") != ESP_OK ||
            nvs_commit(s_reminder_nvs) != ESP_OK) {
            return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "deactivation failed");
        }
        s_active_schedule_ids[0] = '\0';
        return send_schedule_list(request);
    }

    char body[128] = {0};
    if (request->content_len <= 0 ||
        request->content_len >= (int)sizeof(body) ||
        httpd_req_recv(request, body, (size_t)request->content_len) !=
            request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid activation");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *id = root ? cJSON_GetObjectItemCaseSensitive(root, "id") : NULL;
    cJSON *confirmed =
        root ? cJSON_GetObjectItemCaseSensitive(root, "confirmed") : NULL;
    cJSON *enabled =
        root ? cJSON_GetObjectItemCaseSensitive(root, "enabled") : NULL;
    bool enable = !cJSON_IsBool(enabled) || cJSON_IsTrue(enabled);
    const schedule_draft_t *selected = NULL;
    if (cJSON_IsString(id) && (!enable || cJSON_IsTrue(confirmed))) {
        for (size_t index = 0; index < s_schedule_store.count; ++index) {
            if (strcmp(s_schedule_store.drafts[index].schedule.id,
                       id->valuestring) == 0) {
                selected = &s_schedule_store.drafts[index];
                break;
            }
        }
    }
    struct tm rtc_time = {0};
    bool rtc_valid = false;
    bool valid = selected != NULL &&
                 (!enable ||
                  (board_rtc_read(&rtc_time, &rtc_valid) == ESP_OK &&
                   rtc_valid && board_health()->servo_ready));
    if (!valid) {
        cJSON_Delete(root);
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request,
                                  "valid RTC and schedule confirmation required");
    }
    char updated[sizeof(s_active_schedule_ids)] = {0};
    const char *cursor = s_active_schedule_ids;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, ',');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (!(length == strlen(selected->schedule.id) &&
              strncmp(cursor, selected->schedule.id, length) == 0)) {
            if (updated[0] != '\0') strlcat(updated, ",", sizeof(updated));
            strncat(updated, cursor,
                    length < sizeof(updated) - strlen(updated) - 1
                        ? length
                        : sizeof(updated) - strlen(updated) - 1);
        }
        if (end == NULL) break;
        cursor = end + 1;
    }
    if (enable && !active_schedule_contains(selected->schedule.id)) {
        if (updated[0] != '\0') strlcat(updated, ",", sizeof(updated));
        strlcat(updated, selected->schedule.id, sizeof(updated));
    }
    esp_err_t result = nvs_set_str(s_reminder_nvs, "active_ids", updated);
    if (result == ESP_OK) result = nvs_commit(s_reminder_nvs);
    if (result == ESP_OK) {
        strlcpy(s_active_schedule_ids, updated,
                sizeof(s_active_schedule_ids));
    }
    cJSON_Delete(root);
    if (result != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "activation failed");
    }
    return send_schedule_list(request);
}

static esp_err_t ui_language(httpd_req_t *request)
{
    if (request->method == HTTP_POST) {
        char body[40] = {0};
        if (request->content_len <= 0 ||
            request->content_len >= (int)sizeof(body) ||
            httpd_req_recv(request, body, (size_t)request->content_len) !=
                request->content_len) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "invalid language");
        }
        cJSON *root = cJSON_Parse(body);
        cJSON *language =
            root ? cJSON_GetObjectItemCaseSensitive(root, "language") : NULL;
        bool valid = cJSON_IsString(language) &&
                     (strcmp(language->valuestring, "zh") == 0 ||
                      strcmp(language->valuestring, "en") == 0);
        if (valid) {
            s_ui_chinese = strcmp(language->valuestring, "zh") == 0;
        }
        cJSON_Delete(root);
        if (!valid || nvs_set_u8(s_ui_nvs, "chinese", s_ui_chinese ? 1 : 0) !=
                          ESP_OK ||
            nvs_commit(s_ui_nvs) != ESP_OK) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "language save failed");
        }
    }
    char json[24];
    int length = snprintf(json, sizeof(json), "{\"language\":\"%s\"}",
                          s_ui_chinese ? "zh" : "en");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t display_rotation(httpd_req_t *request)
{
    if (request->method == HTTP_POST) {
        if (!require_caregiver(request)) return ESP_OK;
        char body[48] = {0};
        if (request->content_len <= 0 ||
            request->content_len >= (int)sizeof(body) ||
            httpd_req_recv(request, body, (size_t)request->content_len) !=
                request->content_len) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "invalid rotation");
        }
        cJSON *root = cJSON_Parse(body);
        cJSON *item = root ? cJSON_GetObjectItemCaseSensitive(root, "rotate_180") : NULL;
        bool valid = cJSON_IsBool(item);
        bool rotate_180 = cJSON_IsTrue(item);
        cJSON_Delete(root);
        if (!valid || board_display_set_rotation(rotate_180) != ESP_OK ||
            nvs_set_u8(s_ui_nvs, "rotate180", rotate_180 ? 1 : 0) != ESP_OK ||
            nvs_commit(s_ui_nvs) != ESP_OK) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "rotation save failed");
        }
    }
    char json[32];
    int length = snprintf(json, sizeof(json), "{\"rotate_180\":%s}",
                          board_display_rotation() ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t reminder_runtime(httpd_req_t *request)
{
    if (request->method == HTTP_POST) {
        char body[48] = {0};
        if (request->content_len <= 0 ||
            request->content_len >= (int)sizeof(body) ||
            httpd_req_recv(request, body, (size_t)request->content_len) !=
                request->content_len) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "invalid reminder action");
        }
        cJSON *root = cJSON_Parse(body);
        cJSON *action =
            root ? cJSON_GetObjectItemCaseSensitive(root, "action") : NULL;
        uint8_t due = 0;
        (void)nvs_get_u8(s_reminder_nvs, "due_active", &due);
        uint8_t value = cJSON_IsString(action) &&
                                strcmp(action->valuestring, "taken") == 0
                            ? 1
                            : cJSON_IsString(action) &&
                                      strcmp(action->valuestring, "snooze") == 0
                                  ? 2
                                  : 0;
        cJSON_Delete(root);
        if (!due || value == 0 ||
            nvs_set_u8(s_reminder_nvs, "action", value) != ESP_OK ||
            nvs_commit(s_reminder_nvs) != ESP_OK) {
            httpd_resp_set_status(request, "409 Conflict");
            return httpd_resp_sendstr(request, "no active reminder");
        }
        pill_audio_stop();
        /* A taken reminder must also be clearable if its old test schedule was
         * deleted. Otherwise no schedule owns it and the wake word stays locked. */
        if (value == 1) {
            nvs_set_u8(s_reminder_nvs, "due_active", 0);
            nvs_set_u8(s_reminder_nvs, "action", 0);
            nvs_erase_key(s_reminder_nvs, "repeat_at");
            nvs_erase_key(s_reminder_nvs, "snooze_at");
            nvs_set_str(s_reminder_nvs, "last_event", "taken");
            if (nvs_commit(s_reminder_nvs) != ESP_OK) {
                return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                           "reminder clear failed");
            }
        }
    }

    uint8_t due = 0;
    char medicine[PILL_MEDICATION_NAME_MAX] = {0};
    char last_event[16] = {0};
    char last_time[24] = {0};
    size_t medicine_size = sizeof(medicine);
    size_t event_size = sizeof(last_event);
    size_t time_size = sizeof(last_time);
    (void)nvs_get_u8(s_reminder_nvs, "due_active", &due);
    (void)nvs_get_str(s_reminder_nvs, "due_med", medicine, &medicine_size);
    (void)nvs_get_str(s_reminder_nvs, "last_event", last_event, &event_size);
    (void)nvs_get_str(s_reminder_nvs, "last_time", last_time, &time_size);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "due", due != 0);
    cJSON_AddStringToObject(root, "medicine", medicine);
    cJSON_AddStringToObject(root, "last_event", last_event);
    cJSON_AddStringToObject(root, "last_time", last_time);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "status allocation failed");
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    esp_err_t result = httpd_resp_sendstr(request, json);
    free(json);
    return result;
}

static esp_err_t test_servo_once(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    char body[40] = {0};
    if (request->content_len <= 0 ||
        request->content_len >= (int)sizeof(body) ||
        httpd_req_recv(request, body, (size_t)request->content_len) !=
            request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "confirmation required");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *confirmed =
        root ? cJSON_GetObjectItemCaseSensitive(root, "confirmed") : NULL;
    bool allowed = cJSON_IsTrue(confirmed) && board_health()->servo_ready &&
                   !s_servo_test_running;
    cJSON_Delete(root);
    if (!allowed) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "servo unavailable or busy");
    }
    s_servo_test_running = true;
    bool success = board_servo_cycle_channel(0);
    s_servo_test_running = false;
    if (!success) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "servo test failed");
    }
    char response[56];
    int length = snprintf(response, sizeof(response),
                          "{\"cycled\":true,\"automatic_dispensing\":%s}",
                          automatic_dispensing_enabled() ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response, length);
}

static esp_err_t set_tray_reference(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    char body[40] = {0};
    if (request->content_len <= 0 || request->content_len >= (int)sizeof(body) ||
        httpd_req_recv(request, body, (size_t)request->content_len) !=
            request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "confirmation required");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *confirmed =
        root ? cJSON_GetObjectItemCaseSensitive(root, "confirmed") : NULL;
    bool allowed = cJSON_IsTrue(confirmed) && board_health()->mt6701 &&
                   board_health()->servo_ready && !s_servo_test_running;
    cJSON_Delete(root);
    if (!allowed) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "sensor unavailable or mechanism busy");
    }
    float degrees = 0.0f;
    if (board_servo_set_current_zero(&degrees) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "reference was not saved");
    }
    char response[48];
    int length = snprintf(response, sizeof(response),
                          "{\"saved\":true,\"angle\":%.2f}", degrees);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response, length);
}

static esp_err_t patient_button_status(httpd_req_t *request)
{
    char json[80];
    int length = snprintf(json, sizeof(json),
                          "{\"pressed\":%s,\"press_count\":%lu}",
                          board_patient_button_pressed() ? "true" : "false",
                          (unsigned long)board_patient_button_press_count());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t set_sta_configuration(void)
{
    if (!s_ai_config.wifi_configured) {
        return ESP_OK;
    }
    wifi_config_t station = {0};
    strlcpy((char *)station.sta.ssid, s_ai_config.wifi.ssid,
            sizeof(station.sta.ssid));
    strlcpy((char *)station.sta.password, s_ai_config.wifi.password,
            sizeof(station.sta.password));
    station.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    station.sta.pmf_cfg.capable = true;
    station.sta.pmf_cfg.required = false;
    return esp_wifi_set_config(WIFI_IF_STA, &station);
}

static esp_err_t apply_sta_configuration(void)
{
    ESP_RETURN_ON_ERROR(set_sta_configuration(), TAG, "STA config");
    if (!s_ai_config.wifi_configured) return ESP_OK;
    s_sta_connected = false;
    s_sta_connecting = true;
    return esp_wifi_connect();
}

static esp_err_t send_ai_config_status(httpd_req_t *request)
{
    char json[160];
    int length = snprintf(
        json, sizeof(json),
        "{\"wifi_configured\":%s,\"remote_configured\":%s,"
        "\"sta_connected\":%s,\"sta_connecting\":%s}",
        s_ai_config.wifi_configured ? "true" : "false",
        s_ai_config.remote_configured ? "true" : "false",
        s_sta_connected ? "true" : "false",
        s_sta_connecting ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t save_ai_configuration(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    enum { BODY_BYTES = 512 };
    if (request->content_len <= 0 || request->content_len >= BODY_BYTES) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid configuration body");
    }
    char body[BODY_BYTES] = {0};
    size_t received = 0;
    while (received < (size_t)request->content_len) {
        int count = httpd_req_recv(request, body + received,
                                   (size_t)request->content_len - received);
        if (count <= 0) {
            return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                       "incomplete configuration body");
        }
        received += (size_t)count;
    }
    cJSON *root = cJSON_ParseWithLength(body, received);
    cJSON *ssid = root ? cJSON_GetObjectItemCaseSensitive(root, "ssid") : NULL;
    cJSON *password = root ? cJSON_GetObjectItemCaseSensitive(root, "password") : NULL;
    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "missing configuration fields");
    }
    pill_ai_saved_config_t candidate = s_ai_config;
    strlcpy(candidate.wifi.ssid, ssid->valuestring,
            sizeof(candidate.wifi.ssid));
    strlcpy(candidate.wifi.password, password->valuestring,
            sizeof(candidate.wifi.password));
    candidate.wifi_configured = true;
    cJSON_Delete(root);
    if (!pill_ai_wifi_config_valid(&candidate.wifi)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid Wi-Fi configuration");
    }
    esp_err_t result = pill_ai_config_store(&candidate);
    if (result != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "configuration was not stored");
    }
    s_ai_config = candidate;
    esp_wifi_disconnect();
    result = apply_sta_configuration();
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Saved configuration; STA connect start failed: %s",
                 esp_err_to_name(result));
    }
    return send_ai_config_status(request);
}

static const char *ai_runtime_name(void)
{
    switch (pill_ai_runtime_state()) {
        case PILL_AI_RUNTIME_CONNECTING: return "connecting";
        case PILL_AI_RUNTIME_LISTENING: return "listening";
        case PILL_AI_RUNTIME_THINKING: return "thinking";
        case PILL_AI_RUNTIME_SPEAKING: return "speaking";
        case PILL_AI_RUNTIME_FAILED: return "failed";
        default: return "idle";
    }
}

static esp_err_t send_ai_runtime_status(httpd_req_t *request)
{
    char json[160];
    int length = snprintf(json, sizeof(json),
                          "{\"state\":\"%s\",\"last_error\":\"%s\","
                          "\"activation_code\":\"%s\"}",
                          ai_runtime_name(), pill_ai_last_error(),
                          pill_ai_activation_code());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t start_ai_conversation(httpd_req_t *request)
{
    if (!s_sta_connected) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "internet unavailable");
    }
    if (!pill_ai_start_conversation()) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request, "AI or medicine audio is busy");
    }
    return send_ai_runtime_status(request);
}

static esp_err_t send_ai_volume(httpd_req_t *request)
{
    char json[24];
    int length = snprintf(json, sizeof(json), "{\"volume\":%u}",
                          (unsigned)pill_ai_volume());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t save_ai_volume(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    char body[48];
    if (request->content_len <= 0 ||
        request->content_len >= (int)sizeof(body)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid volume");
    }
    int received = httpd_req_recv(request, body, (size_t)request->content_len);
    if (received != request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid volume");
    }
    body[received] = '\0';
    cJSON *root = cJSON_Parse(body);
    cJSON *item = root ? cJSON_GetObjectItemCaseSensitive(root, "volume") : NULL;
    int volume = cJSON_IsNumber(item) ? item->valueint : 0;
    cJSON_Delete(root);
    if (volume < 0 || volume > 100 ||
        pill_ai_set_volume((uint8_t)volume) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "volume must be 0 to 100");
    }
    return send_ai_volume(request);
}

static esp_err_t send_ai_vad(httpd_req_t *request)
{
    char json[128];
    int length = snprintf(json, sizeof(json),
                          "{\"threshold\":%u,\"recommended\":%u,"
                          "\"minimum\":%lu,\"maximum\":%lu}",
                          (unsigned)pill_ai_vad_threshold(),
                          (unsigned)PILL_AI_VAD_RECOMMENDED,
                          (unsigned long)pill_ai_vad_minimum(),
                          (unsigned long)pill_ai_vad_maximum());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, json, length);
}

static esp_err_t save_ai_vad(httpd_req_t *request)
{
    if (!require_caregiver(request)) return ESP_OK;
    char body[56];
    if (request->content_len <= 0 ||
        request->content_len >= (int)sizeof(body)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid threshold");
    }
    int received = httpd_req_recv(request, body, (size_t)request->content_len);
    if (received != request->content_len) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid threshold");
    }
    body[received] = '\0';
    cJSON *root = cJSON_Parse(body);
    cJSON *item = root ? cJSON_GetObjectItemCaseSensitive(root, "threshold") : NULL;
    int threshold = cJSON_IsNumber(item) ? item->valueint : 0;
    cJSON_Delete(root);
    if (threshold < 10 || threshold > 500 ||
        pill_ai_set_vad_threshold((uint16_t)threshold) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "threshold must be 10 to 500");
    }
    return send_ai_vad(request);
}

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
} fleet_response_t;

static esp_err_t fleet_http_event(esp_http_client_event_t *event)
{
    fleet_response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_DATA && response && response->data &&
        response->length < response->capacity - 1) {
        size_t available = response->capacity - response->length - 1;
        size_t copy = (size_t)event->data_len < available ? (size_t)event->data_len : available;
        memcpy(response->data + response->length, event->data, copy);
        response->length += copy;
        response->data[response->length] = '\0';
    }
    return ESP_OK;
}

static esp_err_t fleet_post(const char *endpoint, const char *body,
                            char *response_data, size_t response_size)
{
    if (!s_fleet.enabled || !s_sta_connected || s_fleet.url[0] == '\0' ||
        s_fleet.device_id[0] == '\0' || s_fleet.token[0] == '\0') return ESP_ERR_INVALID_STATE;
    char url[224];
    if (snprintf(url, sizeof(url), "%s%s", s_fleet.url, endpoint) >= (int)sizeof(url)) return ESP_ERR_INVALID_SIZE;
    fleet_response_t response = {.data = response_data, .capacity = response_size};
    if (response_data && response_size) response_data[0] = '\0';
    esp_http_client_config_t config = {
        .url = url, .method = HTTP_METHOD_POST, .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach, .event_handler = fleet_http_event,
        .user_data = &response,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    char authorization[112];
    snprintf(authorization, sizeof(authorization), "Bearer %s", s_fleet.token);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Device-ID", s_fleet.device_id);
    esp_http_client_set_header(client, "Authorization", authorization);
    esp_http_client_set_post_field(client, body ? body : "{}", body ? strlen(body) : 2);
    esp_err_t result = esp_http_client_perform(client);
    int status = result == ESP_OK ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_cleanup(client);
    return result == ESP_OK && status >= 200 && status < 300 ? ESP_OK : ESP_FAIL;
}

static void fleet_heartbeat(void)
{
    const board_health_t *health = board_health();
    float angle = 0.0f;
    uint16_t raw = 0;
    board_mt6701_read(&raw, &angle);
    wifi_ap_record_t access_point = {0};
    int rssi = esp_wifi_sta_get_ap_info(&access_point) == ESP_OK ? access_point.rssi : -127;
    char json[640];
    snprintf(json, sizeof(json),
        "{\"firmware\":\"rotary-2\",\"rssi\":%d,\"rtc\":%s,\"tf_card\":%s,"
        "\"display\":%s,\"audio\":%s,\"servo\":%s,\"mt6701\":%s,"
        "\"mt6701_degrees\":%.2f,\"ai_state\":\"%s\",\"last_error\":\"%s\"}",
        rssi, health->rtc ? "true" : "false", health->tf_card ? "true" : "false",
        health->display ? "true" : "false", health->audio_codec ? "true" : "false",
        health->servo_ready ? "true" : "false", health->mt6701 ? "true" : "false",
        angle, ai_runtime_name(), pill_ai_last_error());
    fleet_post("/api/device/heartbeat", json, NULL, 0);
}

static void fleet_complete(int command_id, bool ok, const char *detail)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddNumberToObject(root, "id", command_id);
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddStringToObject(root, "detail", detail ? detail : "");
    char *json = cJSON_PrintUnformatted(root);
    if (json) fleet_post("/api/device/result", json, NULL, 0);
    free(json);
    cJSON_Delete(root);
}

static bool fleet_api_allowed(const char *method, const char *path)
{
    static const char *const exact[] = {
        "GET /api/health", "GET /api/schedules", "POST /api/schedules",
        "GET /api/reminders/status", "POST /api/reminders/status",
        "POST /api/reminders/active", "DELETE /api/reminders/active",
        "GET /api/ui/language", "POST /api/ui/language", "POST /api/clock",
        "GET /api/display/rotation", "POST /api/display/rotation",
        "GET /api/ai/volume", "POST /api/ai/volume", "GET /api/ai/vad",
        "POST /api/ai/vad", "GET /api/audio/reminder-sound",
        "POST /api/audio/reminder-sound", "GET /api/ai/status",
    };
    char candidate[112];
    snprintf(candidate, sizeof(candidate), "%s %s", method, path);
    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i) {
        if (strcmp(candidate, exact[i]) == 0) return true;
    }
    return strncmp(path, "/api/schedules/", 15) == 0 &&
           (strcmp(method, "PUT") == 0 || strcmp(method, "DELETE") == 0);
}

static bool fleet_local_api(cJSON *payload, char *detail, size_t detail_size)
{
    cJSON *method_item = cJSON_GetObjectItemCaseSensitive(payload, "method");
    cJSON *path_item = cJSON_GetObjectItemCaseSensitive(payload, "path");
    cJSON *body_item = cJSON_GetObjectItemCaseSensitive(payload, "body");
    if (!cJSON_IsString(method_item) || !cJSON_IsString(path_item) ||
        !fleet_api_allowed(method_item->valuestring, path_item->valuestring)) {
        strlcpy(detail, "request_not_allowed", detail_size);
        return false;
    }
    esp_http_client_method_t method = HTTP_METHOD_GET;
    if (strcmp(method_item->valuestring, "POST") == 0) method = HTTP_METHOD_POST;
    else if (strcmp(method_item->valuestring, "PUT") == 0) method = HTTP_METHOD_PUT;
    else if (strcmp(method_item->valuestring, "DELETE") == 0) method = HTTP_METHOD_DELETE;
    char url[144];
    snprintf(url, sizeof(url), "http://127.0.0.1%s", path_item->valuestring);
    fleet_response_t response = {.data = detail, .capacity = detail_size};
    detail[0] = '\0';
    esp_http_client_config_t config = {
        .url = url, .method = method, .timeout_ms = 6000,
        .event_handler = fleet_http_event, .user_data = &response,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;
    esp_http_client_set_header(client, "X-Mendo-Fleet-Internal", s_fleet_internal_secret);
    char *body = cJSON_IsObject(body_item) ? cJSON_PrintUnformatted(body_item) : NULL;
    if (body) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));
    }
    esp_err_t result = esp_http_client_perform(client);
    int status = result == ESP_OK ? esp_http_client_get_status_code(client) : 0;
    free(body);
    esp_http_client_cleanup(client);
    if (detail[0] == '\0') snprintf(detail, detail_size, "http_%d", status);
    return result == ESP_OK && status >= 200 && status < 300;
}

static void fleet_poll(void)
{
    char response[384];
    if (fleet_post("/api/device/poll", "{}", response, sizeof(response)) != ESP_OK) return;
    cJSON *root = cJSON_Parse(response);
    cJSON *id = root ? cJSON_GetObjectItemCaseSensitive(root, "id") : NULL;
    cJSON *command = root ? cJSON_GetObjectItemCaseSensitive(root, "command") : NULL;
    cJSON *payload = root ? cJSON_GetObjectItemCaseSensitive(root, "payload") : NULL;
    if (!cJSON_IsNumber(id) || !cJSON_IsString(command)) { cJSON_Delete(root); return; }
    bool ok = false;
    const char *detail = "unsupported";
    if (strcmp(command->valuestring, "refresh_health") == 0) {
        fleet_heartbeat(); ok = true; detail = "health_sent";
    } else if (strcmp(command->valuestring, "test_quiet_tone") == 0) {
        ok = pill_audio_test_tone(); detail = ok ? "tone_started" : "audio_busy";
    } else if (strcmp(command->valuestring, "test_reminder_audio") == 0) {
        ok = pill_audio_play_file_at_volume("/sdcard/smartpill/audio/reminder.wav", pill_ai_volume());
        if (!ok) ok = pill_audio_test_tone();
        detail = ok ? "reminder_or_fallback_started" : "audio_busy";
    } else if (strcmp(command->valuestring, "api_request") == 0 && cJSON_IsObject(payload)) {
        static char api_detail[768];
        ok = fleet_local_api(payload, api_detail, sizeof(api_detail));
        detail = api_detail;
    }
    int command_id = id->valueint;
    cJSON_Delete(root);
    fleet_complete(command_id, ok, detail);
}

static void fleet_task(void *unused)
{
    (void)unused;
    unsigned ticks = 0;
    while (true) {
        if (s_fleet.enabled && s_sta_connected) {
            if (ticks % 6 == 0) fleet_heartbeat();
            fleet_poll();
        }
        ++ticks;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static esp_err_t start_access_point(void)
{
    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG, "read MAC");

    char ssid[33];
    char password[65];
    snprintf(ssid, sizeof(ssid), "SmartPill-%02X%02X", mac[4], mac[5]);
    snprintf(password, sizeof(password), "pill-%02x%02x%02x-setup", mac[3], mac[4], mac[5]);
    size_t password_size = sizeof(password);
    if (s_ui_nvs == 0 ||
        nvs_get_str(s_ui_nvs, "ap_password", password,
                    &password_size) != ESP_OK ||
        strlen(password) < 8 || strlen(password) > 63) {
        snprintf(password, sizeof(password), "pill-%02x%02x%02x-setup",
                 mac[3], mac[4], mac[5]);
    }
    strlcpy(s_setup_ap_ssid, ssid, sizeof(s_setup_ap_ssid));

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_init), TAG, "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "AP+STA mode");

    wifi_config_t config = {0};
    strlcpy((char *)config.ap.ssid, ssid, sizeof(config.ap.ssid));
    strlcpy((char *)config.ap.password, password, sizeof(config.ap.password));
    config.ap.ssid_len = strlen(ssid);
    config.ap.channel = 6;
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.pmf_cfg.required = true;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "AP config");
    ESP_RETURN_ON_ERROR(set_sta_configuration(), TAG, "STA config");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   wifi_event_handler, NULL),
                        TAG, "Wi-Fi event handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   wifi_event_handler, NULL),
                        TAG, "IP event handler");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "AP start");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG,
                        "disable Wi-Fi power saving");
    ESP_LOGW(TAG, "Caregiver setup AP: %s password: %s", ssid, password);
    ESP_LOGI(TAG, "Caregiver web UI ready; dispensing remains safety controlled");
    return ESP_OK;
}

esp_err_t pill_web_start(void)
{
    ESP_RETURN_ON_ERROR(caregiver_auth_init(), TAG, "caregiver auth");
    create_caregiver_session();
    strlcpy(s_fleet_internal_secret, s_caregiver_session,
            sizeof(s_fleet_internal_secret));
    s_caregiver_session[0] = '\0';
    fleet_load();
    initialize_schedule_storage();
    if (nvs_open("ui_prefs", NVS_READWRITE, &s_ui_nvs) == ESP_OK) {
        uint8_t chinese = 1;
        if (nvs_get_u8(s_ui_nvs, "chinese", &chinese) == ESP_OK) {
            s_ui_chinese = chinese != 0;
        } else {
            s_ui_chinese = true;
            (void)nvs_set_u8(s_ui_nvs, "chinese", 1);
            (void)nvs_commit(s_ui_nvs);
        }
    }
    esp_err_t ai_config_result = pill_ai_config_load(&s_ai_config);
    if (ai_config_result != ESP_OK) {
        memset(&s_ai_config, 0, sizeof(s_ai_config));
        ESP_LOGW(TAG, "AI configuration unavailable; setup AP recovery remains active");
    }
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_ERROR(start_access_point(), TAG, "setup AP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_init();

    const asset_t index_asset = {"text/html; charset=utf-8", index_html_start, index_html_end};
    const asset_t css_asset = {"text/css; charset=utf-8", styles_css_start, styles_css_end};
    const asset_t js_asset = {"text/javascript; charset=utf-8", app_js_start, app_js_end};
    static asset_t assets[3];
    assets[0] = index_asset;
    assets[1] = css_asset;
    assets[2] = js_asset;

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    pill_web_http_policy_t http_policy = pill_web_http_policy();
    /* Audio upload validation and FATFS calls need more headroom than the
     * ESP-IDF HTTP server default stack provides. */
    server_config.stack_size = 12288;
    server_config.max_uri_handlers = 64;
    server_config.max_open_sockets = http_policy.max_open_sockets;
    server_config.recv_wait_timeout = http_policy.receive_timeout_seconds;
    server_config.send_wait_timeout = http_policy.send_timeout_seconds;
    server_config.lru_purge_enable = http_policy.lru_purge_enable;
    server_config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &server_config), TAG, "HTTP server");

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = send_asset, .user_ctx = &assets[0]},
        {.uri = "/setup-v6", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/setup-v7", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/setup-v8", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/setup-v9", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/setup-v10", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/setup-v11", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/setup-v12", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[0]},
        {.uri = "/styles.css", .method = HTTP_GET, .handler = send_asset, .user_ctx = &assets[1]},
        {.uri = "/app.js", .method = HTTP_GET, .handler = send_asset, .user_ctx = &assets[2]},
        {.uri = "/app-v12.js", .method = HTTP_GET, .handler = send_asset,
         .user_ctx = &assets[2]},
        {.uri = "/api/health", .method = HTTP_GET, .handler = send_health},
        {.uri = "/api/clock", .method = HTTP_POST, .handler = sync_clock},
        {.uri = "/api/schedules", .method = HTTP_GET, .handler = send_schedule_list},
        {.uri = "/api/reminders/active", .method = HTTP_POST,
         .handler = set_active_reminder},
        {.uri = "/api/reminders/active", .method = HTTP_DELETE,
         .handler = set_active_reminder},
        {.uri = "/api/ui/language", .method = HTTP_GET, .handler = ui_language},
        {.uri = "/api/ui/language", .method = HTTP_POST, .handler = ui_language},
        {.uri = "/api/display/rotation", .method = HTTP_GET,
         .handler = display_rotation},
        {.uri = "/api/display/rotation", .method = HTTP_POST,
         .handler = display_rotation},
        {.uri = "/api/network/hotspot", .method = HTTP_GET,
         .handler = setup_ap_password},
        {.uri = "/api/network/hotspot", .method = HTTP_POST,
         .handler = setup_ap_password},
        {.uri = "/api/fleet/config", .method = HTTP_GET,
         .handler = fleet_configuration},
        {.uri = "/api/fleet/config", .method = HTTP_POST,
         .handler = fleet_configuration},
        {.uri = "/api/caregiver/access", .method = HTTP_GET,
         .handler = caregiver_access},
        {.uri = "/api/caregiver/access", .method = HTTP_POST,
         .handler = caregiver_access},
        {.uri = "/api/history/doses.csv", .method = HTTP_GET,
         .handler = download_dose_history},
        {.uri = "/api/reminders/status", .method = HTTP_GET,
         .handler = reminder_runtime},
        {.uri = "/api/reminders/status", .method = HTTP_POST,
         .handler = reminder_runtime},
        {.uri = "/api/dispenser/test", .method = HTTP_POST,
         .handler = test_servo_once},
        {.uri = "/api/dispenser/reference", .method = HTTP_POST,
         .handler = set_tray_reference},
        {.uri = "/api/button/status", .method = HTTP_GET,
         .handler = patient_button_status},
        {.uri = "/api/audio/status", .method = HTTP_GET, .handler = send_audio_status},
        {.uri = "/api/audio/test-tone", .method = HTTP_POST, .handler = start_audio_test},
        {.uri = "/api/audio/stop", .method = HTTP_POST, .handler = stop_audio_test},
        {.uri = "/api/audio/microphone-level", .method = HTTP_POST,
         .handler = microphone_level_test},
        {.uri = "/api/audio/upload-preview", .method = HTTP_POST, .handler = upload_preview},
        {.uri = "/api/audio/play-preview", .method = HTTP_POST, .handler = play_preview},
        {.uri = "/api/audio/reminder-sound", .method = HTTP_GET,
         .handler = reminder_sound},
        {.uri = "/api/audio/reminder-sound", .method = HTTP_POST,
         .handler = reminder_sound},
        {.uri = "/api/ai/config", .method = HTTP_GET,
         .handler = send_ai_config_status},
        {.uri = "/api/ai/config", .method = HTTP_POST,
         .handler = save_ai_configuration},
        {.uri = "/api/ai/conversation", .method = HTTP_GET,
         .handler = send_ai_runtime_status},
        {.uri = "/api/ai/conversation", .method = HTTP_POST,
         .handler = start_ai_conversation},
        {.uri = "/api/ai/volume", .method = HTTP_GET,
         .handler = send_ai_volume},
        {.uri = "/api/ai/volume", .method = HTTP_POST,
         .handler = save_ai_volume},
        {.uri = "/api/ai/vad", .method = HTTP_GET,
         .handler = send_ai_vad},
        {.uri = "/api/ai/vad", .method = HTTP_POST,
         .handler = save_ai_vad},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &routes[i]),
                            TAG, "register route");
    }
    if (s_schedule_writable) {
        const httpd_uri_t writes[] = {
            {.uri = "/api/schedules", .method = HTTP_POST, .handler = mutate_schedule},
            {.uri = "/api/schedules/*", .method = HTTP_PUT, .handler = mutate_schedule},
            {.uri = "/api/schedules/*", .method = HTTP_DELETE, .handler = mutate_schedule},
        };
        for (size_t i = 0; i < sizeof(writes) / sizeof(writes[0]); ++i) {
            ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &writes[i]), TAG,
                                "register schedule route");
        }
    }
    if (xTaskCreate(fleet_task, "fleet_https", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Remote monitoring task unavailable");
    }
    return ESP_OK;
}
