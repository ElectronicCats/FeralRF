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
    HostIFTask_flushTx();

    int32_t ch = HostIF_readByteNonBlocking();
    if (ch < 0) {
        return;
    }

    uint8_t byte = (uint8_t)ch;
    if (byte == 0x00u) {
        if (s_overflow) {
            s_overflow = false;
            s_encoded_len = 0;
            TaskEvent_set(TASK_EVENT_HOST_IF_RX_OVERFLOW);
            CommandProcessor_sendFrameTooLongError();
            return;
        }

        if (s_encoded_len == 0) {
            return;
        }

        TaskEvent_set(TASK_EVENT_HOST_IF_RX_FRAME);
        CommandProcessor_processEncodedFrame(s_encoded_frame, s_encoded_len);
        s_encoded_len = 0;
        HostIFTask_flushTx();
        return;
    }

    if (!s_overflow) {
        if (s_encoded_len < sizeof(s_encoded_frame)) {
            s_encoded_frame[s_encoded_len++] = byte;
        } else {
            s_overflow = true;
        }
    }
}
