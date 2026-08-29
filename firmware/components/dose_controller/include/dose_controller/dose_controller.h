#ifndef DOSE_CONTROLLER_DOSE_CONTROLLER_H
#define DOSE_CONTROLLER_DOSE_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "pill_domain/dose.h"
#include "pill_domain/schedule.h"

typedef struct {
    char occurrence_id[PILL_OCCURRENCE_ID_MAX];
    pill_dose_state_t state;
    int64_t intended_utc;
    int64_t updated_utc;
} dose_record_t;

typedef bool (*dose_load_fn)(void *context,
                             const char *occurrence_id,
                             dose_record_t *record,
                             bool *found);
typedef bool (*dose_store_fn)(void *context, const dose_record_t *record);
typedef bool (*dose_servo_cycle_fn)(void *context);

typedef struct {
    void *context;
    dose_load_fn load;
    dose_store_fn store;
    dose_servo_cycle_fn servo_cycle;
} dose_controller_t;

typedef enum {
    DOSE_CONTROLLER_DISPENSED = 0,
    DOSE_CONTROLLER_ALREADY_RECORDED,
    DOSE_CONTROLLER_NOT_DUE,
    DOSE_CONTROLLER_MISSED,
    DOSE_CONTROLLER_TIME_INVALID,
    DOSE_CONTROLLER_FAULT,
    DOSE_CONTROLLER_STORAGE_ERROR,
    DOSE_CONTROLLER_INVALID_ARGUMENT,
} dose_controller_result_t;

dose_controller_result_t dose_controller_process_due(dose_controller_t *controller,
                                                      const char *occurrence_id,
                                                      int64_t intended_utc,
                                                      int64_t now_utc,
                                                      bool clock_valid);

dose_controller_result_t dose_controller_recover(dose_controller_t *controller,
                                                  const char *occurrence_id);

#endif

