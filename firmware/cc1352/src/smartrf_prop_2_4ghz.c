/*
 * FeralRF CC1352 - Proprietary 2.4 GHz SmartRF config
 *
 * Default: GFSK 250 kbps, deviation 125 kHz, sync word 0x930B51DE,
 * frequency 2440 MHz. RF_MODE_MULTIPLE + multi_protocol patch.
 */

#include "smartrf_prop_2_4ghz.h"

#include <ti/devices/DeviceFamily.h>

/* clang-format off */
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_multi_protocol.h)
/* clang-format on */

/* Register overrides for 2.4 GHz proprietary mode.
 * Conservative defaults — no special analog tuning. If hardware smoke
 * fails (RX-side decode or TX-side level), regenerate from SmartRF
 * Studio "Proprietary 2.4 GHz GFSK" preset and replace this table.
 * Terminator 0xFFFFFFFF marks end of list. */
uint32_t Prop24g_pOverrides[] = {
    0xFFFFFFFF,
};

RF_Mode Prop24g_mode = {
    .rfMode = RF_MODE_MULTIPLE,
    .cpePatchFxn = &rf_patch_cpe_multi_protocol,
    .mcePatchFxn = 0,
    .rfePatchFxn = 0,
};

/* CMD_PROP_RADIO_SETUP (0x3806) — proprietary 2.4 GHz radio setup.
 * NO loDivider, NO centerFreq fields (those are for Sub-1G via
 * CMD_PROP_RADIO_DIV_SETUP). Frequency is set via CMD_FS. */
rfc_CMD_PROP_RADIO_SETUP_t Prop24g_cmdPropRadioSetup = {
    .commandNo = 0x3806,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .modulation.modType = 0x1,    /* GFSK */
    .modulation.deviation = 0x64, /* 100 in 250-Hz steps = 25 kHz... */
    /* Note: deviation 0x64 with stepSz 250 Hz gives 25 kHz. For 125 kHz
     * deviation (typical for 250 kbps GFSK), use 0x1F4 (500) with
     * stepSz 250 Hz, OR 0x7D (125) with stepSz 1000 Hz. We use the
     * 1000-Hz step option for cleaner config: */
    .modulation.deviationStepSz = 0x1, /* 1000 Hz step */
    /* deviation = 125 kHz with stepSz=1000: deviation = 125 */
    /* Will set in code via configure_prop() override; default below */
    .symbolRate.preScale = 0xF,     /* preScale 15 */
    .symbolRate.rateWord = 0x10000, /* 250 kbps with preScale 15 */
    .symbolRate.decimMode = 0x0,
    .rxBw = 0x59,                 /* RX bandwidth 311.5 kHz */
    .preamConf.nPreamBytes = 0x4, /* 4 preamble bytes */
    .preamConf.preamMode = 0x0,
    .formatConf.nSwBits = 0x20, /* 32-bit sync word */
    .formatConf.bBitReversal = 0x0,
    .formatConf.bMsbFirst = 0x1,
    .formatConf.fecMode = 0x0,
    .formatConf.whitenMode = 0x0, /* no whitening (user can add via configure_prop) */
    .config.frontEndMode = 0x0,   /* differential — CatSniffer hardware */
    .config.biasMode = 0x1,       /* external bias — CatSniffer hardware */
    .config.analogCfgMode = 0x0,
    .config.bNoFsPowerUp = 0x0,
    .txPower = 0x801F, /* default ~+5 dBm 2.4 GHz; will be set via RF_setTxPower */
    .pRegOverride = Prop24g_pOverrides,
};

/* Re-set deviation in code: TI's bit-field assignment of 125 above is
 * subtle, do it explicitly in radio_if init. */

rfc_CMD_FS_t Prop24g_cmdFs = {
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
    .frequency = 0x0988, /* 2440 MHz */
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x0,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000,
};

/* CMD_PROP_TX (0x3801) — proprietary TX command.
 * pPkt set at runtime by RadioIF_transmitProp24ghzRaw; pktLen from caller. */
rfc_CMD_PROP_TX_t Prop24g_cmdPropTx = {
    .commandNo = 0x3801,
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
    .pktConf.bUseCrc = 0x1,
    .pktConf.bVarLen = 0x1,
    .pktLen = 0,            /* set by caller */
    .syncWord = 0x930B51DE, /* default sync word; configure_prop can override */
    .pPkt = 0,              /* set by caller */
};

/* CMD_PROP_RX (0x3802) — proprietary RX command. bRepeatOk/bRepeatNok=1
 * for continuous RX; pRxQ set at runtime by startProp24ghzRfBackend.
 * bAutoFlushIgnored=1 per ti-rtos-rf-cc1352 skill rule. */
rfc_CMD_PROP_RX_t Prop24g_cmdPropRx = {
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
    .rxConf.bAutoFlushIgnored = 0x1,
    .rxConf.bAutoFlushCrcErr = 0x0,
    .rxConf.bIncludeHdr = 0x1,
    .rxConf.bIncludeCrc = 0x0,
    .rxConf.bAppendRssi = 0x1,
    .rxConf.bAppendTimestamp = 0x1,
    .rxConf.bAppendStatus = 0x1,
    .syncWord = 0x930B51DE, /* default sync word */
    .maxPktLen = 0xFF,      /* max 255 bytes */
    .address0 = 0xAA,
    .address1 = 0xBB,
    .endTrigger.triggerType = 0x0, /* TRIG_NEVER — continuous */
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
    .pQueue = 0,  /* set by caller */
    .pOutput = 0, /* set by caller (or NULL) */
};
