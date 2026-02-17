/*
 * FeralRF CC1352 - Data Task (polling variant)
 *
 * Placeholder task for the future RF pipeline. It currently tracks RX state
 * transitions signaled by the command processor through task events.
 */

#include "data_task.h"

#include <stddef.h>
#include <stdint.h>

#include "output_if.h"
#include "radio_if.h"
#include "task_event.h"

static bool s_rx_active = false;
static bool s_rx_packet_emitted = false;
static uint64_t s_fake_timestamp_us = 0;
static uint8_t s_rx_rsp_seq = 0;

#define RSP_RX_PACKET 0x90u

static void DataTask_emitSyntheticRxPacket(void) {
    /* Payload format expected by python/feralrf/radio.py::read_packets():
     * [ts:8][ch:1][rssi:1][lqi:1][crc_ok:1][len:1][data:len]
     */
    uint8_t payload[16];
    const uint8_t fake_data[] = {0x8Eu, 0x89u, 0xBEu};
    const uint8_t fake_channel = 37u;
    const int8_t fake_rssi = -42;
    const uint8_t fake_lqi = 100u;
    const uint8_t fake_crc_ok = 1u;
    const uint8_t fake_len = (uint8_t)sizeof(fake_data);

    s_fake_timestamp_us += 10000u;

    payload[0] = (uint8_t)(s_fake_timestamp_us & 0xFFu);
    payload[1] = (uint8_t)((s_fake_timestamp_us >> 8) & 0xFFu);
    payload[2] = (uint8_t)((s_fake_timestamp_us >> 16) & 0xFFu);
    payload[3] = (uint8_t)((s_fake_timestamp_us >> 24) & 0xFFu);
    payload[4] = (uint8_t)((s_fake_timestamp_us >> 32) & 0xFFu);
    payload[5] = (uint8_t)((s_fake_timestamp_us >> 40) & 0xFFu);
    payload[6] = (uint8_t)((s_fake_timestamp_us >> 48) & 0xFFu);
    payload[7] = (uint8_t)((s_fake_timestamp_us >> 56) & 0xFFu);
    payload[8] = fake_channel;
    payload[9] = (uint8_t)fake_rssi;
    payload[10] = fake_lqi;
    payload[11] = fake_crc_ok;
    payload[12] = fake_len;
    for (size_t i = 0; i < sizeof(fake_data); i++) {
        payload[13 + i] = fake_data[i];
    }

    OutputIF_sendResponse(RSP_RX_PACKET, s_rx_rsp_seq++, payload, 13u + fake_len);
}

void DataTask_init(void) {
    s_rx_active = false;
    s_rx_packet_emitted = false;
    s_fake_timestamp_us = 0;
    s_rx_rsp_seq = 0;
    RadioIF_init();
}

void DataTask_poll(void) {
    if (TaskEvent_isSet(TASK_EVENT_CONTROL_RX_START)) {
        s_rx_active = RadioIF_startRx();
        s_rx_packet_emitted = false;
        if (s_rx_active) {
            TaskEvent_set(TASK_EVENT_DATA_RX_ACTIVE);
        }
        TaskEvent_clear(TASK_EVENT_CONTROL_RX_START);
    }

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_RX_STOP)) {
        s_rx_active = false;
        s_rx_packet_emitted = false;
        RadioIF_stopRx();
        TaskEvent_clear(TASK_EVENT_DATA_RX_ACTIVE);
        TaskEvent_clear(TASK_EVENT_CONTROL_RX_STOP);
    }

    if (s_rx_active && !s_rx_packet_emitted) {
        DataTask_emitSyntheticRxPacket();
        s_rx_packet_emitted = true;
    }

    RadioIF_poll();
}

bool DataTask_isRxActive(void) {
    return s_rx_active;
}
