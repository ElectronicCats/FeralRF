/*
 * BLE Channel Selection Algorithm #2 (CSA#2)
 * Based on Sniffle csa2.c — Copyright (c) 2018, NCC Group plc (GPLv3)
 */

#ifndef CSA2_H
#define CSA2_H

#include <stdint.h>

void csa2_computeMapping(uint32_t accessAddress, uint64_t channelMap);
uint8_t csa2_computeChannel(uint32_t connEventCounter);

#endif /* CSA2_H */
