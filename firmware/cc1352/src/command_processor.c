/*
 * FeralRF CC1352 - Command Processor (COBS + CRC16)
 */

#include "command_processor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "control_task.h"
#include "ll_manager.h"
#include "output_if.h"
#include "protocol.h"
#include "radio_if.h"
#include "task_event.h"

/* Commands (match python/feralrf/enums.py) */
#define CMD_RADIO_INIT 0x01u
#define CMD_SET_CHANNEL 0x02u
#define CMD_SET_POWER 0x03u
#define CMD_SET_PHY 0x04u
#define CMD_GET_INFO 0x05u
#define CMD_GET_STATS 0x06u
#define CMD_RX_START 0x10u
#define CMD_RX_STOP 0x11u
#define CMD_TX_RAW 0x20u

/* Responses (match python/feralrf/enums.py) */
#define RSP_ACK 0x80u
#define RSP_ERROR 0x81u
#define RSP_STATS 0x93u
#define RSP_INFO 0x94u

/* Error codes */
#define ERR_INVALID_CMD 0x01u
#define ERR_INVALID_PAYLOAD 0x02u
#define ERR_INVALID_FRAME 0x03u
#define ERR_FRAME_TOO_LONG 0x04u
#define ERR_INVALID_STATE 0x05u

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void send_response(uint8_t rsp_cmd, uint8_t seq, const uint8_t *payload,
                          uint16_t payload_len) {
    OutputIF_sendResponse(rsp_cmd, seq, payload, payload_len);
}

static void send_ack(uint8_t seq) {
    send_response(RSP_ACK, seq, NULL, 0);
}

static void send_error(uint8_t seq, uint8_t error_code) {
    uint8_t payload[1] = {error_code};
    send_response(RSP_ERROR, seq, payload, sizeof(payload));
}

static void send_info(uint8_t seq) {
    uint8_t payload[12];

    ControlTask_getInfoPayload(payload, sizeof(payload));
    send_response(RSP_INFO, seq, payload, sizeof(payload));
}

static void send_stats(uint8_t seq) {
    uint8_t payload[36];
    RadioIF_Metrics metrics;
    LLManager_Stats ll_stats;

    RadioIF_getMetrics(&metrics);
    LLManager_getStats(&ll_stats);
    write_u32_le(&payload[0], metrics.rx_ok);
    write_u32_le(&payload[4], metrics.rx_crc_err);
    write_u32_le(&payload[8], metrics.rx_drop);
    write_u32_le(&payload[12], metrics.rx_overflow);
    write_u32_le(&payload[16], ll_stats.kind_unknown);
    write_u32_le(&payload[20], ll_stats.kind_adv);
    write_u32_le(&payload[24], ll_stats.kind_scan);
    write_u32_le(&payload[28], ll_stats.kind_connect);
    write_u32_le(&payload[32], ll_stats.kind_data);
    send_response(RSP_STATS, seq, payload, sizeof(payload));
}

static void handle_command(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t payload_len) {
    switch (cmd) {
    case CMD_RADIO_INIT:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onRadioInit();
        send_ack(seq);
        return;

    case CMD_GET_INFO:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        send_info(seq);
        return;

    case CMD_GET_STATS:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        send_stats(seq);
        return;

    case CMD_SET_PHY:
        if (!(payload_len == 1 || payload_len == 7)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!ControlTask_onSetPhy(payload[0], payload_len >= 3 ? read_u16_le(&payload[1]) : 0u,
                                  payload_len == 7 ? read_u32_le(&payload[3]) : 0u)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        send_ack(seq);
        return;

    case CMD_SET_CHANNEL:
        if (payload_len != 1) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onSetChannel(payload[0]);
        send_ack(seq);
        return;

    case CMD_SET_POWER:
        if (payload_len != 1) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onSetPower((int8_t)payload[0]);
        send_ack(seq);
        return;

    case CMD_RX_START:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onRxStart();
        send_ack(seq);
        return;

    case CMD_RX_STOP:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onRxStop();
        send_ack(seq);
        return;

    case CMD_TX_RAW:
        if (payload_len < 2u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        {
            uint8_t tx_len = payload[0];
            uint16_t expected_payload_len = (uint16_t)tx_len + 2u;

            if (tx_len == 0u || expected_payload_len != payload_len) {
                send_error(seq, ERR_INVALID_PAYLOAD);
                return;
            }

            if (!ControlTask_onTxRaw(&payload[1], tx_len, (int8_t)payload[1u + tx_len])) {
                send_error(seq, ERR_INVALID_STATE);
                return;
            }
        }
        send_ack(seq);
        return;

    default:
        send_error(seq, ERR_INVALID_CMD);
        return;
    }
}

void CommandProcessor_init(void) {
    /* State lives in control_task; keep init for module parity. */
}

void CommandProcessor_sendFrameTooLongError(void) {
    send_error(0, ERR_FRAME_TOO_LONG);
}

void CommandProcessor_processEncodedFrame(const uint8_t *encoded_frame, size_t encoded_len) {
    uint8_t frame[PROTOCOL_MAX_FRAME];
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    uint8_t cmd = 0;
    uint8_t seq = 0;
    uint16_t payload_len = 0;

    size_t frame_len = cobs_decode(encoded_frame, encoded_len, frame);
    if (frame_len == 0) {
        send_error(0, ERR_INVALID_FRAME);
        TaskEvent_set(TASK_EVENT_CMD_PROCESSED);
        return;
    }

    if (!protocol_parse_frame(frame, frame_len, &cmd, &seq, payload, &payload_len)) {
        send_error(seq, ERR_INVALID_FRAME);
        TaskEvent_set(TASK_EVENT_CMD_PROCESSED);
        return;
    }

    handle_command(cmd, seq, payload, payload_len);
    TaskEvent_set(TASK_EVENT_CMD_PROCESSED);
}
