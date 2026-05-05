/* FeralRF CC1352 - L2CAP/ATT RX dispatcher header (F20.a.1). */
#ifndef BLE20_DISPATCH_H
#define BLE20_DISPATCH_H

#include <stdint.h>

void Ble20_drainAndDispatch(uint8_t *reason_out);

#endif /* BLE20_DISPATCH_H */
