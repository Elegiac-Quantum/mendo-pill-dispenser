#include "pill_ai_transport/policy.h"

#include <string.h>

enum {
    SERVER_URL_MAX = 256,
    DEVICE_ID_MIN = 8,
    DEVICE_ID_MAX = 64,
};

bool pill_ai_config_valid(const pill_ai_config_t *config)
{
    if (config == NULL || config->server_url == NULL || config->device_id == NULL ||
        config->secret == NULL || config->secret_bytes < PILL_AI_SECRET_BYTES) {
        return false;
    }
    size_t url_bytes = strlen(config->server_url);
    size_t id_bytes = strlen(config->device_id);
    return url_bytes > 6 && url_bytes < SERVER_URL_MAX &&
           strncmp(config->server_url, "wss://", 6) == 0 &&
           id_bytes >= DEVICE_ID_MIN && id_bytes <= DEVICE_ID_MAX;
}

bool pill_ai_frame_valid(size_t bytes)
{
    return bytes == PILL_AI_PCM_FRAME_BYTES;
}

size_t pill_ai_websocket_task_stack_bytes(void)
{
    return 28672;
}

size_t pill_ai_upload_batch_frames(void)
{
    return 5;
}

uint32_t pill_ai_pcm_peak(const int16_t *samples, size_t count)
{
    uint32_t peak = 0;
    if (samples == NULL) return 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t value = samples[i];
        uint32_t magnitude = value < 0 ? (uint32_t)(-value) : (uint32_t)value;
        if (magnitude > peak) peak = magnitude;
    }
    return peak;
}

void pill_ai_session_init(pill_ai_session_t *session, bool enabled)
{
    if (session != NULL) session->state = enabled ? PILL_AI_OFFLINE : PILL_AI_DISABLED;
}

bool pill_ai_transition(pill_ai_session_t *session, pill_ai_event_t event)
{
    if (session == NULL || session->state == PILL_AI_DISABLED) return false;
    if (event == PILL_AI_EVENT_REMINDER_PRIORITY ||
        event == PILL_AI_EVENT_DISCONNECTED) {
        session->state = PILL_AI_OFFLINE;
        return true;
    }
    if (event == PILL_AI_EVENT_ERROR) {
        session->state = PILL_AI_FAULT;
        return true;
    }
    switch (session->state) {
        case PILL_AI_OFFLINE:
        case PILL_AI_FAULT:
            if (event == PILL_AI_EVENT_CONNECT) {
                session->state = PILL_AI_CONNECTING;
                return true;
            }
            break;
        case PILL_AI_CONNECTING:
            if (event == PILL_AI_EVENT_CONNECTED) {
                session->state = PILL_AI_LISTENING;
                return true;
            }
            break;
        case PILL_AI_LISTENING:
            if (event == PILL_AI_EVENT_TTS_START) {
                session->state = PILL_AI_SPEAKING;
                return true;
            }
            break;
        case PILL_AI_SPEAKING:
            if (event == PILL_AI_EVENT_TTS_STOP) {
                session->state = PILL_AI_LISTENING;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}
