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
#include "att_client.h"
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
#define AO_TARG 2000u /* 500us anchor offset (matches Sniffle) */

/* ── Static state ── */
static bool s_running;
static int s_last_status;
static uint32_t s_hop_interval_ticks;
static uint32_t s_superv_timeout_ticks;
static uint32_t s_next_hop_time;
static uint32_t s_last_rx_time;
static uint16_t s_event_counter;
static uint16_t s_dbg_l2cap_rx_count;
static uint16_t s_dbg_total_rx_count;
static uint32_t s_dbg_total_tx_done;

/* Debug timing ring buffer — populated each call to BleConnMgr_poll().
 * Cleared on BleConnMgr_start so each connect attempt sees a fresh log. */
static BleConnMgr_DbgTimingEntry s_dbg_timing[BLE_CONN_MGR_DBG_TIMING_DEPTH];
static uint8_t s_dbg_timing_head;  /* next write slot 0..DEPTH-1 */
static uint8_t s_dbg_timing_count; /* number of valid entries (saturates at DEPTH) */

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
        s_dbg_total_rx_count++;

        if (pkt.data_len < 2) {
            continue;
        }
        uint8_t llid = pkt.data[0] & 0x03u;
        uint8_t pdu_len = pkt.data[1];

        if (llid == 3 && pdu_len > 0) {
            /* LL Control PDU */
            handle_ll_ctrl(&pkt.data[2], pdu_len);
        } else if ((llid == 1 || llid == 2) && pdu_len > 0) {
            /* L2CAP data — route to ATT client */
            s_dbg_l2cap_rx_count++;
            AttClient_onL2capRx(&pkt.data[2], pdu_len);
        }
    }

    return got_data;
}

/* ── Public API ── */

void BleConnMgr_init(void) {
    s_running = false;
    s_event_counter = 0;
    TXQueue_init();
    AttClient_init();
}

uint16_t BleConnMgr_getL2capRxCount(void) {
    return s_dbg_l2cap_rx_count;
}

uint16_t BleConnMgr_getTotalRxCount(void) {
    return s_dbg_total_rx_count;
}

uint32_t BleConnMgr_getTotalTxDone(void) {
    return s_dbg_total_tx_done;
}

void BleConnMgr_start(void) {
    const BleConn_State *st = BleConn_getState();

    if (!st->connected) {
        return;
    }

    s_event_counter = 0;
    s_dbg_l2cap_rx_count = 0;
    s_dbg_total_rx_count = 0;
    s_dbg_total_tx_done = 0;
    s_dbg_timing_head = 0;
    s_dbg_timing_count = 0;

    s_hop_interval_ticks = CONN_INTERVAL_TO_TICKS(st->connInterval);
    s_superv_timeout_ticks = SUPERV_TO_TICKS(st->supervTimeout);

    /* Sniffle formula (RadioTask.c L467):
     * nextHopTime = connTime - AO_TARG + hopIntervalTicks
     * connTime = RAT time of CONNECT_IND TX from initiator output.
     * First MASTER starts at curHopTime = connTime, ends at nextHopTime. */
    s_next_hop_time = st->connTime - AO_TARG + s_hop_interval_ticks;
    s_last_rx_time = RF_getCurrentTime();

    if (st->useCsa2) {
        uint64_t map = 0;
        for (uint8_t i = 0; i < 5; i++) {
            map |= (uint64_t)st->channelMap[i] << (8 * i);
        }
        csa2_computeMapping(st->accessAddr, map);
    }

    TXQueue_init();
    RadioIF_bleResetSeqStat();
    RadioIF_bleResetRxQueue();

    /* Set running LAST so poll() sees fully initialized state */
    s_running = true;
}

void BleConnMgr_stop(void) {
    s_running = false;
    s_event_counter = 0;
    AttClient_reset();
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

    /* Sniffle formula (RadioTask.c L481):
     * curHopTime = nextHopTime - hopInterval + AO_TARG
     * MASTER startTime = curHopTime, endTime = nextHopTime */
    uint32_t curHopTime = s_next_hop_time - s_hop_interval_ticks + AO_TARG;

    /* Wait until anchor point (sleep until ~500us before) */
    uint32_t now = RF_getCurrentTime();
    uint32_t wait = curHopTime - now;
    if (wait < 0x80000000u && wait > 2000u) {
        Task_sleep(wait / 40u);
    }

    /* Check supervision timeout */
    now = RF_getCurrentTime();
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        BleConnMgr_stop();
        BleConn_disconnect();
        return false;
    }

    uint8_t chan = csa2_computeChannel(s_event_counter);

    /* Queue pending ATT requests before building TX queue */
    AttClient_poll();

    TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);
    dataQueue_t txq;
    TXQueue_take(&txq);

    uint32_t startTime = curHopTime;
    uint32_t endTime = s_next_hop_time;
    uint32_t numSent = 0;

    int status =
        RadioIF_bleCentral(chan, st->accessAddr, st->crcInit, &txq, startTime, endTime, &numSent);
    s_last_status = status;
    s_dbg_total_tx_done += numSent;

    /* Snapshot timing for host-side correlation (Session 3 telemetry). */
    {
        BleConnMgr_DbgTimingEntry *e = &s_dbg_timing[s_dbg_timing_head];
        e->eventIdx = s_event_counter;
        e->startRAT = startTime;
        e->endRAT = endTime;
        e->status = (uint16_t)status;
        e->numSent = (uint8_t)numSent;
        s_dbg_timing_head = (uint8_t)((s_dbg_timing_head + 1u) % BLE_CONN_MGR_DBG_TIMING_DEPTH);
        if (s_dbg_timing_count < BLE_CONN_MGR_DBG_TIMING_DEPTH) {
            s_dbg_timing_count++;
        }
    }

    TXQueue_flush(numSent);

    /* Drain RF data queue into software RX queue, then reset for next event.
     * RadioIF_poll() skips processing when s_rx_running=false (central mode).
     * MUST reset queue after drain — RF core's pCurrEntry advances past consumed
     * entries and won't re-check them even if we set status=PENDING. */
    RadioIF_bleDrainRxQueue();
    RadioIF_bleResetRxQueue();

    /* BLE_DONE_OK=0x1400, BLE_DONE_ENDED=0x1403, BLE_DONE_STOPPED=0x1404 */
    if (status == 0x1400 || status == 0x1403 || status == 0x1404) {
        s_last_rx_time = RF_getCurrentTime();
        process_rx_packets();
    }

    /* Advance to next anchor */
    s_event_counter++;
    s_next_hop_time += s_hop_interval_ticks;

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

uint16_t BleConnMgr_getEventCount(void) {
    return s_event_counter;
}

int BleConnMgr_getLastStatus(void) {
    return s_last_status;
}

uint8_t BleConnMgr_getDebugTiming(BleConnMgr_DbgTimingEntry *out, uint8_t maxEntries) {
    if (out == NULL || maxEntries == 0u) {
        return 0u;
    }
    uint8_t n = (s_dbg_timing_count < maxEntries) ? s_dbg_timing_count : maxEntries;

    /* Walk oldest-to-newest. With a saturating ring of DEPTH entries,
     * the oldest slot is (head - count) mod DEPTH. */
    uint8_t start =
        (uint8_t)((BLE_CONN_MGR_DBG_TIMING_DEPTH + s_dbg_timing_head - s_dbg_timing_count) %
                  BLE_CONN_MGR_DBG_TIMING_DEPTH);
    for (uint8_t i = 0; i < n; i++) {
        out[i] = s_dbg_timing[(start + i) % BLE_CONN_MGR_DBG_TIMING_DEPTH];
    }
    return n;
}
