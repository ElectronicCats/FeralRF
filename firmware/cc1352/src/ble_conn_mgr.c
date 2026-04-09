/*
 * FeralRF CC1352 - BLE Connection Manager (Central Mode)
 *
 * Maintains a BLE connection by running one connection event per poll() call.
 * After Phase 1's CMD_BLE5_INITIATOR establishes the link, this module
 * computes the data channel (CSA#2 or legacy hop), prepares the TX queue,
 * runs CMD_BLE5_MASTER, and processes RX packets.
 *
 * Reference: Sniffle RadioTask.c CENTRAL state (L471-530)
 */

#include "ble_conn_mgr.h"
#include "ble_conn.h"
#include "csa2.h"
#include "radio_if.h"
#include "tx_queue.h"

#include <ti/drivers/rf/RF.h>
#include <ti/sysbios/knl/Task.h>

/* ── LL Control PDU opcodes (Core Spec Vol 6, Part B, 2.4.2) ── */
#define LL_CONNECTION_UPDATE_IND 0x00u
#define LL_CHANNEL_MAP_IND 0x01u
#define LL_TERMINATE_IND 0x02u
#define LL_UNKNOWN_RSP 0x07u
#define LL_FEATURE_REQ 0x08u
#define LL_FEATURE_RSP 0x09u
#define LL_VERSION_IND 0x0Cu

/* Version exchange constants */
#define BLE_VERSION_5_0 0x09u
#define COMPANY_ID_TI 0x000Du
#define SUBVERSION_FERALRF 0x0001u

/* RAT clock: 4 MHz ticks */
#define CONN_INTERVAL_TO_TICKS(x) ((uint32_t)(x) * 5000u) /* 1.25ms * 4MHz */
#define SUPERV_TO_TICKS(x) ((uint32_t)(x) * 40000u)       /* 10ms * 4MHz */
#define TRANSMIT_WINDOW_DELAY 5000u                       /* 1.25ms in RAT ticks */
#define ANCHOR_OFFSET 800u                                /* 200us guard before anchor */

/* ── Static state ── */
static bool s_running;
static uint32_t s_hop_interval_ticks;
static uint32_t s_superv_timeout_ticks;
static uint32_t s_next_hop_time;
static uint32_t s_last_rx_time;
static uint16_t s_event_counter;

/* ── LL Control PDU handling ── */

static void handle_ll_ctrl(const uint8_t *payload, uint8_t len) {
    if (len < 1) {
        return;
    }
    uint8_t opcode = payload[0];

    switch (opcode) {
    case LL_TERMINATE_IND:
        BleConnMgr_stop();
        BleConn_disconnect();
        break;

    case LL_FEATURE_REQ: {
        /* Respond with empty feature set — we don't support any optional LL features */
        uint8_t rsp[9];
        rsp[0] = LL_FEATURE_RSP;
        for (uint8_t i = 1; i < 9; i++) {
            rsp[i] = 0;
        }
        TXQueue_insert(9, TX_QUEUE_LLID_CTRL, rsp);
        break;
    }

    case LL_VERSION_IND: {
        uint8_t rsp[6];
        rsp[0] = LL_VERSION_IND;
        rsp[1] = BLE_VERSION_5_0;
        rsp[2] = (uint8_t)(COMPANY_ID_TI & 0xFF);
        rsp[3] = (uint8_t)(COMPANY_ID_TI >> 8);
        rsp[4] = (uint8_t)(SUBVERSION_FERALRF & 0xFF);
        rsp[5] = (uint8_t)(SUBVERSION_FERALRF >> 8);
        TXQueue_insert(6, TX_QUEUE_LLID_CTRL, rsp);
        break;
    }

    case LL_CHANNEL_MAP_IND: {
        /* payload: [opcode][chM 5 bytes][instant 2 bytes] */
        if (len >= 8) {
            uint64_t new_map = 0;
            for (uint8_t i = 0; i < 5; i++) {
                new_map |= (uint64_t)payload[1 + i] << (8 * i);
            }
            const BleConn_State *st = BleConn_getState();
            csa2_computeMapping(st->accessAddr, new_map);
        }
        break;
    }

    default: {
        /* Unknown opcode — respond with LL_UNKNOWN_RSP per spec */
        uint8_t rsp[2];
        rsp[0] = LL_UNKNOWN_RSP;
        rsp[1] = opcode;
        TXQueue_insert(2, TX_QUEUE_LLID_CTRL, rsp);
        break;
    }
    }
}

/* ── Process RX packets from connection event ── */

static bool process_rx_packets(void) {
    bool got_data = false;
    RadioIF_RxPacket pkt;

    while (RadioIF_popRxPacket(&pkt)) {
        got_data = true;

        if (pkt.data_len < 2) {
            continue;
        }
        uint8_t llid = pkt.data[0] & 0x03u;
        uint8_t pdu_len = pkt.data[1];

        if (llid == 3 && pdu_len > 0) {
            /* LL Control PDU */
            handle_ll_ctrl(&pkt.data[2], pdu_len);
        }
        /* llid 1 or 2 = L2CAP data — Phase 3 will handle */
    }

    return got_data;
}

/* ── Public API ── */

void BleConnMgr_init(void) {
    s_running = false;
    s_event_counter = 0;
    TXQueue_init();
}

void BleConnMgr_start(void) {
    const BleConn_State *st = BleConn_getState();

    if (!st->connected) {
        return;
    }

    s_running = true;
    s_event_counter = 0;

    s_hop_interval_ticks = CONN_INTERVAL_TO_TICKS(st->connInterval);
    s_superv_timeout_ticks = SUPERV_TO_TICKS(st->supervTimeout);

    /* First anchor point: connTime + transmitWindowDelay + one interval */
    s_next_hop_time = st->connTime + TRANSMIT_WINDOW_DELAY + s_hop_interval_ticks;
    s_last_rx_time = RF_getCurrentTime();

    /* Initialize CSA#2 channel mapping if negotiated */
    if (st->useCsa2) {
        uint64_t map = 0;
        for (uint8_t i = 0; i < 5; i++) {
            map |= (uint64_t)st->channelMap[i] << (8 * i);
        }
        csa2_computeMapping(st->accessAddr, map);
    }

    TXQueue_init();
    RadioIF_bleResetSeqStat();

    /* First TX: empty PDU keepalive */
    TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);
}

void BleConnMgr_stop(void) {
    s_running = false;
    s_event_counter = 0;
}

bool BleConnMgr_poll(void) {
    if (!s_running) {
        return false;
    }

    const BleConn_State *st = BleConn_getState();
    if (!st->connected) {
        BleConnMgr_stop();
        return false;
    }

    /* Check if it's time for the next connection event */
    uint32_t now = RF_getCurrentTime();
    uint32_t remaining = s_next_hop_time - ANCHOR_OFFSET - now;
    if (remaining < 0x80000000u && remaining > 2000u) {
        /* Not yet time — yield to RTOS scheduler.
         * RAT runs at 4MHz, Task_sleep uses BIOS ticks (~10us each). */
        Task_sleep(remaining / 40u);
        return false;
    }

    /* Check supervision timeout */
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        BleConnMgr_stop();
        BleConn_disconnect();
        return false;
    }

    /* Compute data channel for this event */
    uint8_t chan;
    if (st->useCsa2) {
        chan = csa2_computeChannel(s_event_counter);
    } else {
        chan = (st->hopIncrement * s_event_counter) % 37;
    }

    /* Prepare TX queue — ensure at least one entry (keepalive) */
    dataQueue_t txq;
    TXQueue_take(&txq);

    if (txq.pCurrEntry == NULL) {
        TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);
        TXQueue_take(&txq);
    }

    /* Run CMD_BLE5_MASTER for this connection event */
    uint32_t endTime = s_next_hop_time + s_hop_interval_ticks - ANCHOR_OFFSET;
    uint32_t numSent = 0;

    int status = RadioIF_bleCentral(chan, st->accessAddr, st->crcInit, &txq,
                                    s_next_hop_time - ANCHOR_OFFSET, endTime, &numSent);

    TXQueue_flush(numSent);

    /* Process received data */
    bool got_data = process_rx_packets();

    if (got_data || status == 0) {
        s_last_rx_time = RF_getCurrentTime();
    }

    /* Advance to next connection event */
    s_event_counter++;
    s_next_hop_time += s_hop_interval_ticks;

    /* Queue keepalive for next event */
    TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);

    return true;
}

bool BleConnMgr_isRunning(void) {
    return s_running;
}

bool BleConnMgr_queueTx(uint8_t llid, const uint8_t *data, uint8_t len) {
    if (!s_running) {
        return false;
    }
    return TXQueue_insert(len, llid, data);
}
