/*
 * FeralRF CC1352 - Link Layer Manager
 *
 * Provides a pluggable processing hook selected by PHY manager.
 */

#ifndef LL_MANAGER_H
#define LL_MANAGER_H

#include <stdint.h>

#include "radio_if.h"

typedef enum {
    LL_MANAGER_DEFAULT = 0u,
    LL_MANAGER_BLE = 1u,
} LLManager_Profile;

void LLManager_init(void);
void LLManager_select(uint8_t profile);
uint8_t LLManager_getSelected(void);
void LLManager_processRxPacket(RadioIF_RxPacket *pkt);

#endif /* LL_MANAGER_H */
