/*
 * FeralRF CC1352 - Packet Queue (fixed-size, no malloc)
 */

#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

#define PACKET_QUEUE_DEPTH 8u
#define PACKET_QUEUE_MAX_FRAME_SIZE (COBS_MAX_ENCODED + 1u)

void PacketQueue_init(void);
bool PacketQueue_enqueue(const uint8_t *data, size_t len);
bool PacketQueue_dequeue(uint8_t *out, size_t *len);
bool PacketQueue_isEmpty(void);
uint32_t PacketQueue_getDropCount(void);

#endif /* PACKET_QUEUE_H */
