/*
 * FeralRF CC1352 - Task Event flags implementation
 */

#include "task_event.h"

#include <ti/sysbios/knl/Semaphore.h>

/* Semaphore shared with RF task (defined in main_rtos.c) */
extern Semaphore_Handle g_rf_semaphore;

/* Events that require waking the RF task */
#define RF_TASK_EVENTS                                                                           \
    (TASK_EVENT_CONTROL_RX_START | TASK_EVENT_CONTROL_RX_STOP | TASK_EVENT_CONTROL_TX_RAW |      \
     TASK_EVENT_CONTROL_TX_BURST | TASK_EVENT_CONTROL_TX_CONTINUOUS)

static volatile uint32_t s_task_events = 0u;

void TaskEvent_init(void) {
    s_task_events = 0u;
}

void TaskEvent_set(uint32_t event_mask) {
    s_task_events |= event_mask;

    /* Wake RF task when RF-related events are posted */
    if ((event_mask & RF_TASK_EVENTS) != 0u && g_rf_semaphore != NULL) {
        Semaphore_post(g_rf_semaphore);
    }
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
    return (s_task_events & RF_TASK_EVENTS) != 0u;
}
