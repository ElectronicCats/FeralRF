#ifndef FERALRF_BLE_CONN_PDU_H
#define FERALRF_BLE_CONN_PDU_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_CONN_IND_PDU_TYPE 0x05u   /* bits [3:0] of header byte 0 */
#define BLE_CONN_IND_PAYLOAD_LEN 34u  /* 6 InitA + 6 AdvA + 22 LLData */
#define BLE_CONN_IND_PDU_LEN 36u      /* 2 header + 34 payload */
#define BLE_CONN_LL_DATA_LEN 22u

typedef struct {
    uint8_t initAddr[6];   /* little-endian as transmitted */
    bool initAddrRandom;   /* TxAdd */
    uint8_t advAddr[6];
    bool advAddrRandom;    /* RxAdd */
    uint32_t accessAddr;
    uint32_t crcInit;      /* 24-bit, lower bits only */
    uint8_t winSize;       /* 1.25 ms units */
    uint16_t winOffset;    /* 1.25 ms units */
    uint16_t interval;     /* 1.25 ms units */
    uint16_t latency;
    uint16_t timeout;      /* 10 ms units */
    uint8_t channelMap[5];
    uint8_t hopIncrement;  /* 5 bits */
    uint8_t sca;           /* 3 bits */
} BleConnIndFields;

/* Fills the 22-byte LLData portion. Returns number of bytes written (always 22). */
uint8_t BleConnPdu_buildLlData(const BleConnIndFields *f, uint8_t *out);

/* Fills the full 36-byte CONNECT_IND PDU (2 header + 34 payload). Returns 36. */
uint8_t BleConnPdu_build(const BleConnIndFields *f, uint8_t *out);

#endif /* FERALRF_BLE_CONN_PDU_H */
