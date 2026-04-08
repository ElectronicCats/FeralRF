/*
 *  ======== ti_radio_config.c ========
 *  Configured RadioConfig module definitions
 *
 *  DO NOT EDIT - This file is generated for the CC1352P7RGZ
 *  by the SysConfig tool.
 *
 *  Radio Config module version : 1.20.0
 *  SmartRF Studio data version : 2.32.0
 */

#include "ti_radio_config.h"
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_bt5.h)

// Custom overrides
#include <sniffle_overrides.h>


// *********************************************************************************
//   RF Frontend configuration
// *********************************************************************************
// RF design based on: LP_CC1352P7-1

// TX Power tables
// The RF_TxPowerTable_DEFAULT_PA_ENTRY and RF_TxPowerTable_HIGH_PA_ENTRY macros are defined in RF.h.
// The following arguments are required:
// RF_TxPowerTable_DEFAULT_PA_ENTRY(bias, gain, boost, coefficient)
// RF_TxPowerTable_HIGH_PA_ENTRY(bias, ibboost, boost, coefficient, ldoTrim)
// See the Technical Reference Manual for further details about the "txPower" Command field.
// The PA settings require the CCFG_FORCE_VDDR_HH = 0 unless stated otherwise.

// 868 MHz, 13 dBm
RF_TxPowerTable_Entry txPowerTable_868_pa13[TXPOWERTABLE_868_PA13_SIZE] =
{
    {-20, RF_TxPowerTable_DEFAULT_PA_ENTRY(0, 3, 0, 2) }, // 0x04C0
    {-15, RF_TxPowerTable_DEFAULT_PA_ENTRY(1, 3, 0, 3) }, // 0x06C1
    {-10, RF_TxPowerTable_DEFAULT_PA_ENTRY(2, 3, 0, 5) }, // 0x0AC2
    {-5, RF_TxPowerTable_DEFAULT_PA_ENTRY(4, 3, 0, 5) }, // 0x0AC4
    {0, RF_TxPowerTable_DEFAULT_PA_ENTRY(8, 3, 0, 8) }, // 0x10C8
    {1, RF_TxPowerTable_DEFAULT_PA_ENTRY(9, 3, 0, 9) }, // 0x12C9
    {2, RF_TxPowerTable_DEFAULT_PA_ENTRY(10, 3, 0, 9) }, // 0x12CA
    {3, RF_TxPowerTable_DEFAULT_PA_ENTRY(11, 3, 0, 10) }, // 0x14CB
    {4, RF_TxPowerTable_DEFAULT_PA_ENTRY(13, 3, 0, 11) }, // 0x16CD
    {5, RF_TxPowerTable_DEFAULT_PA_ENTRY(14, 3, 0, 14) }, // 0x1CCE
    {6, RF_TxPowerTable_DEFAULT_PA_ENTRY(17, 3, 0, 16) }, // 0x20D1
    {7, RF_TxPowerTable_DEFAULT_PA_ENTRY(20, 3, 0, 19) }, // 0x26D4
    {8, RF_TxPowerTable_DEFAULT_PA_ENTRY(24, 3, 0, 22) }, // 0x2CD8
    {9, RF_TxPowerTable_DEFAULT_PA_ENTRY(28, 3, 0, 31) }, // 0x3EDC
    {10, RF_TxPowerTable_DEFAULT_PA_ENTRY(18, 2, 0, 31) }, // 0x3E92
    {11, RF_TxPowerTable_DEFAULT_PA_ENTRY(26, 2, 0, 51) }, // 0x669A
    {12, RF_TxPowerTable_DEFAULT_PA_ENTRY(30, 1, 1, 68) }, // 0x895E
    // The original PA value (12.5 dBm) has been rounded to an integer value.
    {13, RF_TxPowerTable_DEFAULT_PA_ENTRY(36, 0, 0, 89) }, // 0xB224
    // This setting requires CCFG_FORCE_VDDR_HH = 1.
    {14, RF_TxPowerTable_DEFAULT_PA_ENTRY(63, 0, 1, 0) }, // 0x013F
    RF_TxPowerTable_TERMINATION_ENTRY
};

// 868 MHz, 20 dBm
RF_TxPowerTable_Entry txPowerTable_868_pa20[TXPOWERTABLE_868_PA20_SIZE] =
{
    {14, RF_TxPowerTable_HIGH_PA_ENTRY(13, 0, 0, 28, 0) }, // 0x00380D
    {15, RF_TxPowerTable_HIGH_PA_ENTRY(18, 0, 0, 36, 0) }, // 0x004812
    {16, RF_TxPowerTable_HIGH_PA_ENTRY(24, 0, 0, 43, 0) }, // 0x005618
    {17, RF_TxPowerTable_HIGH_PA_ENTRY(28, 0, 0, 51, 2) }, // 0x02661C
    {18, RF_TxPowerTable_HIGH_PA_ENTRY(34, 0, 0, 64, 4) }, // 0x048022
    {19, RF_TxPowerTable_HIGH_PA_ENTRY(15, 3, 0, 36, 4) }, // 0x0448CF
    {20, RF_TxPowerTable_HIGH_PA_ENTRY(18, 3, 0, 71, 27) }, // 0x1B8ED2
    RF_TxPowerTable_TERMINATION_ENTRY
};


// 2400 MHz, 5 dBm
RF_TxPowerTable_Entry txPowerTable_2400_pa5[TXPOWERTABLE_2400_PA5_SIZE] =
{
    {-20, RF_TxPowerTable_DEFAULT_PA_ENTRY(8, 3, 0, 2) }, // 0x04C8
    {-18, RF_TxPowerTable_DEFAULT_PA_ENTRY(10, 3, 0, 2) }, // 0x04CA
    {-15, RF_TxPowerTable_DEFAULT_PA_ENTRY(13, 3, 0, 3) }, // 0x06CD
    {-12, RF_TxPowerTable_DEFAULT_PA_ENTRY(16, 3, 0, 5) }, // 0x0AD0
    {-10, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 3, 0, 5) }, // 0x0AD3
    {-9, RF_TxPowerTable_DEFAULT_PA_ENTRY(20, 3, 0, 6) }, // 0x0CD4
    {-6, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 2, 0, 11) }, // 0x1693
    {-5, RF_TxPowerTable_DEFAULT_PA_ENTRY(21, 2, 0, 11) }, // 0x1695
    {-3, RF_TxPowerTable_DEFAULT_PA_ENTRY(25, 2, 0, 12) }, // 0x1899
    {0, RF_TxPowerTable_DEFAULT_PA_ENTRY(29, 1, 0, 22) }, // 0x2C5D
    {1, RF_TxPowerTable_DEFAULT_PA_ENTRY(33, 1, 0, 25) }, // 0x3261
    {2, RF_TxPowerTable_DEFAULT_PA_ENTRY(38, 1, 0, 31) }, // 0x3E66
    {3, RF_TxPowerTable_DEFAULT_PA_ENTRY(47, 1, 0, 36) }, // 0x486F
    {4, RF_TxPowerTable_DEFAULT_PA_ENTRY(32, 0, 0, 65) }, // 0x8220
    {5, RF_TxPowerTable_DEFAULT_PA_ENTRY(46, 0, 0, 59) }, // 0x762E
    RF_TxPowerTable_TERMINATION_ENTRY
};



//*********************************************************************************
//  RF Setting:   BLE, 1 Mbps, LE 1M
//
//  PHY:          bt5le1m
//  Setting file: setting_bt5_le_1m.json
//*********************************************************************************

// PARAMETER SUMMARY
// Channel - Frequency (MHz): 2440
// TX Power (dBm): 5
// Whitening: true

// TI-RTOS RF Mode Object
RF_Mode RF_prop =
{
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_bt5,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0
};

// Overrides for CMD_BLE5_RADIO_SETUP_PA
uint32_t pOverridesCommon[] =
{
    // override_ble5_setup_override_common_hpa.json
    // Bluetooth 5: Set IPEAK = 3 and DCDC dither off for TX
    (uint32_t)0x00F388D3,
    // Synth: Increase mid code calibration time to 5 us
    (uint32_t)0x00058683,
    // Synth: Increase mid code calibration time to 5 us
    HW32_ARRAY_OVERRIDE(0x4004,0x0001),
    // Synth: Increase mid code calibration time to 5 us
    (uint32_t)0x38183C30,
    // Bluetooth 5: Default to no CTE.
    HW_REG_OVERRIDE(0x5328,0x0000),
    // Synth: Set calibration fine point code to 60 (default: 64)
    HW_REG_OVERRIDE(0x4064,0x003C),
    // Bluetooth 5: Set DTX threshold 1 Mbps
    (uint32_t)0x00950803,
    // Bluetooth 5: Set DTX threshold 2 Mbps
    (uint32_t)0x012A0823,
    // Bluetooth 5: Set DTX gain -5% for 1 Mbps
    (uint32_t)0x00E787E3,
    // Bluetooth 5: Set DTX gain -2.5% for 2 Mbps
    (uint32_t)0x00F487F3,
    // Bluetooth 5: Set synth fine code calibration interval
    HW32_ARRAY_OVERRIDE(0x4020,0x0001),
    // Bluetooth 5: Set synth fine code calibration interval
    (uint32_t)0x41005F00,
    // Bluetooth 5: Adapt to synth fine code calibration interval
    (uint32_t)0xC0040141,
    // Bluetooth 5: Adapt to synth fine code calibration interval
    (uint32_t)0x0007DD44,
    // Bluetooth 5: Set pilot tone length to 35 us
    HW_REG_OVERRIDE(0x6024,0x5B20),
    // Bluetooth 5: Compensate for 35 us pilot tone length
    (uint32_t)0x01640263,
    // Rx: Set RSSI offset to adjust reported RSSI by -1 dB
    (uint32_t)0x000188A3,
    // sniffle_overrides.h
    BLE_APP_OVERRIDES(),
    (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_BLE5_RADIO_SETUP_PA
uint32_t pOverrides1Mbps[] =
{
    // override_ble5_setup_override_1mbps_hpa.json
    // Bluetooth 5: Set pilot tone length to 35 us
    HW_REG_OVERRIDE(0x5320,0x0690),
    // Bluetooth 5: Compensate for modified pilot tone length
    (uint32_t)0x018F02A3,
    // override_ble5_symbol_error_tracking.json
    // Symbol tracking: timing correction
    HW_REG_OVERRIDE(0x50D4,0x00F9),
    // Symbol tracking: reduce sample delay
    HW_REG_OVERRIDE(0x50E0,0x0087),
    // Symbol tracking: demodulation order
    HW_REG_OVERRIDE(0x50F8,0x0014),
    (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_BLE5_RADIO_SETUP_PA
uint32_t pOverrides2Mbps[] =
{
    // override_ble5_setup_override_2mbps_hpa.json
    // Bluetooth 5: increase low gain AGC delay for 2 Mbps
    HW_REG_OVERRIDE(0x60A4,0x7D00),
    // Bluetooth 5: Set pilot tone length to 35 us
    HW_REG_OVERRIDE(0x5320,0x0690),
    // Bluetooth 5: Compensate for modified pilot tone length
    (uint32_t)0x012D02A3,
    // override_ble5_symbol_error_tracking.json
    // Symbol tracking: timing correction
    HW_REG_OVERRIDE(0x50D4,0x00F9),
    // Symbol tracking: reduce sample delay
    HW_REG_OVERRIDE(0x50E0,0x0087),
    // Symbol tracking: demodulation order
    HW_REG_OVERRIDE(0x50F8,0x0014),
    (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_BLE5_RADIO_SETUP_PA
uint32_t pOverridesCoded[] =
{
    // override_ble5_setup_override_coded_hpa.json
    // Bluetooth 5: Set pilot tone length to 35 us
    HW_REG_OVERRIDE(0x5320,0x0690),
    // Bluetooth 5: Compensate for modified pilot tone length
    (uint32_t)0x07E502A3,
    // Bluetooth 5: Set AGC mangnitude target to 0x21.
    HW_REG_OVERRIDE(0x609C,0x0021),
    (uint32_t)0xFFFFFFFF
};



// CMD_BLE5_RADIO_SETUP_PA
// Bluetooth 5 Radio Setup Command for all PHYs
rfc_CMD_BLE5_RADIO_SETUP_PA_t RF_cmdBle5RadioSetup =
{
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
    .pRegOverrideCommon = pOverridesCommon,
    .pRegOverride1Mbps = pOverrides1Mbps,
    .pRegOverride2Mbps = pOverrides2Mbps,
    .pRegOverrideCoded = pOverridesCoded,
    .pRegOverrideTxStd = 0,
    .pRegOverrideTx20 = 0
};

// CMD_FS
// Frequency Synthesizer Programming Command
rfc_CMD_FS_t RF_cmdFs =
{
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
    .frequency = 0x0988,
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x0,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000
};

// Structure for CMD_BLE5_GENERIC_RX.pParam
rfc_bleGenericRxPar_t bleGenericRxPar =
{
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x1,
    .rxConfig.bIncludeCrc = 0x1,
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
    .endTime = 0x00000001
};

// CMD_BLE5_GENERIC_RX
// Bluetooth 5 Generic Receiver Command
rfc_CMD_BLE5_GENERIC_RX_t RF_cmdBle5GenericRx =
{
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
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &bleGenericRxPar,
    .pOutput = 0,
    .tx20Power = 0x00000000
};

// Structure for CMD_BLE_ADV.pParam
rfc_bleAdvPar_t bleAdvPar =
{
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
    .advConfig.deviceAddrType = 0x0,
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
    .__dummy0 = 0x0000,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000
};

// CMD_BLE_ADV
// BLE Connectable Undirected Advertiser Command
rfc_CMD_BLE_ADV_t RF_cmdBleAdv =
{
    .commandNo = 0x1803,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .pParams = &bleAdvPar,
    .pOutput = 0
};

// Structure for CMD_BLE_SCANNER.pParam
rfc_bleScannerPar_t bleScannerPar =
{
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .scanConfig.scanFilterPolicy = 0x0,
    .scanConfig.bActiveScan = 0x0,
    .scanConfig.deviceAddrType = 0x0,
    .scanConfig.rpaFilterPolicy = 0x0,
    .scanConfig.bStrictLenFilter = 0x0,
    .scanConfig.bAutoWlIgnore = 0x0,
    .scanConfig.bEndOnRpt = 0x0,
    .scanConfig.rpaMode = 0x0,
    .randomState = 0x0000,
    .backoffCount = 0x0000,
    .backoffPar.logUpperLimit = 0x0,
    .backoffPar.bLastSucceeded = 0x0,
    .backoffPar.bLastFailed = 0x0,
    .scanReqLen = 0x00,
    .pScanReqData = 0,
    .pDeviceAddress = 0,
    .pWhiteList = 0,
    .__dummy0 = 0x0000,
    .timeoutTrigger.triggerType = 0x0,
    .timeoutTrigger.bEnaCmd = 0x0,
    .timeoutTrigger.triggerNo = 0x0,
    .timeoutTrigger.pastTrig = 0x0,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .timeoutTime = 0x00000000,
    .endTime = 0x00000000
};

// CMD_BLE_SCANNER
// BLE Scanner Command
rfc_CMD_BLE_SCANNER_t RF_cmdBleScanner =
{
    .commandNo = 0x1807,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .pParams = &bleScannerPar,
    .pOutput = 0
};

// Structure for CMD_BLE5_SLAVE.pParam
rfc_ble5SlavePar_t ble5SlavePar =
{
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
    .timeoutTrigger.triggerType = 0x0,
    .timeoutTrigger.bEnaCmd = 0x0,
    .timeoutTrigger.triggerNo = 0x0,
    .timeoutTrigger.pastTrig = 0x0,
    .timeoutTime = 0x00000000,
    .maxRxPktLen = 0x00,
    .maxLenLowRate = 0x00,
    .__dummy0 = 0x00,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000
};

// CMD_BLE5_SLAVE
// Bluetooth 5 Slave Command
rfc_CMD_BLE5_SLAVE_t RF_cmdBle5Slave =
{
    .commandNo = 0x1821,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &ble5SlavePar,
    .pOutput = 0,
    .tx20Power = 0x00000000
};

// Structure for CMD_BLE5_MASTER.pParam
rfc_ble5MasterPar_t ble5MasterPar =
{
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
    .maxRxPktLen = 0x00,
    .maxLenLowRate = 0x00
};

// CMD_BLE5_MASTER
// Bluetooth 5 Master Command
rfc_CMD_BLE5_MASTER_t RF_cmdBle5Master =
{
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
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &ble5MasterPar,
    .pOutput = 0,
    .tx20Power = 0x00000000
};

// Structure for CMD_BLE5_SCANNER.pParam
rfc_ble5ScannerPar_t ble5ScannerPar =
{
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .scanConfig.scanFilterPolicy = 0x0,
    .scanConfig.bActiveScan = 0x0,
    .scanConfig.deviceAddrType = 0x0,
    .scanConfig.rpaFilterPolicy = 0x0,
    .scanConfig.bStrictLenFilter = 0x0,
    .scanConfig.bAutoWlIgnore = 0x0,
    .scanConfig.bEndOnRpt = 0x0,
    .scanConfig.rpaMode = 0x0,
    .randomState = 0x0000,
    .backoffCount = 0x0000,
    .backoffPar.logUpperLimit = 0x0,
    .backoffPar.bLastSucceeded = 0x0,
    .backoffPar.bLastFailed = 0x0,
    .extFilterConfig.bCheckAdi = 0x0,
    .extFilterConfig.bAutoAdiUpdate = 0x0,
    .extFilterConfig.bApplyDuplicateFiltering = 0x0,
    .extFilterConfig.bAutoWlIgnore = 0x0,
    .extFilterConfig.bAutoAdiProcess = 0x0,
    .extFilterConfig.bExclusiveSid = 0x0,
    .extFilterConfig.bAcceptSyncInfo = 0x0,
    .adiStatus.lastAcceptedSid = 0x0,
    .adiStatus.state = 0x0,
    .__dummy0 = 0x0000,
    .__dummy1 = 0x00,
    .pDeviceAddress = 0,
    .pWhiteList = 0,
    .pAdiList = 0,
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
    .phyMode = 0x00
};

// CMD_BLE5_SCANNER
// Bluetooth 5 Scanner Command
rfc_CMD_BLE5_SCANNER_t RF_cmdBle5Scanner =
{
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
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &ble5ScannerPar,
    .pOutput = 0,
    .tx20Power = 0x00000000
};

// Structure for CMD_BLE5_INITIATOR.pParam
rfc_ble5InitiatorPar_t ble5InitiatorPar =
{
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
    .backoffCount = 0x0000,
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
    .phyMode = 0x00
};

// CMD_BLE5_INITIATOR
// Bluetooth 5 Initiator Command
rfc_CMD_BLE5_INITIATOR_t RF_cmdBle5Initiator =
{
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
    .channel = 0x8C,
    .whitening.init = 0x51,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &ble5InitiatorPar,
    .pOutput = 0,
    .tx20Power = 0x00000000
};
