/*
 * FeralRF CC1352 - PHY Manager
 */

#include "phy_manager.h"

#include <stddef.h>

typedef struct {
    uint8_t phy;
    uint8_t ll;
    bool ble_family;
    bool rf_backend_rx_supported;
} PhyManager_Profile;

static const PhyManager_Profile s_phy_profiles[] = {
    {PHY_MANAGER_PHY_BLE_1M, PHY_MANAGER_LL_BLE, true, true},
    {PHY_MANAGER_PHY_BLE_2M, PHY_MANAGER_LL_BLE, true, true},
    {PHY_MANAGER_PHY_BLE_CODED_S8, PHY_MANAGER_LL_BLE, true, true},
    {PHY_MANAGER_PHY_BLE_CODED_S2, PHY_MANAGER_LL_BLE, true, true},
    {PHY_MANAGER_PHY_IEEE_802_15_4, PHY_MANAGER_LL_DEFAULT, false, true},
    {PHY_MANAGER_PHY_SUB_1GHZ_868, PHY_MANAGER_LL_DEFAULT, false, true},
    {PHY_MANAGER_PHY_SUB_1GHZ_915, PHY_MANAGER_LL_DEFAULT, false, true},
    {PHY_MANAGER_PHY_PROPRIETARY_GFSK, PHY_MANAGER_LL_DEFAULT, false, true},
    {PHY_MANAGER_PHY_PROP_2_4GHZ, PHY_MANAGER_LL_DEFAULT, false, true},
};

static uint8_t s_selected_phy = PHY_MANAGER_PHY_BLE_1M;

static const PhyManager_Profile *PhyManager_findProfile(uint8_t phy) {
    for (size_t i = 0; i < sizeof(s_phy_profiles) / sizeof(s_phy_profiles[0]); i++) {
        if (s_phy_profiles[i].phy == phy) {
            return &s_phy_profiles[i];
        }
    }
    return NULL;
}

void PhyManager_init(void) {
    s_selected_phy = PHY_MANAGER_PHY_BLE_1M;
}

bool PhyManager_select(uint8_t phy) {
    if (PhyManager_findProfile(phy) == NULL) {
        return false;
    }

    s_selected_phy = phy;
    return true;
}

uint8_t PhyManager_getSelected(void) {
    return s_selected_phy;
}

uint8_t PhyManager_getSelectedLinkLayer(void) {
    const PhyManager_Profile *profile = PhyManager_findProfile(s_selected_phy);
    if (profile == NULL) {
        return PHY_MANAGER_LL_DEFAULT;
    }
    return profile->ll;
}

bool PhyManager_isValid(uint8_t phy) {
    return PhyManager_findProfile(phy) != NULL;
}

bool PhyManager_isBlePhy(uint8_t phy) {
    const PhyManager_Profile *profile = PhyManager_findProfile(phy);
    return (profile != NULL) && profile->ble_family;
}

bool PhyManager_isProp24ghzPhy(uint8_t phy) {
    return phy == PHY_MANAGER_PHY_PROP_2_4GHZ;
}

bool PhyManager_supportsRfBackendRx(uint8_t phy) {
    const PhyManager_Profile *profile = PhyManager_findProfile(phy);
    return (profile != NULL) && profile->rf_backend_rx_supported;
}
