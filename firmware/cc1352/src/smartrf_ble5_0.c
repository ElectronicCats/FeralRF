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

/* bt5 patch: required for CMD_BLE5_ADV_EXT + CMD_BLE5_ADV_AUX (2M extended advertising).
 * multi_protocol does NOT support CMD_BLE5_ADV_AUX (hangs).
 * 868/IEEE mode-switch works fine with bt5 — 868 TX weakness on some boards is hardware. */
RF_Mode Ble5_0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_bt5,
    .mcePatchFxn = &rf_patch_mce_bt5,
    .rfePatchFxn = 0,
};

/* Common overrides for SDK 8.30 BLE5 (from SysConfig output) */
uint32_t Ble5_0_pOverridesCommon[] = {
    /* Bluetooth 5: Set IPEAK = 3 and DCDC dither off for TX */
    (uint32_t)0x00F388D3,
    /* Synth: Increase mid code calibration time to 5 us */
    (uint32_t)0x00058683,
    HW32_ARRAY_OVERRIDE(0x4004, 0x0001),
    (uint32_t)0x38183C30,
    /* Bluetooth 5: Default to no CTE */
    HW_REG_OVERRIDE(0x5328, 0x0000),
    /* Synth: Set calibration fine point code to 60 */
    HW_REG_OVERRIDE(0x4064, 0x003C),
    /* Bluetooth 5: Set DTX threshold 1 Mbps */
    (uint32_t)0x00950803,
    /* Bluetooth 5: Set DTX threshold 2 Mbps */
    (uint32_t)0x012A0823,
    /* Bluetooth 5: Set DTX gain -5% for 1 Mbps */
    (uint32_t)0x00E787E3,
    /* Bluetooth 5: Set DTX gain -2.5% for 2 Mbps */
    (uint32_t)0x00F487F3,
    /* Bluetooth 5: Set synth fine code calibration interval */
    HW32_ARRAY_OVERRIDE(0x4020, 0x0001),
    (uint32_t)0x41005F00,
    /* Bluetooth 5: Adapt to synth fine code calibration interval */
    (uint32_t)0xC0040141,
    (uint32_t)0x0007DD44,
    /* Bluetooth 5: Set pilot tone length to 35 us */
    HW_REG_OVERRIDE(0x6024, 0x5B20),
    /* Bluetooth 5: Compensate for 35 us pilot tone length */
    (uint32_t)0x01640263,
    /* Rx: Set RSSI offset to adjust reported RSSI by -1 dB */
    (uint32_t)0x000188A3,
#ifdef ICALL_JT
    BLE_STACK_OVERRIDES(),
#endif
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides1Mbps[] = {
    /* Bluetooth 5: Set pilot tone length to 35 us */
    HW_REG_OVERRIDE(0x5320, 0x0690),
    /* Bluetooth 5: Compensate for modified pilot tone length */
    (uint32_t)0x018F02A3,
    /* Symbol tracking: timing correction */
    HW_REG_OVERRIDE(0x50D4, 0x00F9),
    /* Symbol tracking: reduce sample delay */
    HW_REG_OVERRIDE(0x50E0, 0x0087),
    /* Symbol tracking: demodulation order */
    HW_REG_OVERRIDE(0x50F8, 0x0014),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides2Mbps[] = {
    /* PHY: Use MCE RAM (patch), RFE ROM */
    MCE_RFE_OVERRIDE(1, 0, 2, 0, 3, 2),
    /* Rx: increase AGC hysteresis */
    HW_REG_OVERRIDE(0x6098, 0x75FB),
    /* Bluetooth 5: increase low gain AGC delay for 2 Mbps */
    HW_REG_OVERRIDE(0x60A4, 0x7D00),
    HW_REG_OVERRIDE(0x5320, 0x0690),
    (uint32_t)0x012D02A3,
    /* Symbol tracking */
    HW_REG_OVERRIDE(0x50D4, 0x00F9),
    HW_REG_OVERRIDE(0x50E0, 0x0087),
    HW_REG_OVERRIDE(0x50F8, 0x0014),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverridesCoded[] = {
    HW_REG_OVERRIDE(0x5320, 0x0690),
    (uint32_t)0x07E502A3,
    /* Bluetooth 5: Set AGC magnitude target */
    HW_REG_OVERRIDE(0x609C, 0x0021),
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
    .endTrigger.triggerType = TRIG_NEVER,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
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

/* F21 BLE Connectable Advertiser — separate params struct so the NC path
 * (s_bleAdvPar / Ble5_0_cmdBleAdvNc) stays untouched. Used by ADV_IND,
 * ADV_DIRECT_IND, ADV_SCAN_IND. RadioIF_transmitBleAdvLegacy populates
 * fields per call. */
rfc_bleAdvPar_t s_f21_bleAdvPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .advConfig.advFilterPolicy = 0x0, /* allow any scanner/initiator */
    .advConfig.deviceAddrType = 0x1,
    .advConfig.peerAddrType = 0x1,
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
    .endTrigger.triggerType = TRIG_NEVER,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x1,
    .endTime = 0x00000000,
};

rfc_CMD_BLE_ADV_t Ble5_0_cmdBleAdv = {
    .commandNo = CMD_BLE_ADV,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0x0,
    .pParams = &s_f21_bleAdvPar,
    .pOutput = 0,
};

rfc_CMD_BLE_ADV_DIR_t Ble5_0_cmdBleAdvDir = {
    .commandNo = CMD_BLE_ADV_DIR,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0x0,
    .pParams = &s_f21_bleAdvPar,
    .pOutput = 0,
};

rfc_CMD_BLE_ADV_SCAN_t Ble5_0_cmdBleAdvScan = {
    .commandNo = CMD_BLE_ADV_SCAN,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0x0,
    .pParams = &s_f21_bleAdvPar,
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

/* Extended Advertising: ADV_EXT_IND (primary channel, 1M) — base for BLE 5.0 attacks */
static rfc_ble5AdvExtPar_t s_ble5AdvExtPar = {
    .advConfig.deviceAddrType = 0x1, /* random address */
    .auxPtrTargetType = TRIG_REL_START,
    .auxPtrTargetTime = 0,
    .pAdvPkt = 0,
    .pDeviceAddress = 0,
};

rfc_CMD_BLE5_ADV_EXT_t Ble5_0_cmdBle5AdvExt = {
    .commandNo = CMD_BLE5_ADV_EXT,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0x0,
    .phyMode.mainMode = 0x0, /* 1M for primary */
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_ble5AdvExtPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};

/* Extended Advertising: AUX_ADV_IND (secondary channel, 2M) — used for BLE 2M TX */
static rfc_ble5AdvAuxPar_t s_ble5AdvAuxPar = {
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
    .advConfig.targetAddrType = 0x0,
    .advConfig.bStrictLenFilter = 0x0,
    .advConfig.bDirected = 0x0,
    .advConfig.privIgnMode = 0x0,
    .advConfig.rpaMode = 0x0,
    .behConfig.scanRspEndType = 0x0,
    .auxPtrTargetType = 0,
    .auxPtrTargetTime = 0,
    .pAdvPkt = 0,
    .pRspPkt = 0,
    .pDeviceAddress = 0,
    .pWhiteList = 0,
};

rfc_CMD_BLE5_ADV_AUX_t Ble5_0_cmdBle5AdvAux = {
    .commandNo = CMD_BLE5_ADV_AUX,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .channel = 0x09, /* data channel 9 (default for 2M) */
    .whitening.init = 0x0,
    .whitening.bOverride = 0x0,
    .phyMode.mainMode = 0x1, /* 2M */
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_ble5AdvAuxPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};

/* ── BLE5 Initiator (CMD_BLE5_INITIATOR, 0x1828) ── */
static rfc_ble5InitiatorPar_t s_ble5InitiatorPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .initConfig.bUseWhiteList = 0x0,
    .initConfig.bDynamicWinOffset = 0x0,
    .initConfig.deviceAddrType = 0x0,
    .initConfig.peerAddrType = 0x0,
    .initConfig.bStrictLenFilter = 0x0,
    .initConfig.chSel = 0x0,
    .randomState = 0x0000,
    .backoffCount = 0x0001,
    .backoffPar.logUpperLimit = 0x0,
    .backoffPar.bLastSucceeded = 0x0,
    .backoffPar.bLastFailed = 0x0,
    .connectReqLen = 0x00,
    .pConnectReqData = 0,
    .pDeviceAddress = 0,
    .pWhiteList = 0,
    .connectTime = 0x00000000,
    .maxWaitTimeForAuxCh = 0x0000,
    .timeoutTrigger.triggerType = 0x0,
    .timeoutTrigger.bEnaCmd = 0x0,
    .timeoutTrigger.triggerNo = 0x0,
    .timeoutTrigger.pastTrig = 0x0,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .timeoutTime = 0x00000000,
    .endTime = 0x00000000,
    .rxStartTime = 0x00000000,
    .rxListenTime = 0x0000,
    .channelNo = 0x00,
    .phyMode = 0x00,
};

static rfc_ble5ScanInitOutput_t s_ble5InitiatorOutput;

rfc_CMD_BLE5_INITIATOR_t Ble5_0_cmdBle5Initiator = {
    .commandNo = 0x1828,
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
    .pParams = &s_ble5InitiatorPar,
    .pOutput = &s_ble5InitiatorOutput,
    .tx20Power = 0x00000000,
};

/* ── BLE5 Master / Central (CMD_BLE5_MASTER, 0x1822) ── */
static rfc_ble5MasterPar_t s_ble5MasterPar = {
    .pRxQ = 0,
    .pTxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .seqStat.lastRxSn = 0x0,
    .seqStat.lastTxSn = 0x0,
    .seqStat.nextTxSn = 0x0,
    .seqStat.bFirstPkt = 0x0,
    .seqStat.bAutoEmpty = 0x0,
    .seqStat.bLlCtrlTx = 0x0,
    .seqStat.bLlCtrlAckRx = 0x0,
    .seqStat.bLlCtrlAckPending = 0x0,
    .maxNack = 0x00,
    .maxPkt = 0x00,
    .accessAddress = 0x00000000,
    .crcInit0 = 0x00,
    .crcInit1 = 0x00,
    .crcInit2 = 0x00,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
    .maxRxPktLen = 0xFF,
};

rfc_CMD_BLE5_MASTER_t Ble5_0_cmdBle5Master = {
    .commandNo = 0x1822,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x00,
    .whitening.init = 0x40,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_ble5MasterPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};
