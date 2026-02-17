/*
 * FeralRF CC1352 - Packet Queue implementation
 */

#include "packet_queue.h"

typedef struct {
    uint16_t len;
    uint8_t data[PACKET_QUEUE_MAX_FRAME_SIZE];
} PacketQueue_Item;

static PacketQueue_Item s_queue[PACKET_QUEUE_DEPTH];
static uint16_t s_head = 0;
static uint16_t s_tail = 0;
static uint16_t s_count = 0;
static uint32_t s_drop_count = 0;

void PacketQueue_init(void) {
    s_head = 0;
    s_tail = 0;
    s_count = 0;
    s_drop_count = 0;
}

bool PacketQueue_enqueue(const uint8_t *data, size_t len) {
    if (data == NULL || len == 0 || len > PACKET_QUEUE_MAX_FRAME_SIZE) {
        return false;
    }

    if (s_count >= PACKET_QUEUE_DEPTH) {
        s_drop_count++;
        return false;
    }

    s_queue[s_head].len = (uint16_t)len;
    for (size_t i = 0; i < len; i++) {
        s_queue[s_head].data[i] = data[i];
    }

    s_head = (uint16_t)((s_head + 1u) % PACKET_QUEUE_DEPTH);
    s_count++;
    return true;
}

bool PacketQueue_dequeue(uint8_t *out, size_t *len) {
    if (out == NULL || len == NULL || s_count == 0u) {
        return false;
    }

    *len = s_queue[s_tail].len;
    for (size_t i = 0; i < *len; i++) {
        out[i] = s_queue[s_tail].data[i];
    }

    s_tail = (uint16_t)((s_tail + 1u) % PACKET_QUEUE_DEPTH);
    s_count--;
    return true;
}

bool PacketQueue_isEmpty(void) {
    return s_count == 0u;
}

uint32_t PacketQueue_getDropCount(void) {
    return s_drop_count;
}
