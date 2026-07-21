/*
 * FeralRF CC1352 - Command Processor (COBS + CRC16)
 */

#include "command_processor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "control_task.h"
#include "crypto_engine.h"
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
