/*
 * FeralRF CC1352 — Sniffle-style passive connection follower.
 *
 * Reuses FeralRF's existing csa2.c (identical algorithm) and
 * radio_if's GenericRx-based primitives. Adapts the state-machine
 * pattern from Sniffle's RadioTask.c (BSD/GPLv3 by Sultan Qasim Khan,
 * NCC Group plc).
 */
#include "ll_follower.h"

#include "csa2.h"
#include "radio_if.h"

#include <ti/drivers/rf/RF.h>

#include <stdint.h>
#include <string.h>

/* ── State ── */

typedef enum {
    LL_FOLLOWER_STATE_IDLE = 0,
    LL_FOLLOWER_STATE_SCAN_ADV,
    LL_FOLLOWER_STATE_FOLLOWING,
} LlFollower_State;

/* CONNECT_IND LL data layout (BT Core Spec 5.4 Vol 6 Part B §2.3.3.1):
 *   AccessAddress:4  CRCInit:3  WinSize:1  WinOffset:2  Interval:2
 *   Latency:2  Timeout:2  ChM:5  Hop:5b+SCA:3b */
#define CONNECT_IND_LL_LEN 22u

static LlFollower_State s_state = LL_FOLLOWER_STATE_IDLE;
static uint8_t s_target_mac[LL_FOLLOWER_MAC_LEN];
static bool s_have_target;
static LlFollower_Callbacks s_cb;

/* Captured connection parameters */
static uint32_t s_access_addr;
static uint32_t s_crc_init;
static uint16_t s_hop_interval; /* 1.25 ms units */
static uint16_t s_supervision;  /* 10 ms units */
static uint64_t s_chan_map;
static uint8_t s_hop_increment;
static bool s_use_csa2;

/* Per-event state */
static uint16_t s_event_counter;
static uint32_t s_next_anchor_rat; /* RAT ticks (4 MHz) */
static uint32_t s_last_rx_rat;
static uint32_t s_packets_captured;
static uint8_t s_scan_channel; /* rotates 37→38→39 */
static uint8_t s_zero_rx_streak;

/* For ADV-channel scan callback marshalling */
static bool s_connect_ind_pending;

/* ── Helpers ── */

static uint32_t s_rd24_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t s_rd32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t s_rd16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint64_t s_rd40_le(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32);
}

static void s_emit_done(uint8_t reason) {
    if (s_cb.onDone) {
        LlFollower_DoneInfo info = {
            .reason = reason,
            .packets_captured = s_packets_captured,
        };
        s_cb.onDone(&info);
    }
    s_state = LL_FOLLOWER_STATE_IDLE;
}

/* Parse CONNECT_IND LLData (22 bytes after the 14-byte adv header) and
 * load the captured connection into module state. Returns true on success.
 */
static bool s_parse_connect_ind_lldata(const uint8_t *lldata) {
    s_access_addr = s_rd32_le(&lldata[0]);
    s_crc_init = s_rd24_le(&lldata[4]);
    /* WinSize at lldata[7], WinOffset at lldata[8..9] — we ignore both;
     * for capture-only we sync to first observed packet anchor. */
    s_hop_interval = s_rd16_le(&lldata[10]);
    /* Latency at lldata[12..13] — ignored */
    s_supervision = s_rd16_le(&lldata[14]);
    s_chan_map = s_rd40_le(&lldata[16]) & 0x1FFFFFFFFFULL;
    s_hop_increment = lldata[21] & 0x1Fu;
    /* SCA at upper 3 bits of lldata[21] — ignored */

    /* Sanity */
    if (s_hop_interval < 6u || s_hop_increment < 5u || s_hop_increment > 16u) {
        return false;
    }
    return true;
}

/* Callback fired by RadioIF_followAdvOnce when an ADV-channel packet arrives.
 * Filters CONNECT_IND with InitA matching our target. */
static void s_on_adv_packet(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                            int8_t rssi_dbm, void *user) {
    (void)user;
    (void)rssi_dbm;
    (void)channel;
    if (pdu_len < 2u) {
        return;
    }
    /* ADV channel PDU header: byte0[3:0]=PduType, [6]=TxAdd, [7]=RxAdd; byte1=Length */
    uint8_t pdu_type = ll_pdu[0] & 0x0Fu;
    /* CONNECT_IND PduType = 0x05 (legacy). AdvLen = 34. Total LL = header(2) + 34 = 36. */
    if (pdu_type != 0x05u || pdu_len < 36u) {
        return;
    }
    /* Layout: byte0..1 hdr, byte2..7 InitA (LE), byte8..13 AdvA (LE), byte14..35 LLData */
    const uint8_t *adv_a = &ll_pdu[8];
    if (s_have_target && memcmp(adv_a, s_target_mac, LL_FOLLOWER_MAC_LEN) != 0) {
        return;
    }
    /* Determine CSA selection from ChSel bit (header byte 0 bit 5) BEFORE
     * computing CSA #2 mapping below. */
    s_use_csa2 = ((ll_pdu[0] & 0x20u) != 0u);

    if (!s_parse_connect_ind_lldata(&ll_pdu[14])) {
        return;
    }
    s_connect_ind_pending = true;
    /* Snapshot t_anchor approximation: first listen window opens at
     * end_of_CONNECT_IND + transmitWindowDelay (1.25 ms) + WinOffset.
     * We use RF_getCurrentTime() as a coarse base; per-event drift correction
     * happens in FOLLOWING (Sniffle's afterConnEvent equivalent).
     * 1.25 ms = 5000 RAT ticks. */
    s_next_anchor_rat = RF_getCurrentTime() + 5000u;
    s_event_counter = 0u;
    s_zero_rx_streak = 0u;
    if (s_use_csa2) {
        csa2_computeMapping(s_access_addr, s_chan_map);
    }
}

static void s_on_data_packet(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                             int8_t rssi_dbm, void *user) {
    (void)user;
    if (pdu_len < 2u) {
        return;
    }
    s_packets_captured++;
    s_last_rx_rat = RF_getCurrentTime();

    /* Direction inference for capture-only is non-trivial without seeing
     * the CONNECT_IND TxAdd bits stored. For F8b Track B we emit
     * direction='?'; the host parser can guess from LLID + SN/NESN flips
     * across consecutive packets if needed. */
    if (s_cb.onPacket) {
        s_cb.onPacket(ll_pdu, pdu_len, channel, rssi_dbm, s_event_counter, '?');
    }

    /* LL Control: watch for LL_TERMINATE_IND to end cleanly */
    uint8_t llid = ll_pdu[0] & 0x03u;
    uint8_t length = ll_pdu[1];
    if (llid == 0x03u && length >= 1u && pdu_len >= 3u && ll_pdu[2] == 0x02u) {
        /* LL_TERMINATE_IND — the linked devices are ending the connection.
         * Mark a flag so poll() can emit done after this event finishes. */
        s_zero_rx_streak = 0xFFu; /* sentinel: terminate */
    } else {
        s_zero_rx_streak = 0u;
    }
}

/* ── Public API ── */

void LlFollower_init(void) {
    s_state = LL_FOLLOWER_STATE_IDLE;
    s_have_target = false;
    memset(&s_cb, 0, sizeof(s_cb));
    s_packets_captured = 0u;
}

void LlFollower_setCallbacks(const LlFollower_Callbacks *cb) {
    if (cb) {
        s_cb = *cb;
    }
}

bool LlFollower_start(const uint8_t target_mac_le[LL_FOLLOWER_MAC_LEN]) {
    if (s_state != LL_FOLLOWER_STATE_IDLE) {
        return false;
    }
    /* All-zero MAC ⇒ wildcard */
    bool any_nonzero = false;
    for (uint8_t i = 0u; i < LL_FOLLOWER_MAC_LEN; i++) {
        if (target_mac_le[i] != 0u) {
            any_nonzero = true;
            break;
        }
    }
    s_have_target = any_nonzero;
    memcpy(s_target_mac, target_mac_le, LL_FOLLOWER_MAC_LEN);

    s_state = LL_FOLLOWER_STATE_SCAN_ADV;
    s_scan_channel = 37u;
    s_connect_ind_pending = false;
    s_packets_captured = 0u;
    return true;
}

bool LlFollower_stop(void) {
    if (s_state == LL_FOLLOWER_STATE_IDLE) {
        return false;
    }
    s_emit_done(LL_FOLLOWER_DONE_HOST_STOP);
    return true;
}

bool LlFollower_isRunning(void) {
    return s_state != LL_FOLLOWER_STATE_IDLE;
}

void LlFollower_poll(void) {
    switch (s_state) {
    case LL_FOLLOWER_STATE_IDLE:
        return;

    case LL_FOLLOWER_STATE_SCAN_ADV: {
        /* Listen on current ADV channel for ~10 ms. If a CONNECT_IND lands
         * for our target, the callback flips s_connect_ind_pending true and
         * we transition. Otherwise rotate to the next ADV channel. */
        uint32_t end = RF_getCurrentTime() + (10u * 4000u); /* 10 ms in RAT */
        (void)RadioIF_followAdvOnce(s_scan_channel, end, s_on_adv_packet, NULL);

        if (s_connect_ind_pending) {
            s_state = LL_FOLLOWER_STATE_FOLLOWING;
            s_connect_ind_pending = false;
            return;
        }
        s_scan_channel = (s_scan_channel == 39u) ? 37u : (uint8_t)(s_scan_channel + 1u);
        return;
    }

    case LL_FOLLOWER_STATE_FOLLOWING: {
        /* Compute data channel for this event */
        uint8_t chan;
        if (s_use_csa2) {
            chan = csa2_computeChannel((uint32_t)s_event_counter);
        } else {
            /* CSA #1 fallback: lastUnmapped(N) = (N+1)*hop mod 37 */
            chan = (uint8_t)(((uint32_t)(s_event_counter + 1u) * s_hop_increment) % 37u);
        }

        /* Listen for one event window. End time = next anchor + half interval
         * to give us plenty of slack for clock drift on first events. */
        uint32_t window_ticks = (uint32_t)s_hop_interval * 5000u;
        uint32_t end = s_next_anchor_rat + window_ticks;
        uint32_t pre = s_packets_captured;
        (void)RadioIF_followDataOnce(chan, s_access_addr, s_crc_init, end, s_on_data_packet, NULL);

        /* Termination conditions */
        if (s_zero_rx_streak == 0xFFu) {
            s_emit_done(LL_FOLLOWER_DONE_PEER_TERMINATE);
            return;
        }
        if (s_packets_captured == pre) {
            s_zero_rx_streak++;
            if (s_zero_rx_streak > 5u && s_packets_captured == 0u) {
                /* never synced */
                s_emit_done(LL_FOLLOWER_DONE_SYNC_FAILED);
                return;
            }
            /* supervision: 10 ms units; very generous timeout for capture-only */
            if (RF_getCurrentTime() - s_last_rx_rat > (uint32_t)s_supervision * 40000u) {
                s_emit_done(LL_FOLLOWER_DONE_SUPERVISION);
                return;
            }
        }

        /* Advance */
        s_event_counter++;
        s_next_anchor_rat += window_ticks;
        return;
    }
    }
}
