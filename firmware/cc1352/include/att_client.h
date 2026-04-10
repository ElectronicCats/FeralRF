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
    ATT_STATE_DONE,
    ATT_STATE_ERROR,
} AttClient_State;

/* Callback for GATT results sent to host */
typedef void (*AttClient_ServiceCb)(uint16_t startHandle, uint16_t endHandle,
                                     const uint8_t *uuid, uint8_t uuidLen);
typedef void (*AttClient_CharCb)(uint16_t handle, uint8_t properties,
                                  uint16_t valueHandle,
                                  const uint8_t *uuid, uint8_t uuidLen);
typedef void (*AttClient_ReadCb)(uint16_t handle, const uint8_t *data, uint8_t len);
typedef void (*AttClient_DoneCb)(uint8_t status); /* 0=ok, 1=error, 2=timeout */

typedef struct {
    AttClient_ServiceCb onService;
    AttClient_CharCb onChar;
    AttClient_ReadCb onRead;
    AttClient_DoneCb onDone;
} AttClient_Callbacks;

void AttClient_init(void);
void AttClient_setCallbacks(const AttClient_Callbacks *cb);

/* Start full GATT discovery (services + characteristics) */
bool AttClient_startDiscover(void);

/* Read a characteristic value by handle */
bool AttClient_startRead(uint16_t handle);

/* Write a characteristic value by handle */
bool AttClient_startWrite(uint16_t handle, const uint8_t *data, uint8_t len);

/* Called by BleConnMgr when L2CAP data arrives (LLID=1 or 2) */
void AttClient_onL2capRx(const uint8_t *l2capPayload, uint8_t len);

/* Called by BleConnMgr each poll to queue pending ATT requests */
void AttClient_poll(void);

/* Reset on disconnect */
void AttClient_reset(void);

AttClient_State AttClient_getState(void);

#endif /* ATT_CLIENT_H */
