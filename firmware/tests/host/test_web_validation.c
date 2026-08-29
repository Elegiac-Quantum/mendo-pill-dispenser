#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pill_web/web_validation.h"
#include "pill_web/schedule_json.h"

static void test_schedule_request_validation(void)
{
    pill_schedule_t schedule = {0};
    strcpy(schedule.id, "blood-pressure");
    strcpy(schedule.medication, "Medicine B");
    schedule.enabled = true;
    schedule.time_count = 1;
    schedule.times[0] = 9 * 60;
    schedule.reminder_minutes = 5;
    schedule.snooze_minutes = 10;

    assert(pill_web_validate_schedule(&schedule) == PILL_WEB_VALID);
    schedule.medication[0] = '\0';
    assert(pill_web_validate_schedule(&schedule) == PILL_WEB_INVALID_SCHEDULE);
}

static void test_schedule_json_is_strict_and_bounded(void)
{
    schedule_json_request_t request;
    const char *valid = "{\"medication\":\"Vitamin D\",\"dose_instruction\":\"1 tablet\",\"times\":[480,1200]}";
    assert(schedule_json_parse(valid, strlen(valid), &request));
    assert(strcmp(request.medication, "Vitamin D") == 0);
    assert(request.time_count == 2 && request.times[1] == 1200);
    assert(request.layer == 1);

    const char *unknown = "{\"medication\":\"A\",\"dose_instruction\":\"B\",\"layer\":1,\"times\":[480],\"id\":\"chosen\"}";
    assert(!schedule_json_parse(unknown, strlen(unknown), &request));
    const char *bad_layer = "{\"medication\":\"A\",\"dose_instruction\":\"B\",\"layer\":2,\"times\":[480]}";
    assert(!schedule_json_parse(bad_layer, strlen(bad_layer), &request));
    assert(!schedule_json_parse("{", 1, &request));
    char oversized[SCHEDULE_JSON_BODY_MAX + 2];
    memset(oversized, 'x', sizeof(oversized));
    assert(!schedule_json_parse(oversized, sizeof(oversized), &request));
}

static void test_session_and_csrf_are_both_required(void)
{
    pill_web_security_t security = {0};
    strcpy(security.session_token, "session-secret");
    strcpy(security.csrf_token, "csrf-secret");

    assert(pill_web_authorize_change(&security, "session-secret", "csrf-secret"));
    assert(!pill_web_authorize_change(&security, "wrong", "csrf-secret"));
    assert(!pill_web_authorize_change(&security, "session-secret", "wrong"));
    assert(!pill_web_authorize_change(&security, NULL, "csrf-secret"));
}

static void test_movement_confirmation_expires(void)
{
    pill_web_confirmation_t confirmation = {0};
    assert(pill_web_issue_confirmation(&confirmation, "move-token", 1000, 30));
    assert(pill_web_consume_confirmation(&confirmation, "move-token", 1029));
    assert(!pill_web_consume_confirmation(&confirmation, "move-token", 1029));

    assert(pill_web_issue_confirmation(&confirmation, "new-token", 2000, 30));
    assert(!pill_web_consume_confirmation(&confirmation, "new-token", 2031));
}

static void test_http_server_recovers_from_stale_mobile_connections(void)
{
    pill_web_http_policy_t policy = pill_web_http_policy();
    assert(policy.max_open_sockets >= 4);
    assert(policy.max_open_sockets <= 7);
    assert(policy.lru_purge_enable);
    assert(policy.receive_timeout_seconds <= 10);
    assert(policy.send_timeout_seconds <= 10);
}

int main(void)
{
    test_schedule_request_validation();
    test_schedule_json_is_strict_and_bounded();
    test_session_and_csrf_are_both_required();
    test_movement_confirmation_expires();
    test_http_server_recovers_from_stale_mobile_connections();
    puts("web validation tests passed");
    return 0;
}
