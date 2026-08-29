#ifndef PILL_AI_TRANSPORT_WIRE_PROTOCOL_H
#define PILL_AI_TRANSPORT_WIRE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PILL_AI_WIRE_UNKNOWN = 0,
    PILL_AI_WIRE_HELLO,
    PILL_AI_WIRE_TTS_START,
    PILL_AI_WIRE_TTS_STOP,
    PILL_AI_WIRE_ERROR,
} pill_ai_wire_event_t;

bool pill_ai_make_hello(char *output, size_t capacity);
bool pill_ai_make_listen(char *output, size_t capacity, bool start);
bool pill_ai_make_listen_session(char *output, size_t capacity,
                                 const char *session_id, bool start);
bool pill_ai_read_session_id(const char *json, char *output, size_t capacity);
pill_ai_wire_event_t pill_ai_wire_event(const char *json);

#endif
