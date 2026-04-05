/*
 * BLE5 SmartRF settings for CC1352P7 — SDK 8.30.01.01
 * Overrides from SysConfig (SmartRF Studio 2.32.0) for LP_CC1352P7_1 HPA.
 * RF_Mode from Sniffle SysConfig output: bt5 CPE patch, no MCE/RFE.
 */

#include "smartrf_ble5_0.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_bt5.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
/* clang-format on */

/* Sniffle + SysConfig: bt5 CPE patch, no MCE/RFE */
RF_Mode Ble5_0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_bt5,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

/* === Overrides from SysConfig LP_CC1352P7_1 HPA (SDK 8.30) === */

uint32_t Ble5_0_pOverridesCommon[] = {
    (uint32_t)0x00F388D3,                /* IPEAK=3, DCDC dither off */
    (uint32_t)0x00058683,                /* Synth mid code cal 5us */
    HW32_ARRAY_OVERRIDE(0x4004, 0x0001),
    (uint32_t)0x38183C30,
    HW_REG_OVERRIDE(0x5328, 0x0000),     /* No CTE */
    HW_REG_OVERRIDE(0x4064, 0x003C),     /* Fine code = 60 */
    (uint32_t)0x00950803,                /* DTX threshold 1M */
    (uint32_t)0x012A0823,                /* DTX threshold 2M */
    (uint32_t)0x00E787E3,                /* DTX gain -5% 1M */
    (uint32_t)0x00F487F3,                /* DTX gain -2.5% 2M */
    HW32_ARRAY_OVERRIDE(0x4020, 0x0001),
    (uint32_t)0x41005F00,
    (uint32_t)0xC0040141,
    (uint32_t)0x0007DD44,
    HW_REG_OVERRIDE(0x6024, 0x5B20),     /* Pilot tone 35us */
    (uint32_t)0x01640263,                /* Compensate pilot */
    (uint32_t)0x000188A3,                /* RSSI offset -1dB */
    (uint32_t)0x00FF8A53,                /* Max RX len=255 (Sniffle) */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides1Mbps[] = {
    HW_REG_OVERRIDE(0x5320, 0x0690),     /* Pilot tone 35us */
    (uint32_t)0x018F02A3,                /* Compensate pilot */
    HW_REG_OVERRIDE(0x50D4, 0x00F9),     /* Symbol tracking */
    HW_REG_OVERRIDE(0x50E0, 0x0087),
    HW_REG_OVERRIDE(0x50F8, 0x0014),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverrides2Mbps[] = {
    HW_REG_OVERRIDE(0x60A4, 0x7D00),     /* AGC delay 2M */
    HW_REG_OVERRIDE(0x5320, 0x0690),
    (uint32_t)0x012D02A3,
    HW_REG_OVERRIDE(0x50D4, 0x00F9),
    HW_REG_OVERRIDE(0x50E0, 0x0087),
    HW_REG_OVERRIDE(0x50F8, 0x0014),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Ble5_0_pOverridesCoded[] = {
    HW_REG_OVERRIDE(0x5320, 0x0690),
    (uint32_t)0x07E502A3,
    HW_REG_OVERRIDE(0x609C, 0x0021),
    (uint32_t)0xFFFFFFFF,
};

/* TX power table — 2400 MHz, 5 dBm PA */
static RF_TxPowerTable_Entry s_txPowerTable_2400[] = {
    {-20, RF_TxPowerTable_DEFAULT_PA_ENTRY(8, 3, 0, 2)},
    {-15, RF_TxPowerTable_DEFAULT_PA_ENTRY(13, 3, 0, 3)},
    {-10, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 3, 0, 5)},
    {-5, RF_TxPowerTable_DEFAULT_PA_ENTRY(21, 2, 0, 11)},
    {0, RF_TxPowerTable_DEFAULT_PA_ENTRY(29, 1, 0, 22)},
    {3, RF_TxPowerTable_DEFAULT_PA_ENTRY(47, 1, 0, 36)},
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
    .config.bSynthNarrowBand = 0x0,
    .txPower = 0x762E,
    .pRegOverrideCommon = Ble5_0_pOverridesCommon,
    .pRegOverride1Mbps = Ble5_0_pOverrides1Mbps,
    .pRegOverride2Mbps = Ble5_0_pOverrides2Mbps,
    .pRegOverrideCoded = Ble5_0_pOverridesCoded,
    .pRegOverrideTxStd = 0,
    .pRegOverrideTx20 = 0,
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
    .frequency = 0x0962,    /* 2402 MHz (ch37) */
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x0, /* RX mode (Sniffle uses 0x0) */
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000,
};

/* RX output structure — rfDiagnostics requires non-NULL pOutput */
static rfc_ble5ScanInitOutput_t s_bleRxOutput;

static rfc_bleGenericRxPar_t s_bleGenericRxPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x1,
    .rxConfig.bAutoFlushCrcErr = 0x1,
    .rxConfig.bAutoFlushEmpty = 0x1,
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
    .pOutput = &s_bleRxOutput,    /* non-NULL (rfDiagnostics pattern) */
    .tx20Power = 0x00000000,
};

/* === Scanner, ADV commands (for later phases) === */

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
    .endTime = 40000u,
};

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
