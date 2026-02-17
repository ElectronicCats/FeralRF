/*
 * FeralRF CC1352 - Control Task (polling variant)
 */

#include "control_task.h"

#include <stddef.h>
#include <stdint.h>

#include "task_event.h"

/* Firmware info payload */
#define FW_VERSION_MAJOR 0x01u
#define FW_VERSION_MINOR 0x00u
#define FW_VERSION_PATCH 0x00u
#define FW_CAPABILITIES 0x01u

static uint8_t s_selected_phy = 0;
static uint16_t s_channel = 0;
static int8_t s_tx_power_dbm = 0;
static uint32_t s_frequency_hz = 0;
static bool s_rx_enabled = false;

static const uint8_t s_serial[8] = {'F', 'E', 'R', 'A', 'L', 'R', 'F', '1'};

void ControlTask_init(void) {
    s_selected_phy = 0;
    s_channel = 0;
    s_tx_power_dbm = 0;
    s_frequency_hz = 0;
    s_rx_enabled = false;
}

void ControlTask_onRadioInit(void) {
    s_rx_enabled = false;
}

void ControlTask_onSetPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz) {
    s_selected_phy = phy;
    s_channel = channel;
    s_frequency_hz = frequency_hz;
}

void ControlTask_onSetChannel(uint8_t channel) {
    s_channel = channel;
}

void ControlTask_onSetPower(int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;
}

void ControlTask_onRxStart(void) {
    s_rx_enabled = true;
    TaskEvent_set(TASK_EVENT_CONTROL_RX_START);
}

void ControlTask_onRxStop(void) {
    s_rx_enabled = false;
    TaskEvent_set(TASK_EVENT_CONTROL_RX_STOP);
}

bool ControlTask_isRxEnabled(void) {
    return s_rx_enabled;
}

void ControlTask_getInfoPayload(uint8_t *payload, uint16_t payload_len) {
    if (payload == NULL || payload_len < 12u) {
        return;
    }

    payload[0] = FW_VERSION_MAJOR;
    payload[1] = FW_VERSION_MINOR;
    payload[2] = FW_VERSION_PATCH;
    payload[3] = FW_CAPABILITIES;
    for (size_t i = 0; i < sizeof(s_serial); i++) {
        payload[4 + i] = s_serial[i];
    }
}
