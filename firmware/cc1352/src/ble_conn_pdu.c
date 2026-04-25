#include "ble_conn_pdu.h"

#include <string.h>

uint8_t BleConnPdu_buildLlData(const BleConnIndFields *f, uint8_t *out) {
    out[0] = (uint8_t)(f->accessAddr & 0xFFu);
    out[1] = (uint8_t)((f->accessAddr >> 8) & 0xFFu);
    out[2] = (uint8_t)((f->accessAddr >> 16) & 0xFFu);
    out[3] = (uint8_t)((f->accessAddr >> 24) & 0xFFu);
    out[4] = (uint8_t)(f->crcInit & 0xFFu);
    out[5] = (uint8_t)((f->crcInit >> 8) & 0xFFu);
    out[6] = (uint8_t)((f->crcInit >> 16) & 0xFFu);
    out[7] = f->winSize;
    out[8] = (uint8_t)(f->winOffset & 0xFFu);
    out[9] = (uint8_t)((f->winOffset >> 8) & 0xFFu);
    out[10] = (uint8_t)(f->interval & 0xFFu);
    out[11] = (uint8_t)((f->interval >> 8) & 0xFFu);
    out[12] = (uint8_t)(f->latency & 0xFFu);
    out[13] = (uint8_t)((f->latency >> 8) & 0xFFu);
    out[14] = (uint8_t)(f->timeout & 0xFFu);
    out[15] = (uint8_t)((f->timeout >> 8) & 0xFFu);
    memcpy(&out[16], f->channelMap, 5);
    out[21] = (uint8_t)((f->hopIncrement & 0x1Fu) | ((f->sca & 0x07u) << 5));
    return BLE_CONN_LL_DATA_LEN;
}

uint8_t BleConnPdu_build(const BleConnIndFields *f, uint8_t *out) {
    uint8_t hdr0 = BLE_CONN_IND_PDU_TYPE & 0x0Fu;
    if (f->initAddrRandom) {
        hdr0 |= (uint8_t)(1u << 6);
    }
    if (f->advAddrRandom) {
        hdr0 |= (uint8_t)(1u << 7);
    }
    out[0] = hdr0;
    out[1] = BLE_CONN_IND_PAYLOAD_LEN;
    memcpy(&out[2], f->initAddr, 6);
    memcpy(&out[8], f->advAddr, 6);
    BleConnPdu_buildLlData(f, &out[14]);
    return BLE_CONN_IND_PDU_LEN;
}
