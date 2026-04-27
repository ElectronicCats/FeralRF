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

#define BLE_CONN_MGR_DBG_TIMING_DEPTH 16u

/* One captured master-event timing record. Layout is wire-stable: see
 * RSP_DEBUG_TIMING in command_processor.c and python/feralrf/_responses.py. */
typedef struct {
    uint16_t eventIdx; /* s_event_counter at capture time */
    uint32_t startRAT; /* curHopTime fed to RadioIF_bleCentral */
    uint32_t endRAT;   /* s_next_hop_time fed to RadioIF_bleCentral */
    uint16_t status;   /* RF status code returned by the command */
    uint8_t numSent;   /* nTxEntryDone returned by the command */
} BleConnMgr_DbgTimingEntry;

/* Returns up to maxEntries snapshots of the most recent master events,
 * oldest first. The returned count equals min(active entries, maxEntries). */
uint8_t BleConnMgr_getDebugTiming(BleConnMgr_DbgTimingEntry *out, uint8_t maxEntries);

#endif /* BLE_CONN_MGR_H */
