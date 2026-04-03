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
#define CMD_TX_CONTINUOUS 0x21u
#define CMD_TX_BURST 0x22u
#define CMD_TX_FRAME 0x23u
#define CMD_TX_STOP 0x24u
#define CMD_SET_ADV_HOP 0x07u
#define CMD_SET_PROP_CONFIG 0x08u
#define CMD_SET_BLE_ADDR 0x09u
#define CMD_JAM_CONTINUOUS 0x30u
#define CMD_JAM_STOP 0x33u

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
#define ERR_RF_INIT_FAILED 0x06u

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool parse_tx_len(const uint8_t *payload, uint16_t payload_len, uint16_t suffix_len,
                         uint8_t *tx_len_out) {
    uint8_t tx_len = 0u;
    uint16_t expected_payload_len = 0u;

    if (payload == NULL || tx_len_out == NULL) {
        return false;
    }
    if (payload_len < (uint16_t)(1u + suffix_len)) {
        return false;
    }

    tx_len = payload[0];
    expected_payload_len = (uint16_t)tx_len + 1u + suffix_len;
    if (tx_len == 0u || expected_payload_len != payload_len) {
        return false;
    }

    *tx_len_out = tx_len;
    return true;
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

    case CMD_SET_ADV_HOP:
        if (payload_len != 1) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        RadioIF_setAdvHopEnabled(payload[0] != 0u);
        send_ack(seq);
        return;

    case CMD_SET_PROP_CONFIG: {
        /* Payload: freq_hz(4) | mod_type(1) | symbol_rate(4) | deviation(2) | rx_bw(1) | sync_word(4) = 16 bytes */
        RadioIF_PropConfig prop_cfg;
        if (payload_len != 16u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        prop_cfg.frequency_hz = read_u32_le(&payload[0]);
        prop_cfg.mod_type = payload[4];
        prop_cfg.symbol_rate = read_u32_le(&payload[5]);
        prop_cfg.deviation = read_u16_le(&payload[9]);
        prop_cfg.rx_bw = payload[11];
        prop_cfg.sync_word = read_u32_le(&payload[12]);
        RadioIF_setPropConfig(&prop_cfg);
        send_ack(seq);
        return;
    }

    case CMD_SET_BLE_ADDR:
        if (payload_len != 6u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        RadioIF_setBleAdvAddress(payload);
        send_ack(seq);
        return;

    case CMD_RX_START:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (ControlTask_isTxBusy()) {
            send_error(seq, ERR_INVALID_STATE);
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

    case CMD_TX_RAW: {
        uint8_t tx_len = 0u;
        if (!parse_tx_len(payload, payload_len, 1u, &tx_len)) {
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

    case CMD_TX_BURST: {
        uint8_t tx_len = 0u;
        uint16_t tx_count = 0u;
        uint32_t interval_us = 0u;

        if (!parse_tx_len(payload, payload_len, 6u, &tx_len)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }

        tx_count = read_u16_le(&payload[1u + tx_len]);
        interval_us = read_u32_le(&payload[1u + tx_len + 2u]);
        if (!ControlTask_onTxBurst(&payload[1], tx_len, tx_count, interval_us)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
    }
        send_ack(seq);
        return;

    case CMD_TX_FRAME: {
        uint8_t tx_len = 0u;
        if (!parse_tx_len(payload, payload_len, 0u, &tx_len)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }

        if (!ControlTask_onTxFrame(&payload[1], tx_len)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
    }
        send_ack(seq);
        return;

    case CMD_TX_CONTINUOUS: {
        uint8_t tx_len = 0u;
        uint32_t interval_us = 0u;

        if (!parse_tx_len(payload, payload_len, 4u, &tx_len)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }

        interval_us = read_u32_le(&payload[1u + tx_len]);
        if (!ControlTask_onTxContinuous(&payload[1], tx_len, interval_us)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
    }
        send_ack(seq);
        return;

    case CMD_TX_STOP:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onTxStop();
        send_ack(seq);
        return;

    case CMD_JAM_CONTINUOUS:
        if (payload_len != 4u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!ControlTask_onJamContinuous(payload[0], (int8_t)payload[1],
                                         read_u16_le(&payload[2]))) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;

    case CMD_JAM_STOP:
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ControlTask_onJamStop();
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
