#ifndef PILL_AI_TRANSPORT_TRANSPORT_H
#define PILL_AI_TRANSPORT_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define PILL_AI_VAD_RECOMMENDED 80u

typedef enum {
    PILL_AI_RUNTIME_IDLE = 0,
    PILL_AI_RUNTIME_CONNECTING,
    PILL_AI_RUNTIME_LISTENING,
    PILL_AI_RUNTIME_THINKING,
    PILL_AI_RUNTIME_SPEAKING,
    PILL_AI_RUNTIME_FAILED,
} pill_ai_runtime_state_t;

bool pill_ai_start_conversation(void);
bool pill_ai_start_conversation_deferred(void);
bool pill_ai_connection_ready(void);
void pill_ai_begin_listening(void);
pill_ai_runtime_state_t pill_ai_runtime_state(void);
const char *pill_ai_last_error(void);
const char *pill_ai_activation_code(void);
void pill_ai_check_activation_async(void);
uint8_t pill_ai_volume(void);
esp_err_t pill_ai_set_volume(uint8_t volume);
uint16_t pill_ai_vad_threshold(void);
esp_err_t pill_ai_set_vad_threshold(uint16_t threshold);
uint32_t pill_ai_vad_minimum(void);
uint32_t pill_ai_vad_maximum(void);

#endif
