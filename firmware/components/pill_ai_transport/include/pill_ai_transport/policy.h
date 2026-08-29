#ifndef PILL_AI_TRANSPORT_POLICY_H
#define PILL_AI_TRANSPORT_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PILL_AI_SECRET_BYTES 32u
#define PILL_AI_PCM_FRAME_SAMPLES 320u
#define PILL_AI_PCM_FRAME_BYTES (PILL_AI_PCM_FRAME_SAMPLES * sizeof(int16_t))

typedef struct {
    const char *server_url;
    const char *device_id;
    const uint8_t *secret;
    size_t secret_bytes;
} pill_ai_config_t;

typedef enum {
    PILL_AI_DISABLED = 0,
    PILL_AI_OFFLINE,
    PILL_AI_CONNECTING,
    PILL_AI_LISTENING,
    PILL_AI_SPEAKING,
    PILL_AI_FAULT,
} pill_ai_state_t;

typedef enum {
    PILL_AI_EVENT_CONNECT = 0,
    PILL_AI_EVENT_CONNECTED,
    PILL_AI_EVENT_TTS_START,
    PILL_AI_EVENT_TTS_STOP,
    PILL_AI_EVENT_DISCONNECTED,
    PILL_AI_EVENT_ERROR,
    PILL_AI_EVENT_REMINDER_PRIORITY,
} pill_ai_event_t;

typedef struct {
    pill_ai_state_t state;
} pill_ai_session_t;

bool pill_ai_config_valid(const pill_ai_config_t *config);
bool pill_ai_frame_valid(size_t bytes);
size_t pill_ai_websocket_task_stack_bytes(void);
size_t pill_ai_upload_batch_frames(void);
uint32_t pill_ai_pcm_peak(const int16_t *samples, size_t count);
void pill_ai_session_init(pill_ai_session_t *session, bool enabled);
bool pill_ai_transition(pill_ai_session_t *session, pill_ai_event_t event);

#endif
