#ifndef PILL_WEB_SCHEDULE_JSON_H
#define PILL_WEB_SCHEDULE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pill_domain/schedule.h"

#define SCHEDULE_JSON_BODY_MAX 1024
#define SCHEDULE_JSON_DOSE_MAX 129

typedef struct {
    char medication[PILL_MEDICATION_NAME_MAX];
    char dose_instruction[SCHEDULE_JSON_DOSE_MAX];
    uint8_t layer;
    uint8_t time_count;
    uint16_t times[PILL_SCHEDULE_TIMES_MAX];
} schedule_json_request_t;

bool schedule_json_parse(const char *json, size_t length, schedule_json_request_t *request);

#endif
