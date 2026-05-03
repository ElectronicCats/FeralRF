/*
 * FeralRF CC1352 - Output Interface implementation
 */

#include "output_if.h"

#include <stddef.h>
#include <stdint.h>

#include "host_if.h"
#include "packet_queue.h"
#include "protocol.h"
#include "task_event.h"

void OutputIF_sendResponse(uint8_t rsp_cmd, uint8_t seq, const uint8_t *payload,
                           uint16_t payload_len) {
    uint8_t raw_frame[PROTOCOL_MAX_FRAME];
    uint8_t encoded[COBS_MAX_ENCODED];
    uint8_t tx_frame[PACKET_QUEUE_MAX_FRAME_SIZE];

    /* F8f #1: defensive — protocol_build_frame writes payload bytes into
     * raw_frame without bounds-checking. PROTOCOL_MAX_FRAME == PROTOCOL_MAX_PAYLOAD
     * + 6 (cmd+seq+len+crc), so payload_len > PROTOCOL_MAX_PAYLOAD overflows
     * raw_frame on the stack. Drop the frame here rather than corrupt memory. */
    if (payload_len > PROTOCOL_MAX_PAYLOAD) {
        return;
    }

    size_t raw_len = protocol_build_frame(rsp_cmd, seq, payload, payload_len, raw_frame);
    size_t encoded_len = cobs_encode(raw_frame, raw_len, encoded);

    if ((encoded_len + 1u) > PACKET_QUEUE_MAX_FRAME_SIZE) {
        return;
    }

    for (size_t i = 0; i < encoded_len; i++) {
        tx_frame[i] = encoded[i];
    }
    tx_frame[encoded_len] = 0x00u;

    if (!PacketQueue_enqueue(tx_frame, encoded_len + 1u)) {
        /* Queue full — drop instead of blocking UARTCharPut.
         * Blocking UART causes deadlock in TI-RTOS. */
        TaskEvent_set(TASK_EVENT_PACKET_QUEUE_DROPPED);
    } else {
        TaskEvent_set(TASK_EVENT_HOST_IF_TX_PENDING);
    }
}
