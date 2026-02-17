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
void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz);
void RadioIF_setChannel(uint8_t channel);
void RadioIF_setPower(int8_t power_dbm);
bool RadioIF_startRx(void);
void RadioIF_stopRx(void);
bool RadioIF_isRxRunning(void);
void RadioIF_poll(void);
bool RadioIF_popRxPacket(RadioIF_RxPacket *out);
void RadioIF_getMetrics(RadioIF_Metrics *out);
void RadioIF_resetMetrics(void);

#endif /* RADIO_IF_H */
