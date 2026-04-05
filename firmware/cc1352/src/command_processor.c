/*
 * FeralRF CC1352 - Command Processor (COBS + CRC16)
 *
 * Fase 0.0: Skeleton — only CMD_RADIO_INIT and CMD_GET_INFO
 */

#include "command_processor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "output_if.h"
#include "protocol.h"
#include "task_event.h"

/* Commands (match python/feralrf/enums.py) */
#define CMD_RADIO_INIT 0x01u
#define CMD_GET_INFO 0x05u
#define CMD_GET_STATS 0x06u

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

/* Firmware version */
#define FW_VERSION_MAJOR 0x02u
#define FW_VERSION_MINOR 0x00u
#define FW_VERSION_PATCH 0x00u
#define FW_CAPABILITY_RX_STATS 0x01u
#define FW_CAPABILITY_LL_PDU_META 0x02u
#define FW_CAPABILITY_LL_STATS_EXT 0x04u
#define FW_CAPABILITIES \
    (FW_CAPABILITY_RX_STATS | FW_CAPABILITY_LL_PDU_META | FW_CAPABILITY_LL_STATS_EXT)

static const uint8_t s_serial[8] = {'F', 'E', 'R', 'A', 'L', 'R', 'F', '2'};

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

    payload[0] = FW_VERSION_MAJOR;
    payload[1] = FW_VERSION_MINOR;
    payload[2] = FW_VERSION_PATCH;
    payload[3] = FW_CAPABILITIES;
    for (size_t i = 0; i < sizeof(s_serial); i++) {
        payload[4 + i] = s_serial[i];
    }
    send_response(RSP_INFO, seq, payload, sizeof(payload));
}

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void send_stats(uint8_t seq) {
    /* BLE RX test: report debug counters */
    extern volatile uint32_t g_ble_rx_callback_count;
    extern volatile uint32_t g_ble_rx_entry_done;
    extern volatile uint32_t g_ble_rx_cmd_status;
    extern volatile uint32_t g_rf_open_result;

    uint8_t payload[36];
    for (uint16_t i = 0; i < sizeof(payload); i++) {
        payload[i] = 0;
    }
    write_u32_le(&payload[0], g_ble_rx_entry_done);       /* rx_ok = packets received */
    write_u32_le(&payload[4], g_ble_rx_callback_count);    /* rx_crc_err = total callbacks */
    write_u32_le(&payload[8], g_ble_rx_cmd_status);        /* rx_drop = cmd status */
    write_u32_le(&payload[12], g_rf_open_result);          /* rx_overflow = RF_open result */
    send_response(RSP_STATS, seq, payload, sizeof(payload));
}

static void handle_command(uint8_t cmd, uint8_t seq, const uint8_t *payload __attribute__((unused)),
                           uint16_t payload_len) {
    switch (cmd) {
    case CMD_RADIO_INIT:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Fase 0.0: no radio to init, just ACK */
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

    default:
        send_error(seq, ERR_INVALID_CMD);
        return;
    }
}

void CommandProcessor_init(void) {
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
