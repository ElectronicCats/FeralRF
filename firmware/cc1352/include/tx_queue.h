/*
 * BLE TX Queue — circular buffer for data channel PDUs
 * Based on Sniffle TXQueue.c — Copyright (c) 2020-2022, NCC Group plc (GPLv3)
 */

#ifndef TX_QUEUE_H
#define TX_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */

#define TX_QUEUE_LLID_DATA_CONT 1u
#define TX_QUEUE_LLID_DATA_START 2u
#define TX_QUEUE_LLID_CTRL 3u

void TXQueue_init(void);
bool TXQueue_insert(uint8_t len, uint8_t llid, const void *data);
uint32_t TXQueue_take(dataQueue_t *pRFQueue);
void TXQueue_flush(uint32_t numEntries);

#endif /* TX_QUEUE_H */
