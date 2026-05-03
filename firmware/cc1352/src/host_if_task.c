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
#include "output_if.h"
#include "packet_queue.h"
#include "protocol.h"
#include "task_event.h"

#define HOST_IF_TASK_RX_BUDGET_PER_POLL 128u

static uint8_t s_encoded_frame[COBS_MAX_ENCODED];
static size_t s_encoded_len = 0;
static bool s_overflow = false;

/* Deferred command: UART task stores frame, RF task processes it */
static uint8_t s_pending_frame[COBS_MAX_ENCODED];
static size_t s_pending_len = 0;
static volatile bool s_pending_ready = false;

static void HostIFTask_flushTx(void) {
    uint8_t tx_frame[PACKET_QUEUE_MAX_FRAME_SIZE];
    size_t tx_len = 0;
    uint8_t max_flush = 4u; /* Limit per poll to prevent starvation in TI-RTOS */

    while (max_flush > 0u && PacketQueue_dequeue(tx_frame, &tx_len)) {
        HostIF_writeBuffer(tx_frame, tx_len);
        max_flush--;
    }

    if (PacketQueue_isEmpty()) {
        TaskEvent_clear(TASK_EVENT_HOST_IF_TX_PENDING);
    }
}

void HostIFTask_init(void) {
    s_encoded_len = 0;
    s_overflow = false;
    s_pending_len = 0;
    s_pending_ready = false;
    PacketQueue_init();
}

void HostIFTask_processPendingCommand(void) {
    if (s_pending_ready) {
        CommandProcessor_processEncodedFrame(s_pending_frame, s_pending_len);
        s_pending_ready = false;
    }
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
            /* Defer to RF task — copy frame and signal via semaphore */
            if (s_pending_ready) {
                /* Previous command still pending — send async error to host.
                 * F8f follow-up: align with the #7a contract used by
                 * data_task / control_task. RSP_ERROR with seq=0 (was 0xFF
                 * + 0x81u literal). Python compat permanently accepts both
                 * seq=0 and seq=0xFF, so older firmware revisions stay
                 * forward-compatible with new hosts. */
                static const uint8_t busy_err[] = {ERR_INVALID_STATE};
                OutputIF_sendResponse(RSP_ERROR, 0u, busy_err, 1u);
            } else if (s_encoded_len <= sizeof(s_pending_frame)) {
                size_t j;
                for (j = 0; j < s_encoded_len; j++) {
                    s_pending_frame[j] = s_encoded_frame[j];
                }
                s_pending_len = s_encoded_len;
                s_pending_ready = true;
                extern void RfTask_signalCommand(void);
                RfTask_signalCommand();
            }
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
