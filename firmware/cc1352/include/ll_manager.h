/*
 * FeralRF CC1352 - Link Layer Manager
 *
 * Provides a pluggable processing hook selected by PHY manager.
 */

#ifndef LL_MANAGER_H
#define LL_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "radio_if.h"

typedef enum {
    LL_MANAGER_DEFAULT = 0u,
    LL_MANAGER_BLE = 1u,
} LLManager_Profile;

typedef enum {
    LL_PDU_KIND_UNKNOWN = 0u,
    LL_PDU_KIND_ADV = 1u,
    LL_PDU_KIND_SCAN = 2u,
    LL_PDU_KIND_CONNECT = 3u,
    LL_PDU_KIND_DATA = 4u,
} LLManager_PduKind;

typedef enum {
    LL_PDU_FLAG_PRIMARY_ADV_CH = 0x01u,
    LL_PDU_FLAG_DATA_CH = 0x02u,
    LL_PDU_FLAG_EXT_ADV = 0x04u,
    LL_PDU_FLAG_RESERVED = 0x08u,
} LLManager_PduFlags;

typedef struct {
    uint32_t kind_unknown;
    uint32_t kind_adv;
    uint32_t kind_scan;
    uint32_t kind_connect;
    uint32_t kind_data;
} LLManager_Stats;

void LLManager_init(void);
void LLManager_select(uint8_t profile);
uint8_t LLManager_getSelected(void);
bool LLManager_processRxPacket(RadioIF_RxPacket *pkt);
uint32_t LLManager_getDroppedPackets(void);
void LLManager_getStats(LLManager_Stats *out);
void LLManager_resetStats(void);

#endif /* LL_MANAGER_H */
