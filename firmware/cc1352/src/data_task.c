/*
 * FeralRF CC1352 - Data Task (polling variant)
 *
 * Consumes control events and drains RX packets produced by radio_if.
 */

#include "data_task.h"

#include <stdint.h>

#include "output_if.h"
#include "radio_if.h"
#include "task_event.h"

static bool s_rx_active = false;
static uint8_t s_rx_rsp_seq = 0;

#define RSP_RX_PACKET 0x90u

static void DataTask_emitRxPacket(const RadioIF_RxPacket *pkt) {
    uint8_t payload[13u + RADIO_IF_MAX_PACKET_DATA];
    uint16_t payload_len = 13u + pkt->data_len;

    payload[0] = (uint8_t)(pkt->timestamp_us & 0xFFu);
    payload[1] = (uint8_t)((pkt->timestamp_us >> 8) & 0xFFu);
    payload[2] = (uint8_t)((pkt->timestamp_us >> 16) & 0xFFu);
    payload[3] = (uint8_t)((pkt->timestamp_us >> 24) & 0xFFu);
    payload[4] = (uint8_t)((pkt->timestamp_us >> 32) & 0xFFu);
    payload[5] = (uint8_t)((pkt->timestamp_us >> 40) & 0xFFu);
    payload[6] = (uint8_t)((pkt->timestamp_us >> 48) & 0xFFu);
    payload[7] = (uint8_t)((pkt->timestamp_us >> 56) & 0xFFu);
    payload[8] = pkt->channel;
    payload[9] = (uint8_t)pkt->rssi_dbm;
    payload[10] = pkt->lqi;
    payload[11] = pkt->crc_ok ? 1u : 0u;
    payload[12] = pkt->data_len;
    for (uint16_t i = 0; i < pkt->data_len; i++) {
        payload[13u + i] = pkt->data[i];
    }

    OutputIF_sendResponse(RSP_RX_PACKET, s_rx_rsp_seq++, payload, payload_len);
}

void DataTask_init(void) {
    s_rx_active = false;
    s_rx_rsp_seq = 0;
    RadioIF_init();
}

void DataTask_poll(void) {
    if (TaskEvent_isSet(TASK_EVENT_CONTROL_RX_START)) {
        s_rx_active = RadioIF_startRx();
        if (s_rx_active) {
            TaskEvent_set(TASK_EVENT_DATA_RX_ACTIVE);
        }
        TaskEvent_clear(TASK_EVENT_CONTROL_RX_START);
    }

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_RX_STOP)) {
        s_rx_active = false;
        RadioIF_stopRx();
        TaskEvent_clear(TASK_EVENT_DATA_RX_ACTIVE);
        TaskEvent_clear(TASK_EVENT_CONTROL_RX_STOP);
    }

    RadioIF_poll();

    if (s_rx_active) {
        RadioIF_RxPacket pkt;
        while (RadioIF_popRxPacket(&pkt)) {
            DataTask_emitRxPacket(&pkt);
        }
    }
}

bool DataTask_isRxActive(void) {
    return s_rx_active;
}
