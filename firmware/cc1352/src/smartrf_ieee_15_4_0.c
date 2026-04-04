/*
 * IEEE 802.15.4 SmartRF settings for CC1352P7 — SDK 8.30.01.01 compatible.
 * Overrides from cc1352p7_ieee_15_4_pg10/setting_ieee_802_15_4.json.
 */

#include "smartrf_ieee_15_4_0.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
/* clang-format on */

RF_Mode Ieee154_0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_multi_protocol,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

uint32_t Ieee154_0_pOverrides[] = {
    /* Rx: Set LNA bias current offset to +15 to saturate trim to max */
    (uint32_t)0x000F8883,
    /* Tx: Set DCDC settings IPEAK=3, dither off */
    (uint32_t)0x00F388D3,
    (uint32_t)0xFFFFFFFF,
};

static uint32_t Ieee154_0_pOverridesTxStd[] = {
    /* TX Standard power override (placeholder, set by RF_TxPowerTable) */
    TX_STD_POWER_OVERRIDE(0x941E),
    /* ANADIV radio parameter based on LO divider and front end settings */
    (uint32_t)0x05320703,
    /* IEEE 15.4: Set IPEAK=3 and DCDC dither off for TX */
    (uint32_t)0x00F388D3,
    /* Set RTIM offset to default for standard PA */
    (uint32_t)0x00008783,
    /* Set synth mux to default value for standard PA */
    (uint32_t)0x050206C3,
    /* Set TXRX pin to 0 in RX and high impedance in idle/TX */
    HW_REG_OVERRIDE(0x60A8, 0x0401),
    (uint32_t)0xFFFFFFFF,
};

static uint32_t Ieee154_0_pOverridesTx20[] = {
    /* TX HighPA power override (placeholder, set by RF_TxPowerTable) */
    TX20_POWER_OVERRIDE(0x003F5BB8),
    /* ANADIV radio parameter based on LO divider and front end settings */
    (uint32_t)0x01C20703,
    /* IEEE 15.4: Set RTIM offset to 3 for high power PA */
    (uint32_t)0x00030783,
    /* IEEE 15.4: Set synth mux for high power PA */
    (uint32_t)0x010206C3,
    /* IEEE 15.4: Set TXRX pin to 0 in RX/TX and high impedance in idle */
    HW_REG_OVERRIDE(0x60A8, 0x0001),
    (uint32_t)0xFFFFFFFF,
};

rfc_CMD_RADIO_SETUP_PA_t Ieee154_0_cmdRadioSetup = {
    .commandNo = 0x0802,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .mode = 0x01,
    .loDivider = 0x00,
    .config.frontEndMode = 0x0,
    .config.biasMode = 0x1,
    .config.analogCfgMode = 0x0,
    .config.bNoFsPowerUp = 0x0,
    .txPower = 0x941E,
    .pRegOverride = Ieee154_0_pOverrides,
    .pRegOverrideTxStd = Ieee154_0_pOverridesTxStd,
    .pRegOverrideTx20 = Ieee154_0_pOverridesTx20,
};

rfc_CMD_FS_t Ieee154_0_cmdFs = {
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
    .frequency = 0x0965, /* 2405 MHz (channel 11) */
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x1,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000,
};

rfc_CMD_IEEE_RX_t Ieee154_0_cmdIeeeRx = {
    .commandNo = 0x2801,
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
    .rxConfig.bAutoFlushCrc = 0x0,
    .rxConfig.bAutoFlushIgn = 0x0,
    .rxConfig.bIncludePhyHdr = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x1,
    .rxConfig.bAppendCorrCrc = 0x1,
    .rxConfig.bAppendSrcInd = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .pRxQ = 0,
    .pOutput = 0,
    .frameFiltOpt.frameFiltEn = 0x0,
    .frameFiltOpt.frameFiltStop = 0x0,
    .frameFiltOpt.autoAckEn = 0x0,
    .frameFiltOpt.slottedAckEn = 0x0,
    .frameFiltOpt.autoPendEn = 0x0,
    .frameFiltOpt.defaultPend = 0x0,
    .frameFiltOpt.bPendDataReqOnly = 0x0,
    .frameFiltOpt.bPanCoord = 0x0,
    .frameFiltOpt.maxFrameVersion = 0x3,
    .frameFiltOpt.fcfReservedMask = 0x0,
    .frameFiltOpt.modifyFtFilter = 0x0,
    .frameFiltOpt.bStrictLenFilter = 0x0,
    .frameTypes.bAcceptFt0Beacon = 0x1,
    .frameTypes.bAcceptFt1Data = 0x1,
    .frameTypes.bAcceptFt2Ack = 0x1,
    .frameTypes.bAcceptFt3MacCmd = 0x1,
    .frameTypes.bAcceptFt4Reserved = 0x1,
    .frameTypes.bAcceptFt5Reserved = 0x1,
    .frameTypes.bAcceptFt6Reserved = 0x1,
    .frameTypes.bAcceptFt7Reserved = 0x1,
    .ccaOpt.ccaEnEnergy = 0x0,
    .ccaOpt.ccaEnCorr = 0x0,
    .ccaOpt.ccaEnSync = 0x0,
    .ccaOpt.ccaCorrOp = 0x1,
    .ccaOpt.ccaSyncOp = 0x1,
    .ccaOpt.ccaCorrThr = 0x0,
    .ccaRssiThr = 0x64,
    .__dummy0 = 0x00,
    .numExtEntries = 0x00,
    .numShortEntries = 0x00,
    .pExtEntryList = 0,
    .pShortEntryList = 0,
    .localExtAddr = 0x0000000012345678ULL,
    .localShortAddr = 0xABBA,
    .localPanID = 0x0000,
    .__dummy1 = 0x000000,
    .endTrigger.triggerType = 0x1,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
};

rfc_CMD_IEEE_TX_t Ieee154_0_cmdIeeeTx = {
    .commandNo = 0x2C01,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .txOpt.bIncludePhyHdr = 0x0,
    .txOpt.bIncludeCrc = 0x0,
    .txOpt.payloadLenMsb = 0x0,
    .payloadLen = 0x00,
    .pPayload = 0,
    .timeStamp = 0x00000000,
};
