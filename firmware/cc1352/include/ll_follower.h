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
    LL_FOLLOWER_DONE_HOST_STOP = 0,    /* host called LlFollower_stop */
    LL_FOLLOWER_DONE_PEER_TERMINATE,   /* LL_TERMINATE_IND seen on link */
    LL_FOLLOWER_DONE_SUPERVISION,      /* > supervisionTimeout without RX */
    LL_FOLLOWER_DONE_SYNC_FAILED,      /* CONNECT_IND captured but >5 events with 0 packets */
    LL_FOLLOWER_DONE_CONNECT_TIMEOUT,  /* no CONNECT_IND for target within scan window */
} LlFollower_DoneReason;

typedef struct {
    uint8_t reason;
    uint32_t packets_captured;
} LlFollower_DoneInfo;

/* Callbacks the host application installs to receive captured packets and
 * the terminal "done" event. Both fire from the same task that calls
 * LlFollower_poll(). */
typedef void (*LlFollower_PacketCb)(const uint8_t *ll_pdu, uint8_t pdu_len,
                                    uint8_t channel, int8_t rssi_dbm,
                                    uint16_t event_counter, uint8_t direction);

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

#endif /* LL_FOLLOWER_H */
