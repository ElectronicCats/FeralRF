/*
 * FeralRF CC1352 — Sniffle-style passive connection follower (F8b Track B).
 *
 * Captures a non-FeralRF central↔peripheral BLE connection without
 * participating. Hops per CSA #2 using the captured CONNECT_IND parameters.
 * Capture-only: never transmits on the followed link.
 *
 * State machine:
 *   IDLE
 *     │ LlFollower_start(target_mac)
 *     ▼
 *   SCAN_ADV — rotates ch37→ch38→ch39 with AA=0x8E89BED6,
 *              filters CONNECT_IND by InitA == target_mac (or wildcard).
 *     │ CONNECT_IND captured
 *     ▼
 *   FOLLOWING — for each conn event, computes channel via CSA #2,
 *               runs GenericRx with captured AA/CRCInit until next anchor.
 *     │ supervision timeout / LL_TERMINATE_IND / LlFollower_stop
 *     ▼
 *   IDLE
 */

#ifndef LL_FOLLOWER_H
#define LL_FOLLOWER_H

#include <stdbool.h>
#include <stdint.h>

#define LL_FOLLOWER_MAC_LEN 6u

/* Reasons reported in LlFollower_DoneInfo.reason */
typedef enum {
    LL_FOLLOWER_DONE_HOST_STOP = 0,   /* host called LlFollower_stop */
    LL_FOLLOWER_DONE_PEER_TERMINATE,  /* LL_TERMINATE_IND seen on link */
    LL_FOLLOWER_DONE_SUPERVISION,     /* > supervisionTimeout without RX */
    LL_FOLLOWER_DONE_SYNC_FAILED,     /* CONNECT_IND captured but >5 events with 0 packets */
    LL_FOLLOWER_DONE_CONNECT_TIMEOUT, /* no CONNECT_IND for target within scan window */
} LlFollower_DoneReason;

typedef struct {
    uint8_t reason;
    uint32_t packets_captured;
} LlFollower_DoneInfo;

/* Callbacks the host application installs to receive captured packets and
 * the terminal "done" event. Both fire from the same task that calls
 * LlFollower_poll(). */
typedef void (*LlFollower_PacketCb)(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                                    int8_t rssi_dbm, uint16_t event_counter, uint8_t direction);

typedef void (*LlFollower_DoneCb)(const LlFollower_DoneInfo *info);

typedef struct {
    LlFollower_PacketCb onPacket;
    LlFollower_DoneCb onDone;
} LlFollower_Callbacks;

void LlFollower_init(void);
void LlFollower_setCallbacks(const LlFollower_Callbacks *cb);

/* Start the follower. target_mac_le is the 6-byte LE MAC to filter on; pass
 * all-zero for wildcard. Returns false if already running. */
bool LlFollower_start(const uint8_t target_mac_le[LL_FOLLOWER_MAC_LEN]);
bool LlFollower_stop(void);
bool LlFollower_isRunning(void);

/* Poll once. Drives the state machine forward by one step (one ADV scan
 * burst, or one connection event). Should be called from the main task
 * loop while the follower is running. */
void LlFollower_poll(void);

/* F8b.b debug snapshot — exposes RF cmd status + counters so the host can
 * diagnose why the follower captures 0 packets. */
typedef struct {
    uint8_t state;                   /* current LlFollower_State */
    uint8_t scan_channel;            /* 37/38/39 */
    uint16_t adv_call_count;         /* RadioIF_followAdvOnce invocations */
    uint16_t data_call_count;        /* RadioIF_followDataOnce invocations */
    uint16_t adv_packets_seen;       /* times s_on_adv_packet entered (any pdu) */
    uint16_t connect_inds_seen;      /* CONNECT_IND that passed pdu_type+len filter */
    uint16_t data_packets_seen;      /* s_on_data_packet entered */
    int16_t last_adv_cmd_status;     /* Ble5_0_cmdBle5GenericRx.status from last followAdvOnce */
    int16_t last_data_cmd_status;    /* status from last followDataOnce */
    uint32_t last_adv_event_mask_lo; /* lower 32 bits of last RF_runCmd EventMask */
    uint32_t packets_captured;       /* cumulative for the current session */
    /* PDU type histogram (counts at s_on_adv_packet entry) */
    uint16_t pt_adv_ind;      /* 0x00 ADV_IND */
    uint16_t pt_adv_direct;   /* 0x01 ADV_DIRECT_IND */
    uint16_t pt_adv_nonconn;  /* 0x02 ADV_NONCONN_IND */
    uint16_t pt_scan_req;     /* 0x03 SCAN_REQ */
    uint16_t pt_scan_rsp;     /* 0x04 SCAN_RSP */
    uint16_t pt_connect_ind;  /* 0x05 CONNECT_IND (pre-len-filter) */
    uint16_t pt_adv_scan_ind; /* 0x06 ADV_SCAN_IND */
    uint16_t pt_ext_adv;      /* 0x07 ADV_EXT_IND (extended adv) */
    uint16_t pt_other;        /* 0x08-0x0F */
} LlFollower_DebugSnapshot;

void LlFollower_getDebug(LlFollower_DebugSnapshot *out);

#endif /* LL_FOLLOWER_H */
