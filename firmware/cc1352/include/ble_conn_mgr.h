/*
 * FeralRF CC1352 - BLE Connection Manager (Central Mode)
 *
 * Runs the connection event loop after Phase 1 establishes a connection.
 * Polled from RfTask — one connection event per poll() call.
 */

#ifndef BLE_CONN_MGR_H
#define BLE_CONN_MGR_H

#include <stdbool.h>
#include <stdint.h>

void BleConnMgr_init(void);
void BleConnMgr_start(void);
void BleConnMgr_stop(void);
bool BleConnMgr_poll(void);
bool BleConnMgr_isRunning(void);
bool BleConnMgr_queueTx(uint8_t llid, const uint8_t *data, uint8_t len);
uint16_t BleConnMgr_getEventCount(void);
int BleConnMgr_getLastStatus(void);
uint16_t BleConnMgr_getL2capRxCount(void);
uint16_t BleConnMgr_getTotalRxCount(void);

/* Depth chosen so RSP_DEBUG_TIMING fits in PROTOCOL_MAX_PAYLOAD (255):
 *   frame_payload = 1 + depth * 18  →  depth=14 → 253 bytes. */
#define BLE_CONN_MGR_DBG_TIMING_DEPTH 14u

/* One captured master-event timing record. Layout is wire-stable: see
 * RSP_DEBUG_TIMING in command_processor.c and python/feralrf/_responses.py.
 * Session 4 grew this from 13 to 17 wire bytes by adding nRxOk, nRxNok,
 * nRxIgnored, and a packed pktStatus byte from rfc_bleMasterSlaveOutput_t. */
typedef struct {
    uint16_t eventIdx;     /* s_event_counter at capture time */
    uint32_t startRAT;     /* curHopTime fed to RadioIF_bleCentral */
    uint32_t endRAT;       /* s_next_hop_time fed to RadioIF_bleCentral */
    uint16_t status;       /* RF status code returned by the command */
    uint8_t  numSent;      /* pOutput.nTxEntryDone */
    uint8_t  nTx;          /* pOutput.nTx — total TX incl. auto-empty (Session 4) */
    uint8_t  nRxOk;        /* pOutput.nRxOk (Session 4) */
    uint8_t  nRxNok;       /* pOutput.nRxNok (Session 4) */
    uint8_t  nRxIgnored;   /* pOutput.nRxIgnored (Session 4) */
    uint8_t  pktStatus;    /* packed pktStatus bitfield, see RadioIF_BleCentralStats */
} BleConnMgr_DbgTimingEntry;

/* Returns up to maxEntries snapshots of the most recent master events,
 * oldest first. The returned count equals min(active entries, maxEntries). */
uint8_t BleConnMgr_getDebugTiming(BleConnMgr_DbgTimingEntry *out, uint8_t maxEntries);

#endif /* BLE_CONN_MGR_H */
