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

/* Keep response payload <= PROTOCOL_MAX_PAYLOAD (255): metadata 13 + data <= 242 */
#define RADIO_IF_MAX_PACKET_DATA 242u

typedef struct {
    uint64_t timestamp_us;
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t lqi;
    bool crc_ok;
    uint8_t data_len;
    uint8_t data[RADIO_IF_MAX_PACKET_DATA];
} RadioIF_RxPacket;

void RadioIF_init(void);
void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz);
void RadioIF_setChannel(uint8_t channel);
void RadioIF_setPower(int8_t power_dbm);
bool RadioIF_startRx(void);
void RadioIF_stopRx(void);
bool RadioIF_isRxRunning(void);
void RadioIF_poll(void);
bool RadioIF_popRxPacket(RadioIF_RxPacket *out);

#endif /* RADIO_IF_H */
