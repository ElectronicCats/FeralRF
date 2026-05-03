/*
 * FeralRF CC1352 - Command Processor (COBS + CRC16)
 */

#include "command_processor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "att_client.h"
#include "ble_conn.h"
#include "ble_conn_mgr.h"
#include "control_task.h"
#include "crypto_engine.h"
#include "ll_follower.h"
#include "ll_manager.h"
#include "output_if.h"
#include "protocol.h"
#include "radio_if.h"
#include "task_event.h"

/* All command, response, and error code #defines moved to protocol.h
 * (F8f #10 — single source of truth for the wire protocol). The helper
 * functions read_u16_le, read_u32_le, parse_tx_len defined below stay. */

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

/* ── GATT ATT callbacks ── */
static uint8_t s_gatt_seq;

/* CMD_GATT_SUBSCRIBE defers its ACK until the ATT WRITE_RSP arrives.
 * Without this, two back-to-back CMD_GATT_SUBSCRIBE calls race: the second
 * arrives while AttClient is still in WAIT_WRITE_RSP from the first, and
 * AttClient_startWrite returns false → ERR_INVALID_STATE.
 * The host's gatt_subscribe() only awaits ACK, so the ACK must mean
 * "AttClient is idle again", not just "command queued". */
static bool s_gatt_subscribe_pending = false;

static void gatt_on_service(uint16_t startHandle, uint16_t endHandle, const uint8_t *uuid,
                            uint8_t uuidLen) {
    uint8_t rsp[4 + 16]; /* max: 4 handles + 16-byte UUID */
    rsp[0] = (uint8_t)(startHandle & 0xFF);
    rsp[1] = (uint8_t)(startHandle >> 8);
    rsp[2] = (uint8_t)(endHandle & 0xFF);
    rsp[3] = (uint8_t)(endHandle >> 8);
    if (uuidLen > 16)
        uuidLen = 16;
    for (uint8_t i = 0; i < uuidLen; i++)
        rsp[4 + i] = uuid[i];
    send_response(RSP_GATT_SERVICE, s_gatt_seq, rsp, 4u + uuidLen);
}

static void gatt_on_char(uint16_t handle, uint8_t properties, uint16_t valueHandle,
                         const uint8_t *uuid, uint8_t uuidLen) {
    uint8_t rsp[5 + 16];
    rsp[0] = (uint8_t)(handle & 0xFF);
    rsp[1] = (uint8_t)(handle >> 8);
    rsp[2] = properties;
    rsp[3] = (uint8_t)(valueHandle & 0xFF);
    rsp[4] = (uint8_t)(valueHandle >> 8);
    if (uuidLen > 16)
        uuidLen = 16;
    for (uint8_t i = 0; i < uuidLen; i++)
        rsp[5 + i] = uuid[i];
    send_response(RSP_GATT_CHAR, s_gatt_seq, rsp, 5u + uuidLen);
}

static void gatt_on_read(uint16_t handle, const uint8_t *data, uint8_t len) {
    uint8_t rsp[2 + 23];
    rsp[0] = (uint8_t)(handle & 0xFF);
    rsp[1] = (uint8_t)(handle >> 8);
    if (len > 23)
        len = 23;
    for (uint8_t i = 0; i < len; i++)
        rsp[2 + i] = data[i];
    send_response(RSP_GATT_READ_R, s_gatt_seq, rsp, 2u + len);
}

static void gatt_on_done(uint8_t status) {
    if (s_gatt_subscribe_pending) {
        /* Deferred ACK for CMD_GATT_SUBSCRIBE: ATT WRITE_RSP has arrived
         * (or AttClient reported error) and the state machine is back to
         * IDLE, so the next subscribe can proceed without racing. */
        s_gatt_subscribe_pending = false;
        if (status == 0u) {
            send_ack(s_gatt_seq);
        } else {
            send_error(s_gatt_seq, ERR_INVALID_STATE);
        }
        return;
    }
    uint8_t rsp[1] = {status};
    send_response(RSP_GATT_DONE, s_gatt_seq, rsp, 1);
}

static void gatt_on_mtu(uint16_t peerServerMtu) {
    uint8_t rsp[2];
    rsp[0] = (uint8_t)(peerServerMtu & 0xFF);
    rsp[1] = (uint8_t)(peerServerMtu >> 8);
    send_response(RSP_GATT_MTU, s_gatt_seq, rsp, sizeof(rsp));
}

static void gatt_on_attribute(uint16_t handle, const uint8_t *value, uint8_t valueLen) {
    /* Wire format: [handle:2LE][value:N], capped by ATT_DEFAULT_MTU - 2
     * (the 2 is the [handle:2] prefix; ATT_DEFAULT_MTU is the firmware
     * TX/RX cap, currently 23, so the value field can hold up to 21 bytes).
     * Bumps to ATT_DEFAULT_MTU automatically widen this. */
    uint8_t rsp[2 + (ATT_DEFAULT_MTU - 2u)];
    rsp[0] = (uint8_t)(handle & 0xFF);
    rsp[1] = (uint8_t)(handle >> 8);
    if (valueLen > (uint8_t)(ATT_DEFAULT_MTU - 2u)) {
        valueLen = (uint8_t)(ATT_DEFAULT_MTU - 2u);
    }
    for (uint8_t i = 0; i < valueLen; i++) {
        rsp[2 + i] = value[i];
    }
    send_response(RSP_GATT_ATTRIBUTE, s_gatt_seq, rsp, (uint16_t)(2u + valueLen));
}

static void gatt_on_disconnected(uint8_t reason) {
    /* Async — seq=0 since this is unsolicited. Host filters by RSP code. */
    uint8_t rsp[1] = {reason};
    OutputIF_sendResponse(RSP_DISCONNECTED, 0u, rsp, sizeof(rsp));
}

/* Set once on the first GATT-touching command. Both the AttClient
 * callbacks (.onService/.onChar/.onRead/.onDone/.onMtu/.onAttribute) and
 * the BleConnMgr disconnect callback are sticky once installed — no need
 * to re-register on subsequent connections. */
static bool gatt_callbacks_installed = false;

static void ensure_gatt_callbacks(void) {
    if (!gatt_callbacks_installed) {
        AttClient_Callbacks cb = {
            .onService = gatt_on_service,
            .onChar = gatt_on_char,
            .onRead = gatt_on_read,
            .onDone = gatt_on_done,
            .onMtu = gatt_on_mtu,
            .onAttribute = gatt_on_attribute,
        };
        AttClient_setCallbacks(&cb);
        BleConnMgr_setDisconnectCb(gatt_on_disconnected);
        gatt_callbacks_installed = true;
    }
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

/* ── F8b Track B follower callbacks ── */

static void follower_on_packet(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                               int8_t rssi_dbm, uint16_t event_counter, uint8_t direction) {
    /* Wire format: [direction:1][channel:1][rssi:1][event:2LE][ll_pdu:N] */
    /* F8f #1: cap to PROTOCOL_MAX_PAYLOAD - 5 (== 250) so the resulting
     * frame fits PROTOCOL_MAX_PAYLOAD. Legitimate BLE LL data PDUs without
     * DLE max at 37 bytes; with DLE max at 257 — but we never enable DLE
     * on the follower path. Anything larger here is malformed; drop it. */
    if (pdu_len > (uint8_t)(PROTOCOL_MAX_PAYLOAD - 5u)) {
        return;
    }
    uint8_t buf[5u + (PROTOCOL_MAX_PAYLOAD - 5u)];
    buf[0] = direction;
    buf[1] = channel;
    buf[2] = (uint8_t)rssi_dbm;
    buf[3] = (uint8_t)(event_counter & 0xFFu);
    buf[4] = (uint8_t)(event_counter >> 8);
    memcpy(&buf[5], ll_pdu, pdu_len);
    OutputIF_sendResponse(RSP_LL_PACKET, 0u, buf, (uint16_t)(5u + pdu_len));
}

static void follower_on_done(const LlFollower_DoneInfo *info) {
    /* Wire format: [reason:1][packets_captured:4LE] */
    uint8_t buf[5];
    buf[0] = info->reason;
    write_u32_le(&buf[1], info->packets_captured);
    OutputIF_sendResponse(RSP_FOLLOW_DONE, 0u, buf, sizeof(buf));
}

static bool follower_callbacks_installed = false;
static void ensure_follower_callbacks(void) {
    if (!follower_callbacks_installed) {
        LlFollower_Callbacks cb = {
            .onPacket = follower_on_packet,
            .onDone = follower_on_done,
        };
        LlFollower_setCallbacks(&cb);
        follower_callbacks_installed = true;
    }
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
        /* Payload: freq_hz(4) | mod_type(1) | symbol_rate(4) | deviation(2) | rx_bw(1) |
         * sync_word(4) | format_conf(2) = 18 bytes (16 accepted for backwards compat) */
        RadioIF_PropConfig prop_cfg;
        if (payload_len != 18u && payload_len != 16u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        prop_cfg.frequency_hz = read_u32_le(&payload[0]);
        prop_cfg.mod_type = payload[4];
        prop_cfg.symbol_rate = read_u32_le(&payload[5]);
        prop_cfg.deviation = read_u16_le(&payload[9]);
        prop_cfg.rx_bw = payload[11];
        prop_cfg.sync_word = read_u32_le(&payload[12]);
        prop_cfg.format_conf = (payload_len >= 18u) ? read_u16_le(&payload[16]) : 0u;
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

    case CMD_SET_BLE_SCAN_MODE:
        if (payload_len != 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        RadioIF_setActiveScan(payload[0] != 0u);
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

    case CMD_TX_CW:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!RadioIF_runTxTest(0u)) {
            send_error(seq, ERR_RF_NOT_READY);
            return;
        }
        send_ack(seq);
        return;

    case CMD_TX_PRBS:
        if (payload_len != 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (payload[0] != 1u && payload[0] != 2u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!RadioIF_runTxTest(payload[0])) {
            send_error(seq, ERR_RF_NOT_READY);
            return;
        }
        send_ack(seq);
        return;

    case CMD_TX_TEST_STOP:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        RadioIF_stopTxTest();
        send_ack(seq);
        return;

    case CMD_RANDOM: {
        if (payload_len != 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t n = payload[0];
        uint8_t buf[240];
        crypto_engine_status_t st = crypto_engine_random(n, buf);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_RANDOM, seq, buf, n);
        break;
    }

    case CMD_AES_ECB: {
        /* payload: op:1 | key:16 | data:16 = 33 B */
        if (payload_len != 33u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t out[16];
        crypto_engine_status_t st =
            crypto_engine_aes_ecb(payload[0], payload + 1, payload + 17, out);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_AES, seq, out, 16);
        break;
    }

    case CMD_AES_CCM: {
        /* payload: op:1 | key:16 | nonce_len:1 | nonce:N | aad_len:2_le | pt_len:2_le | tag_len:1 |
         * aad | data | (tag if decrypt) */
        if (payload_len < 24u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t op = payload[0];
        const uint8_t *key = payload + 1;
        uint8_t nonce_len = payload[17];
        if (nonce_len < 7u || nonce_len > 13u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        const uint8_t *nonce = payload + 18;
        size_t off = 18u + nonce_len;
        if (payload_len < off + 5u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint16_t aad_len = (uint16_t)payload[off] | ((uint16_t)payload[off + 1] << 8);
        uint16_t pt_len = (uint16_t)payload[off + 2] | ((uint16_t)payload[off + 3] << 8);
        uint8_t tag_len = payload[off + 4];
        off += 5u;
        if (payload_len <
            off + (size_t)aad_len + (size_t)pt_len + ((op == 1u) ? (size_t)tag_len : 0u)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        if (pt_len > 200u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        const uint8_t *aad = payload + off;
        const uint8_t *data = aad + aad_len;
        const uint8_t *tag_in = data + pt_len;

        uint8_t out[200];
        uint8_t tag_buf[16];
        if (op == 1u) {
            memcpy(tag_buf, tag_in, tag_len);
        }
        crypto_engine_status_t st = crypto_engine_aes_ccm(op, key, nonce, nonce_len, aad, aad_len,
                                                          data, pt_len, tag_len, out, tag_buf);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }

        if (op == 0u) {
            uint8_t resp[216];
            memcpy(resp, out, pt_len);
            memcpy(resp + pt_len, tag_buf, tag_len);
            send_response(RSP_AES_CCM, seq, resp, (uint16_t)(pt_len + tag_len));
        } else {
            send_response(RSP_AES_CCM, seq, out, pt_len);
        }
        break;
    }

    case CMD_AES_CTR: {
        /* payload: op:1 | key:16 | iv:16 | data:N */
        if (payload_len < 33u + 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        size_t data_len = payload_len - 33u;
        if (data_len > 200u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t out[200];
        crypto_engine_status_t st = crypto_engine_aes_ctr(payload[0], payload + 1, payload + 17,
                                                          payload + 33, data_len, out);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_AES, seq, out, (uint16_t)data_len);
        break;
    }

    case CMD_AES_CBC: {
        /* payload: op:1 | key:16 | iv:16 | data:N (multiple of 16) */
        if (payload_len < 33u + 16u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        size_t data_len = payload_len - 33u;
        if (data_len > 192u || (data_len % 16u) != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t out[192];
        crypto_engine_status_t st = crypto_engine_aes_cbc(payload[0], payload + 1, payload + 17,
                                                          payload + 33, data_len, out);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_AES, seq, out, (uint16_t)data_len);
        break;
    }

    case CMD_AES_GCM: {
        /* payload: op:1 | key:16 | iv:12 | aad_len:2_le | pt_len:2_le | aad | data | (tag if
         * decrypt) */
        if (payload_len < 33u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t op = payload[0];
        const uint8_t *key = payload + 1;
        const uint8_t *iv = payload + 17;
        uint16_t aad_len = (uint16_t)payload[29] | ((uint16_t)payload[30] << 8);
        uint16_t pt_len = (uint16_t)payload[31] | ((uint16_t)payload[32] << 8);
        if (pt_len > 200u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        size_t off = 33u;
        if (payload_len < off + (size_t)aad_len + (size_t)pt_len + ((op == 1u) ? 16u : 0u)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        const uint8_t *aad = payload + off;
        const uint8_t *data = aad + aad_len;
        const uint8_t *tag_in = data + pt_len;

        uint8_t out[200];
        uint8_t tag_buf[16];
        if (op == 1u)
            memcpy(tag_buf, tag_in, 16);

        crypto_engine_status_t st =
            crypto_engine_aes_gcm(op, key, iv, aad, aad_len, data, pt_len, out, tag_buf);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }

        if (op == 0u) {
            uint8_t resp[216];
            memcpy(resp, out, pt_len);
            memcpy(resp + pt_len, tag_buf, 16);
            send_response(RSP_AES_GCM, seq, resp, (uint16_t)(pt_len + 16u));
        } else {
            send_response(RSP_AES_GCM, seq, out, pt_len);
        }
        break;
    }

    case CMD_SHA256: {
        if (payload_len > 240u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t digest[32];
        crypto_engine_status_t st = crypto_engine_sha256(payload, payload_len, digest);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_SHA256, seq, digest, 32);
        break;
    }

    case CMD_ECDH: {
        /* payload: curve:1 | priv:32 | peer_pub:32 or 64 */
        if (payload_len < 1u + 32u + 32u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t curve = payload[0];
        size_t pub_len = payload_len - 33u;
        if ((curve == 0u && pub_len != 64u) || (curve == 1u && pub_len != 32u)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        if (curve > 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t shared[32];
        crypto_engine_status_t st =
            crypto_engine_ecdh((crypto_curve_t)curve, payload + 1, payload + 33, pub_len, shared);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_ECDH, seq, shared, 32);
        break;
    }

    case CMD_ECDSA_SIGN: {
        /* payload: curve:1 | priv:32 | hash:32 = 65 B */
        if (payload_len != 65u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t sig[64];
        crypto_engine_status_t st =
            crypto_engine_ecdsa_sign((crypto_curve_t)payload[0], payload + 1, payload + 33, sig);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        send_response(RSP_ECDSA_SIG, seq, sig, 64);
        break;
    }

    case CMD_ECDSA_VERIFY: {
        /* payload: curve:1 | pub:32 or 64 | hash:32 | sig:64 */
        if (payload_len < 1u + 32u + 32u + 64u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        uint8_t curve = payload[0];
        size_t pub_len = payload_len - 1u - 32u - 64u;
        if ((curve == 0u && pub_len != 64u) || (curve == 1u && pub_len != 32u)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        if (curve > 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            break;
        }
        bool valid = false;
        crypto_engine_status_t st = crypto_engine_ecdsa_verify(
            (crypto_curve_t)curve, payload + 1, pub_len, payload + 1u + pub_len,
            payload + 1u + pub_len + 32u, &valid);
        if (st != CRYPTO_OK) {
            send_error(seq, (uint8_t)st);
            break;
        }
        uint8_t result = valid ? 1u : 0u;
        send_response(RSP_ECDSA_VERIFY, seq, &result, 1);
        break;
    }

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

    case CMD_CONNECT: {
        /* Payload: addr[6] + addr_type(1) = 7 bytes */
        if (payload_len != 7u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (BleConn_isConnected() || BleConn_isInitiating()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        /* F8f #3: install disconnect callback BEFORE initiating so that a
         * peer-immediate-DC after BleConn_initiate() but before any GATT
         * command still emits RSP_DISCONNECTED to the host. Idempotent
         * (ensure_gatt_callbacks() gates on gatt_callbacks_installed). */
        ensure_gatt_callbacks();
        /* Default: interval=24 (30ms), timeout=100 (1000ms) */
        BleConn_Result res = BleConn_initiate(payload, payload[6], 24u, 100u);
        uint8_t rsp[1] = {(uint8_t)res};
        send_response(RSP_CONN_RESULT, seq, rsp, sizeof(rsp));
        return;
    }

    case CMD_DISCONNECT:
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ensure_gatt_callbacks();
        /* Send the ACK FIRST. The cooperative disconnect flow below
         * (BleConnMgr_initiateGracefulDisconnect + BleConnMgr_poll
         * teardown) eventually fires the async RSP_DISCONNECTED frame
         * on the wire. Python's ble_disconnect waits for ACK with
         * _read_response and would discard an out-of-order async event;
         * emitting ACK first preserves cause-and-effect ordering. */
        send_ack(seq);
        /* F8d: queue LL_TERMINATE_IND with reason 0x16
         * (LOCAL_HOST_TERMINATED) and let BleConnMgr_poll complete the
         * teardown cooperatively. Replaces the earlier
         * BleConnMgr_stopWithReason(0x16u) + BleConn_disconnect() pair,
         * which (a) immediately stopped the manager so the LL_TERMINATE
         * was never TX'd to the peer, and (b) called Task_sleep() inside
         * BleConn_disconnect from the same RF task that would TX it. */
        BleConnMgr_initiateGracefulDisconnect(0x16u);
        return;

    case CMD_CONN_STATUS: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        const BleConn_State *st = BleConn_getState();
        uint16_t evts = BleConnMgr_getEventCount();
        int last_st = BleConnMgr_getLastStatus();
        uint16_t l2cap_rx = BleConnMgr_getL2capRxCount();
        uint8_t att_state = (uint8_t)AttClient_getState();
        uint16_t total_rx = BleConnMgr_getTotalRxCount();
        uint8_t rsp[18];
        rsp[0] = st->connected ? 1u : 0u;
        rsp[1] = (uint8_t)(st->connInterval & 0xFFu);
        rsp[2] = (uint8_t)((st->connInterval >> 8) & 0xFFu);
        rsp[3] = (uint8_t)(st->supervTimeout & 0xFFu);
        rsp[4] = (uint8_t)((st->supervTimeout >> 8) & 0xFFu);
        rsp[5] = (uint8_t)(evts & 0xFFu);
        rsp[6] = (uint8_t)((evts >> 8) & 0xFFu);
        rsp[7] = (uint8_t)(last_st & 0xFFu);
        rsp[8] = (uint8_t)((last_st >> 8) & 0xFFu);
        uint32_t tx_done = 0;
        {
            extern uint32_t BleConnMgr_getTotalTxDone(void);
            tx_done = BleConnMgr_getTotalTxDone();
        }
        rsp[9] = (uint8_t)(tx_done & 0xFFu);
        rsp[10] = (uint8_t)((tx_done >> 8) & 0xFFu);
        rsp[11] = att_state;
        rsp[12] = (uint8_t)(total_rx & 0xFFu);
        rsp[13] = (uint8_t)((total_rx >> 8) & 0xFFu);
        /* connTime: RAT-tick origin of the connection anchor. Set by
         * BleConn_initiate from CMD_BLE5_INITIATOR's connectTime field.
         * Session 2 telemetry will correlate this with first MASTER event
         * timing to diagnose any residual NOSYNC. */
        rsp[14] = (uint8_t)(st->connTime & 0xFFu);
        rsp[15] = (uint8_t)((st->connTime >> 8) & 0xFFu);
        rsp[16] = (uint8_t)((st->connTime >> 16) & 0xFFu);
        rsp[17] = (uint8_t)((st->connTime >> 24) & 0xFFu);
        send_response(RSP_CONN_STATUS_R, seq, rsp, sizeof(rsp));
        return;
    }

    case CMD_GATT_DISCOVER: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        if (!AttClient_startDiscover()) {
            /* F8f #4: do NOT touch s_gatt_seq before the start guard; if the
             * AttClient is busy with a prior transaction, overwriting seq
             * here corrupts the in-flight completion's reported seq. */
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        s_gatt_seq = seq;
        send_ack(seq);
        return;
    }

    case CMD_GATT_READ: {
        /* Payload: handle[2] */
        if (payload_len != 2u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        uint16_t handle = read_u16_le(payload);
        if (!AttClient_startRead(handle)) {
            /* F8f #4 — see CMD_GATT_DISCOVER comment */
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        s_gatt_seq = seq;
        send_ack(seq);
        return;
    }

    case CMD_GATT_WRITE: {
        /* Payload: handle[2] + data[N] */
        if (payload_len < 3u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        uint16_t handle = read_u16_le(payload);
        if (!AttClient_startWrite(handle, &payload[2], (uint8_t)(payload_len - 2u))) {
            /* F8f #4 — see CMD_GATT_DISCOVER comment */
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        s_gatt_seq = seq;
        send_ack(seq);
        return;
    }

    case CMD_GATT_SUBSCRIBE: {
        /* Payload: handle_le[2] + enable[1] + indicate[1] = 4 bytes */
        if (payload_len != 4u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            /* Clear any stale pending flag from a connection that dropped
             * mid-subscribe — otherwise the first subscribe of the next
             * connection would falsely report ERR_INVALID_STATE. */
            s_gatt_subscribe_pending = false;
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        /* If a previous subscribe is still in flight (WRITE_RSP not yet
         * received), reject so the host gets a clean error rather than
         * silently mixing acks. */
        if (s_gatt_subscribe_pending) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        uint16_t handle = read_u16_le(payload);
        bool enable = (payload[2] != 0u);
        bool indicate = (payload[3] != 0u);
        uint16_t ccc_value = enable ? (indicate ? 0x0002u : 0x0001u) : 0x0000u;
        uint8_t ccc_bytes[2] = {(uint8_t)(ccc_value & 0xFFu), (uint8_t)(ccc_value >> 8)};
        /* CCC handle convention: char value handle + 1.
         * If a peripheral lays out descriptors differently, the host must call
         * gatt_write directly to the correct CCC handle. */
        if (!AttClient_startWrite((uint16_t)(handle + 1u), ccc_bytes, 2u)) {
            /* F8f #4 — see CMD_GATT_DISCOVER comment */
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        s_gatt_seq = seq;
        /* ACK is deferred to gatt_on_done(): once ATT WRITE_RSP arrives and
         * AttClient is back to IDLE, the host can issue the next subscribe
         * without racing the state machine. */
        s_gatt_subscribe_pending = true;
        return;
    }

    case CMD_GATT_EXCHANGE_MTU: {
        /* Payload: client_mtu[2LE]. */
        if (payload_len != 2u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        uint16_t client_mtu = read_u16_le(payload);
        if (!AttClient_startMtuExchange(client_mtu)) {
            /* F8f #4 — see CMD_GATT_DISCOVER comment */
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        s_gatt_seq = seq;
        send_ack(seq);
        return;
    }

    case CMD_GATT_READ_BY_UUID: {
        /* Payload: start[2LE] + end[2LE] + uuid16[2LE] = 6 bytes. */
        if (payload_len != 6u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        uint16_t start = read_u16_le(&payload[0]);
        uint16_t end = read_u16_le(&payload[2]);
        uint16_t uuid = read_u16_le(&payload[4]);
        if (!AttClient_startReadByUuid(start, end, uuid)) {
            /* F8f #4 — see CMD_GATT_DISCOVER comment */
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        s_gatt_seq = seq;
        send_ack(seq);
        return;
    }

    case CMD_DEBUG_TIMING: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Wire layout: count(u8) + count × 18 bytes per entry, where the
         * 18 bytes are: eventIdx(u16) startRAT(u32) endRAT(u32) status(u16)
         * numSent(u8) nTx(u8) nRxOk(u8) nRxNok(u8) nRxIgnored(u8) pktStatus(u8).
         * 1 + 14*18 = 253 bytes max (≤ PROTOCOL_MAX_PAYLOAD = 255). */
        uint8_t rsp[1u + BLE_CONN_MGR_DBG_TIMING_DEPTH * 18u];
        BleConnMgr_DbgTimingEntry entries[BLE_CONN_MGR_DBG_TIMING_DEPTH];
        uint8_t n = BleConnMgr_getDebugTiming(entries, BLE_CONN_MGR_DBG_TIMING_DEPTH);
        rsp[0] = n;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t *p = &rsp[1u + (uint16_t)i * 18u];
            p[0] = (uint8_t)(entries[i].eventIdx & 0xFFu);
            p[1] = (uint8_t)(entries[i].eventIdx >> 8);
            p[2] = (uint8_t)(entries[i].startRAT & 0xFFu);
            p[3] = (uint8_t)((entries[i].startRAT >> 8) & 0xFFu);
            p[4] = (uint8_t)((entries[i].startRAT >> 16) & 0xFFu);
            p[5] = (uint8_t)((entries[i].startRAT >> 24) & 0xFFu);
            p[6] = (uint8_t)(entries[i].endRAT & 0xFFu);
            p[7] = (uint8_t)((entries[i].endRAT >> 8) & 0xFFu);
            p[8] = (uint8_t)((entries[i].endRAT >> 16) & 0xFFu);
            p[9] = (uint8_t)((entries[i].endRAT >> 24) & 0xFFu);
            p[10] = (uint8_t)(entries[i].status & 0xFFu);
            p[11] = (uint8_t)((entries[i].status >> 8) & 0xFFu);
            p[12] = entries[i].numSent;
            p[13] = entries[i].nTx;
            p[14] = entries[i].nRxOk;
            p[15] = entries[i].nRxNok;
            p[16] = entries[i].nRxIgnored;
            p[17] = entries[i].pktStatus;
        }
        send_response(RSP_DEBUG_TIMING, seq, rsp, (uint16_t)(1u + (uint16_t)n * 18u));
        return;
    }

    case CMD_DEBUG_CONN_PARAMS: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Wire layout (50 bytes total):
         *   accessAddr      u32 LE   (4)   ← s_state, post-initiator re-snapshot
         *   crcInit         u32 LE   (4)   ← s_state, post-initiator re-snapshot
         *   channelMap[5]            (5)   ← s_state, post-initiator re-snapshot
         *   hopIncrement    u8       (1)   ← s_state, post-initiator re-snapshot
         *   winOffset       u16 LE   (2)   ← s_state, post-initiator re-snapshot
         *   eventCounter    u16 LE   (2)   ← s_state, current
         *   connTime        u32 LE   (4)   ← s_state, RAT @ end-of-CONNECT_IND
         *   connInterval    u16 LE   (2)
         *   supervTimeout   u16 LE   (2)
         *   useCsa2         u8       (1)
         *   connected       u8       (1)
         *   ll_data[22]              (22)  ← raw bytes left in s_ll_data after
         *                                    initiator returned. The single
         *                                    most diagnostic field — bytes
         *                                    here ARE what the slave decoded
         *                                    (modulo SDK rewrites we know
         *                                    about: WinOffset, WinSize). */
        const BleConn_State *st = BleConn_getState();
        const uint8_t *ll = BleConn_getLlData();
        uint8_t rsp[50];
        rsp[0] = (uint8_t)(st->accessAddr & 0xFFu);
        rsp[1] = (uint8_t)((st->accessAddr >> 8) & 0xFFu);
        rsp[2] = (uint8_t)((st->accessAddr >> 16) & 0xFFu);
        rsp[3] = (uint8_t)((st->accessAddr >> 24) & 0xFFu);
        rsp[4] = (uint8_t)(st->crcInit & 0xFFu);
        rsp[5] = (uint8_t)((st->crcInit >> 8) & 0xFFu);
        rsp[6] = (uint8_t)((st->crcInit >> 16) & 0xFFu);
        rsp[7] = (uint8_t)((st->crcInit >> 24) & 0xFFu);
        memcpy(&rsp[8], st->channelMap, 5);
        rsp[13] = st->hopIncrement;
        rsp[14] = (uint8_t)(st->winOffset & 0xFFu);
        rsp[15] = (uint8_t)((st->winOffset >> 8) & 0xFFu);
        rsp[16] = (uint8_t)(st->eventCounter & 0xFFu);
        rsp[17] = (uint8_t)((st->eventCounter >> 8) & 0xFFu);
        rsp[18] = (uint8_t)(st->connTime & 0xFFu);
        rsp[19] = (uint8_t)((st->connTime >> 8) & 0xFFu);
        rsp[20] = (uint8_t)((st->connTime >> 16) & 0xFFu);
        rsp[21] = (uint8_t)((st->connTime >> 24) & 0xFFu);
        rsp[22] = (uint8_t)(st->connInterval & 0xFFu);
        rsp[23] = (uint8_t)((st->connInterval >> 8) & 0xFFu);
        rsp[24] = (uint8_t)(st->supervTimeout & 0xFFu);
        rsp[25] = (uint8_t)((st->supervTimeout >> 8) & 0xFFu);
        rsp[26] = st->useCsa2 ? 1u : 0u;
        rsp[27] = st->connected ? 1u : 0u;
        memcpy(&rsp[28], ll, 22);
        send_response(RSP_DEBUG_CONN_PARAMS, seq, rsp, 50u);
        return;
    }

    case CMD_ATT_DEBUG: {
        /* F8b Track A Task 0.2: dump AttClient state-machine event ring buffer.
         * Wire layout: count(u8) + count × 8 bytes per entry, where the 8
         * bytes are: seq(u16) tag(u8) oldState(u8) newState(u8) connAlive(u8)
         * mtu(u8) reqPending(u8). 1 + 32*8 = 257 — but we cap at 31 entries
         * to fit in PROTOCOL_MAX_PAYLOAD=255. */
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        AttClient_DbgEntry entries[ATT_DBG_LOG_DEPTH];
        uint8_t cap = (uint8_t)(ATT_DBG_LOG_DEPTH > 31u ? 31u : ATT_DBG_LOG_DEPTH);
        uint8_t n = AttClient_getDebugLog(entries, cap);
        uint8_t rsp[1u + 31u * 8u];
        rsp[0] = n;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t *p = &rsp[1u + (uint16_t)i * 8u];
            p[0] = (uint8_t)(entries[i].seq & 0xFFu);
            p[1] = (uint8_t)((entries[i].seq >> 8) & 0xFFu);
            p[2] = entries[i].tag;
            p[3] = entries[i].oldState;
            p[4] = entries[i].newState;
            p[5] = entries[i].connAlive;
            p[6] = entries[i].mtu;
            p[7] = entries[i].reqPending;
        }
        send_response(RSP_ATT_DEBUG, seq, rsp, (uint16_t)(1u + (uint16_t)n * 8u));
        return;
    }

    case CMD_FOLLOW_START: {
        if (payload_len != LL_FOLLOWER_MAC_LEN) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ensure_follower_callbacks();
        if (!LlFollower_start(payload)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;
    }

    case CMD_FOLLOW_STOP: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!LlFollower_stop()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;
    }

    case CMD_FOLLOW_DEBUG: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        LlFollower_DebugSnapshot snap;
        LlFollower_getDebug(&snap);
        uint8_t buf[42];
        buf[0] = snap.state;
        buf[1] = snap.scan_channel;
        buf[2] = (uint8_t)(snap.adv_call_count & 0xFFu);
        buf[3] = (uint8_t)(snap.adv_call_count >> 8);
        buf[4] = (uint8_t)(snap.data_call_count & 0xFFu);
        buf[5] = (uint8_t)(snap.data_call_count >> 8);
        buf[6] = (uint8_t)(snap.adv_packets_seen & 0xFFu);
        buf[7] = (uint8_t)(snap.adv_packets_seen >> 8);
        buf[8] = (uint8_t)(snap.connect_inds_seen & 0xFFu);
        buf[9] = (uint8_t)(snap.connect_inds_seen >> 8);
        buf[10] = (uint8_t)(snap.data_packets_seen & 0xFFu);
        buf[11] = (uint8_t)(snap.data_packets_seen >> 8);
        buf[12] = (uint8_t)(snap.last_adv_cmd_status & 0xFFu);
        buf[13] = (uint8_t)((uint16_t)snap.last_adv_cmd_status >> 8);
        buf[14] = (uint8_t)(snap.last_data_cmd_status & 0xFFu);
        buf[15] = (uint8_t)((uint16_t)snap.last_data_cmd_status >> 8);
        write_u32_le(&buf[16], snap.last_adv_event_mask_lo);
        write_u32_le(&buf[20], snap.packets_captured);
        /* PDU type histogram — 9 × uint16 LE = 18 bytes */
        struct {
            uint16_t v;
        } *p;
        (void)p;
        uint16_t hist[9] = {snap.pt_adv_ind,      snap.pt_adv_direct, snap.pt_adv_nonconn,
                            snap.pt_scan_req,     snap.pt_scan_rsp,   snap.pt_connect_ind,
                            snap.pt_adv_scan_ind, snap.pt_ext_adv,    snap.pt_other};
        for (uint8_t i = 0u; i < 9u; i++) {
            buf[24 + i * 2] = (uint8_t)(hist[i] & 0xFFu);
            buf[24 + i * 2 + 1] = (uint8_t)(hist[i] >> 8);
        }
        send_response(RSP_FOLLOW_DEBUG, seq, buf, sizeof(buf));
        return;
    }

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
