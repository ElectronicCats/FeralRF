/*
 * BLE5 SmartRF settings for CC1352P7 — SDK 8.30.01.01 compatible.
 * Based on wero's justworks_scanner SysConfig output (SmartRF Studio 2.32.0).
 * Uses rf_patch_cpe_bt5 + rf_patch_mce_bt5 (not multi_protocol).
 */

#include "smartrf_ble5_0.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_bt5.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_mce_bt5.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
/* clang-format on */

/* BLE stack overrides — only if BLE stack is linked */
#ifdef ICALL_JT
#include <ti/ble5stack/icall/inc/ble_overrides.h>
#endif

RF_Mode Ble5_0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_multi_protocol,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

/* Overrides — CatSniffer reference (proven working for GENERIC_RX) */
uint32_t Ble5_0_pOverridesCommon[] = {
    /* Reconfigure pilot tone length */
    HW_REG_OVERRIDE(0x6024, 0x4C20),
    /* Compensate for modified pilot tone length */
    (uint32_t)0x01500263,
    /* Default to no CTE */
    HW_REG_OVERRIDE(0x5328, 0x0000),
    /* Support for BLE 4.2 Data Length Extension */
    (uint32_t)0x00FF8A53,
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides1Mbps[] = {
    /* Reconfigure pilot tone length */
    HW_REG_OVERRIDE(0x5320, 0x05A0),
    /* Compensate for modified pilot tone length */
    (uint32_t)0x017B02A3,
    /* AGC tuning */
    HW_REG_OVERRIDE(0x6098, 0x25F8),
    HW_REG_OVERRIDE(0x60A0, 0x0026),
    (uint32_t)0xFFFFFFFF,
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

/* TX power table — 2400 MHz, 5 dBm PA (from wero's SysConfig) */
static RF_TxPowerTable_Entry s_txPowerTable_2400[] = {
    {-20, RF_TxPowerTable_DEFAULT_PA_ENTRY(8, 3, 0, 2)},
    {-18, RF_TxPowerTable_DEFAULT_PA_ENTRY(10, 3, 0, 2)},
    {-15, RF_TxPowerTable_DEFAULT_PA_ENTRY(13, 3, 0, 3)},
    {-12, RF_TxPowerTable_DEFAULT_PA_ENTRY(16, 3, 0, 5)},
    {-10, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 3, 0, 5)},
    {-9, RF_TxPowerTable_DEFAULT_PA_ENTRY(20, 3, 0, 6)},
    {-6, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 2, 0, 11)},
    {-5, RF_TxPowerTable_DEFAULT_PA_ENTRY(21, 2, 0, 11)},
    {-3, RF_TxPowerTable_DEFAULT_PA_ENTRY(25, 2, 0, 12)},
    {0, RF_TxPowerTable_DEFAULT_PA_ENTRY(29, 1, 0, 22)},
    {1, RF_TxPowerTable_DEFAULT_PA_ENTRY(33, 1, 0, 25)},
    {2, RF_TxPowerTable_DEFAULT_PA_ENTRY(38, 1, 0, 31)},
    {3, RF_TxPowerTable_DEFAULT_PA_ENTRY(47, 1, 0, 36)},
    {4, RF_TxPowerTable_DEFAULT_PA_ENTRY(32, 0, 0, 65)},
    {5, RF_TxPowerTable_DEFAULT_PA_ENTRY(46, 0, 0, 59)},
    RF_TxPowerTable_TERMINATION_ENTRY,
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
    .config.biasMode = 0x1,
    .config.analogCfgMode = 0x0,
    .config.bNoFsPowerUp = 0x0,
    .txPower = 0x762E,
    .pRegOverrideCommon = Ble5_0_pOverridesCommon,
    .pRegOverride1Mbps = Ble5_0_pOverrides1Mbps,
    .pRegOverride2Mbps = Ble5_0_pOverrides2Mbps,
    .pRegOverrideCoded = Ble5_0_pOverridesCoded,
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
    .synthConf.bTxMode = 0x1,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000,
};

static rfc_bleGenericRxPar_t s_bleGenericRxPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x1,
    .rxConfig.bAutoFlushCrcErr = 0x1,
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
    .endTrigger.triggerType = TRIG_NEVER,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
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

/* BLE5 Scanner */
static uint16_t s_bleScannerDevAddr[3] = {0xDDEE, 0xBBCC, 0xC0AA};

static rfc_ble5ScannerPar_t s_bleScannerPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x1,
    .rxConfig.bIncludeCrc = 0x1,
    .rxConfig.bAppendRssi = 0x1,
    .rxConfig.bAppendStatus = 0x1,
    .rxConfig.bAppendTimestamp = 0x1,
    .scanConfig.scanFilterPolicy = 0x0,
    .scanConfig.bActiveScan = 0x0,
    .scanConfig.deviceAddrType = 0x1,
    .scanConfig.rpaFilterPolicy = 0x0,
    .scanConfig.bStrictLenFilter = 0x0,
    .scanConfig.bAutoWlIgnore = 0x0,
    .scanConfig.bEndOnRpt = 0x0,
    .scanConfig.rpaMode = 0x0,
    .randomState = 0,
    .backoffCount = 1,
    .backoffPar.logUpperLimit = 0,
    .backoffPar.bLastSucceeded = 0,
    .backoffPar.bLastFailed = 0,
    .extFilterConfig.bCheckAdi = 0,
    .extFilterConfig.bAutoAdiUpdate = 0,
    .extFilterConfig.bApplyDuplicateFiltering = 0,
    .extFilterConfig.bAutoWlIgnore = 0,
    .extFilterConfig.bAutoAdiProcess = 0,
    .extFilterConfig.bExclusiveSid = 0,
    .extFilterConfig.bAcceptSyncInfo = 0,
    .adiStatus.lastAcceptedSid = 0,
    .adiStatus.state = 0,
    .__dummy0 = 0,
    .__dummy1 = 0,
    .pDeviceAddress = s_bleScannerDevAddr,
    .pWhiteList = 0,
    .pAdiList = 0,
    .maxWaitTimeForAuxCh = 0,
    .timeoutTrigger.triggerType = TRIG_NEVER,
    .endTrigger.triggerType = TRIG_NEVER,
    .timeoutTime = 0,
    .endTime = 0,
    .rxStartTime = 0,
    .rxListenTime = 0,
    .channelNo = 0,
    .phyMode = 0,
};

rfc_CMD_BLE5_SCANNER_t Ble5_0_cmdBle5Scanner = {
    .commandNo = 0x1827,
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
    .pParams = &s_bleScannerPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};

/* ADV commands */
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
    .endTrigger.triggerType = TRIG_REL_START,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 40000u, /* 10ms at 4MHz RAT — enough for one ADV packet */
};

static rfc_bleAdvOutput_t s_bleAdvOutput;

rfc_CMD_BLE_ADV_NC_t Ble5_0_cmdBleAdvNc = {
    .commandNo = CMD_BLE_ADV_NC,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0x0,
    .pParams = &s_bleAdvPar,
    .pOutput = 0,
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
