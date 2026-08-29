#include "pill_app/clock_policy.h"

#include <stddef.h>

pill_clock_quality_t pill_clock_quality(const pill_clock_evidence_t *evidence)
{
    if (evidence == NULL || !evidence->rtc_present || evidence->rtc_oscillator_stopped ||
        evidence->year < 2024 || evidence->year > 2099 ||
        evidence->backward_jump_seconds > 300) {
        return PILL_CLOCK_INVALID;
    }
    if (evidence->ntp_synchronized || evidence->seconds_since_sync <= 7U * 24U * 60U * 60U) {
        return PILL_CLOCK_VALID;
    }
    return PILL_CLOCK_DEGRADED;
}

