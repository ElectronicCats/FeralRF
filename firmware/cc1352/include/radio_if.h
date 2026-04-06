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
RF_Object *RadioIF_get433Object(void);
void RadioIF_set433Handle(RF_Handle h);
RF_Handle RadioIF_get433Handle(void);

void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz);
void RadioIF_setChannel(uint8_t channel);
void RadioIF_setPower(int8_t power_dbm);
void RadioIF_setAdvHopEnabled(bool enabled);
bool RadioIF_transmitRaw(const uint8_t *data, uint8_t data_len, int8_t power_dbm);
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
    uint8_t mod_type;       /* 0=FSK, 1=GFSK, 2=OOK/ASK, 4=MSK */
    uint32_t symbol_rate;   /* baud */
    uint16_t deviation;     /* Hz (for FSK/GFSK) */
    uint8_t rx_bw;          /* RX bandwidth register value */
    uint32_t sync_word;
} RadioIF_PropConfig;

void RadioIF_setPropConfig(const RadioIF_PropConfig *config);
void RadioIF_setActiveScan(bool enabled);
void RadioIF_getScannerStats(uint16_t *tx_req, uint16_t *rx_adv_ok, uint16_t *rx_rsp_ok);
void RadioIF_setBleAdvAddress(const uint8_t *addr);

/* RF debug diagnostics */
typedef struct {
    uint8_t  rf_handle_ok;      /* 1 if RF_open succeeded */
    uint8_t  rf_mode;           /* current s_rf_mode */
    uint16_t setup_status;      /* RadioSetup command status */
    uint16_t fs_status;         /* CMD_FS status */
    uint16_t rx_status;         /* CMD_PROP_RX status */
    uint16_t tx_status;         /* last CMD_PROP_TX status */
    uint16_t fs_freq;           /* CMD_FS frequency field */
    uint8_t  lo_divider;        /* RadioSetup loDivider */
    uint32_t freq_hz;           /* s_frequency_hz */
    uint8_t  tx_result_ok;      /* 1 if last transmitRaw returned true */
    uint32_t tx_event_mask;     /* RF_EventMask from last RF_runCmd(TX) */
    uint16_t fs_frac;           /* CMD_FS fractional freq */
    uint16_t setup_center;      /* RadioSetup centerFreq */
    uint8_t  tx_session_ok;     /* 1 if s_tx_session_handle != NULL */
    uint8_t  tx_session_match;  /* 1 if s_tx_session_mode == expected */
    uint16_t tx_cmd_no;         /* CMD_PROP_TX commandNo check */
} RadioIF_RfDebug;

void RadioIF_getRfDebug(RadioIF_RfDebug *out);
void *RadioIF_getRfHandle(void);

/* Jamming functions - optimized continuous transmission */
bool RadioIF_startJamSession(uint8_t phy, uint8_t channel, int8_t power_dbm);
void RadioIF_stopJamSession(void);
bool RadioIF_isJamSessionActive(void);
void RadioIF_pollJamSession(void);

#endif /* RADIO_IF_H */
