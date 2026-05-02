/*
 * FeralRF CC1352 - Radio Interface
 *
 * This module owns the radio backend contract used by data_task.
 * Current backend is synthetic; RF Core integration will plug in here.
 */

#ifndef RADIO_IF_H
#define RADIO_IF_H

#include <stdbool.h>
#include <stdint.h>

/* Data task clamps emitted data length to keep RX payload <= 255 with LL metadata. */
#define RADIO_IF_MAX_PACKET_DATA 242u

typedef struct {
    uint64_t timestamp_us;
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t lqi;
    bool crc_ok;
    uint8_t ll_pdu_kind;
    uint8_t ll_pdu_type;
    uint8_t ll_pdu_flags;
    uint8_t data_len;
    uint8_t data[RADIO_IF_MAX_PACKET_DATA];
} RadioIF_RxPacket;

typedef struct {
    uint32_t rx_ok;
    uint32_t rx_crc_err;
    uint32_t rx_drop;
    uint32_t rx_overflow;
} RadioIF_Metrics;

void RadioIF_init(void);

/* 433 MHz boot handle — opened once at boot, never closed */
#include <ti/drivers/rf/RF.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
RF_Object *RadioIF_get433Object(void);
void RadioIF_set433Handle(RF_Handle h);
RF_Handle RadioIF_get433Handle(void);

void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz);
void RadioIF_setChannel(uint8_t channel);
void RadioIF_setPower(int8_t power_dbm);
void RadioIF_setAdvHopEnabled(bool enabled);
bool RadioIF_transmitRaw(const uint8_t *data, uint8_t data_len, int8_t power_dbm);

/* F22 test modes — CW (mode=0) or PRBS-9 (mode=1) / PRBS-15 (mode=2).
 * Requires a prior set_phy() so the active RF handle exists. Returns false
 * if no PHY is configured. */
bool RadioIF_runTxTest(uint8_t mode);
void RadioIF_stopTxTest(void);

bool RadioIF_startRx(void);
void RadioIF_stopRx(void);
bool RadioIF_isRxRunning(void);
bool RadioIF_isRfBackendActive(void);
void RadioIF_poll(void);
bool RadioIF_popRxPacket(RadioIF_RxPacket *out);
void RadioIF_getMetrics(RadioIF_Metrics *out);
void RadioIF_resetMetrics(void);

/* Proprietary radio configuration */
typedef struct {
    uint32_t frequency_hz;
    uint8_t mod_type;     /* 0=FSK, 1=GFSK, 2=OOK/ASK, 4=MSK, 5=4-FSK, 6=4-GFSK */
    uint32_t symbol_rate; /* baud */
    uint16_t deviation;   /* Hz (for FSK/GFSK) */
    uint8_t rx_bw;        /* RX bandwidth register value */
    uint32_t sync_word;
    uint16_t format_conf; /* raw formatConf bitfield (0=use defaults) */
} RadioIF_PropConfig;

void RadioIF_setPropConfig(const RadioIF_PropConfig *config);
void RadioIF_setActiveScan(bool enabled);
void RadioIF_getScannerStats(uint16_t *tx_req, uint16_t *rx_adv_ok, uint16_t *rx_rsp_ok);
void RadioIF_setBleAdvAddress(const uint8_t *addr);

/* BLE connection initiation — runs CMD_BLE5_INITIATOR (blocking) */
int RadioIF_bleInitiate(void);

/* Per-event RF stats lifted from rfc_bleMasterSlaveOutput_t after the
 * CMD_BLE5_MASTER mailbox finishes. Lives in radio_if so callers don't
 * need to include rf_ble_cmd.h. Session 4 telemetry.
 *
 * The combination (nTx, pStats->nRxOk + nRxNok + nRxIgnored, pktStatus
 * bTimeStampValid) lets the host distinguish:
 *   - Master state machine never executed:  nTx==0, all RX counters==0.
 *   - Master TX'd, slave silent:             nTx>=1, all RX counters==0.
 *   - Master TX'd, slave RX'd:               nTx>=1, nRxOk>=1 (or nRxNok>=1
 *                                            for CRC errors). */
typedef struct {
    uint8_t nTx;         /* pOutput->nTx — total TX incl. auto-empty + retrans */
    uint8_t nRxOk;       /* pOutput->nRxOk */
    uint8_t nRxNok;      /* pOutput->nRxNok */
    uint8_t nRxIgnored;  /* pOutput->nRxIgnored */
    uint8_t pktStatus;   /* packed pktStatus bitfield byte (see ble_conn_mgr.h) */
} RadioIF_BleCentralStats;

/* BLE central mode — run CMD_BLE5_MASTER for one connection event.
 * pStats may be NULL if the caller doesn't need per-event RF stats. */
int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit,
                       dataQueue_t *pTxQueue, uint32_t startTime,
                       uint32_t endTime, uint32_t *pNumSent,
                       RadioIF_BleCentralStats *pStats);

/* Reset seqStat for initiator→central transition */
void RadioIF_bleResetSeqStat(void);
void RadioIF_bleResetRxQueue(void);
void RadioIF_bleDrainRxQueue(void);

/* RF debug diagnostics */
typedef struct {
    uint8_t rf_handle_ok;     /* 1 if RF_open succeeded */
    uint8_t rf_mode;          /* current s_rf_mode */
    uint16_t setup_status;    /* RadioSetup command status */
    uint16_t fs_status;       /* CMD_FS status */
    uint16_t rx_status;       /* CMD_PROP_RX status */
    uint16_t tx_status;       /* last CMD_PROP_TX status */
    uint16_t fs_freq;         /* CMD_FS frequency field */
    uint8_t lo_divider;       /* RadioSetup loDivider */
    uint32_t freq_hz;         /* s_frequency_hz */
    uint8_t tx_result_ok;     /* 1 if last transmitRaw returned true */
    uint32_t tx_event_mask;   /* RF_EventMask from last RF_runCmd(TX) */
    uint16_t fs_frac;         /* CMD_FS fractional freq */
    uint16_t setup_center;    /* RadioSetup centerFreq */
    uint8_t tx_session_ok;    /* 1 if s_tx_session_handle != NULL */
    uint8_t tx_session_match; /* 1 if s_tx_session_mode == expected */
    uint16_t tx_cmd_no;       /* CMD_PROP_TX commandNo check */
} RadioIF_RfDebug;

void RadioIF_getRfDebug(RadioIF_RfDebug *out);
void *RadioIF_getRfHandle(void);

/* F8b Track B — Passive connection follower primitives.
 *
 * Both helpers wrap CMD_BLE5_GENERIC_RX as a blocking single-shot RX.
 * The packet callback is invoked from the same task context that called
 * the helper, after the RF command terminates and the data queue has been
 * drained. Callback signature receives the raw LL bytes (header + body),
 * channel, RSSI, and a user pointer for state.
 *
 * Returns 0 on RX_OK / TIMEOUT / END (normal terminations), non-zero on
 * RF stack error. */
typedef void (*RadioIF_FollowPacketCb)(const uint8_t *ll_pdu, uint8_t pdu_len,
                                       uint8_t channel, int8_t rssi_dbm, void *user);

/* Listen on an advertising channel (37/38/39) with AA=0x8E89BED6,
 * CRC init=0x555555. end_time_rat is absolute RAT tick (4 MHz); 0 means
 * listen forever. */
int RadioIF_followAdvOnce(uint8_t adv_channel, uint32_t end_time_rat,
                          RadioIF_FollowPacketCb cb, void *user);

/* Listen on a data channel (0..36) with caller-supplied accessAddr and
 * crcInit. end_time_rat is absolute RAT tick; 0 means listen forever. */
int RadioIF_followDataOnce(uint8_t data_channel, uint32_t accessAddr,
                           uint32_t crcInit, uint32_t end_time_rat,
                           RadioIF_FollowPacketCb cb, void *user);

/* Jamming functions - optimized continuous transmission */
bool RadioIF_startJamSession(uint8_t phy, uint8_t channel, int8_t power_dbm);
void RadioIF_stopJamSession(void);
bool RadioIF_isJamSessionActive(void);
void RadioIF_pollJamSession(void);

#endif /* RADIO_IF_H */
