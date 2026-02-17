/*
 * FeralRF CC1352 - Link Layer Manager
 */

#include "ll_manager.h"

#include <stddef.h>

#include "phy_manager.h"

static uint8_t s_selected_profile = LL_MANAGER_DEFAULT;

void LLManager_init(void) {
    s_selected_profile = LL_MANAGER_DEFAULT;
}

void LLManager_select(uint8_t profile) {
    if (profile == PHY_MANAGER_LL_BLE) {
        s_selected_profile = LL_MANAGER_BLE;
        return;
    }

    s_selected_profile = LL_MANAGER_DEFAULT;
}

uint8_t LLManager_getSelected(void) {
    return s_selected_profile;
}

void LLManager_processRxPacket(RadioIF_RxPacket *pkt) {
    if (pkt == NULL) {
        return;
    }

    switch (s_selected_profile) {
    case LL_MANAGER_BLE:
        /* BLE parser path will extend metadata here in future iterations. */
        break;
    case LL_MANAGER_DEFAULT:
    default:
        break;
    }
}
