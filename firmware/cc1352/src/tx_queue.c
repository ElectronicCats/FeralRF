/*
 * BLE TX Queue — circular buffer for data channel PDUs
 * Based on Sniffle TXQueue.c — Copyright (c) 2020-2022, NCC Group plc (GPLv3)
 */

#include "tx_queue.h"

#include <string.h>

#define TX_QUEUE_SIZE 8u
#define TX_QUEUE_MASK (TX_QUEUE_SIZE - 1u)
#define TX_QUEUE_PACKET_SIZE 258u

static uint8_t s_packet_buf[TX_QUEUE_PACKET_SIZE * TX_QUEUE_SIZE];
static rfc_dataEntryPointer_t s_queue_entries[TX_QUEUE_SIZE];

static volatile uint32_t s_head;
static volatile uint32_t s_tail;

void TXQueue_init(void) {
    s_head = 0;
    s_tail = 0;

    for (uint32_t i = 0; i < TX_QUEUE_SIZE; i++) {
        uint32_t next_idx = (i + 1u) & TX_QUEUE_MASK;
        s_queue_entries[i].pNextEntry = (uint8_t *)(s_queue_entries + next_idx);
        s_queue_entries[i].status = DATA_ENTRY_PENDING;
        s_queue_entries[i].config.type = DATA_ENTRY_TYPE_PTR;
        s_queue_entries[i].config.lenSz = 0;
        s_queue_entries[i].length = 0;
        s_queue_entries[i].pData = s_packet_buf + (i * TX_QUEUE_PACKET_SIZE);
    }
}

bool TXQueue_insert(uint8_t len, uint8_t llid, const void *data) {
    if (((s_head - s_tail) & TX_QUEUE_MASK) == TX_QUEUE_MASK) {
        return false;
    }

    uint32_t h = s_head & TX_QUEUE_MASK;

    if (s_queue_entries[h].status == DATA_ENTRY_ACTIVE ||
        s_queue_entries[h].status == DATA_ENTRY_BUSY) {
        return false;
    }

    s_queue_entries[h].status = DATA_ENTRY_PENDING;
    s_queue_entries[h].length = 1u + len;
    uint8_t *pData = s_queue_entries[h].pData;
    *pData = llid & 0x3u;
    if (len > 0 && data != NULL) {
        memcpy(pData + 1, data, len);
    }

    s_head++;
    return true;
}

uint32_t TXQueue_take(dataQueue_t *pRFQueue) {
    uint32_t h = s_head;
    uint32_t t = s_tail;
    uint32_t qsize = (h - t) & TX_QUEUE_MASK;

    if (qsize) {
        uint32_t first = t & TX_QUEUE_MASK;
        uint32_t last = (h - 1u) & TX_QUEUE_MASK;
        pRFQueue->pCurrEntry = (uint8_t *)(s_queue_entries + first);
        pRFQueue->pLastEntry = (uint8_t *)(s_queue_entries + last);
    } else {
        pRFQueue->pCurrEntry = NULL;
        pRFQueue->pLastEntry = NULL;
    }

    return qsize;
}

void TXQueue_flush(uint32_t numEntries) {
    uint32_t qsize = (s_head - s_tail) & TX_QUEUE_MASK;
    if (numEntries > qsize) {
        numEntries = qsize;
    }
    s_tail += numEntries;
}
