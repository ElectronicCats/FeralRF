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

#include "ble_conn_mgr.h"

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
#include <ti/devices/DeviceFamily.h>
#include <ti/drivers/rf/RF.h>
/* The macro path below uses '/' as a path separator, not division.
 * clang-format mangles it without the directives. */
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */
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

/* F21 — BLE Connectable advertiser (legacy ADV_IND / DIRECT / SCAN_IND).
 * pdu_type: 0x0=ADV_IND, 0x1=ADV_DIRECT_IND, 0x6=ADV_SCAN_IND.
 * For DIRECT, init_addr is the peer address; adv_data and scan_rsp ignored.
 * For IND/SCAN, init_addr ignored.
 * count: number of advertising events. interval_units: 0.625ms units. */
bool RadioIF_transmitBleAdvLegacy(uint8_t pdu_type, uint8_t addr_type, const uint8_t *addr,
                                  uint8_t channel, int8_t power_dbm, uint16_t count,
                                  uint16_t interval_units, const uint8_t *adv_data,
                                  uint8_t adv_data_len, const uint8_t *scan_rsp,
                                  uint8_t scan_rsp_len, uint8_t init_addr_type,
                                  const uint8_t *init_addr);

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
    uint8_t nTx;        /* pOutput->nTx — total TX incl. auto-empty + retrans */
    uint8_t nRxOk;      /* pOutput->nRxOk */
    uint8_t nRxNok;     /* pOutput->nRxNok */
    uint8_t nRxIgnored; /* pOutput->nRxIgnored */
    uint8_t pktStatus;  /* packed pktStatus bitfield byte (see ble_conn_mgr.h) */
} RadioIF_BleCentralStats;

/* BLE central mode — run CMD_BLE5_MASTER for one connection event.
 * pStats may be NULL if the caller doesn't need per-event RF stats. */
int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit, dataQueue_t *pTxQueue,
                       uint32_t startTime, uint32_t endTime, uint32_t *pNumSent,
                       RadioIF_BleCentralStats *pStats);

/* F20.a.1 — Run one BLE peripheral connection event.
 * chan: data channel (0..36).
 * accessAddr / crcInit: from CONNECT_IND captured by F21.
 * pTxQueue: ATT responses to enqueue (built by BleConnMgr_pollSlave).
 * startTime: anchor RAT tick.
 * endTime: supervision deadline RAT tick.
 * pStats output (nullable). */
int RadioIF_bleSlave(uint8_t chan, uint32_t accessAddr, uint32_t crcInit, dataQueue_t *pTxQueue,
                     uint32_t startTime, uint32_t endTime, uint32_t *pNumSent,
                     RadioIF_BleCentralStats *pStats);

/* Reset seqStat for initiator→central transition */
void RadioIF_bleResetSeqStat(void);
/* Reset seqStat for peripheral connection start (mirrors RadioIF_bleResetSeqStat). */
void RadioIF_bleResetSlaveSeqStat(void);
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
typedef void (*RadioIF_FollowPacketCb)(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                                       int8_t rssi_dbm, void *user);

/* Listen on an advertising channel (37/38/39) with AA=0x8E89BED6,
 * CRC init=0x555555. end_time_rat is absolute RAT tick (4 MHz); 0 means
 * listen forever. */
int RadioIF_followAdvOnce(uint8_t adv_channel, uint32_t end_time_rat, RadioIF_FollowPacketCb cb,
                          void *user);

/* Listen on a data channel (0..36) with caller-supplied accessAddr and
 * crcInit. end_time_rat is absolute RAT tick; 0 means listen forever. */
int RadioIF_followDataOnce(uint8_t data_channel, uint32_t accessAddr, uint32_t crcInit,
                           uint32_t end_time_rat, RadioIF_FollowPacketCb cb, void *user);

/* F20.a.1 — Extract CONNECT_IND params from RX queue after ADV exit.
 * Returns true and populates out_params if a CONNECT_IND (PDU type 0x5) is
 * found. The caller must set out_params->connectIndEndRat separately via
 * RF_getCurrentTime() since bAppendTimestamp is not enabled on the ADV cmd. */
bool RadioIF_extractConnectIndParams(BleConnMgr_SlaveParams *out_params);

/* F20.a.1.c — internal-state trace exposed via RSP_DEBUG_SLAVE.
 * All counters saturate (no wrap) so a stuck condition reads as 0xFF/0xFFFF
 * rather than as 0. Reset is implicit on each new advertise/extract cycle:
 * extract counters reset on entry to RadioIF_extractConnectIndParams,
 * advertise iterations reset on entry to RadioIF_transmitBleAdvLegacy. */
typedef struct {
    /* F20.a.1.d — f21LastStatus replaces the polluted lastTxStatus.
     * Written ONLY from RadioIF_transmitBleAdvLegacy's CMD_BLE_ADV loop.
     * f21FirstNonzeroStatus pins the first iteration whose status was
     * not BLE_DONE_OK (0x1400). f21AdvA[6] records the address bytes
     * actually used as pDeviceAddress for the most recent advertise call. */
    uint16_t f21LastStatus;
    uint16_t f21FirstNonzeroStatus;
    uint16_t advertiseIterations;
    uint8_t extractCallCount;
    uint8_t extractEntriesSeen;
    uint8_t extractFirstPduType;
    uint8_t f21AdvA[6];
    /* F20.a.1.e — HW counters accumulated across the F21 ADV loop.
     * f21LastRssi is the RSSI of the most recent RX'd packet (any kind).
     * Use these to disambiguate NOSYNC: nRxConnectReq>0 means radio HW saw
     * a valid CONNECT_IND; nRxIgnored>0 means a packet arrived but HW filter
     * rejected it; both 0 + nTxAdvInd > 0 means no packet ever entered the
     * RX window (timing miss). */
    uint16_t f21TotalTxAdvInd;
    uint16_t f21TotalRxConnectReq;
    uint16_t f21TotalRxIgnored;
    uint16_t f21TotalRxNok;
    int8_t f21LastRssi;
} RadioIF_DbgF21Trace;

void RadioIF_getDbgF21Trace(RadioIF_DbgF21Trace *out);

/* F20.a.1 — Return a pointer to the internal RX data queue so that
 * ble20_dispatch.c can walk it without exposing the static variable. */
dataQueue_t *RadioIF_getRxQueue(void);

/* Jamming functions - optimized continuous transmission */
bool RadioIF_startJamSession(uint8_t phy, uint8_t channel, int8_t power_dbm);
void RadioIF_stopJamSession(void);
bool RadioIF_isJamSessionActive(void);
void RadioIF_pollJamSession(void);

#endif /* RADIO_IF_H */
