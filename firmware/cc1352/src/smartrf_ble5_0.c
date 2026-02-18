/*
 * Minimal BLE5 SmartRF settings adapted from CatSniffer example.
 */

#include "smartrf_ble5_0.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
/* clang-format on */

RF_Mode Ble5_0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_multi_protocol,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

uint32_t Ble5_0_pOverridesCommon[] = {
    HW_REG_OVERRIDE(0x6024, 0x4C20),
    (uint32_t)0x01500263,
    HW_REG_OVERRIDE(0x5328, 0x0000),
    (uint32_t)0x00FF8A53,
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides1Mbps[] = {
    HW_REG_OVERRIDE(0x5320, 0x05A0), (uint32_t)0x017B02A3, HW_REG_OVERRIDE(0x6098, 0x25F8),
    HW_REG_OVERRIDE(0x60A0, 0x0026), (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides2Mbps[] = {
    HW_REG_OVERRIDE(0x5320, 0x05A0),
    (uint32_t)0x011902A3,
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverridesCoded[] = {
    HW_REG_OVERRIDE(0x5320, 0x05A0),
    (uint32_t)0x07D102A3,
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverridesTxStd[] = {
    TX_STD_POWER_OVERRIDE(0x941E),
    (uint32_t)0x05320703,
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverridesTx20[] = {
    TX20_POWER_OVERRIDE(0x003F5BB8),
    (uint32_t)0x01C20703,
    (uint32_t)0xFFFFFFFF,
};

rfc_CMD_BLE5_RADIO_SETUP_PA_t Ble5_0_cmdBle5RadioSetup = {
    .commandNo = 0x1820,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .defaultPhy.mainMode = 0x0,
    .defaultPhy.coding = 0x0,
    .loDivider = 0x00,
    .config.frontEndMode = 0x0,
    .config.biasMode = 0x0,
    .config.analogCfgMode = 0x0,
    .config.bNoFsPowerUp = 0x0,
    .txPower = 0x941E,
    .pRegOverrideCommon = Ble5_0_pOverridesCommon,
    .pRegOverride1Mbps = Ble5_0_pOverrides1Mbps,
    .pRegOverride2Mbps = Ble5_0_pOverrides2Mbps,
    .pRegOverrideCoded = Ble5_0_pOverridesCoded,
    .pRegOverrideTxStd = Ble5_0_pOverridesTxStd,
    .pRegOverrideTx20 = Ble5_0_pOverridesTx20,
};

rfc_CMD_FS_t Ble5_0_cmdFs = {
    .commandNo = 0x0803,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .frequency = 0x0962,
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x0,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000,
};

static rfc_bleGenericRxPar_t s_bleGenericRxPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x1,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x1,
    .rxConfig.bAppendStatus = 0x1,
    .rxConfig.bAppendTimestamp = 0x0,
    .bRepeat = 0x01,
    .__dummy0 = 0x0000,
    .accessAddress = 0x8E89BED6,
    .crcInit0 = 0x55,
    .crcInit1 = 0x55,
    .crcInit2 = 0x55,
    .endTrigger.triggerType = 0x1,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000001,
};

rfc_CMD_BLE5_GENERIC_RX_t Ble5_0_cmdBle5GenericRx = {
    .commandNo = 0x1829,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x65,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_bleGenericRxPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};

static rfc_bleAdvPar_t s_bleAdvPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .advConfig.advFilterPolicy = 0x0,
    .advConfig.deviceAddrType = 0x1,
    .advConfig.peerAddrType = 0x0,
    .advConfig.bStrictLenFilter = 0x0,
    .advConfig.chSel = 0x0,
    .advConfig.privIgnMode = 0x0,
    .advConfig.rpaMode = 0x0,
    .advLen = 0x00,
    .scanRspLen = 0x00,
    .pAdvData = 0,
    .pScanRspData = 0,
    .pDeviceAddress = 0,
    .pWhiteList = 0,
    .behConfig.scanRspEndType = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .endTrigger.triggerType = 0x1,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x1,
    .endTime = 0x00000000,
};

rfc_CMD_BLE5_ADV_NC_t Ble5_0_cmdBle5AdvNc = {
    .commandNo = 0x182D,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x65,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_bleAdvPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};
