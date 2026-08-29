#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pill_ai_transport/policy.h"

static void test_configuration(void)
{
    uint8_t secret[PILL_AI_SECRET_BYTES];
    memset(secret, 0x5a, sizeof(secret));
    pill_ai_config_t valid = {
        .server_url = "wss://voice.example.test/device",
        .device_id = "smartpill-a1b2c3d4",
        .secret = secret,
        .secret_bytes = sizeof(secret),
    };
    assert(pill_ai_config_valid(&valid));
    valid.server_url = "ws://voice.example.test/device";
    assert(!pill_ai_config_valid(&valid));
    valid.server_url = "https://voice.example.test/device";
    assert(!pill_ai_config_valid(&valid));
    valid.server_url = "wss://voice.example.test/device";
    valid.secret_bytes--;
    assert(!pill_ai_config_valid(&valid));
}

static void test_frames(void)
{
    assert(pill_ai_frame_valid(PILL_AI_PCM_FRAME_BYTES));
    assert(!pill_ai_frame_valid(0));
    assert(!pill_ai_frame_valid(PILL_AI_PCM_FRAME_BYTES + 1));
    const int16_t samples[] = {-20, 1200, -32768, 42};
    assert(pill_ai_pcm_peak(samples, 4) == 32768u);
    assert(pill_ai_pcm_peak(NULL, 4) == 0u);
}

static void test_secure_websocket_has_enough_stack(void)
{
    assert(pill_ai_websocket_task_stack_bytes() >= 8192);
}

static void test_audio_upload_is_batched_for_wide_area_networks(void)
{
    assert(pill_ai_upload_batch_frames() >= 2);
    assert(pill_ai_upload_batch_frames() <= 10);
    assert(250 % pill_ai_upload_batch_frames() == 0);
}

static void test_state_machine(void)
{
    pill_ai_session_t session;
    pill_ai_session_init(&session, true);
    assert(session.state == PILL_AI_OFFLINE);
    assert(pill_ai_transition(&session, PILL_AI_EVENT_CONNECT));
    assert(session.state == PILL_AI_CONNECTING);
    assert(pill_ai_transition(&session, PILL_AI_EVENT_CONNECTED));
    assert(session.state == PILL_AI_LISTENING);
    assert(pill_ai_transition(&session, PILL_AI_EVENT_TTS_START));
    assert(session.state == PILL_AI_SPEAKING);
    assert(pill_ai_transition(&session, PILL_AI_EVENT_TTS_STOP));
    assert(session.state == PILL_AI_LISTENING);
    assert(pill_ai_transition(&session, PILL_AI_EVENT_REMINDER_PRIORITY));
    assert(session.state == PILL_AI_OFFLINE);
    assert(!pill_ai_transition(&session, PILL_AI_EVENT_TTS_START));

    pill_ai_session_init(&session, false);
    assert(session.state == PILL_AI_DISABLED);
    assert(!pill_ai_transition(&session, PILL_AI_EVENT_CONNECT));
}

int main(void)
{
    test_configuration();
    test_frames();
    test_secure_websocket_has_enough_stack();
    test_audio_upload_is_batched_for_wide_area_networks();
    test_state_machine();
    puts("ai transport policy tests passed");
    return 0;
}
