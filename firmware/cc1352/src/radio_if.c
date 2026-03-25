/*
 * FeralRF CC1352 - Radio Interface (hybrid backend)
 *
 * Real RF backends: BLE5 RX and IEEE 802.15.4 RX (based on sniffer_fw_cc1252P_7 semantics).
 * Fallback backend: synthetic stream (keeps host pipeline alive if RF init fails).
 */

#include "radio_if.h"

#include "config.h"
#include "phy_manager.h"
#include "smartrf_ble5_0.h"
#include "smartrf_ieee_15_4_0.h"

#include <stddef.h>
#include <string.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/systick.h)
/* clang-format on */
#include <ti/drivers/rf/RF.h>

#define RADIO_IF_RX_QUEUE_DEPTH 8u
#define RADIO_IF_SYSTICK_MAX 0x00FFFFFFu

#define RF_QUEUE_DATA_ENTRY_HEADER_SIZE 8u
#define RF_QUEUE_NUM_DATA_ENTRIES 3u
#define RF_QUEUE_MAX_PACKET_LEN 270u
#define RF_QUEUE_ENTRY_LEN_FIELD_SIZE 2u
#define RF_QUEUE_ENTRY_PAYLOAD_LEN (RF_QUEUE_MAX_PACKET_LEN + RF_QUEUE_ENTRY_LEN_FIELD_SIZE)
#define RF_QUEUE_ALIGN_PAD(len) ((4u - (((len) + RF_QUEUE_DATA_ENTRY_HEADER_SIZE) % 4u)) % 4u)
#define RF_QUEUE_TOTAL_ENTRY_SIZE                                   \
    (RF_QUEUE_DATA_ENTRY_HEADER_SIZE + RF_QUEUE_ENTRY_PAYLOAD_LEN + \
     RF_QUEUE_ALIGN_PAD(RF_QUEUE_ENTRY_PAYLOAD_LEN))
#define RF_QUEUE_DATA_BUFFER_SIZE (RF_QUEUE_NUM_DATA_ENTRIES * RF_QUEUE_TOTAL_ENTRY_SIZE)
#define BLE_ADV_HEADER_LEN 2u
#define BLE_APPENDED_CRC_LEN 3u
#define BLE_APPENDED_RSSI_LEN 1u
#define BLE_APPENDED_STATUS_LEN 2u
#define BLE_APPENDED_TIMESTAMP_LEN 4u
#define BLE_APPENDED_TOTAL_LEN                                                \
    (BLE_APPENDED_CRC_LEN + BLE_APPENDED_RSSI_LEN + BLE_APPENDED_STATUS_LEN + \
     BLE_APPENDED_TIMESTAMP_LEN)
#define BLE_STATUS0_CHANNEL_MASK 0x3Fu
#define BLE_ADV_TX_MAX_PAYLOAD_LEN 31u
#define BLE_ADV_TX_DEVICE_ADDR_LEN 6u
#define IEEE_15_4_CHANNEL_MIN 11u
#define IEEE_15_4_CHANNEL_MAX 26u
#define IEEE_15_4_DEFAULT_CHANNEL IEEE_15_4_CHANNEL_MIN
#define IEEE_15_4_FS_BASE_FREQUENCY_MHZ 2405u
#define IEEE_15_4_FS_STEP_MHZ 5u
#define IEEE_15_4_TX_MAX_PAYLOAD_LEN 125u
#define IEEE_15_4_APPENDED_RSSI_LEN 1u
#define IEEE_15_4_APPENDED_CORRCRC_LEN 1u
#define IEEE_15_4_APPENDED_TIMESTAMP_LEN 4u
#define IEEE_15_4_APPENDED_TOTAL_LEN                                \
    (IEEE_15_4_APPENDED_RSSI_LEN + IEEE_15_4_APPENDED_CORRCRC_LEN + \
     IEEE_15_4_APPENDED_TIMESTAMP_LEN)
#define IEEE_15_4_CORRCRC_CRC_ERR_BIT 0x80u
#define RADIO_IF_TX_TERM_EVENTS                                                          \
    (RF_EventCmdDone | RF_EventLastCmdDone | RF_EventFGCmdDone | RF_EventLastFGCmdDone | \
     RF_EventTxDone | RF_EventCmdCancelled | RF_EventCmdAborted | RF_EventCmdStopped)
#define RADIO_IF_TX_SUCCESS_EVENTS                                                       \
    (RF_EventCmdDone | RF_EventLastCmdDone | RF_EventFGCmdDone | RF_EventLastFGCmdDone | \
     RF_EventTxDone)

typedef enum {
    RADIO_IF_BACKEND_SYNTH = 0,
    RADIO_IF_BACKEND_RF = 1,
} RadioIF_Backend;

typedef enum {
    RADIO_IF_RF_MODE_NONE = 0,
    RADIO_IF_RF_MODE_BLE = 1,
    RADIO_IF_RF_MODE_IEEE_15_4 = 2,
} RadioIF_RfMode;

static bool s_rx_running = false;
static uint64_t s_timestamp_us = 0;
static uint8_t s_selected_phy = 0;
static uint16_t s_channel = 37;
static int8_t s_tx_power_dbm = 0;
static uint32_t s_frequency_hz = 2402000000u;

static RadioIF_RxPacket s_rx_queue[RADIO_IF_RX_QUEUE_DEPTH];
static uint8_t s_rx_head = 0;
static uint8_t s_rx_tail = 0;
static uint8_t s_rx_count = 0;

static bool s_timebase_ready = false;
static uint32_t s_systick_last = 0;
static uint32_t s_systick_cycles_accum = 0;
static uint32_t s_systick_cycles_per_packet = 0;
static RadioIF_Metrics s_metrics = {0u, 0u, 0u, 0u};

static RadioIF_Backend s_backend = RADIO_IF_BACKEND_SYNTH;
static RadioIF_RfMode s_rf_mode = RADIO_IF_RF_MODE_NONE;

/* RF backend state */
static RF_Object s_rf_object;
static RF_Handle s_rf_handle = NULL;
static RF_CmdHandle s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
static RF_Object s_rf_tx_object;

/* RF event flags produced by callback, consumed in poll() */
static volatile uint32_t s_rf_event_flags = 0u;
#define RADIO_IF_RF_EVENT_RX_ENTRY_DONE 0x01u
#define RADIO_IF_RF_EVENT_RX_BUF_FULL 0x02u

static dataQueue_t s_rf_data_queue;
static rfc_dataEntryGeneral_t *s_rf_read_entry = NULL;
static bool s_ble_adv_hop_enabled = false;
static uint8_t s_ble_adv_hop_index = 0u;
static bool s_ble_hop_timebase_ready = false;
static uint32_t s_ble_hop_systick_last = 0u;
static uint32_t s_ble_hop_cycles_accum = 0u;
static uint32_t s_ble_hop_cycles_per_step = 0u;
static const uint8_t s_ble_adv_hop_channels[3] = {37u, 38u, 39u};

/* Jam session state - maintains open RF handle for continuous transmission */
static bool s_jam_session_active = false;
static RF_Object s_jam_rf_object;
static RF_Handle s_jam_rf_handle = NULL;
static uint8_t s_jam_phy = 0u;
static uint8_t s_jam_channel = 0u;
static int8_t s_jam_power_dbm = 0;
#if defined(__GNUC__)
static uint8_t s_ieee154_tx_buffer[IEEE_15_4_TX_MAX_PAYLOAD_LEN] __attribute__((aligned(4)));
static uint8_t s_ble_adv_tx_payload[BLE_ADV_TX_MAX_PAYLOAD_LEN] __attribute__((aligned(4)));
static uint8_t s_ble_adv_tx_device_addr[BLE_ADV_TX_DEVICE_ADDR_LEN]
    __attribute__((aligned(4))) = {0x01u, 0xEEu, 0xDDu, 0xCCu, 0xBBu, 0xAAu};
#else
static uint8_t s_ieee154_tx_buffer[IEEE_15_4_TX_MAX_PAYLOAD_LEN];
static uint8_t s_ble_adv_tx_payload[BLE_ADV_TX_MAX_PAYLOAD_LEN];
static uint8_t s_ble_adv_tx_device_addr[BLE_ADV_TX_DEVICE_ADDR_LEN] = {0x01u, 0xEEu, 0xDDu,
                                                                       0xCCu, 0xBBu, 0xAAu};
#endif

/* Output structure for BLE ADV commands — RF core writes TX stats here. */
static rfc_bleAdvOutput_t s_ble_adv_output;

/* 2.4 GHz power table (LP_CC1352P7-1 style, includes High PA up to 20 dBm). */
static RF_TxPowerTable_Entry s_tx_power_table_24g[] = {
    {-20, RF_TxPowerTable_DEFAULT_PA_ENTRY(6, 3, 0, 2)},
    {-18, RF_TxPowerTable_DEFAULT_PA_ENTRY(8, 3, 0, 3)},
    {-15, RF_TxPowerTable_DEFAULT_PA_ENTRY(10, 3, 0, 3)},
    {-12, RF_TxPowerTable_DEFAULT_PA_ENTRY(12, 3, 0, 5)},
    {-10, RF_TxPowerTable_DEFAULT_PA_ENTRY(15, 3, 0, 5)},
    {-9, RF_TxPowerTable_DEFAULT_PA_ENTRY(16, 3, 0, 5)},
    {-6, RF_TxPowerTable_DEFAULT_PA_ENTRY(20, 3, 0, 8)},
    {-5, RF_TxPowerTable_DEFAULT_PA_ENTRY(22, 3, 0, 9)},
    {-3, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 2, 0, 12)},
    {0, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 1, 0, 20)},
    {1, RF_TxPowerTable_DEFAULT_PA_ENTRY(22, 1, 0, 20)},
    {2, RF_TxPowerTable_DEFAULT_PA_ENTRY(25, 1, 0, 25)},
    {3, RF_TxPowerTable_DEFAULT_PA_ENTRY(29, 1, 0, 28)},
    {4, RF_TxPowerTable_DEFAULT_PA_ENTRY(35, 1, 0, 39)},
    {5, RF_TxPowerTable_DEFAULT_PA_ENTRY(23, 0, 0, 57)},
    {14, RF_TxPowerTable_DEFAULT_PA_ENTRY(63, 0, 1, 0)},
    {15, RF_TxPowerTable_HIGH_PA_ENTRY(18, 0, 0, 36, 0)},
    {16, RF_TxPowerTable_HIGH_PA_ENTRY(24, 0, 0, 43, 0)},
    {17, RF_TxPowerTable_HIGH_PA_ENTRY(28, 0, 0, 51, 2)},
    {18, RF_TxPowerTable_HIGH_PA_ENTRY(34, 0, 0, 64, 4)},
    {19, RF_TxPowerTable_HIGH_PA_ENTRY(15, 3, 0, 36, 4)},
    {20, RF_TxPowerTable_HIGH_PA_ENTRY(18, 3, 0, 71, 27)},
    RF_TxPowerTable_TERMINATION_ENTRY,
};

#if defined(__GNUC__)
static uint8_t s_rf_rx_data_buffer[RF_QUEUE_DATA_BUFFER_SIZE] __attribute__((aligned(4)));
#else
static uint8_t s_rf_rx_data_buffer[RF_QUEUE_DATA_BUFFER_SIZE];
#endif

static RF_TxPowerTable_Value RadioIF_resolveTxPowerValue(int8_t requested_dbm) {
    RF_TxPowerTable_Value value = RF_TxPowerTable_findValue(s_tx_power_table_24g, requested_dbm);
    size_t i = 0u;
    size_t best_index = 0u;
    int16_t best_diff = 0x7FFF;

    if (value.rawValue != RF_TxPowerTable_INVALID_VALUE) {
        return value;
    }

    while (s_tx_power_table_24g[i].value.rawValue != RF_TxPowerTable_INVALID_VALUE) {
        int16_t diff = (int16_t)s_tx_power_table_24g[i].power - (int16_t)requested_dbm;
        if (diff < 0) {
            diff = (int16_t)(-diff);
        }
        if (diff < best_diff) {
            best_diff = diff;
            best_index = i;
        }
        i++;
    }

    if (best_diff == 0x7FFF) {
        return value;
    }

    return s_tx_power_table_24g[best_index].value;
}

static void RadioIF_applyRfTxPower(RF_Handle rf_handle, RF_TxPowerTable_Value value) {
    if (rf_handle == NULL || value.rawValue == RF_TxPowerTable_INVALID_VALUE) {
        return;
    }
    (void)RF_setTxPower(rf_handle, value);
}

static bool RadioIF_executeTxCommand(RF_Mode *mode, RF_RadioSetup *setup_cmd, RF_Op *fs_cmd,
                                     RF_Op *tx_cmd, RF_TxPowerTable_Value tx_power) {
    RF_Params rf_params;
    RF_Handle tx_rf_handle = NULL;
    RF_EventMask result = 0u;

    RF_Params_init(&rf_params);
    tx_rf_handle = RF_open(&s_rf_tx_object, mode, setup_cmd, &rf_params);
    if (tx_rf_handle == NULL) {
        return false;
    }
    RadioIF_applyRfTxPower(tx_rf_handle, tx_power);

    result = RF_runCmd(tx_rf_handle, fs_cmd, RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
    if ((result & RADIO_IF_TX_SUCCESS_EVENTS) == 0u) {
        RF_close(tx_rf_handle);
        return false;
    }

    result = RF_runCmd(tx_rf_handle, tx_cmd, RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
    RF_close(tx_rf_handle);

    return (result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u;
}

static uint8_t RadioIF_convertToBleChannel(uint8_t channel) {
    /* Matches sniffer_fw_cc1252P_7 conversion semantics. */
    if (channel <= 39u) {
        return channel;
    }
    if (channel == 102u) {
        return 37u;
    }
    if (channel == 126u) {
        return 38u;
    }
    if (channel == 180u) {
        return 39u;
    }
    if (channel >= 104u && channel <= 124u) {
        return (uint8_t)((channel - 104u) / 2u);
    }
    if (channel >= 128u && channel <= 178u) {
        return (uint8_t)(11u + ((channel - 128u) / 2u));
    }
    return channel;
}

static uint16_t RadioIF_bleChannelToFrequency(uint8_t channel) {
    if (channel == 37u) {
        return 2402u;
    }
    if (channel == 38u) {
        return 2426u;
    }
    if (channel == 39u) {
        return 2480u;
    }
    if (channel <= 10u) {
        return (uint16_t)(2404u + (2u * channel));
    }
    if (channel <= 36u) {
        return (uint16_t)(2428u + (2u * (channel - 11u)));
    }

    /* Custom channel encoding uses F = channel + 2300 MHz */
    return (uint16_t)(2300u + channel);
}

static uint8_t RadioIF_convertToIeee154Channel(uint16_t channel) {
    if (channel >= IEEE_15_4_CHANNEL_MIN && channel <= IEEE_15_4_CHANNEL_MAX) {
        return (uint8_t)channel;
    }

    /* Allow raw RF core channel index (0..15) for compatibility with some tools. */
    if (channel <= (IEEE_15_4_CHANNEL_MAX - IEEE_15_4_CHANNEL_MIN)) {
        return (uint8_t)(channel + IEEE_15_4_CHANNEL_MIN);
    }

    return IEEE_15_4_DEFAULT_CHANNEL;
}

static uint16_t RadioIF_ieee154ChannelToFrequency(uint8_t channel) {
    return (uint16_t)(IEEE_15_4_FS_BASE_FREQUENCY_MHZ +
                      ((uint16_t)(channel - IEEE_15_4_CHANNEL_MIN) * IEEE_15_4_FS_STEP_MHZ));
}

static void RadioIF_applyBleChannelConfig(uint8_t channel) {
    uint8_t ble_channel = RadioIF_convertToBleChannel(channel);

    Ble5_0_cmdBle5GenericRx.channel = ble_channel;
    Ble5_0_cmdBle5GenericRx.whitening.init = (uint8_t)(0x40u | (ble_channel & 0x3Fu));
    Ble5_0_cmdBle5GenericRx.whitening.bOverride = 1u;

    Ble5_0_cmdFs.frequency = RadioIF_bleChannelToFrequency(ble_channel);
    Ble5_0_cmdFs.fractFreq = 0u;
}

static void RadioIF_applyIeee154ChannelConfig(uint16_t channel) {
    uint8_t ieee_channel = RadioIF_convertToIeee154Channel(channel);

    Ieee154_0_cmdIeeeRx.channel = ieee_channel;
    Ieee154_0_cmdFs.frequency = RadioIF_ieee154ChannelToFrequency(ieee_channel);
    Ieee154_0_cmdFs.fractFreq = 0u;
}

static bool RadioIF_isBleAdvChannel(uint16_t channel);

static bool RadioIF_transmitIeee154Raw(const uint8_t *payload, uint8_t payload_len) {
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(s_tx_power_dbm);

    if (payload == NULL || payload_len == 0u || payload_len > IEEE_15_4_TX_MAX_PAYLOAD_LEN) {
        return false;
    }

    memcpy(s_ieee154_tx_buffer, payload, payload_len);

    RadioIF_applyIeee154ChannelConfig(s_channel);

    Ieee154_0_cmdIeeeTx.pPayload = s_ieee154_tx_buffer;
    Ieee154_0_cmdIeeeTx.payloadLen = payload_len;
    Ieee154_0_cmdIeeeTx.startTrigger.triggerType = TRIG_NOW;
    Ieee154_0_cmdIeeeTx.startTrigger.pastTrig = 1u;
    Ieee154_0_cmdIeeeTx.condition.rule = COND_NEVER;
    Ieee154_0_cmdIeeeTx.pNextOp = 0;

    return RadioIF_executeTxCommand(&Ieee154_0_mode, (RF_RadioSetup *)&Ieee154_0_cmdRadioSetup,
                                    (RF_Op *)&Ieee154_0_cmdFs, (RF_Op *)&Ieee154_0_cmdIeeeTx,
                                    tx_power);
}

static bool RadioIF_transmitBleAdvRaw(const uint8_t *payload, uint8_t payload_len) {
    RF_Params rf_params;
    RF_Handle tx_handle = NULL;
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(s_tx_power_dbm);
    RF_EventMask result;
    uint8_t ble_channel = RadioIF_convertToBleChannel((uint8_t)s_channel);
    uint16_t frequency_mhz;

    if (payload == NULL || payload_len == 0u || payload_len > BLE_ADV_TX_MAX_PAYLOAD_LEN) {
        return false;
    }

    if (!RadioIF_isBleAdvChannel(ble_channel)) {
        return false;
    }

    memcpy(s_ble_adv_tx_payload, payload, payload_len);

    /* Get frequency for this channel */
    frequency_mhz = RadioIF_bleChannelToFrequency(ble_channel);

    /* Open RF handle */
    RF_Params_init(&rf_params);
    tx_handle = RF_open(&s_rf_tx_object, &Ble5_0_mode,
                        (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup, &rf_params);
    if (tx_handle == NULL) {
        return false;
    }

    RadioIF_applyRfTxPower(tx_handle, tx_power);

    /* Set TX power on command struct as well as handle */
    if (tx_power.paType == RF_TxPowerTable_HighPA) {
        Ble5_0_cmdBle5AdvNc.txPower = 0x0000u;
        Ble5_0_cmdBle5AdvNc.tx20Power = tx_power.rawValue;
    } else {
        Ble5_0_cmdBle5AdvNc.txPower = (uint16_t)tx_power.rawValue;
        Ble5_0_cmdBle5AdvNc.tx20Power = 0x00000000u;
    }

    /* Configure ADV_NC command — BLE5 ADV handles frequency synth internally */
    Ble5_0_cmdBle5AdvNc.channel = ble_channel;
    Ble5_0_cmdBle5AdvNc.whitening.init = 0u;
    Ble5_0_cmdBle5AdvNc.whitening.bOverride = 0u;
    Ble5_0_cmdBle5AdvNc.startTrigger.triggerType = TRIG_NOW;
    Ble5_0_cmdBle5AdvNc.startTrigger.pastTrig = 1u;
    Ble5_0_cmdBle5AdvNc.condition.rule = COND_NEVER;
    Ble5_0_cmdBle5AdvNc.pNextOp = 0;

    if (Ble5_0_cmdBle5AdvNc.pParams == NULL) {
        RF_close(tx_handle);
        return false;
    }

    Ble5_0_cmdBle5AdvNc.pParams->advLen = payload_len;
    Ble5_0_cmdBle5AdvNc.pParams->scanRspLen = 0u;
    Ble5_0_cmdBle5AdvNc.pParams->pAdvData = s_ble_adv_tx_payload;
    Ble5_0_cmdBle5AdvNc.pParams->pScanRspData = NULL;
    Ble5_0_cmdBle5AdvNc.pParams->pDeviceAddress = (uint16_t *)s_ble_adv_tx_device_addr;
    Ble5_0_cmdBle5AdvNc.pParams->pWhiteList = NULL;

    /* Provide output structure for RF core to write TX stats */
    memset(&s_ble_adv_output, 0, sizeof(s_ble_adv_output));
    Ble5_0_cmdBle5AdvNc.pOutput = &s_ble_adv_output;

    /* Execute TX */
    result = RF_runCmd(tx_handle, (RF_Op *)&Ble5_0_cmdBle5AdvNc,
                       RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);

    /* Check command status for debug */
    uint16_t cmd_status = Ble5_0_cmdBle5AdvNc.status;

    RF_close(tx_handle);

    /* Debug: return false if command didn't complete properly */
    if ((result & RADIO_IF_TX_SUCCESS_EVENTS) == 0u) {
        return false;
    }

    /* BLE_DONE_OK = 0x3400 */
    if (cmd_status != 0x3400u) {
        return false;
    }

    return true;
}

static bool RadioIF_isBleAdvChannel(uint16_t channel) {
    return (channel >= 37u) && (channel <= 39u);
}

static bool RadioIF_isIeee154PhySelected(void) {
    return s_selected_phy == PHY_MANAGER_PHY_IEEE_802_15_4;
}

static void RadioIF_updateBleHopMode(void) {
#if RADIO_IF_BLE_ADV_HOP_ENABLE
    if (PhyManager_isBlePhy(s_selected_phy) && RadioIF_isBleAdvChannel(s_channel)) {
        s_ble_adv_hop_enabled = true;
        s_ble_adv_hop_index = (uint8_t)(s_channel - 37u);
        return;
    }
#endif
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
}

static void RadioIF_initBleHopCadence(void) {
    s_ble_hop_timebase_ready = false;
    s_ble_hop_systick_last = 0u;
    s_ble_hop_cycles_accum = 0u;
    s_ble_hop_cycles_per_step = 0u;
}

static uint32_t RadioIF_getBleHopElapsedCycles(void) {
    uint32_t systick_now = SysTickValueGet();
    uint32_t elapsed_cycles = 0u;

    if (!s_ble_hop_timebase_ready) {
        s_ble_hop_systick_last = systick_now;
        s_ble_hop_timebase_ready = true;
        return 0u;
    }

    if (s_ble_hop_systick_last >= systick_now) {
        elapsed_cycles = s_ble_hop_systick_last - systick_now;
    } else {
        elapsed_cycles = s_ble_hop_systick_last + (RADIO_IF_SYSTICK_MAX - systick_now) + 1u;
    }

    s_ble_hop_systick_last = systick_now;
    return elapsed_cycles;
}

static bool RadioIF_shouldHopBleChannel(void) {
    if (!s_ble_adv_hop_enabled) {
        return false;
    }

    if (s_ble_hop_cycles_per_step == 0u) {
        uint32_t clock_hz = SysCtrlClockGet();
        s_ble_hop_cycles_per_step = (clock_hz / 1000u) * RADIO_IF_BLE_ADV_HOP_INTERVAL_MS;
        if (s_ble_hop_cycles_per_step == 0u) {
            s_ble_hop_cycles_per_step = 1u;
        }
    }

    s_ble_hop_cycles_accum += RadioIF_getBleHopElapsedCycles();
    if (s_ble_hop_cycles_accum < s_ble_hop_cycles_per_step) {
        return false;
    }

    s_ble_hop_cycles_accum -= s_ble_hop_cycles_per_step;
    return true;
}

static void RadioIF_resetRxQueue(void) {
    s_rx_head = 0;
    s_rx_tail = 0;
    s_rx_count = 0;
}

static bool RadioIF_enqueuePacket(const RadioIF_RxPacket *pkt) {
    if (s_rx_count >= RADIO_IF_RX_QUEUE_DEPTH || pkt == NULL) {
        return false;
    }

    s_rx_queue[s_rx_head] = *pkt;
    s_rx_head = (uint8_t)((s_rx_head + 1u) % RADIO_IF_RX_QUEUE_DEPTH);
    s_rx_count++;
    return true;
}

void RadioIF_getMetrics(RadioIF_Metrics *out) {
    if (out == NULL) {
        return;
    }

    *out = s_metrics;
}

void RadioIF_resetMetrics(void) {
    s_metrics.rx_ok = 0u;
    s_metrics.rx_crc_err = 0u;
    s_metrics.rx_drop = 0u;
    s_metrics.rx_overflow = 0u;
}

static uint32_t RadioIF_getElapsedCycles(void) {
    uint32_t systick_now = SysTickValueGet();
    uint32_t elapsed_cycles = 0;

    if (!s_timebase_ready) {
        s_systick_last = systick_now;
        s_timebase_ready = true;
        return 0;
    }

    if (s_systick_last >= systick_now) {
        elapsed_cycles = s_systick_last - systick_now;
    } else {
        elapsed_cycles = s_systick_last + (RADIO_IF_SYSTICK_MAX - systick_now) + 1u;
    }

    s_systick_last = systick_now;
    return elapsed_cycles;
}

static void RadioIF_initSyntheticCadence(void) {
    s_timebase_ready = false;
    s_systick_last = 0;
    s_systick_cycles_accum = 0;
    s_systick_cycles_per_packet = 0;
}

static void RadioIF_updateSyntheticCadence(uint32_t elapsed_cycles) {
    if (s_systick_cycles_per_packet == 0u) {
        uint32_t clock_hz = SysCtrlClockGet();
        s_systick_cycles_per_packet = (clock_hz / 1000u) * RADIO_IF_SYNTH_PACKET_INTERVAL_MS;
        if (s_systick_cycles_per_packet == 0u) {
            s_systick_cycles_per_packet = 1u;
        }
    }

    s_systick_cycles_accum += elapsed_cycles;

    while (s_systick_cycles_accum >= s_systick_cycles_per_packet) {
        RadioIF_RxPacket pkt = {0};

        s_systick_cycles_accum -= s_systick_cycles_per_packet;
        s_timestamp_us += ((uint64_t)RADIO_IF_SYNTH_PACKET_INTERVAL_MS * 1000u);

        pkt.timestamp_us = s_timestamp_us;
        pkt.channel = (uint8_t)s_channel;
        pkt.rssi_dbm = (int8_t)(-42 + (s_selected_phy & 0x01u));
        pkt.lqi = 100u;
        pkt.crc_ok = true;
        pkt.data_len = 3u;
        pkt.data[0] = 0x8Eu;
        pkt.data[1] = 0x89u;
        pkt.data[2] = 0xBEu;

        if (!RadioIF_enqueuePacket(&pkt)) {
            break;
        }
    }
}

static bool RadioIF_createRfDataQueue(dataQueue_t *queue, uint8_t *buf, uint16_t buf_len,
                                      uint8_t num_entries, uint16_t entry_len) {
    uint8_t *first_entry = NULL;
    uint8_t *entry = NULL;
    uint8_t pad = 0;

    if (queue == NULL || buf == NULL || num_entries == 0u) {
        return false;
    }

    pad = (uint8_t)RF_QUEUE_ALIGN_PAD(entry_len);

    if (buf_len < (uint16_t)(num_entries * (RF_QUEUE_DATA_ENTRY_HEADER_SIZE + entry_len + pad))) {
        return false;
    }

    first_entry = buf;
    for (uint8_t i = 0; i < num_entries; i++) {
        entry = first_entry + (i * (RF_QUEUE_DATA_ENTRY_HEADER_SIZE + entry_len + pad));

        ((rfc_dataEntry_t *)entry)->status = DATA_ENTRY_PENDING;
        ((rfc_dataEntry_t *)entry)->config.type = DATA_ENTRY_TYPE_GEN;
        ((rfc_dataEntry_t *)entry)->config.lenSz = RF_QUEUE_ENTRY_LEN_FIELD_SIZE;
        ((rfc_dataEntry_t *)entry)->length = entry_len;

        ((rfc_dataEntryGeneral_t *)entry)->pNextEntry =
            (uint8_t *)(&((rfc_dataEntryGeneral_t *)entry)->data) + entry_len + pad;
    }

    ((rfc_dataEntry_t *)entry)->pNextEntry = first_entry;

    queue->pCurrEntry = first_entry;
    queue->pLastEntry = NULL;
    s_rf_read_entry = (rfc_dataEntryGeneral_t *)first_entry;
    return true;
}

static void RadioIF_resetRfDataQueue(void) {
    rfc_dataEntryGeneral_t *entry = NULL;

    if (s_rf_read_entry == NULL) {
        return;
    }

    entry = s_rf_read_entry;
    s_rf_data_queue.pCurrEntry = s_rf_rx_data_buffer;
    s_rf_read_entry = (rfc_dataEntryGeneral_t *)s_rf_rx_data_buffer;

    for (uint8_t i = 0; i < RF_QUEUE_NUM_DATA_ENTRIES; i++) {
        entry->status = DATA_ENTRY_PENDING;
        entry = (rfc_dataEntryGeneral_t *)entry->pNextEntry;
    }
}

static bool RadioIF_rfHasPacket(void) {
    return (s_rf_read_entry != NULL) && (s_rf_read_entry->status == DATA_ENTRY_FINISHED);
}

static void RadioIF_rfConsumeEntry(void) {
    if (s_rf_read_entry == NULL) {
        return;
    }

    s_rf_read_entry->status = DATA_ENTRY_PENDING;
    s_rf_read_entry = (rfc_dataEntryGeneral_t *)s_rf_read_entry->pNextEntry;
}

static void RadioIF_rfCallback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e) {
    (void)h;
    (void)ch;

    if ((e & RF_EventRxEntryDone) != 0u) {
        s_rf_event_flags |= RADIO_IF_RF_EVENT_RX_ENTRY_DONE;
    }

    if ((e & RF_EventRxBufFull) != 0u) {
        if ((s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4) && (s_rf_handle != NULL)) {
            /* IEEE 802.15.4 RX does not always self-terminate on buffer full. */
            RF_cancelCmd(s_rf_handle, s_rf_rx_cmd, 0);
        }
        s_rf_event_flags |= RADIO_IF_RF_EVENT_RX_BUF_FULL;
    }
}

static bool RadioIF_runFsAndPostRx(void) {
    RF_EventMask event_mask = RF_EventRxEntryDone | RF_EventRxBufFull;
    RF_Op *fs_cmd = NULL;
    RF_Op *rx_cmd = NULL;

    if (s_rf_handle == NULL) {
        return false;
    }

    switch (s_rf_mode) {
    case RADIO_IF_RF_MODE_BLE:
        fs_cmd = (RF_Op *)&Ble5_0_cmdFs;
        rx_cmd = (RF_Op *)&Ble5_0_cmdBle5GenericRx;
        break;
    case RADIO_IF_RF_MODE_IEEE_15_4:
        fs_cmd = (RF_Op *)&Ieee154_0_cmdFs;
        rx_cmd = (RF_Op *)&Ieee154_0_cmdIeeeRx;
        event_mask |= RF_EventLastFGCmdDone;
        break;
    case RADIO_IF_RF_MODE_NONE:
    default:
        return false;
    }

    (void)RF_runCmd(s_rf_handle, fs_cmd, RF_PriorityNormal, NULL, 0);
    s_rf_rx_cmd =
        RF_postCmd(s_rf_handle, rx_cmd, RF_PriorityNormal, &RadioIF_rfCallback, event_mask);
    return s_rf_rx_cmd >= 0;
}

static bool RadioIF_restartRfRx(void) {
    if (s_rf_handle == NULL) {
        return false;
    }

    RadioIF_resetRfDataQueue();
    RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
    return RadioIF_runFsAndPostRx();
}

static bool RadioIF_rearmIeee154RxAfterOverflow(void) {
    if (s_rf_handle == NULL || s_rf_mode != RADIO_IF_RF_MODE_IEEE_15_4) {
        return false;
    }

    /* Match base sniffer behavior: after IEEE RX buffer full + cancel, reset queue and re-post RX.
     */
    RadioIF_resetRfDataQueue();
    s_rf_rx_cmd = RF_postCmd(s_rf_handle, (RF_Op *)&Ieee154_0_cmdIeeeRx, RF_PriorityNormal,
                             &RadioIF_rfCallback,
                             RF_EventRxEntryDone | RF_EventRxBufFull | RF_EventLastFGCmdDone);
    return s_rf_rx_cmd >= 0;
}

static bool RadioIF_advanceBleHopChannel(void) {
    if (!s_ble_adv_hop_enabled) {
        return false;
    }

    s_ble_adv_hop_index = (uint8_t)((s_ble_adv_hop_index + 1u) % 3u);
    s_channel = s_ble_adv_hop_channels[s_ble_adv_hop_index];
    RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    return RadioIF_restartRfRx();
}

static bool RadioIF_startBleRfBackend(void) {
    RF_Params rf_params;

    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return false;
    }

    s_rf_mode = RADIO_IF_RF_MODE_BLE;
    RadioIF_applyBleChannelConfig((uint8_t)s_channel);

    Ble5_0_cmdBle5GenericRx.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAutoFlushIgnored = 0u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAutoFlushCrcErr = 0u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAutoFlushEmpty = 0u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bIncludeLenByte = 1u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bIncludeCrc = 1u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAppendRssi = 1u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAppendStatus = 1u;
    Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAppendTimestamp = 1u;

    RF_Params_init(&rf_params);
    s_rf_handle =
        RF_open(&s_rf_object, &Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup, &rf_params);

    if (s_rf_handle == NULL) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    if (!RadioIF_runFsAndPostRx()) {
        RF_close(s_rf_handle);
        s_rf_handle = NULL;
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    s_rf_event_flags = 0u;
    return true;
}

static bool RadioIF_startIeee154RfBackend(void) {
    RF_Params rf_params;

    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return false;
    }

    s_rf_mode = RADIO_IF_RF_MODE_IEEE_15_4;
    RadioIF_applyIeee154ChannelConfig(s_channel);

    Ieee154_0_cmdIeeeRx.pRxQ = &s_rf_data_queue;
    Ieee154_0_cmdIeeeRx.rxConfig.bAutoFlushCrc = 0u;
    Ieee154_0_cmdIeeeRx.rxConfig.bAutoFlushIgn = 0u;
    Ieee154_0_cmdIeeeRx.rxConfig.bIncludePhyHdr = 1u;
    Ieee154_0_cmdIeeeRx.rxConfig.bIncludeCrc = 1u;
    Ieee154_0_cmdIeeeRx.rxConfig.bAppendRssi = 1u;
    Ieee154_0_cmdIeeeRx.rxConfig.bAppendCorrCrc = 1u;
    Ieee154_0_cmdIeeeRx.rxConfig.bAppendSrcInd = 0u;
    Ieee154_0_cmdIeeeRx.rxConfig.bAppendTimestamp = 1u;

    RF_Params_init(&rf_params);
    s_rf_handle = RF_open(&s_rf_object, &Ieee154_0_mode, (RF_RadioSetup *)&Ieee154_0_cmdRadioSetup,
                          &rf_params);

    if (s_rf_handle == NULL) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    if (!RadioIF_runFsAndPostRx()) {
        RF_close(s_rf_handle);
        s_rf_handle = NULL;
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    s_rf_event_flags = 0u;
    return true;
}

static void RadioIF_stopRfBackend(void) {
    if (s_rf_handle != NULL) {
        if ((s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4) && (s_rf_rx_cmd >= 0)) {
            RF_cancelCmd(s_rf_handle, s_rf_rx_cmd, 0);
        }
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
        RF_close(s_rf_handle);
        s_rf_handle = NULL;
    }

    s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
    s_rf_event_flags = 0u;
    s_rf_mode = RADIO_IF_RF_MODE_NONE;
    RadioIF_resetRfDataQueue();
}

static void RadioIF_processBlePackets(void) {
    while (RadioIF_rfHasPacket()) {
        RadioIF_RxPacket pkt = {0};
        uint16_t entry_len = 0;
        uint8_t *entry_data = NULL;
        uint8_t *raw_entry = NULL;
        uint16_t pdu_len = 0;
        uint8_t pdu_offset = 0;
        bool layout_ok = false;
        uint16_t status0_idx = 0;
        uint16_t rssi_idx = 0;
        uint16_t timestamp_idx = 0;
        uint8_t status0 = 0;
        uint32_t rat_timestamp = 0;

        raw_entry = (uint8_t *)&s_rf_read_entry->data;
        entry_len = (uint16_t)raw_entry[0] | ((uint16_t)raw_entry[1] << 8);
        entry_data = raw_entry + RF_QUEUE_ENTRY_LEN_FIELD_SIZE;

        if (entry_len < (uint16_t)(BLE_ADV_HEADER_LEN + BLE_APPENDED_TOTAL_LEN)) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        /* Layout A: [PDU][CRC(3)][RSSI][STATUS(2)][TIMESTAMP(4)] */
        pdu_len = (uint16_t)BLE_ADV_HEADER_LEN + (uint16_t)(entry_data[1] & 0x3Fu);
        if ((uint16_t)(pdu_len + BLE_APPENDED_TOTAL_LEN) == entry_len) {
            pdu_offset = 0u;
            layout_ok = true;
        } else if (entry_len > 1u) {
            /* Layout B: [LEN][PDU][CRC(3)][RSSI][STATUS(2)][TIMESTAMP(4)] */
            pdu_len = (uint16_t)BLE_ADV_HEADER_LEN + (uint16_t)(entry_data[2] & 0x3Fu);
            if ((uint16_t)(1u + pdu_len + BLE_APPENDED_TOTAL_LEN) == entry_len) {
                pdu_offset = 1u;
                layout_ok = true;
            }
        }

        if (!layout_ok) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        /* BLE appended fields are at the end of raw payload:
         * [ ... PDU ... ][CRC(3)][RSSI(1)][STATUS0][STATUS1][TIMESTAMP(4)].
         */
        timestamp_idx = (uint16_t)(entry_len - BLE_APPENDED_TIMESTAMP_LEN);
        status0_idx = (uint16_t)(entry_len - BLE_APPENDED_TIMESTAMP_LEN - BLE_APPENDED_STATUS_LEN);
        rssi_idx = (uint16_t)(status0_idx - BLE_APPENDED_RSSI_LEN);

        status0 = entry_data[status0_idx];
        rat_timestamp = (uint32_t)entry_data[timestamp_idx] |
                        ((uint32_t)entry_data[timestamp_idx + 1u] << 8) |
                        ((uint32_t)entry_data[timestamp_idx + 2u] << 16) |
                        ((uint32_t)entry_data[timestamp_idx + 3u] << 24);

        pkt.timestamp_us = ((uint64_t)rat_timestamp) / 4u;
        pkt.channel = (uint8_t)(status0 & BLE_STATUS0_CHANNEL_MASK);
        pkt.rssi_dbm = (int8_t)entry_data[rssi_idx];
        pkt.lqi = 0u;
        pkt.crc_ok = (status0 & 0x80u) == 0u;
        if (!pkt.crc_ok) {
            s_metrics.rx_crc_err++;
            RadioIF_rfConsumeEntry();
            continue;
        }
        pkt.data_len = (pdu_len > RADIO_IF_MAX_PACKET_DATA) ? RADIO_IF_MAX_PACKET_DATA : pdu_len;
        memcpy(pkt.data, &entry_data[pdu_offset], pkt.data_len);

        if (RadioIF_enqueuePacket(&pkt)) {
            s_metrics.rx_ok++;
        } else {
            s_metrics.rx_drop++;
        }
        RadioIF_rfConsumeEntry();
    }
}

static void RadioIF_processIeee154Packets(void) {
    while (RadioIF_rfHasPacket()) {
        RadioIF_RxPacket pkt = {0};
        uint16_t entry_len = 0u;
        uint8_t *entry_data = NULL;
        uint8_t *raw_entry = NULL;
        uint16_t frame_with_hdr_len = 0u;
        uint16_t expected_with_hdr_len = 0u;
        uint16_t rssi_idx = 0u;
        uint16_t corrcrc_idx = 0u;
        uint16_t timestamp_idx = 0u;
        uint8_t corrcrc = 0u;
        uint32_t rat_timestamp = 0u;

        raw_entry = (uint8_t *)&s_rf_read_entry->data;
        entry_len = (uint16_t)raw_entry[0] | ((uint16_t)raw_entry[1] << 8);
        entry_data = raw_entry + RF_QUEUE_ENTRY_LEN_FIELD_SIZE;

        if (entry_len <= IEEE_15_4_APPENDED_TOTAL_LEN) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        frame_with_hdr_len = (uint16_t)(entry_len - IEEE_15_4_APPENDED_TOTAL_LEN);
        expected_with_hdr_len = (uint16_t)(1u + entry_data[0]);

        if (frame_with_hdr_len < 1u || expected_with_hdr_len > frame_with_hdr_len) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        if (expected_with_hdr_len < frame_with_hdr_len) {
            frame_with_hdr_len = expected_with_hdr_len;
        }

        rssi_idx = frame_with_hdr_len;
        corrcrc_idx = (uint16_t)(rssi_idx + IEEE_15_4_APPENDED_RSSI_LEN);
        timestamp_idx = (uint16_t)(corrcrc_idx + IEEE_15_4_APPENDED_CORRCRC_LEN);

        corrcrc = entry_data[corrcrc_idx];
        rat_timestamp = (uint32_t)entry_data[timestamp_idx] |
                        ((uint32_t)entry_data[timestamp_idx + 1u] << 8) |
                        ((uint32_t)entry_data[timestamp_idx + 2u] << 16) |
                        ((uint32_t)entry_data[timestamp_idx + 3u] << 24);

        pkt.timestamp_us = ((uint64_t)rat_timestamp) / 4u;
        pkt.channel = (uint8_t)s_channel;
        pkt.rssi_dbm = (int8_t)entry_data[rssi_idx];
        pkt.lqi = (uint8_t)(corrcrc & 0x7Fu);
        pkt.crc_ok = (corrcrc & IEEE_15_4_CORRCRC_CRC_ERR_BIT) == 0u;
        if (!pkt.crc_ok) {
            s_metrics.rx_crc_err++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        pkt.data_len = entry_data[0];
        if (pkt.data_len > RADIO_IF_MAX_PACKET_DATA) {
            pkt.data_len = RADIO_IF_MAX_PACKET_DATA;
        }
        memcpy(pkt.data, &entry_data[1], pkt.data_len);

        if (RadioIF_enqueuePacket(&pkt)) {
            s_metrics.rx_ok++;
        } else {
            s_metrics.rx_drop++;
        }
        RadioIF_rfConsumeEntry();
    }
}

static void RadioIF_processRfPackets(void) {
    switch (s_rf_mode) {
    case RADIO_IF_RF_MODE_BLE:
        RadioIF_processBlePackets();
        break;
    case RADIO_IF_RF_MODE_IEEE_15_4:
        RadioIF_processIeee154Packets();
        break;
    case RADIO_IF_RF_MODE_NONE:
    default:
        /* No active RF parser. */
        break;
    }
}

void RadioIF_init(void) {
    s_rx_running = false;
    s_timestamp_us = 0;
    s_selected_phy = 0;
    s_channel = 37;
    s_tx_power_dbm = 0;
    s_frequency_hz = 2402000000u;
    s_backend = RADIO_IF_BACKEND_SYNTH;

    RadioIF_resetRxQueue();
    RadioIF_initSyntheticCadence();

    s_rf_handle = NULL;
    s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
    s_rf_event_flags = 0u;
    s_rf_mode = RADIO_IF_RF_MODE_NONE;
    s_rf_read_entry = NULL;
    RadioIF_resetMetrics();
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
    RadioIF_initBleHopCadence();
}

void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz) {
    s_selected_phy = phy;

    if (channel != 0u) {
        if (PhyManager_isBlePhy(phy)) {
            s_channel = RadioIF_convertToBleChannel((uint8_t)channel);
        } else if (phy == PHY_MANAGER_PHY_IEEE_802_15_4) {
            s_channel = RadioIF_convertToIeee154Channel(channel);
        } else {
            s_channel = channel;
        }
    } else if ((phy == PHY_MANAGER_PHY_IEEE_802_15_4) &&
               (s_channel < IEEE_15_4_CHANNEL_MIN || s_channel > IEEE_15_4_CHANNEL_MAX)) {
        s_channel = IEEE_15_4_DEFAULT_CHANNEL;
    }

    if (frequency_hz != 0u) {
        s_frequency_hz = frequency_hz;
    }

    if (PhyManager_isBlePhy(s_selected_phy)) {
        RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    } else if (RadioIF_isIeee154PhySelected()) {
        RadioIF_applyIeee154ChannelConfig(s_channel);
    }

    RadioIF_updateBleHopMode();
    if (s_ble_adv_hop_enabled) {
        RadioIF_initBleHopCadence();
    }

    if ((s_backend == RADIO_IF_BACKEND_RF) && (s_rf_handle != NULL) &&
        ((PhyManager_isBlePhy(s_selected_phy) && (s_rf_mode == RADIO_IF_RF_MODE_BLE)) ||
         (RadioIF_isIeee154PhySelected() && (s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4)))) {
        (void)RadioIF_restartRfRx();
    }
}

void RadioIF_setChannel(uint8_t channel) {
    if (PhyManager_isBlePhy(s_selected_phy)) {
        s_channel = RadioIF_convertToBleChannel(channel);
    } else if (RadioIF_isIeee154PhySelected()) {
        s_channel = RadioIF_convertToIeee154Channel(channel);
    } else {
        s_channel = channel;
    }

    if (PhyManager_isBlePhy(s_selected_phy)) {
        RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    } else if (RadioIF_isIeee154PhySelected()) {
        RadioIF_applyIeee154ChannelConfig(s_channel);
    }

    RadioIF_updateBleHopMode();
    if (s_ble_adv_hop_enabled) {
        RadioIF_initBleHopCadence();
    }

    if ((s_backend == RADIO_IF_BACKEND_RF) && (s_rf_handle != NULL) &&
        ((PhyManager_isBlePhy(s_selected_phy) && (s_rf_mode == RADIO_IF_RF_MODE_BLE)) ||
         (RadioIF_isIeee154PhySelected() && (s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4)))) {
        (void)RadioIF_restartRfRx();
    }
}

void RadioIF_setPower(int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;
}

bool RadioIF_transmitRaw(const uint8_t *data, uint8_t data_len, int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;

    if (s_rx_running || (s_rf_handle != NULL)) {
        return false;
    }

    if (RadioIF_isIeee154PhySelected()) {
        return RadioIF_transmitIeee154Raw(data, data_len);
    }

    if (PhyManager_isBlePhy(s_selected_phy)) {
        return RadioIF_transmitBleAdvRaw(data, data_len);
    }

    return false;
}

bool RadioIF_startRx(void) {
    s_rx_running = true;
    RadioIF_resetRxQueue();

    if (PhyManager_supportsRfBackendRx(s_selected_phy)) {
        bool rf_started = false;

        if (PhyManager_isBlePhy(s_selected_phy)) {
            rf_started = RadioIF_startBleRfBackend();
        } else if (RadioIF_isIeee154PhySelected()) {
            rf_started = RadioIF_startIeee154RfBackend();
        }

        if (rf_started) {
            s_backend = RADIO_IF_BACKEND_RF;
            RadioIF_updateBleHopMode();
            if (s_ble_adv_hop_enabled) {
                RadioIF_initBleHopCadence();
            }
            return true;
        }
    }

    /* RF backend failed to start — report failure instead of silent synthetic fallback. */
    s_rx_running = false;
    s_backend = RADIO_IF_BACKEND_SYNTH;
    s_rf_mode = RADIO_IF_RF_MODE_NONE;
    return false;
}

void RadioIF_stopRx(void) {
    s_rx_running = false;
    RadioIF_resetRxQueue();

    if (s_backend == RADIO_IF_BACKEND_RF) {
        RadioIF_stopRfBackend();
    }

    s_backend = RADIO_IF_BACKEND_SYNTH;
    s_rf_mode = RADIO_IF_RF_MODE_NONE;
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
    RadioIF_initBleHopCadence();
    RadioIF_initSyntheticCadence();
}

bool RadioIF_isRxRunning(void) {
    return s_rx_running;
}

bool RadioIF_isRfBackendActive(void) {
    return s_backend == RADIO_IF_BACKEND_RF;
}

void RadioIF_poll(void) {
    if (!s_rx_running) {
        return;
    }

    if (s_backend == RADIO_IF_BACKEND_RF) {
        uint32_t events = s_rf_event_flags;
        s_rf_event_flags = 0u;

        if ((events & RADIO_IF_RF_EVENT_RX_BUF_FULL) != 0u && s_rf_handle != NULL) {
            s_metrics.rx_overflow++;
            if (s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4) {
                (void)RadioIF_rearmIeee154RxAfterOverflow();
            } else {
                (void)RadioIF_restartRfRx();
            }
        }

        if ((events & RADIO_IF_RF_EVENT_RX_ENTRY_DONE) != 0u) {
            RadioIF_processRfPackets();
        }

        /* Also drain if callback raced and queue already has entries. */
        if (RadioIF_rfHasPacket()) {
            RadioIF_processRfPackets();
        }

        if ((s_rf_mode == RADIO_IF_RF_MODE_BLE) && RadioIF_shouldHopBleChannel()) {
            (void)RadioIF_advanceBleHopChannel();
        }
    } else {
        RadioIF_updateSyntheticCadence(RadioIF_getElapsedCycles());
    }

    (void)s_tx_power_dbm;
    (void)s_frequency_hz;
}

bool RadioIF_popRxPacket(RadioIF_RxPacket *out) {
    if (out == NULL || s_rx_count == 0u) {
        return false;
    }

    *out = s_rx_queue[s_rx_tail];
    s_rx_tail = (uint8_t)((s_rx_tail + 1u) % RADIO_IF_RX_QUEUE_DEPTH);
    s_rx_count--;
    return true;
}

/*
 * Jamming Session Functions
 *
 * For BLE: Uses repeated ADV_NC transmissions with minimal gap
 * For IEEE 802.15.4: Uses repeated IEEE TX transmissions
 * Maintains open RF handle to eliminate per-packet open/close overhead.
 */

/* Jam payload - all 0xFF for maximum interference */
static const uint8_t s_jam_ble_payload[31] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const uint8_t s_jam_ieee154_payload[125] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static bool RadioIF_executeJamTx(RF_Handle rf_handle, uint8_t phy, uint8_t channel,
                                  int8_t power_dbm) {
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(power_dbm);
    RF_EventMask result;

    if (PhyManager_isBlePhy(phy)) {
        uint8_t ble_channel = RadioIF_convertToBleChannel(channel);
        RadioIF_applyBleChannelConfig(ble_channel);

        Ble5_0_cmdBle5AdvNc.channel = ble_channel;
        Ble5_0_cmdBle5AdvNc.whitening.init = (uint8_t)(0x40u | (ble_channel & 0x3Fu));
        Ble5_0_cmdBle5AdvNc.whitening.bOverride = 1u;
        Ble5_0_cmdBle5AdvNc.startTrigger.triggerType = TRIG_NOW;
        Ble5_0_cmdBle5AdvNc.startTrigger.pastTrig = 1u;
        Ble5_0_cmdBle5AdvNc.condition.rule = COND_NEVER;
        Ble5_0_cmdBle5AdvNc.pNextOp = 0;

        if (tx_power.paType == RF_TxPowerTable_HighPA) {
            Ble5_0_cmdBle5AdvNc.txPower = 0x0000u;
            Ble5_0_cmdBle5AdvNc.tx20Power = tx_power.rawValue;
        } else {
            Ble5_0_cmdBle5AdvNc.txPower = (uint16_t)tx_power.rawValue;
            Ble5_0_cmdBle5AdvNc.tx20Power = 0x00000000u;
        }

        if (Ble5_0_cmdBle5AdvNc.pParams == NULL) {
            return false;
        }

        Ble5_0_cmdBle5AdvNc.pParams->advLen = 31u;
        Ble5_0_cmdBle5AdvNc.pParams->pAdvData = (uint8_t *)s_jam_ble_payload;
        memset(&s_ble_adv_output, 0, sizeof(s_ble_adv_output));
        Ble5_0_cmdBle5AdvNc.pOutput = &s_ble_adv_output;

        result = RF_runCmd(rf_handle, (RF_Op *)&Ble5_0_cmdBle5AdvNc,
                          RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
        return (result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u;
    }

    if (phy == PHY_MANAGER_PHY_IEEE_802_15_4) {
        RadioIF_applyIeee154ChannelConfig((uint16_t)channel);

        Ieee154_0_cmdIeeeTx.pPayload = (uint8_t *)s_jam_ieee154_payload;
        Ieee154_0_cmdIeeeTx.payloadLen = 125u;
        Ieee154_0_cmdIeeeTx.startTrigger.triggerType = TRIG_NOW;
        Ieee154_0_cmdIeeeTx.startTrigger.pastTrig = 1u;
        Ieee154_0_cmdIeeeTx.condition.rule = COND_NEVER;
        Ieee154_0_cmdIeeeTx.pNextOp = 0;

        result = RF_runCmd(rf_handle, (RF_Op *)&Ieee154_0_cmdIeeeTx,
                          RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
        return (result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u;
    }

    return false;
}

bool RadioIF_startJamSession(uint8_t phy, uint8_t channel, int8_t power_dbm) {
    RF_Params rf_params;
    RF_Mode *mode;
    RF_RadioSetup *setup_cmd;
    bool tx_ok;

    /* Cannot start jam if RX is running or another jam is active */
    if (s_rx_running || s_jam_session_active) {
        return false;
    }

    /* Determine mode based on PHY */
    if (PhyManager_isBlePhy(phy)) {
        mode = &Ble5_0_mode;
        setup_cmd = (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup;
    } else if (phy == PHY_MANAGER_PHY_IEEE_802_15_4) {
        mode = &Ieee154_0_mode;
        setup_cmd = (RF_RadioSetup *)&Ieee154_0_cmdRadioSetup;
    } else {
        return false;
    }

    /* Open RF handle for jamming session */
    RF_Params_init(&rf_params);
    s_jam_rf_handle = RF_open(&s_jam_rf_object, mode, setup_cmd, &rf_params);
    if (s_jam_rf_handle == NULL) {
        return false;
    }

    /* Execute first TX to start the jam session */
    tx_ok = RadioIF_executeJamTx(s_jam_rf_handle, phy, channel, power_dbm);
    if (!tx_ok) {
        RF_close(s_jam_rf_handle);
        s_jam_rf_handle = NULL;
        return false;
    }

    s_jam_session_active = true;
    s_jam_phy = phy;
    s_jam_channel = channel;
    s_jam_power_dbm = power_dbm;
    return true;
}

void RadioIF_pollJamSession(void) {
    if (!s_jam_session_active || s_jam_rf_handle == NULL) {
        return;
    }

    /* Execute multiple TX per poll to increase jamming rate */
    for (uint8_t i = 0u; i < 10u; i++) {
        if (!RadioIF_executeJamTx(s_jam_rf_handle, s_jam_phy, s_jam_channel, s_jam_power_dbm)) {
            break;
        }
    }
}

void RadioIF_stopJamSession(void) {
    if (!s_jam_session_active) {
        return;
    }

    /* Close RF handle */
    if (s_jam_rf_handle != NULL) {
        RF_close(s_jam_rf_handle);
        s_jam_rf_handle = NULL;
    }

    s_jam_session_active = false;
    s_jam_phy = 0u;
    s_jam_channel = 0u;
    s_jam_power_dbm = 0;
}

bool RadioIF_isJamSessionActive(void) {
    return s_jam_session_active;
}
