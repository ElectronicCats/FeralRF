/*
 * FeralRF CC1352 - PHY Manager
 *
 * Centralizes PHY metadata in a table so control/radio logic does not rely on
 * hardcoded "if phy == X" checks.
 */

#ifndef PHY_MANAGER_H
#define PHY_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PHY_MANAGER_PHY_BLE_1M = 0u,
    PHY_MANAGER_PHY_BLE_2M = 1u,
    PHY_MANAGER_PHY_BLE_CODED_S8 = 2u,
    PHY_MANAGER_PHY_BLE_CODED_S2 = 3u,
    PHY_MANAGER_PHY_IEEE_802_15_4 = 4u,
    PHY_MANAGER_PHY_SUB_1GHZ_868 = 5u,
    PHY_MANAGER_PHY_SUB_1GHZ_915 = 6u,
    PHY_MANAGER_PHY_PROPRIETARY_GFSK = 7u,
    PHY_MANAGER_PHY_PROP_2_4GHZ = 8u,
} PhyManager_Phy;

typedef enum {
    PHY_MANAGER_LL_DEFAULT = 0u,
    PHY_MANAGER_LL_BLE = 1u,
} PhyManager_LinkLayer;

void PhyManager_init(void);
bool PhyManager_select(uint8_t phy);
uint8_t PhyManager_getSelected(void);
uint8_t PhyManager_getSelectedLinkLayer(void);
bool PhyManager_isValid(uint8_t phy);
bool PhyManager_isBlePhy(uint8_t phy);
bool PhyManager_isProp24ghzPhy(uint8_t phy);
bool PhyManager_supportsRfBackendRx(uint8_t phy);

#endif /* PHY_MANAGER_H */
