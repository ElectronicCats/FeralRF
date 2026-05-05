/* FeralRF CC1352 - ATT server (F20.a.1).
 * Handles discovery + Read paths over L2CAP CID 0x0004.
 * Wired into BleConnMgr_pollSlave at Bundle 4. */
#ifndef ATT_SERVER_H
#define ATT_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#define ATT_DEFAULT_MTU 23u
#define ATT_MAX_RSP_LEN ATT_DEFAULT_MTU

/* TX queue: pending response to enqueue at next connection event.
 * Single-slot FIFO — A3.1 only ever has one outstanding RSP. */
void AttServer_init(void);
void AttServer_handleRequest(const uint8_t *pdu, uint8_t pdu_len);
bool AttServer_hasPendingTx(void);
uint8_t AttServer_takePendingTx(uint8_t *out_buf, uint8_t buf_len);

#endif /* ATT_SERVER_H */
