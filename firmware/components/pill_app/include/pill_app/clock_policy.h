#ifndef PILL_APP_CLOCK_POLICY_H
#define PILL_APP_CLOCK_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PILL_CLOCK_INVALID = 0,
    PILL_CLOCK_DEGRADED,
    PILL_CLOCK_VALID,
} pill_clock_quality_t;

typedef struct {
    bool rtc_present;
    bool rtc_oscillator_stopped;
    int year;
    bool ntp_synchronized;
    uint32_t seconds_since_sync;
    uint32_t backward_jump_seconds;
} pill_clock_evidence_t;

pill_clock_quality_t pill_clock_quality(const pill_clock_evidence_t *evidence);

#endif

