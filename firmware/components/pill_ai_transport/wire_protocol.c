#include "pill_ai_transport/wire_protocol.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static bool write_json(char *output, size_t capacity, const char *text)
{
    if (output == NULL || text == NULL || capacity == 0) {
        return false;
    }
    int length = snprintf(output, capacity, "%s", text);
    return length > 0 && (size_t)length < capacity;
}

bool pill_ai_make_hello(char *output, size_t capacity)
{
    return write_json(
        output, capacity,
        "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
        "\"features\":{\"mcp\":false},"
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,"
        "\"channels\":1,\"frame_duration\":60}}");
}

bool pill_ai_make_listen(char *output, size_t capacity, bool start)
{
    return pill_ai_make_listen_session(output, capacity, "", start);
}

bool pill_ai_make_listen_session(char *output, size_t capacity,
                                 const char *session_id, bool start)
{
    if (output == NULL || session_id == NULL || capacity == 0) return false;
    int length = snprintf(
        output, capacity,
        start ? "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"start\"," 
                "\"mode\":\"auto\"}"
              : "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"stop\"}",
        session_id);
    return length > 0 && (size_t)length < capacity;
}

bool pill_ai_read_session_id(const char *json, char *output, size_t capacity)
{
    if (json == NULL || output == NULL || capacity == 0) return false;
    cJSON *root = cJSON_Parse(json);
    cJSON *item = root ? cJSON_GetObjectItemCaseSensitive(root, "session_id") : NULL;
    bool valid = cJSON_IsString(item) && item->valuestring[0] != '\0' &&
                 strnlen(item->valuestring, capacity) < capacity;
    if (valid) snprintf(output, capacity, "%s", item->valuestring);
    cJSON_Delete(root);
    return valid;
}

pill_ai_wire_event_t pill_ai_wire_event(const char *json)
{
    cJSON *root = json ? cJSON_Parse(json) : NULL;
    cJSON *type = root ? cJSON_GetObjectItemCaseSensitive(root, "type") : NULL;
    pill_ai_wire_event_t event = PILL_AI_WIRE_UNKNOWN;
    if (cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "hello") == 0) {
            event = PILL_AI_WIRE_HELLO;
        } else if (strcmp(type->valuestring, "error") == 0) {
            event = PILL_AI_WIRE_ERROR;
        } else if (strcmp(type->valuestring, "tts") == 0) {
            cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
            if (cJSON_IsString(state) && strcmp(state->valuestring, "start") == 0) {
                event = PILL_AI_WIRE_TTS_START;
            } else if (cJSON_IsString(state) &&
                       strcmp(state->valuestring, "stop") == 0) {
                event = PILL_AI_WIRE_TTS_STOP;
            }
        }
    }
    cJSON_Delete(root);
    return event;
}
