/*
 * FeralRF CC1352 - BLE Connection Manager (Central + Peripheral Mode)
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
#include "att_server.h"
#include "ble_conn.h"
#include "csa2.h"
#include "radio_if.h"
#include "tx_queue.h"

#include <string.h>

#include <ti/drivers/rf/RF.h>
#include <ti/sysbios/knl/Task.h>

/* F20.a.1 — Bundle 4 fills this in; weak stub keeps link clean until then. */
void Ble20_drainAndDispatch(uint8_t *reason_out);

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
static BleConnMgr_DisconnectCb s_disconnect_cb;
/* Sticky reason for the most recent termination — captured on entry into
 * BleConnMgr_stop so the callback always sees the value the FIRST caller
 * intended, even if a later path also calls stop.
 *
 * Concurrency: these statics are only mutated from the BleConnMgr_poll
 * task context (handle_ll_ctrl runs from process_rx_packets which is
 * called by poll; the supervision-timeout branch is also in poll). They
 * are NOT touched from RF Hwi/Swi callbacks. The mutex-free first-caller-
 * wins pattern below is therefore safe. If a future caller from a
 * different context is added, this needs revisiting. */
static uint8_t s_pending_dc_reason;
static bool s_dc_reason_pending;

/* F8d — cooperative graceful disconnect state.
 *
 * Concurrency: written by both UartTask (via
 * BleConnMgr_initiateGracefulDisconnect from command_processor's
 * CMD_DISCONNECT path) AND RfTask (the BleConnMgr_poll hook decrements
 * s_disconnect_events_remaining and clears both fields on teardown).
 * F8c's sibling sticky-reason fields (s_pending_dc_reason,
 * s_dc_reason_pending) have the same dual-writer pattern and have been
 * validated wire-level. Single-word ARM stores are atomic on the
 * Cortex-M4, and the state machine tolerates one stale-read iteration
 * (the worst case extends the grace window by one or two events; no
 * crash or stuck state). The write-ordering inside
 * BleConnMgr_initiateGracefulDisconnect (counter BEFORE flag) closes
 * the most-likely race. If a future change adds a non-atomic field
 * here, switch to Hwi_disable/restore around the writes. */
static bool s_pending_disconnect;
static uint8_t s_disconnect_events_remaining;
#define DISCONNECT_TX_GRACE_EVENTS 5u /* ~150 ms at 30 ms interval */

/* ── LL / L2CAP named constants ── */
#define LL_REASON_SUPERVISION_TIMEOUT 0x22u
#define L2CAP_CID_ATT 0x0004u

/* Debug timing ring buffer — populated each call to BleConnMgr_poll().
 * Cleared on BleConnMgr_start so each connect attempt sees a fresh log. */
static BleConnMgr_DbgTimingEntry s_dbg_timing[BLE_CONN_MGR_DBG_TIMING_DEPTH];
static uint8_t s_dbg_timing_head;  /* next write slot 0..DEPTH-1 */
static uint8_t s_dbg_timing_count; /* number of valid entries (saturates at DEPTH) */

/* ── F20.a.1 — Slave / Peripheral state ── */
static bool s_slave_running = false;
static BleConnMgr_SlaveParams s_slave_params;
static uint16_t s_slave_event_counter = 0u;
static uint32_t s_slave_anchor_rat = 0u;
static uint32_t s_slave_last_rx_rat = 0u;

/* ── LL Control PDU handling ── */

static void handle_ll_ctrl(const uint8_t *payload, uint8_t len) {
    if (len < 1) {
        return;
    }
    uint8_t opcode = payload[0];

    switch (opcode) {
    case LL_TERMINATE_IND: {
        /* payload = [opcode:1][reason:1] per BT Core Spec Vol 6 Part B §2.4.2.6 */
        uint8_t reason = (len >= 2) ? payload[1] : 0x13u; /* default REMOTE_USER_TERMINATED */
        /* F8d: peer already TX'd LL_TERMINATE, no point retransmitting.
         * Apply sticky reason (fires DC callback) then pure cleanup —
         * skip the queue+sleep path entirely. */
        BleConnMgr_stopWithReason(reason);
        BleConn_finalizeDisconnect();
        break;
    }

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

void BleConnMgr_setDisconnectCb(BleConnMgr_DisconnectCb cb) {
    /* NULL is allowed and clears the registered callback — _stop guards
     * with `if (s_disconnect_cb)` so a cleared hook is silently skipped. */
    s_disconnect_cb = cb;
}

void BleConnMgr_stopWithReason(uint8_t reason) {
    if (!s_dc_reason_pending) {
        s_pending_dc_reason = reason;
        s_dc_reason_pending = true;
    }
    BleConnMgr_stop();
}

void BleConnMgr_initiateGracefulDisconnect(uint8_t reason) {
    if (!s_running) {
        /* No active connection — degenerate case. Apply sticky reason
         * and tear down immediately. BleConnMgr_stopWithReason fires
         * the disconnect callback with the reason; finalizeDisconnect
         * does the radio-state cleanup. */
        BleConnMgr_stopWithReason(reason);
        BleConn_finalizeDisconnect();
        return;
    }

    /* Queue LL_TERMINATE_IND on the connection's TX queue. The next
     * BleConnMgr_poll event will TX it. Note: opcode 0x02 + reason
     * byte per BT Core Spec Vol 6 Part B §2.4.2.6.
     *
     * Return value of TXQueue_insert intentionally ignored: if the
     * queue is full the PDU is dropped, but the cooperative path
     * still tears down cleanly via the 5-event grace expiry — peer
     * falls back to supervision timeout instead of receiving the
     * graceful LL_TERMINATE. Acceptable degraded behavior. */
    uint8_t pdu[2];
    pdu[0] = 0x02u; /* LL_TERMINATE_IND */
    pdu[1] = reason;
    (void)TXQueue_insert(2, TX_QUEUE_LLID_CTRL, pdu);

    /* Set sticky reason now so when BleConnMgr_stop fires from the
     * poll-hook teardown, the callback sees the host-initiated reason.
     * Sticky-first-caller from F8c handles a racing peer LL_TERMINATE
     * within the grace window. */
    if (!s_dc_reason_pending) {
        s_pending_dc_reason = reason;
        s_dc_reason_pending = true;
    }

    /* Publish the events-remaining counter BEFORE flipping the flag
     * that triggers the poll-loop hook. RfTask reads s_pending_disconnect
     * to decide whether to enter the hook; if it sees true while the
     * counter is still 0 (stale from BleConnMgr_start), the
     * --counter == 0u decrement underflows and the grace becomes 256
     * events instead of 5. Reverse-order writes ensure either the
     * counter is already valid OR s_pending_disconnect is still false. */
    s_disconnect_events_remaining = DISCONNECT_TX_GRACE_EVENTS;
    s_pending_disconnect = true;
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
    s_dc_reason_pending = false;
    s_pending_dc_reason = 0;
    s_pending_disconnect = false;
    s_disconnect_events_remaining = 0u;
    s_dbg_l2cap_rx_count = 0;
    s_dbg_total_rx_count = 0;
    s_dbg_total_tx_done = 0;
    s_dbg_timing_head = 0;
    s_dbg_timing_count = 0;

    s_hop_interval_ticks = CONN_INTERVAL_TO_TICKS(st->connInterval);
    s_superv_timeout_ticks = SUPERV_TO_TICKS(st->supervTimeout);

    /* Anchor calibration (Session 3 telemetry against CH573 + Sniffle oracle):
     *
     * In SDK 8.30 with bDynamicWinOffset=1, pParams->connectTime corresponds to
     * end-of-CONNECT_IND-on-air, NOT to the peer's expected first-anchor. The
     * peer opens its first listen window at:
     *
     *     end_of_connect_ind + transmitWindowDelay + WinOffset * 1.25 ms
     *
     * So the corrected first anchor (where master TX must fire) is
     * connTime + TRANSMIT_WINDOW_DELAY + winOffset*5000 ticks. The rest of the
     * Sniffle-style formula (subtract AO_TARG, add hopInterval to get the
     * end-of-event marker) is unchanged.
     *
     * winOffset is read back from s_ll_data[8..9] in BleConn_initiate after
     * the SDK overwrites it (with bDynamicWinOffset=1), so this correction is
     * dynamic across connections. */
    /* Session 5 anchor formula — match Sniffle's RadioTask.c:467:
     *     nextHopTime = connTime - AO_TARG + hopIntervalTicks
     *
     * TI's `connectTime` returned by CMD_BLE5_INITIATOR is already the
     * slave's first-event anchor (the SDK accounts for transmitWindowDelay
     * and WinOffset internally). Session 4 H4 added transmitWindowDelay +
     * WinOffset*1.25ms ON TOP of connTime — that's a double-application of
     * a delay the SDK has already absorbed. Sniffle on the same hardware
     * uses just `connTime + hopInterval - AO_TARG` and connects cleanly to
     * CH573 (Session 4 Task 3 evidence).
     *
     * Old formula kept here for reference if this regresses on a future
     * peer that does NOT have its anchor at TI's connectTime tick:
     *   uint32_t anchor_correction = TRANSMIT_WINDOW_DELAY + (uint32_t)st->winOffset * 5000u;
     *   s_next_hop_time = st->connTime + anchor_correction - AO_TARG + s_hop_interval_ticks;
     */
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
    bool was_running = s_running;
    s_running = false;
    s_event_counter = 0;
    AttClient_reset();
    /* Fire the disconnect callback HERE rather than in BleConnMgr_stopWithReason
     * because internal callers (notably BleConn_disconnect, which calls back
     * into BleConnMgr_stop) bypass _stopWithReason. By gating on `was_running`
     * we still fire exactly once per connection lifetime — the second _stop
     * call from BleConn_disconnect sees was_running=false and skips. */
    if (was_running && s_dc_reason_pending) {
        uint8_t reason = s_pending_dc_reason;
        s_dc_reason_pending = false;
        s_pending_dc_reason = 0;
        if (s_disconnect_cb) {
            s_disconnect_cb(reason);
        }
    }
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
        /* 0x22 = LL_RESPONSE_TIMEOUT per BT Core Spec Vol 1 Part F §1.3.2.
         * F8d: peer is unresponsive (no RX in supervision window), so
         * skip the queue+sleep path — just clean up. */
        BleConnMgr_stopWithReason(0x22u);
        BleConn_finalizeDisconnect();
        return false;
    }

    uint8_t chan;
    if (st->useCsa2) {
        chan = csa2_computeChannel(s_event_counter);
    } else {
        /* CSA#1 (legacy hop). BLE Core Spec Vol 6 Part B 4.5.8.1:
         *   unmappedChannel(N) = (lastUnmapped + hopIncrement) mod 37
         *   with lastUnmapped(0) = 0 → unmappedChannel(N) = (N+1)*hop mod 37.
         * With our channelMap = 0x1F_FFFF_FFFF (all 37 enabled) the remap is
         * identity, so channel == unmappedChannel. */
        chan = (uint8_t)(((uint32_t)(s_event_counter + 1u) * st->hopIncrement) % 37u);
    }

    /* Queue pending ATT requests before building TX queue */
    AttClient_poll();

    TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);
    dataQueue_t txq;
    TXQueue_take(&txq);

    uint32_t startTime = curHopTime;
    uint32_t endTime = s_next_hop_time;
    uint32_t numSent = 0;
    RadioIF_BleCentralStats stats = {0};

    int status = RadioIF_bleCentral(chan, st->accessAddr, st->crcInit, &txq, startTime, endTime,
                                    &numSent, &stats);
    s_last_status = status;
    s_dbg_total_tx_done += numSent;

    /* Snapshot timing + per-event RF stats for host-side correlation
     * (Session 3 + Session 4 telemetry). */
    {
        BleConnMgr_DbgTimingEntry *e = &s_dbg_timing[s_dbg_timing_head];
        e->eventIdx = s_event_counter;
        e->startRAT = startTime;
        e->endRAT = endTime;
        e->status = (uint16_t)status;
        e->numSent = (uint8_t)numSent;
        e->nTx = stats.nTx;
        e->nRxOk = stats.nRxOk;
        e->nRxNok = stats.nRxNok;
        e->nRxIgnored = stats.nRxIgnored;
        e->pktStatus = stats.pktStatus;
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

    /* F8d — cooperative graceful disconnect: if the host queued
     * LL_TERMINATE_IND via BleConnMgr_initiateGracefulDisconnect,
     * tear the connection down once at least one PDU was TX'd this
     * event (numSent >= 1) OR once the 5-event grace window expires.
     *
     * NOTE: numSent counts ALL PDUs TX'd this event (including the
     * empty LL_DATA keepalive that BleConnMgr always queues). It does
     * NOT prove our specific LL_TERMINATE made it on-air. In the rare
     * case where the radio op stops after only the keepalive slot,
     * we tear down without delivering LL_TERMINATE — peer falls back
     * to supervision timeout (~1 s), same as the give_up branch.
     * Acceptable: imprecise but never stuck.
     *
     * If TXQueue_insert in initiateGracefulDisconnect dropped the PDU
     * (queue full), the give_up branch handles teardown after grace
     * expiry — no special path needed. */
    if (s_pending_disconnect) {
        bool tx_confirmed = (numSent >= 1u);
        bool give_up = (--s_disconnect_events_remaining == 0u);
        if (tx_confirmed || give_up) {
            s_pending_disconnect = false;
            s_disconnect_events_remaining = 0u;
            BleConnMgr_stop();            /* fires DC callback w/ sticky reason */
            BleConn_finalizeDisconnect(); /* pure radio cleanup */
            return false;                 /* signal poll loop: connection ended */
        }
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

/* ── F20.a.1 — Slave / Peripheral public API ── */

void BleConnMgr_startSlave(const BleConnMgr_SlaveParams *params) {
    if (params == NULL) {
        return;
    }
    if (s_running || s_slave_running) {
        return;
    }
    s_slave_params = *params;
    s_slave_event_counter = 0u;

    /* I3 — first-event anchor: end_of_CONNECT_IND + transmitWindowDelay + winOffset*1.25ms.
     * If Bundle 4 hasn't populated connectIndEndRat yet (= 0), fall back to
     * RF_getCurrentTime() + 1 interval — preserves Bundle 3 testability. */
    uint32_t hop_ticks = CONN_INTERVAL_TO_TICKS(params->hopInterval_125us);
    if (params->connectIndEndRat != 0u) {
        s_slave_anchor_rat = params->connectIndEndRat + TRANSMIT_WINDOW_DELAY +
                             (uint32_t)params->winOffset_125us * 5000u;
    } else {
        s_slave_anchor_rat = RF_getCurrentTime() + hop_ticks;
    }

    s_slave_last_rx_rat = RF_getCurrentTime();
    TXQueue_init();
    RadioIF_bleResetSlaveSeqStat();
    RadioIF_bleResetRxQueue();
    s_slave_running = true;
}

bool BleConnMgr_pollSlave(void) {
    if (!s_slave_running) {
        return false;
    }

    uint32_t hop_ticks = CONN_INTERVAL_TO_TICKS(s_slave_params.hopInterval_125us);

    /* Check supervision timeout (in 10ms units → 40000 4MHz ticks per unit). */
    uint32_t superv_ticks = SUPERV_TO_TICKS(s_slave_params.supervTimeout_10ms);
    uint32_t now = RF_getCurrentTime();
    if (now - s_slave_last_rx_rat > superv_ticks) {
        s_slave_running = false;
        if (s_disconnect_cb) {
            s_disconnect_cb(LL_REASON_SUPERVISION_TIMEOUT);
        }
        return false;
    }

    /* Wait until anchor point (sleep until ~500us before) */
    uint32_t wait = s_slave_anchor_rat - now;
    if (wait < 0x80000000u && wait > 2000u) {
        Task_sleep(wait / 40u);
    }

    /* CSA#1 channel calc per BLE Core Spec Vol 6 Part B §4.5.8.1. */
    uint8_t chan =
        (uint8_t)(((uint32_t)(s_slave_event_counter + 1u) * s_slave_params.hopIncrement) % 37u);

    /* Build TX queue from AttServer pending RSP (if any). */
    if (AttServer_hasPendingTx()) {
        uint8_t att[ATT_DEFAULT_MTU];
        uint8_t att_len = AttServer_takePendingTx(att, sizeof(att));
        if (att_len > 0u) {
            uint8_t l2cap_frame[ATT_DEFAULT_MTU + 4u];
            l2cap_frame[0] = att_len;
            l2cap_frame[1] = 0x00u;
            l2cap_frame[2] = (uint8_t)(L2CAP_CID_ATT & 0xFFu);
            l2cap_frame[3] = 0x00u;
            memcpy(&l2cap_frame[4], att, att_len);
            TXQueue_insert((uint8_t)(att_len + 4u), TX_QUEUE_LLID_DATA_START, l2cap_frame);
        }
    }
    TXQueue_insert(0u, TX_QUEUE_LLID_DATA_CONT, NULL);
    dataQueue_t txq;
    TXQueue_take(&txq);

    uint32_t startTime = s_slave_anchor_rat;
    uint32_t endTime = s_slave_anchor_rat + hop_ticks;
    RadioIF_BleCentralStats stats = {0};
    uint32_t numSent = 0u;

    int status = RadioIF_bleSlave(chan, s_slave_params.accessAddr, s_slave_params.crcInit, &txq,
                                  startTime, endTime, &numSent, &stats);
    (void)status;

    TXQueue_flush((uint8_t)numSent);

    /* Drain RX queue: route ATT to AttServer, detect LL_TERMINATE_IND. */
    RadioIF_bleDrainRxQueue();
    RadioIF_bleResetRxQueue();

    if (stats.nRxOk > 0u) {
        s_slave_last_rx_rat = RF_getCurrentTime();
    }

    /* Bundle 4 implements Ble20_drainAndDispatch — weak stub until then. */
    uint8_t reason = 0u;
    Ble20_drainAndDispatch(&reason);
    if (reason != 0u) {
        s_slave_running = false;
        if (s_disconnect_cb) {
            s_disconnect_cb(reason);
        }
        return false;
    }

    s_slave_event_counter++;
    s_slave_anchor_rat += hop_ticks;
    return true;
}

/* Weak stub — Bundle 4 provides the real implementation in ble20_dispatch.c */
__attribute__((weak)) void Ble20_drainAndDispatch(uint8_t *reason_out) {
    if (reason_out) {
        *reason_out = 0u;
    }
}
