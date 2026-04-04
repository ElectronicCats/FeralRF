/*
 * FeralRF CC1352 - Control Task (polling variant)
 */

#include "control_task.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/aon_rtc.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/systick.h)
/* clang-format on */

#include "config.h"
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
#define CONTROL_TASK_JAM_BLE_PAYLOAD_LEN 31u
#define CONTROL_TASK_JAM_IEEE154_PAYLOAD_LEN 125u

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
static bool s_tx_continuous_pending = false;
static uint8_t s_tx_continuous_payload[CONTROL_TASK_TX_RAW_MAX_LEN];
static uint8_t s_tx_continuous_len = 0u;
static uint32_t s_tx_continuous_interval_us = 0u;
static uint64_t s_tx_continuous_next_due_us = 0u;
static int8_t s_tx_continuous_power_dbm = 0;
static bool s_tx_timebase_ready = false;
static uint32_t s_tx_systick_last = 0u;
static uint32_t s_tx_cycles_per_us = 1u;
static uint32_t s_tx_cycles_carry = 0u;
static uint64_t s_tx_time_us = 0u;
static bool s_jam_active = false;
static uint64_t s_jam_stop_due_us = 0u;
static uint64_t s_jam_cooldown_due_us = 0u;
static uint8_t s_jam_payload[CONTROL_TASK_TX_RAW_MAX_LEN];

static const uint8_t s_serial[8] = {'F', 'E', 'R', 'A', 'L', 'R', 'F', '1'};
static uint64_t ControlTask_getTimeUs(void);

static uint64_t ControlTask_getWallTimeUs(void) {
    uint64_t rtc = AONRTCCurrent64BitValueGet();
    uint64_t sec = rtc >> 32;
    uint64_t subsec = rtc & 0xFFFFFFFFu;
    uint64_t us = sec * 1000000u;

    us += (subsec * 1000000u) >> 32;
    return us;
}

static bool ControlTask_isAnyTxPending(void) {
    return s_tx_raw_pending || s_tx_burst_pending || s_tx_continuous_pending;
}

static bool ControlTask_canStartTx(const uint8_t *payload, uint8_t payload_len) {
    return payload != NULL && payload_len > 0u && payload_len <= CONTROL_TASK_TX_RAW_MAX_LEN &&
           !s_rx_enabled;
}

static void ControlTask_clearJamWindow(void) {
    s_jam_active = false;
    s_jam_stop_due_us = 0u;
}

static bool ControlTask_isJamChannelAllowed(uint8_t channel) {
    if (PhyManager_isBlePhy(s_selected_phy)) {
        return channel >= JAM_BLE_CHANNEL_MIN && channel <= JAM_BLE_CHANNEL_MAX;
    }
    if (s_selected_phy == PHY_MANAGER_PHY_IEEE_802_15_4) {
        return channel >= JAM_IEEE154_CHANNEL_MIN && channel <= JAM_IEEE154_CHANNEL_MAX;
    }
    return false;
}

static void ControlTask_armJamCooldown(void) {
    s_jam_cooldown_due_us = ControlTask_getWallTimeUs() + ((uint64_t)JAM_COOLDOWN_MS * 1000u);
}

static void ControlTask_clearTxBurstState(void) {
    s_tx_burst_pending = false;
    s_tx_burst_len = 0u;
    s_tx_burst_remaining = 0u;
    s_tx_burst_interval_us = 0u;
    s_tx_burst_next_due_us = 0u;
}

static void ControlTask_clearTxContinuousState(void) {
    s_tx_continuous_pending = false;
    s_tx_continuous_len = 0u;
    s_tx_continuous_interval_us = 0u;
    s_tx_continuous_next_due_us = 0u;
}

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

static void ControlTask_resetTxPowerState(void) {
    s_tx_raw_power_dbm = 0;
    s_tx_burst_power_dbm = 0;
    s_tx_continuous_power_dbm = 0;
}

static void ControlTask_resetTxJamState(bool clear_task_events) {
    s_tx_raw_pending = false;
    s_tx_raw_len = 0u;

    ControlTask_clearTxBurstState();
    ControlTask_clearTxContinuousState();
    ControlTask_clearJamWindow();

    /* Stop any active jam session */
    if (RadioIF_isJamSessionActive()) {
        RadioIF_stopJamSession();
    }

    s_tx_raw_power_dbm = 0;
    s_tx_burst_power_dbm = 0;
    s_tx_continuous_power_dbm = 0;

    if (clear_task_events) {
        TaskEvent_clear(TASK_EVENT_CONTROL_TX_RAW);
        TaskEvent_clear(TASK_EVENT_CONTROL_TX_BURST);
        TaskEvent_clear(TASK_EVENT_CONTROL_TX_CONTINUOUS);
    }
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
    s_jam_cooldown_due_us = 0u;
    ControlTask_resetTxJamState(false);
    ControlTask_resetTxPowerState();
    memset(s_jam_payload, 0xFF, sizeof(s_jam_payload));
}

void ControlTask_onRadioInit(void) {
    ControlTask_initTxTimebase();
    s_rx_enabled = false;
    s_jam_cooldown_due_us = 0u;
    ControlTask_resetTxJamState(true);
    ControlTask_resetTxPowerState();
    TaskEvent_clear(TASK_EVENT_CONTROL_RX_START);
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
    if (!ControlTask_canStartTx(payload, payload_len)) {
        return false;
    }

    memcpy(s_tx_raw_payload, payload, payload_len);
    s_tx_raw_len = payload_len;
    s_tx_raw_power_dbm = power_dbm;
    memcpy(s_tx_raw_payload, payload, payload_len);
    s_tx_raw_len = payload_len;
    s_tx_raw_power_dbm = power_dbm;
    s_tx_raw_pending = true;
    TaskEvent_set(TASK_EVENT_CONTROL_TX_RAW);
    return true;
}

bool ControlTask_onTxFrame(const uint8_t *payload, uint8_t payload_len) {
    return ControlTask_onTxRaw(payload, payload_len, s_tx_power_dbm);
}

bool ControlTask_onTxBurst(const uint8_t *payload, uint8_t payload_len, uint16_t count,
                           uint32_t interval_us) {
    if (count == 0u || !ControlTask_canStartTx(payload, payload_len)) {
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

bool ControlTask_onTxContinuous(const uint8_t *payload, uint8_t payload_len, uint32_t interval_us) {
    if (!ControlTask_canStartTx(payload, payload_len)) {
        return false;
    }

    memcpy(s_tx_continuous_payload, payload, payload_len);
    s_tx_continuous_len = payload_len;
    s_tx_continuous_interval_us = interval_us;
    s_tx_continuous_next_due_us = ControlTask_getTimeUs();
    s_tx_continuous_power_dbm = s_tx_power_dbm;
    s_tx_continuous_pending = true;
    TaskEvent_set(TASK_EVENT_CONTROL_TX_CONTINUOUS);
    return true;
}

void ControlTask_onTxStop(void) {
    ControlTask_resetTxJamState(true);
}

bool ControlTask_onJamContinuous(uint8_t channel, int8_t power_dbm, uint16_t duration_ms) {
    uint64_t now_wall_us = ControlTask_getWallTimeUs();

    if (duration_ms == 0u || duration_ms > JAM_MAX_DURATION_MS || s_rx_enabled ||
        ControlTask_isAnyTxPending()) {
        return false;
    }
    if (now_wall_us < s_jam_cooldown_due_us) {
        return false;
    }
    if (!ControlTask_isJamChannelAllowed(channel)) {
        return false;
    }

    /* Use optimized jam session with continuous wave */
    if (!RadioIF_startJamSession(s_selected_phy, channel, power_dbm)) {
        return false;
    }

    s_jam_active = true;
    s_jam_stop_due_us = ControlTask_getWallTimeUs() + ((uint64_t)duration_ms * 1000u);
    return true;
}

void ControlTask_onJamStop(void) {
    RadioIF_stopJamSession();
    ControlTask_armJamCooldown();
    ControlTask_clearJamWindow();
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
        ControlTask_clearTxBurstState();
        return;
    }

    if (s_tx_burst_remaining > 0u) {
        s_tx_burst_remaining--;
    }
    if (s_tx_burst_remaining == 0u) {
        ControlTask_clearTxBurstState();
        return;
    }

    s_tx_burst_next_due_us = now_us + (uint64_t)s_tx_burst_interval_us;
}

void ControlTask_processTxContinuous(void) {
    uint64_t now_us = 0u;

    if (!s_tx_continuous_pending) {
        return;
    }

    now_us = ControlTask_getTimeUs();
    if (now_us < s_tx_continuous_next_due_us) {
        return;
    }

    RadioIF_setPower(s_tx_continuous_power_dbm);
    if (!RadioIF_transmitRaw(s_tx_continuous_payload, s_tx_continuous_len,
                             s_tx_continuous_power_dbm)) {
        ControlTask_clearTxContinuousState();
        return;
    }

    s_tx_continuous_next_due_us = now_us + (uint64_t)s_tx_continuous_interval_us;
}

void ControlTask_processJamTimeout(void) {
    uint64_t now_wall_us = 0u;

    if (!s_jam_active) {
        return;
    }

    now_wall_us = ControlTask_getWallTimeUs();
    if (now_wall_us >= s_jam_stop_due_us) {
        ControlTask_onJamStop();
    }
}

bool ControlTask_isTxBurstPending(void) {
    return s_tx_burst_pending;
}

bool ControlTask_isTxContinuousPending(void) {
    return s_tx_continuous_pending;
}

bool ControlTask_isTxBusy(void) {
    return ControlTask_isAnyTxPending() || s_jam_active;
}

bool ControlTask_isJamActive(void) {
    return s_jam_active;
}

void ControlTask_onRxStart(void) {
    s_rx_enabled = true;
    /* Single-task: start RX synchronously */
    (void)RadioIF_startRx();
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
