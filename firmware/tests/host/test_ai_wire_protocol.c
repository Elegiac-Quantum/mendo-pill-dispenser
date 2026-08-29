#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pill_ai_transport/wire_protocol.h"

int main(void)
{
    char json[256];
    assert(pill_ai_make_hello(json, sizeof(json)));
    assert(strstr(json, "\"type\":\"hello\"") != NULL);
    assert(strstr(json, "\"format\":\"opus\"") != NULL);
    assert(strstr(json, "\"frame_duration\":60") != NULL);

    assert(pill_ai_make_listen(json, sizeof(json), true));
    assert(strstr(json, "\"state\":\"start\"") != NULL);
    assert(strstr(json, "\"mode\":\"auto\"") != NULL);
    assert(pill_ai_make_listen(json, sizeof(json), false));
    assert(strstr(json, "\"state\":\"stop\"") != NULL);
    assert(pill_ai_make_listen_session(json, sizeof(json), "session-1", true));
    assert(strstr(json, "\"session_id\":\"session-1\"") != NULL);
    char session[32] = {0};
    assert(pill_ai_read_session_id("{\"session_id\":\"abc\"}", session,
                                   sizeof(session)));
    assert(strcmp(session, "abc") == 0);

    assert(pill_ai_wire_event("{\"type\":\"hello\"}") == PILL_AI_WIRE_HELLO);
    assert(pill_ai_wire_event("{\"type\":\"tts\",\"state\":\"start\"}") ==
           PILL_AI_WIRE_TTS_START);
    assert(pill_ai_wire_event("{\"type\":\"tts\",\"state\":\"stop\"}") ==
           PILL_AI_WIRE_TTS_STOP);
    assert(pill_ai_wire_event("{\"type\":\"error\"}") == PILL_AI_WIRE_ERROR);
    assert(pill_ai_wire_event("not-json") == PILL_AI_WIRE_UNKNOWN);
    puts("ai wire protocol tests passed");
    return 0;
}
