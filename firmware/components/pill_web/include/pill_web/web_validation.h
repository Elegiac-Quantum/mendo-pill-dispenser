#ifndef PILL_WEB_WEB_VALIDATION_H
#define PILL_WEB_WEB_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>

#include "pill_domain/schedule.h"

#define PILL_WEB_TOKEN_MAX 48

typedef enum {
    PILL_WEB_VALID = 0,
    PILL_WEB_INVALID_SCHEDULE,
} pill_web_validation_result_t;

typedef struct {
    char session_token[PILL_WEB_TOKEN_MAX];
    char csrf_token[PILL_WEB_TOKEN_MAX];
} pill_web_security_t;

typedef struct {
    char token[PILL_WEB_TOKEN_MAX];
    int64_t expires_utc;
    bool active;
} pill_web_confirmation_t;

typedef struct {
    unsigned max_open_sockets;
    unsigned receive_timeout_seconds;
    unsigned send_timeout_seconds;
    bool lru_purge_enable;
} pill_web_http_policy_t;

pill_web_http_policy_t pill_web_http_policy(void);
pill_web_validation_result_t pill_web_validate_schedule(const pill_schedule_t *schedule);
bool pill_web_authorize_change(const pill_web_security_t *security,
                               const char *session_token,
                               const char *csrf_token);
bool pill_web_issue_confirmation(pill_web_confirmation_t *confirmation,
                                 const char *token,
                                 int64_t now_utc,
                                 uint32_t lifetime_seconds);
bool pill_web_consume_confirmation(pill_web_confirmation_t *confirmation,
                                   const char *token,
                                   int64_t now_utc);

#endif
