/*
 * Sub-1GHz Proprietary SmartRF settings for CC1352P/CC1352P7.
 * 868 MHz, 50 kBaud GFSK, 25 kHz deviation.
 * Based on CatSniffer sniffer_fw reference (cc1352p1lp/rf_prop/smartrf_settings_rf_prop_0).
 */

#include "smartrf_prop_0.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_prop.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_mce_genook.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_rfe_genook.h)
/* clang-format on */

RF_Mode Prop0_mode = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_multi_protocol,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

/* Prop mode with cpe_prop patch — works at 433 MHz (multi_protocol fails there) */
RF_Mode Prop0_modeSub1g = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_prop,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

/* OOK mode — requires dedicated MCE+RFE patches for amplitude demodulation */
RF_Mode Prop0_modeOok = {
    .rfMode = RF_MODE_AUTO,
    .cpePatchFxn = &rf_patch_cpe_prop,
    .mcePatchFxn = &rf_patch_mce_genook,
    .rfePatchFxn = &rf_patch_rfe_genook,
};

/* Overrides for 868 MHz Sub-1GHz prop mode */
uint32_t Prop0_pOverrides[] = {
    /* SDK 8.30 SysConfig overrides for LP_CC1352P7_1 868 MHz */
    ADI_2HALFREG_OVERRIDE(0, 16, 0x8, 0x8, 17, 0x1, 0x1),
    HW_REG_OVERRIDE(0x609C, 0x001A),       /* AGC ref level */
    (uint32_t)0x000188A3,                  /* RSSI offset -1dB */
    ADI_HALFREG_OVERRIDE(0, 61, 0xF, 0xD), /* Anti-aliasing BW */
    HW32_ARRAY_OVERRIDE(0x405C, 0x0001),   /* FSCA divider bias */
    (uint32_t)0x08141131,                  /* FSCA divider bias */
    (uint32_t)0x00F788D3,                  /* DC/DC IPEAK=7 */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesTxStd[] = {
    TX_STD_POWER_OVERRIDE(0x013F),
    (uint32_t)0x11310703,            /* ANADIV */
    HW_REG_OVERRIDE(0x6028, 0x001A), /* PA ramp */
    HW_REG_OVERRIDE(0x60A8, 0x0401), /* TXRX pin (SDK 8.30) */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesTx20[] = {
    TX20_POWER_OVERRIDE(0x001B8ED2), /* SDK 8.30 value */
    (uint32_t)0x11C10703,            /* ANADIV */
    HW_REG_OVERRIDE(0x6028, 0x001F), /* PA ramp */
    HW_REG_OVERRIDE(0x60A8, 0x0001), /* TXRX pin (SDK 8.30) */
    (uint32_t)0xFFFFFFFF,
};

/* 433 MHz overrides — exact match of SysConfig tc112 (LP_CC1352P7_4 rfPacketTx)
 * These are used by setPropConfig when dynamically switching to 433 MHz.
 * The SysConfig-generated pOverrides433Sys (in ti_radio_config_433.c) is used
 * by the boot handle. These must match exactly. */
uint32_t Prop0_pOverrides433[] = {
    ADI_2HALFREG_OVERRIDE(0, 16, 0x8, 0x8, 17, 0x1, 0x1), /* PA ramp PACTL2 */
    HW_REG_OVERRIDE(0x609C, 0x001C),                      /* AGC ref level (tc112) */
    (uint32_t)0x000688A3,                                 /* RSSI offset -6dB (tc112) */
    ADI_HALFREG_OVERRIDE(0, 61, 0xF, 0xD),                /* Anti-aliasing filter BW */
    HW_REG_OVERRIDE(0x6028, 0x001A),                      /* TX analog ramp time */
    (uint32_t)0x00F788D3,                                 /* DC/DC IPEAK=7 */
    HW32_ARRAY_OVERRIDE(0x405C, 0x0001),                  /* FSCA divider bias */
    (uint32_t)0x08141131,                                 /* FSCA divider bias */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverrides433TxStd[] = {
    TX_STD_POWER_OVERRIDE(0x013F),   /* SDK tc112 value (was 0x003F) */
    (uint32_t)0x11310703,            /* ANADIV */
    HW_REG_OVERRIDE(0x6028, 0x001A), /* PA ramp */
    HW_REG_OVERRIDE(0x60A8, 0x0401), /* TXRX pin (SDK 8.30) */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverrides433Tx20[] = {
    TX20_POWER_OVERRIDE(0x001B8ED2), /* SDK 8.30 value (was 0x003F00FF) */
    (uint32_t)0x11C10703,            /* ANADIV */
    HW_REG_OVERRIDE(0x6028, 0x001F), /* PA ramp */
    HW_REG_OVERRIDE(0x60A8, 0x0001), /* TXRX pin (SDK 8.30) */
    (uint32_t)0xFFFFFFFF,
};

/* 169 MHz band overrides (from SDK setting_tc220_rx.json — WMBUS N-mode) */
uint32_t Prop0_pOverrides169[] = {
    (uint32_t)0x00F788D3,                  /* DC/DC regulator config */
    HW32_ARRAY_OVERRIDE(0x405C, 0x0001),   /* FSCA divider bias */
    (uint32_t)0x08141131,                  /* FSCA divider bias (cont) */
    (uint32_t)0x40024029,                  /* IIR filter enable, 2nd order */
    (uint32_t)0x38000000,                  /* IIR_FILT_BW=1 */
    (uint32_t)0x01608402,                  /* IIR clk div */
    (uint32_t)0x424C0583,                  /* Synth loop BW K2 = 150 kHz */
    (uint32_t)0x000205A3,                  /* Synth K2 continuation */
    (uint32_t)0x98630603,                  /* Synth loop BW K3 (LSB) */
    (uint32_t)0x00030623,                  /* Synth loop BW K3 (MSB) */
    (uint32_t)0x000684A3,                  /* Synth FREF = 8 MHz */
    ADI_HALFREG_OVERRIDE(0, 61, 0xF, 0xD), /* Anti-aliasing filter BW */
    (uint32_t)0x000E88A3,                  /* RSSI offset -14 dB */
    HW_REG_OVERRIDE(0x609C, 0x0019),       /* AGC ref level */
    HW_REG_OVERRIDE(0x6098, 0x34D1),       /* AGC max gain */
    ADI_REG_OVERRIDE(0, 12, 0xF8),         /* PA trim max */
    (uint32_t)0xFFFFFFFF,
};

/* OOK overrides (from SDK setting_tc599.json).
 * WARNING: Once applied, RF Core cannot switch to other modes without
 * power cycle. MCE_RFE_OVERRIDE loads genook patches into MCE/RFE RAM
 * that cannot be unloaded (TI SDK bug: RFCCpePatchReset is a no-op). */
uint32_t Prop0_pOverridesOok[] = {
    MCE_RFE_OVERRIDE(1, 0, 0, 1, 0, 0), /* Enable MCE+RFE genook patches */
    ADI_2HALFREG_OVERRIDE(0, 16, 0x8, 0x8, 17, 0x1, 0x1),
    HW_REG_OVERRIDE(0x609C, 0x001E),       /* AGC ref level for OOK */
    (uint32_t)0x000288A3,                  /* RSSI offset */
    ADI_HALFREG_OVERRIDE(0, 61, 0xF, 0xF), /* Anti-aliasing filter */
    HW32_ARRAY_OVERRIDE(0x405C, 1),        /* FSCA divider bias */
    (uint32_t)0x08141131,
    HW_REG_OVERRIDE(0x51E4, 0x80AF), /* OOK duty cycle compensation */
    HW_REG_OVERRIDE(0x5270, 0x0002), /* Viterbi k=7 */
    HW_REG_OVERRIDE(0x6028, 0x001A), /* PA ramp timing */
    (uint32_t)0x00F788D3,            /* DC/DC regulator */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesOokTxStd[] = {
    TX_STD_POWER_OVERRIDE(0x003F),
    (uint32_t)0x11310703,
    HW_REG_OVERRIDE(0x6028, 0x001A),
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesOokTx20[] = {
    TX20_POWER_OVERRIDE(0x003F00FF),
    (uint32_t)0x11C10703,
    HW_REG_OVERRIDE(0x6028, 0x001F),
    (uint32_t)0xFFFFFFFF,
};

/* OOK overrides for 433 MHz — OOK patches + 433-band RF tuning.
 * Merges OOK-specific (MCE_RFE, duty cycle, Viterbi) with 433-band
 * (AGC ref 0x1C, RSSI -6dB) from tc112. */
uint32_t Prop0_pOverridesOok433[] = {
    MCE_RFE_OVERRIDE(1, 0, 0, 1, 0, 0), /* Enable MCE+RFE genook patches */
    ADI_2HALFREG_OVERRIDE(0, 16, 0x8, 0x8, 17, 0x1, 0x1),
    HW_REG_OVERRIDE(0x609C, 0x001C),       /* AGC ref level (433 band) */
    (uint32_t)0x000688A3,                  /* RSSI offset -6dB (433 band) */
    ADI_HALFREG_OVERRIDE(0, 61, 0xF, 0xF), /* Anti-aliasing filter (OOK wide) */
    HW32_ARRAY_OVERRIDE(0x405C, 1),        /* FSCA divider bias */
    (uint32_t)0x08141131,
    HW_REG_OVERRIDE(0x51E4, 0x80AF), /* OOK duty cycle compensation */
    HW_REG_OVERRIDE(0x5270, 0x0002), /* Viterbi k=7 */
    HW_REG_OVERRIDE(0x6028, 0x001A), /* PA ramp timing */
    (uint32_t)0x00F788D3,            /* DC/DC regulator */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesOok433TxStd[] = {
    TX_STD_POWER_OVERRIDE(0x013F),   /* 433 band value */
    (uint32_t)0x11310703,
    HW_REG_OVERRIDE(0x6028, 0x001A),
    HW_REG_OVERRIDE(0x60A8, 0x0401), /* TXRX pin (433 band) */
    (uint32_t)0xFFFFFFFF,
};

uint32_t Prop0_pOverridesOok433Tx20[] = {
    TX20_POWER_OVERRIDE(0x001B8ED2), /* 433 band value */
    (uint32_t)0x11C10703,
    HW_REG_OVERRIDE(0x6028, 0x001F),
    HW_REG_OVERRIDE(0x60A8, 0x0001), /* TXRX pin (433 band) */
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
    .config.bSynthNarrowBand = 0x0,
    .txPower = 0x9413, /* 12 dBm (433 MHz PA table) */
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
