/*
 * FeralRF CC1352 - ATT/GATT Client (Phase 3)
 *
 * Implements ATT protocol over L2CAP for GATT discovery.
 * Driven by BleConnMgr — one ATT transaction per connection event.
 * Reference: BT Core Spec Vol 3, Part F (ATT) + Part G (GATT)
 */

#ifndef ATT_CLIENT_H
#define ATT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/* ATT client state */
typedef enum {
    ATT_STATE_IDLE = 0,
    ATT_STATE_WAIT_MTU_RSP,
    ATT_STATE_WAIT_DISCOVER_RSP,
    ATT_STATE_WAIT_CHAR_RSP,
    ATT_STATE_WAIT_READ_RSP,
    ATT_STATE_WAIT_WRITE_RSP,
    ATT_STATE_WAIT_MTU_EXCHANGE,
    ATT_STATE_WAIT_READ_BY_UUID_RSP,
    ATT_STATE_DONE,
    ATT_STATE_ERROR,
} AttClient_State;

/* Callback for GATT results sent to host */
typedef void (*AttClient_ServiceCb)(uint16_t startHandle, uint16_t endHandle, const uint8_t *uuid,
                                    uint8_t uuidLen);
typedef void (*AttClient_CharCb)(uint16_t handle, uint8_t properties, uint16_t valueHandle,
                                 const uint8_t *uuid, uint8_t uuidLen);
typedef void (*AttClient_ReadCb)(uint16_t handle, const uint8_t *data, uint8_t len);
typedef void (*AttClient_DoneCb)(uint8_t status); /* 0=ok, 1=error, 2=timeout */
/* Delivers the peer's advertised server MTU verbatim. NOT the negotiated
 * min(client, server) — the firmware's TX/RX buffers remain capped at
 * ATT_DEFAULT_MTU regardless. The host can record peerServerMtu for
 * diagnostics or re-issue the exchange after firmware buffer changes. */
typedef void (*AttClient_MtuCb)(uint16_t peerServerMtu);
typedef void (*AttClient_AttributeCb)(uint16_t handle, const uint8_t *value, uint8_t valueLen);

typedef struct {
    AttClient_ServiceCb onService;
    AttClient_CharCb onChar;
    AttClient_ReadCb onRead;
    AttClient_DoneCb onDone;
    AttClient_MtuCb onMtu;
    AttClient_AttributeCb onAttribute;
} AttClient_Callbacks;

void AttClient_init(void);
void AttClient_setCallbacks(const AttClient_Callbacks *cb);

/* Start full GATT discovery (services + characteristics) */
bool AttClient_startDiscover(void);

/* Read a characteristic value by handle */
bool AttClient_startRead(uint16_t handle);

/* Write a characteristic value by handle */
bool AttClient_startWrite(uint16_t handle, const uint8_t *data, uint8_t len);

/* Trigger an explicit ATT MTU exchange. clientMtu is what we advertise to
 * the peer (must be >= 23; smaller values are floored to ATT_DEFAULT_MTU).
 *
 * The exchange completes asynchronously via two callbacks in order:
 *   1. onMtu(peerServerMtu) — the peer's advertised server MTU, verbatim.
 *      The firmware does NOT widen its buffers based on this; ATT_DEFAULT_MTU
 *      remains the wire cap until firmware buffers are widened.
 *   2. onDone(0) — flow finished, AttClient is back in IDLE.
 *
 * Returns false if not currently IDLE (caller can retry after onDone).
 */
bool AttClient_startMtuExchange(uint16_t clientMtu);

/* Issue ATT_READ_BY_TYPE_REQ for an arbitrary 16-bit UUID. Each matching
 * attribute is delivered via onAttribute(handle, value, len); the flow
 * terminates with onDone(status):
 *   - status 0: peer replied (with 1+ entries OR ATTRIBUTE_NOT_FOUND).
 *               Empty result is a valid answer, not an error.
 *   - status 1: real ATT error (e.g. INSUFFICIENT_AUTHENTICATION 0x05,
 *               READ_NOT_PERMITTED 0x02, INVALID_HANDLE 0x01) or a
 *               malformed response. Host should not retry without
 *               addressing the underlying cause.
 * Returns false if not IDLE. Single-shot — if the peer has more matches
 * than fit in one response, the rest are silently dropped (multi-shot
 * continuation is a future enhancement). */
bool AttClient_startReadByUuid(uint16_t startHandle, uint16_t endHandle, uint16_t uuid16);

/* Called by BleConnMgr when L2CAP data arrives (LLID=1 or 2) */
void AttClient_onL2capRx(const uint8_t *l2capPayload, uint8_t len);

/* Called by BleConnMgr each poll to queue pending ATT requests */
void AttClient_poll(void);

/* Reset on disconnect */
void AttClient_reset(void);

AttClient_State AttClient_getState(void);

/* ── Debug instrumentation (Task 0.2 of F8b Track A) ──
 *
 * Captures every public-API entry/exit and every state-machine transition
 * into a small ring buffer that can be dumped via CMD_ATT_DEBUG.
 */
#define ATT_DBG_LOG_DEPTH 32u

/* Tag identifies which call site emitted the entry. Keep small (u8). */
typedef enum {
    ATT_DBG_TAG_INIT = 1,
    ATT_DBG_TAG_RESET = 2,
    ATT_DBG_TAG_START_DISC_ENTER = 3,
    ATT_DBG_TAG_START_DISC_EXIT_OK = 4,
    ATT_DBG_TAG_START_DISC_EXIT_FAIL = 5,
    ATT_DBG_TAG_START_READ_ENTER = 6,
    ATT_DBG_TAG_START_READ_EXIT_OK = 7,
    ATT_DBG_TAG_START_READ_EXIT_FAIL = 8,
    ATT_DBG_TAG_START_WRITE_ENTER = 9,
    ATT_DBG_TAG_START_WRITE_EXIT_OK = 10,
    ATT_DBG_TAG_START_WRITE_EXIT_FAIL = 11,
    ATT_DBG_TAG_L2CAP_RX = 12,
    ATT_DBG_TAG_MTU_RSP = 13,
    ATT_DBG_TAG_GROUP_RSP = 14,
    ATT_DBG_TAG_TYPE_RSP = 15,
    ATT_DBG_TAG_READ_RSP = 16,
    ATT_DBG_TAG_WRITE_RSP = 17,
    ATT_DBG_TAG_ERROR_RSP = 18,
    ATT_DBG_TAG_POLL_DONE = 19,
    ATT_DBG_TAG_POLL_TX_GROUP = 20,
    ATT_DBG_TAG_POLL_TX_TYPE = 21,
    ATT_DBG_TAG_POLL_TX_READ = 22,
    ATT_DBG_TAG_POLL_TX_MTU = 23,
    ATT_DBG_TAG_DONE_CB = 24,
    ATT_DBG_TAG_NOTIFY_RX = 25,
    ATT_DBG_TAG_INDICATE_RX = 26,
    ATT_DBG_TAG_START_MTU_ENTER = 27,
    ATT_DBG_TAG_START_MTU_EXIT_OK = 28,
    ATT_DBG_TAG_START_MTU_EXIT_FAIL = 29,
    ATT_DBG_TAG_START_READ_UUID_ENTER = 30,
    ATT_DBG_TAG_START_READ_UUID_EXIT_OK = 31,
    ATT_DBG_TAG_START_READ_UUID_EXIT_FAIL = 32,
    ATT_DBG_TAG_READ_UUID_RSP = 33,
    ATT_DBG_TAG_POLL_TX_READ_UUID = 34,
} AttClient_DbgTag;

/* 8 bytes per entry. Keep packed-ish (struct alignment puts u16 first). */
typedef struct {
    uint16_t seq;       /* monotonic counter, never reset */
    uint8_t tag;        /* AttClient_DbgTag */
    uint8_t oldState;   /* AttClient_State before this event */
    uint8_t newState;   /* AttClient_State after */
    uint8_t connAlive;  /* 1 if BleConn_isConnected at entry */
    uint8_t mtu;        /* low byte of s_mtu (always ≤ 23 in practice) */
    uint8_t reqPending; /* s_request_pending at entry */
} AttClient_DbgEntry;

/* Copy oldest-to-newest into out[]; returns number copied (≤ maxEntries). */
uint8_t AttClient_getDebugLog(AttClient_DbgEntry *out, uint8_t maxEntries);

#endif /* ATT_CLIENT_H */
