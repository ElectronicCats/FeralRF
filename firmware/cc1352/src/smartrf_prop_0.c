/*
 * Sub-1GHz Proprietary SmartRF settings for CC1352P/CC1352P7.
 * 868 MHz, 50 kBaud GFSK, 25 kHz deviation.
 * Based on CatSniffer sniffer_fw reference (cc1352p1lp/rf_prop/smartrf_settings_rf_prop_0).
 */

#include "smartrf_prop_0.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
/* clang-format on */

RF_Mode Prop0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_multi_protocol,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

/* Overrides for 868 MHz Sub-1GHz prop mode */
uint32_t Prop0_pOverrides[] = {
    ADI_2HALFREG_OVERRIDE(0, 16, 0x8, 0x8, 17, 0x1, 0x1),
    HW_REG_OVERRIDE(0x609C, 0x001A),
    (uint32_t)0x000288A3,
    ADI_HALFREG_OVERRIDE(0, 61, 0xF, 0xD),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesTxStd[] = {
    TX_STD_POWER_OVERRIDE(0x013F),
    (uint32_t)0x11310703,
    HW_REG_OVERRIDE(0x6028, 0x001A),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesTx20[] = {
    TX20_POWER_OVERRIDE(0x0020AA1B),
    (uint32_t)0x11C10703,
    HW_REG_OVERRIDE(0x6028, 0x001F),
    (uint32_t)0xFFFFFFFF,
};

rfc_CMD_PROP_RADIO_DIV_SETUP_PA_t Prop0_cmdPropRadioDivSetup = {
    .commandNo = 0x3807,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .modulation.modType = 0x1,
    .modulation.deviation = 0x64,
    .modulation.deviationStepSz = 0x0,
    .symbolRate.preScale = 0xF,
    .symbolRate.rateWord = 0x8000,
    .symbolRate.decimMode = 0x0,
    .rxBw = 0x52,
    .preamConf.nPreamBytes = 0x4,
    .preamConf.preamMode = 0x0,
    .formatConf.nSwBits = 0x20,
    .formatConf.bBitReversal = 0x0,
    .formatConf.bMsbFirst = 0x1,
    .formatConf.fecMode = 0x0,
    .formatConf.whitenMode = 0x0,
    .config.frontEndMode = 0x0,
    .config.biasMode = 0x1,
    .config.analogCfgMode = 0x0,
    .config.bNoFsPowerUp = 0x0,
    .txPower = 0xFFFF,
    .pRegOverride = Prop0_pOverrides,
    .centerFreq = 0x0364,
    .intFreq = 0x8000,
    .loDivider = 0x05,
    .pRegOverrideTxStd = Prop0_pOverridesTxStd,
    .pRegOverrideTx20 = Prop0_pOverridesTx20,
};

rfc_CMD_FS_t Prop0_cmdFs = {
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
    .frequency = 0x0364,
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x1,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000,
};

rfc_CMD_PROP_TX_t Prop0_cmdPropTx = {
    .commandNo = 0x3801,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0x0,
    .pktConf.bFsOff = 0x0,
    .pktConf.bUseCrc = 0x1,
    .pktConf.bVarLen = 0x1,
    .pktLen = 0x14,
    .syncWord = 0x930B51DE,
    .pPkt = 0,
};

rfc_CMD_PROP_RX_t Prop0_cmdPropRx = {
    .commandNo = 0x3802,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .pktConf.bFsOff = 0x0,
    .pktConf.bRepeatOk = 0x1,
    .pktConf.bRepeatNok = 0x1,
    .pktConf.bUseCrc = 0x1,
    .pktConf.bVarLen = 0x1,
    .pktConf.bChkAddress = 0x0,
    .pktConf.endType = 0x0,
    .pktConf.filterOp = 0x0,
    .rxConf.bAutoFlushIgnored = 0x0,
    .rxConf.bAutoFlushCrcErr = 0x0,
    .rxConf.bIncludeHdr = 0x1,
    .rxConf.bIncludeCrc = 0x1,
    .rxConf.bAppendRssi = 0x1,
    .rxConf.bAppendTimestamp = 0x1,
    .rxConf.bAppendStatus = 0x1,
    .syncWord = 0x930B51DE,
    .maxPktLen = 0xFF,
    .address0 = 0x00,
    .address1 = 0x00,
    .endTrigger.triggerType = TRIG_NEVER,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
    .pQueue = 0,
    .pOutput = 0,
};
