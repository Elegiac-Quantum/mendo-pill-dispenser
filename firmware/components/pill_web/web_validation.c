#include "pill_web/web_validation.h"

#include <stddef.h>
#include <string.h>

pill_web_http_policy_t pill_web_http_policy(void)
{
    return (pill_web_http_policy_t){
        .max_open_sockets = 7,
        .receive_timeout_seconds = 8,
        .send_timeout_seconds = 8,
        .lru_purge_enable = true,
    };
}

static bool token_equal(const char *expected, const char *provided)
{
    if (expected == NULL || provided == NULL) {
        return false;
    }
    size_t expected_length = strnlen(expected, PILL_WEB_TOKEN_MAX);
    size_t provided_length = strnlen(provided, PILL_WEB_TOKEN_MAX);
    if (expected_length == 0 || expected_length >= PILL_WEB_TOKEN_MAX ||
        provided_length != expected_length) {
        return false;
    }

    unsigned difference = 0;
    for (size_t index = 0; index < expected_length; ++index) {
        difference |= (unsigned char)expected[index] ^ (unsigned char)provided[index];
    }
    return difference == 0;
}

pill_web_validation_result_t pill_web_validate_schedule(const pill_schedule_t *schedule)
{
    return pill_schedule_validate(schedule) == PILL_SCHEDULE_OK ? PILL_WEB_VALID
                                                                : PILL_WEB_INVALID_SCHEDULE;
}

bool pill_web_authorize_change(const pill_web_security_t *security,
                               const char *session_token,
                               const char *csrf_token)
{
    return security != NULL && token_equal(security->session_token, session_token) &&
           token_equal(security->csrf_token, csrf_token);
}

bool pill_web_issue_confirmation(pill_web_confirmation_t *confirmation,
                                 const char *token,
                                 int64_t now_utc,
                                 uint32_t lifetime_seconds)
{
    if (confirmation == NULL || token == NULL || token[0] == '\0' ||
        strnlen(token, PILL_WEB_TOKEN_MAX) >= PILL_WEB_TOKEN_MAX || lifetime_seconds == 0 ||
        lifetime_seconds > 300) {
        return false;
    }
    strcpy(confirmation->token, token);
    confirmation->expires_utc = now_utc + lifetime_seconds;
    confirmation->active = true;
    return true;
}

bool pill_web_consume_confirmation(pill_web_confirmation_t *confirmation,
                                   const char *token,
                                   int64_t now_utc)
{
    if (confirmation == NULL || !confirmation->active) {
        return false;
    }
    if (now_utc > confirmation->expires_utc) {
        confirmation->active = false;
        return false;
    }
    if (!token_equal(confirmation->token, token)) {
        return false;
    }
    confirmation->active = false;
    return true;
}
