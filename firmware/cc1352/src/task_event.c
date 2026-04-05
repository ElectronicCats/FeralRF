/*
 * FeralRF CC1352 - Task Event flags implementation
 *
 * Fase 0.0: simple event flags, no semaphore (single task)
 */

#include "task_event.h"

static volatile uint32_t s_task_events = 0u;

void TaskEvent_init(void) {
    s_task_events = 0u;
}

void TaskEvent_set(uint32_t event_mask) {
    s_task_events |= event_mask;
}

void TaskEvent_clear(uint32_t event_mask) {
    s_task_events &= ~event_mask;
}

bool TaskEvent_isSet(uint32_t event_mask) {
    return (s_task_events & event_mask) != 0u;
}

uint32_t TaskEvent_get(void) {
    return s_task_events;
}

bool TaskEvent_hasWork(void) {
    return s_task_events != 0u;
}
