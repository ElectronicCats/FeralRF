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

#endif /* BLE_CONN_MGR_H */
