#ifndef SCHEDULE_STORE_H
#define SCHEDULE_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "pill_domain/schedule.h"

#define SCHEDULE_STORE_MAX_DRAFTS 8
#define SCHEDULE_DOSE_TEXT_MAX 129

typedef struct {
    const char *medication;
    const char *dose_instruction;
    uint8_t time_count;
    uint16_t times[PILL_SCHEDULE_TIMES_MAX];
    uint8_t reminder_minutes;
    uint8_t snooze_minutes;
} schedule_draft_input_t;

typedef struct {
    pill_schedule_t schedule;
    char dose_instruction[SCHEDULE_DOSE_TEXT_MAX];
} schedule_draft_t;

typedef struct {
    uint32_t generation;
    uint32_t next_id;
    size_t count;
    schedule_draft_t drafts[SCHEDULE_STORE_MAX_DRAFTS];
} schedule_store_t;

typedef bool (*schedule_store_read_slot_fn)(void *, unsigned, void *, size_t);
typedef bool (*schedule_store_write_slot_fn)(void *, unsigned, const void *, size_t);
typedef bool (*schedule_store_read_active_fn)(void *, uint32_t *);
typedef bool (*schedule_store_write_active_fn)(void *, uint32_t);

typedef struct {
    void *context;
    schedule_store_read_slot_fn read_slot;
    schedule_store_write_slot_fn write_slot;
    schedule_store_read_active_fn read_active_generation;
    schedule_store_write_active_fn write_active_generation;
} schedule_store_backend_t;

typedef enum {
    SCHEDULE_STORE_OK,
    SCHEDULE_STORE_INVALID,
    SCHEDULE_STORE_FULL,
    SCHEDULE_STORE_NOT_FOUND,
    SCHEDULE_STORE_IO_ERROR,
    SCHEDULE_STORE_CORRUPT
} schedule_store_result_t;

void schedule_store_init_empty(schedule_store_t *store);
size_t schedule_store_count(const schedule_store_t *store);
schedule_store_result_t schedule_store_create(schedule_store_t *store,
                                              const schedule_draft_input_t *input,
                                              schedule_draft_t *created);
schedule_store_result_t schedule_store_update(schedule_store_t *store,
                                              const char *id,
                                              const schedule_draft_input_t *input,
                                              schedule_draft_t *updated);
schedule_store_result_t schedule_store_delete(schedule_store_t *store, const char *id);
schedule_store_result_t schedule_store_commit(schedule_store_t *store,
                                              const schedule_store_backend_t *backend);
schedule_store_result_t schedule_store_load(schedule_store_t *store,
                                            const schedule_store_backend_t *backend);

#endif
