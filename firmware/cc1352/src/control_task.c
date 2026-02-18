/*
 * FeralRF CC1352 - Control Task (polling variant)
 */

#include "control_task.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/sys_ctrl.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/systick.h>

#include "ll_manager.h"
#include "phy_manager.h"
#include "radio_if.h"
#include "task_event.h"

/* Firmware info payload */
#define FW_VERSION_MAJOR 0x01u
#define FW_VERSION_MINOR 0x00u
#define FW_VERSION_PATCH 0x00u
#define FW_CAPABILITY_RX_STATS 0x01u
#define FW_CAPABILITY_LL_PDU_META 0x02u
#define FW_CAPABILITY_LL_STATS_EXT 0x04u
#define FW_CAPABILITIES \
    (FW_CAPABILITY_RX_STATS | FW_CAPABILITY_LL_PDU_META | FW_CAPABILITY_LL_STATS_EXT)
#define CONTROL_TASK_TX_RAW_MAX_LEN 125u
#define CONTROL_TASK_SYSTICK_MAX 0x00FFFFFFu

static uint8_t s_selected_phy = 0;
static uint16_t s_channel = 0;
static int8_t s_tx_power_dbm = 0;
static uint32_t s_frequency_hz = 0;
static bool s_rx_enabled = false;
static bool s_tx_raw_pending = false;
static uint8_t s_tx_raw_payload[CONTROL_TASK_TX_RAW_MAX_LEN];
static uint8_t s_tx_raw_len = 0u;
static int8_t s_tx_raw_power_dbm = 0;
static bool s_tx_burst_pending = false;
static uint8_t s_tx_burst_payload[CONTROL_TASK_TX_RAW_MAX_LEN];
static uint8_t s_tx_burst_len = 0u;
static uint16_t s_tx_burst_remaining = 0u;
static uint32_t s_tx_burst_interval_us = 0u;
static uint64_t s_tx_burst_next_due_us = 0u;
static int8_t s_tx_burst_power_dbm = 0;
static bool s_tx_timebase_ready = false;
static uint32_t s_tx_systick_last = 0u;
static uint32_t s_tx_cycles_per_us = 1u;
static uint32_t s_tx_cycles_carry = 0u;
static uint64_t s_tx_time_us = 0u;

static const uint8_t s_serial[8] = {'F', 'E', 'R', 'A', 'L', 'R', 'F', '1'};

static void ControlTask_initTxTimebase(void) {
    uint32_t clock_hz = SysCtrlClockGet();

    s_tx_timebase_ready = false;
    s_tx_systick_last = 0u;
    s_tx_cycles_carry = 0u;
    s_tx_time_us = 0u;
    s_tx_cycles_per_us = clock_hz / 1000000u;
    if (s_tx_cycles_per_us == 0u) {
        s_tx_cycles_per_us = 1u;
    }
}

static uint64_t ControlTask_getTimeUs(void) {
    uint32_t systick_now = SysTickValueGet();
    uint32_t elapsed_cycles = 0u;
    uint32_t total_cycles = 0u;

    if (!s_tx_timebase_ready) {
        s_tx_systick_last = systick_now;
        s_tx_timebase_ready = true;
        return s_tx_time_us;
    }

    if (s_tx_systick_last >= systick_now) {
        elapsed_cycles = s_tx_systick_last - systick_now;
    } else {
        elapsed_cycles = s_tx_systick_last + (CONTROL_TASK_SYSTICK_MAX - systick_now) + 1u;
    }
    s_tx_systick_last = systick_now;

    total_cycles = s_tx_cycles_carry + elapsed_cycles;
    s_tx_time_us += (uint64_t)(total_cycles / s_tx_cycles_per_us);
    s_tx_cycles_carry = total_cycles % s_tx_cycles_per_us;
    return s_tx_time_us;
}

void ControlTask_init(void) {
    PhyManager_init();
    LLManager_init();
    LLManager_select(PhyManager_getSelectedLinkLayer());
    ControlTask_initTxTimebase();

    s_selected_phy = 0;
    s_channel = 0;
    s_tx_power_dbm = 0;
    s_frequency_hz = 0;
    s_rx_enabled = false;
    s_tx_raw_pending = false;
    s_tx_raw_len = 0u;
    s_tx_raw_power_dbm = 0;
    s_tx_burst_pending = false;
    s_tx_burst_len = 0u;
    s_tx_burst_remaining = 0u;
    s_tx_burst_interval_us = 0u;
    s_tx_burst_next_due_us = 0u;
    s_tx_burst_power_dbm = 0;
}

void ControlTask_onRadioInit(void) {
    ControlTask_initTxTimebase();
    s_rx_enabled = false;
    s_tx_raw_pending = false;
    s_tx_raw_len = 0u;
    s_tx_burst_pending = false;
    s_tx_burst_len = 0u;
    s_tx_burst_remaining = 0u;
    s_tx_burst_interval_us = 0u;
    s_tx_burst_next_due_us = 0u;
    s_tx_burst_power_dbm = 0;
    TaskEvent_clear(TASK_EVENT_CONTROL_RX_START);
    TaskEvent_clear(TASK_EVENT_CONTROL_TX_RAW);
    TaskEvent_clear(TASK_EVENT_CONTROL_TX_BURST);
    TaskEvent_set(TASK_EVENT_CONTROL_RX_STOP);
    TaskEvent_clear(TASK_EVENT_DATA_RX_ACTIVE);
    RadioIF_stopRx();
    RadioIF_resetMetrics();
    LLManager_resetStats();
}

bool ControlTask_onSetPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz) {
    if (!PhyManager_select(phy)) {
        return false;
    }

    LLManager_select(PhyManager_getSelectedLinkLayer());

    s_selected_phy = phy;
    s_channel = channel;
    s_frequency_hz = frequency_hz;
    RadioIF_setPhy(phy, channel, frequency_hz);
    return true;
}

void ControlTask_onSetChannel(uint8_t channel) {
    s_channel = channel;
    RadioIF_setChannel(channel);
}

void ControlTask_onSetPower(int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;
    RadioIF_setPower(power_dbm);
}

bool ControlTask_onTxRaw(const uint8_t *payload, uint8_t payload_len, int8_t power_dbm) {
    if (payload == NULL || payload_len == 0u || payload_len > CONTROL_TASK_TX_RAW_MAX_LEN ||
        s_rx_enabled || s_tx_raw_pending || s_tx_burst_pending) {
        return false;
    }

    memcpy(s_tx_raw_payload, payload, payload_len);
    s_tx_raw_len = payload_len;
    s_tx_raw_power_dbm = power_dbm;
    s_tx_raw_pending = true;
    TaskEvent_set(TASK_EVENT_CONTROL_TX_RAW);
    return true;
}

bool ControlTask_onTxBurst(const uint8_t *payload, uint8_t payload_len, uint16_t count,
                           uint32_t interval_us) {
    if (payload == NULL || payload_len == 0u || payload_len > CONTROL_TASK_TX_RAW_MAX_LEN ||
        count == 0u || s_rx_enabled || s_tx_raw_pending || s_tx_burst_pending) {
        return false;
    }

    memcpy(s_tx_burst_payload, payload, payload_len);
    s_tx_burst_len = payload_len;
    s_tx_burst_remaining = count;
    s_tx_burst_interval_us = interval_us;
    s_tx_burst_next_due_us = ControlTask_getTimeUs();
    s_tx_burst_power_dbm = s_tx_power_dbm;
    s_tx_burst_pending = true;
    TaskEvent_set(TASK_EVENT_CONTROL_TX_BURST);
    return true;
}

void ControlTask_processTxRaw(void) {
    if (!s_tx_raw_pending) {
        return;
    }

    s_tx_power_dbm = s_tx_raw_power_dbm;
    RadioIF_setPower(s_tx_raw_power_dbm);
    (void)RadioIF_transmitRaw(s_tx_raw_payload, s_tx_raw_len, s_tx_raw_power_dbm);

    s_tx_raw_pending = false;
    s_tx_raw_len = 0u;
}

void ControlTask_processTxBurst(void) {
    uint64_t now_us = 0u;

    if (!s_tx_burst_pending) {
        return;
    }

    now_us = ControlTask_getTimeUs();
    if (now_us < s_tx_burst_next_due_us) {
        return;
    }

    RadioIF_setPower(s_tx_burst_power_dbm);
    if (!RadioIF_transmitRaw(s_tx_burst_payload, s_tx_burst_len, s_tx_burst_power_dbm)) {
        s_tx_burst_pending = false;
        s_tx_burst_len = 0u;
        s_tx_burst_remaining = 0u;
        return;
    }

    if (s_tx_burst_remaining > 0u) {
        s_tx_burst_remaining--;
    }
    if (s_tx_burst_remaining == 0u) {
        s_tx_burst_pending = false;
        s_tx_burst_len = 0u;
        return;
    }

    s_tx_burst_next_due_us = now_us + (uint64_t)s_tx_burst_interval_us;
}

bool ControlTask_isTxBurstPending(void) {
    return s_tx_burst_pending;
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
