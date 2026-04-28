/*
 * FeralRF CC1352 - Packet Queue (fixed-size, no malloc)
 */

#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

/* 32 entries: GATT discovery can burst-queue up to ~13 frames
 * (3-5 services + ~9 chars + GATT_DONE) faster than HostIFTask_poll can
 * drain UART, because RF_runCmd(BLE5_MASTER) blocks the cooperative scheduler
 * for the entire connection event window (~10 ms). 8 entries overflowed and
 * silently dropped RSP_GATT_DONE. */
#define PACKET_QUEUE_DEPTH 32u
#define PACKET_QUEUE_MAX_FRAME_SIZE (COBS_MAX_ENCODED + 1u)

void PacketQueue_init(void);
bool PacketQueue_enqueue(const uint8_t *data, size_t len);
bool PacketQueue_dequeue(uint8_t *out, size_t *len);
bool PacketQueue_isEmpty(void);
uint32_t PacketQueue_getDropCount(void);

#endif /* PACKET_QUEUE_H */
