/*
 * FeralRF CC1352 - Task Event flags
 *
 * Bare-metal equivalent of the task event signaling used in TI sniffer examples.
 */

#ifndef TASK_EVENT_H
#define TASK_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#define TASK_EVENT_HOST_IF_RX_FRAME 0x00000001u
#define TASK_EVENT_HOST_IF_RX_OVERFLOW 0x00000002u
#define TASK_EVENT_HOST_IF_TX_PENDING 0x00000004u
#define TASK_EVENT_CMD_PROCESSED 0x00000008u
#define TASK_EVENT_PACKET_QUEUE_DROPPED 0x00000010u
#define TASK_EVENT_CONTROL_RX_START 0x00000020u
#define TASK_EVENT_CONTROL_RX_STOP 0x00000040u
#define TASK_EVENT_DATA_RX_ACTIVE 0x00000080u
#define TASK_EVENT_CONTROL_TX_RAW 0x00000100u

void TaskEvent_init(void);
void TaskEvent_set(uint32_t event_mask);
void TaskEvent_clear(uint32_t event_mask);
bool TaskEvent_isSet(uint32_t event_mask);
uint32_t TaskEvent_get(void);

#endif /* TASK_EVENT_H */
