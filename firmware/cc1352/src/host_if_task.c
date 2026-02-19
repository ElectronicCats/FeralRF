/*
 * FeralRF CC1352 - Host Interface Task (polling variant)
 *
 * Mirrors the reference "host_if_task" role, but in bare-metal style.
 */

#include "host_if_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "command_processor.h"
#include "host_if.h"
#include "packet_queue.h"
#include "protocol.h"
#include "task_event.h"

#define HOST_IF_TASK_RX_BUDGET_PER_POLL 128u

static uint8_t s_encoded_frame[COBS_MAX_ENCODED];
static size_t s_encoded_len = 0;
static bool s_overflow = false;

static void HostIFTask_flushTx(void) {
    uint8_t tx_frame[PACKET_QUEUE_MAX_FRAME_SIZE];
    size_t tx_len = 0;

    while (PacketQueue_dequeue(tx_frame, &tx_len)) {
        HostIF_writeBuffer(tx_frame, tx_len);
    }

    if (PacketQueue_isEmpty()) {
        TaskEvent_clear(TASK_EVENT_HOST_IF_TX_PENDING);
    }
}

void HostIFTask_init(void) {
    s_encoded_len = 0;
    s_overflow = false;
    PacketQueue_init();
}

void HostIFTask_poll(void) {
    uint16_t processed = 0u;

    HostIFTask_flushTx();

    while (processed < HOST_IF_TASK_RX_BUDGET_PER_POLL) {
        int32_t ch = 0;
        uint8_t byte = 0u;

        ch = HostIF_readByteNonBlocking();
        if (ch < 0) {
            break;
        }
        processed++;

        byte = (uint8_t)ch;
        if (byte == 0x00u) {
            if (s_overflow) {
                s_overflow = false;
                s_encoded_len = 0;
                TaskEvent_set(TASK_EVENT_HOST_IF_RX_OVERFLOW);
                CommandProcessor_sendFrameTooLongError();
                continue;
            }

            if (s_encoded_len == 0) {
                continue;
            }

            TaskEvent_set(TASK_EVENT_HOST_IF_RX_FRAME);
            CommandProcessor_processEncodedFrame(s_encoded_frame, s_encoded_len);
            s_encoded_len = 0;
            continue;
        }

        if (!s_overflow) {
            if (s_encoded_len < sizeof(s_encoded_frame)) {
                s_encoded_frame[s_encoded_len++] = byte;
            } else {
                s_overflow = true;
            }
        }
    }

    HostIFTask_flushTx();
}
