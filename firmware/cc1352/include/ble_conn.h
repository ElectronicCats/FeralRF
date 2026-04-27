/*
 * FeralRF CC1352 - BLE Connection Manager
 *
 * Manages BLE connection initiation via CMD_BLE5_INITIATOR and
 * connection state for data channel communication (Phase 2).
 * Single connection at a time, static allocation, no encryption.
 */

#ifndef BLE_CONN_H
#define BLE_CONN_H

#include <stdbool.h>
#include <stdint.h>

/* CONNECT_IND LLData layout (BLE Core Spec Vol 6, Part B, 2.3.3.1):
 *   [0..3]   Access Address (4 bytes)
 *   [4..6]   CRC Init (3 bytes, little-endian)
 *   [7]      WinSize
 *   [8..9]   WinOffset (1.25ms units)
 *   [10..11] Interval (1.25ms units)
 *   [12..13] Latency
 *   [14..15] Timeout (10ms units)
 *   [16..20] Channel Map (5 bytes, 37 bits)
 *   [21]     Hop (bits 4:0) | SCA (bits 7:5)
 */
#define BLE_CONN_LLDATA_LEN 22u

/* Connection initiation result codes */
typedef enum {
    BLE_CONN_OK = 0,          /* CONNECT_IND sent, connection established */
    BLE_CONN_ERR_BUSY = 1,    /* Already connected or initiating */
    BLE_CONN_ERR_TIMEOUT = 2, /* No ADV_IND from target within timeout */
    BLE_CONN_ERR_RF = 3,      /* RF command error */
    BLE_CONN_ERR_NO_SYNC = 4, /* No sync found */
} BleConn_Result;

/* Connection state — exposed for status queries */
typedef struct {
    uint32_t accessAddr;
    uint32_t crcInit;       /* 24-bit, stored in lower 3 bytes */
    uint8_t channelMap[5];  /* 37 data channels */
    uint8_t hopIncrement;   /* 5..16 */
    uint16_t connInterval;  /* 1.25ms units */
    uint16_t supervTimeout; /* 10ms units */
    uint16_t peripheralLatency;
    uint16_t eventCounter;
    uint8_t peerAddr[6];
    uint8_t peerAddrType; /* 0=public, 1=random */
    uint8_t ownAddr[6];
    bool connected;
    bool initiating;
    uint16_t winOffset; /* 1.25ms units, from CONNECT_IND LLData */
    bool useCsa2;       /* true if CSA#2 negotiated */
    uint32_t connTime;  /* RAT timestamp of connection event */
} BleConn_State;

void BleConn_init(void);
BleConn_Result BleConn_initiate(const uint8_t *peerAddr, uint8_t peerAddrType,
                                uint16_t connIntervalUnits, uint16_t supervTimeoutUnits);
void BleConn_disconnect(void);
bool BleConn_isConnected(void);
bool BleConn_isInitiating(void);
const BleConn_State *BleConn_getState(void);

/* Read-only view of the 22-byte CONNECT_IND LLData buffer that was
 * actually transmitted by the SDK (after any bDynamicWinOffset-style
 * SDK rewrites). Used for Session 5 wire-vs-state diagnostics. */
const uint8_t *BleConn_getLlData(void);

#endif /* BLE_CONN_H */
