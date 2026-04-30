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
#include "smartrf_prop_0.h"
#include "smartrf_prop_2_4ghz.h"
#include "ti_radio_config_433.h"

#include <stddef.h>
#include <string.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_ble_mailbox.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/systick.h)
/* clang-format on */
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/rf/RF.h>
#include <ti/sysbios/knl/Semaphore.h>

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
#define PROP_APPENDED_STATUS_LEN 1u
#define PROP_APPENDED_RSSI_LEN 1u
#define PROP_APPENDED_TIMESTAMP_LEN 4u
#define PROP_APPENDED_TOTAL_LEN \
    (PROP_APPENDED_STATUS_LEN + PROP_APPENDED_RSSI_LEN + PROP_APPENDED_TIMESTAMP_LEN)
#define PROP_TX_MAX_PAYLOAD_LEN 255u
#define PROP_STATUS_CRC_OK_BIT 0x80u

typedef enum {
    RADIO_IF_BACKEND_SYNTH = 0,
    RADIO_IF_BACKEND_RF = 1,
} RadioIF_Backend;

static void RadioIF_applyBlePhyMode(uint8_t phy);
static bool RadioIF_isSub1ghzPhySelected(void);
static bool RadioIF_switchRfMode(RF_Mode *mode, RF_RadioSetup *setup);
static rfc_CMD_PROP_RADIO_DIV_SETUP_PA_t s_prop_setup_copy;
static void RadioIF_applySub1ghzChannelConfig(uint16_t channel, uint32_t frequency_hz);

typedef enum {
    RADIO_IF_RF_MODE_NONE = 0,
    RADIO_IF_RF_MODE_BLE = 1,
    RADIO_IF_RF_MODE_IEEE_15_4 = 2,
    RADIO_IF_RF_MODE_SUB_1GHZ = 3,
    RADIO_IF_RF_MODE_PROP_2_4GHZ = 4,
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

/* RF backend state — single handle, RF_yield+RF_close+RF_open for switch (rfDiagnostics) */
static RF_Object s_rf_object;
static RF_Handle s_rf_handle = NULL;
static RF_CmdHandle s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;

/* 433 MHz persistent handle — opened at boot, never closed */
static RF_Object s_433_rf_object;
static RF_Handle s_433_handle = NULL;
static RF_Mode *s_current_rf_mode = NULL;

RF_Object *RadioIF_get433Object(void) {
    return &s_433_rf_object;
}
void RadioIF_set433Handle(RF_Handle h) {
    s_433_handle = h;
}
RF_Handle RadioIF_get433Handle(void) {
    return s_433_handle;
}

/* RF event signaling — TI-RTOS Semaphore for ISR→task notification */
static volatile uint32_t s_rf_event_flags = 0u;
#define RADIO_IF_RF_EVENT_RX_ENTRY_DONE 0x01u
#define RADIO_IF_RF_EVENT_RX_BUF_FULL 0x02u
static Semaphore_Handle s_rf_semaphore = NULL;
static Semaphore_Struct s_rf_semaphore_struct;

static dataQueue_t s_rf_data_queue;
static rfc_dataEntryGeneral_t *s_rf_read_entry = NULL;
static bool s_ble_active_scan = false;
static rfc_ble5ScanInitOutput_t s_ble5_scanner_output;
static bool s_ble_adv_hop_enabled = false;
static bool s_ble_adv_hop_allowed = RADIO_IF_BLE_ADV_HOP_ENABLE;
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
static bool s_prop_ook_active = false;
static uint16_t s_last_tx_status = 0u;
static uint8_t s_last_tx_result = 0u;
static uint32_t s_last_tx_event = 0u;

static RF_Mode *RadioIF_getPropMode(void) {
    /* OOK requires genook MCE/RFE patches for TX and RX. Once loaded, these
     * patches cannot be unloaded safely (TI SDK bug: RFCCpePatchReset is a
     * no-op). Switching to another mode after OOK will hang the RF Core.
     * The user must power-cycle/reflash to use other modes after OOK. */
    if (s_prop_ook_active) {
        return &Prop0_modeOok;
    }
    /* SysConfig 433 mode for <861 MHz — pristine struct, never modified at runtime */
    if (s_frequency_hz > 0u && (s_frequency_hz / 1000000u) < 861u) {
        return &Prop0_mode433;
    }
    return &Prop0_mode;
}
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

#if defined(__GNUC__)
static uint8_t s_prop_tx_buffer[PROP_TX_MAX_PAYLOAD_LEN] __attribute__((aligned(4)));
#else
static uint8_t s_prop_tx_buffer[PROP_TX_MAX_PAYLOAD_LEN];
#endif

/* Sub-1GHz power table (CC1352P7, 868/915 MHz band). */
static RF_TxPowerTable_Entry s_tx_power_table_sub1g[] = {
    {-20, RF_TxPowerTable_DEFAULT_PA_ENTRY(6, 3, 0, 2)},
    {-15, RF_TxPowerTable_DEFAULT_PA_ENTRY(10, 3, 0, 3)},
    {-10, RF_TxPowerTable_DEFAULT_PA_ENTRY(15, 3, 0, 5)},
    {-5, RF_TxPowerTable_DEFAULT_PA_ENTRY(22, 3, 0, 9)},
    {0, RF_TxPowerTable_DEFAULT_PA_ENTRY(19, 1, 0, 20)},
    {3, RF_TxPowerTable_DEFAULT_PA_ENTRY(29, 1, 0, 28)},
    {5, RF_TxPowerTable_DEFAULT_PA_ENTRY(23, 0, 0, 57)},
    {8, RF_TxPowerTable_DEFAULT_PA_ENTRY(31, 0, 0, 67)},
    {10, RF_TxPowerTable_DEFAULT_PA_ENTRY(37, 0, 0, 76)},
    {12, RF_TxPowerTable_DEFAULT_PA_ENTRY(16, 0, 0, 82)},
    {14, RF_TxPowerTable_DEFAULT_PA_ENTRY(63, 0, 1, 0)},
    {15, RF_TxPowerTable_HIGH_PA_ENTRY(18, 0, 0, 36, 0)},
    {17, RF_TxPowerTable_HIGH_PA_ENTRY(28, 0, 0, 51, 2)},
    {20, RF_TxPowerTable_HIGH_PA_ENTRY(18, 3, 0, 71, 27)},
    RF_TxPowerTable_TERMINATION_ENTRY,
};

/* Persistent TX session — avoids RF_open/FS/RF_close overhead per packet */
static RF_Object s_rf_tx_session_object;
static RF_Handle s_tx_session_handle = NULL;
static RF_Mode *s_tx_session_mode = NULL;

/* Persistent non-433 RF handle. Opened on first non-433 set_phy, kept open
 * for the firmware's lifetime. Mode switches between BLE/IEEE/Sub1G(non-433)
 * are done by re-running RadioSetup as a command on this handle, NOT by
 * RF_close+RF_open (which leaks SemaphoreP/CPE state across cycles and breaks
 * after ~12 PHY switches — the F9 root cause). */
static RF_Handle s_non433_handle = NULL;

/* F22 test mode — CW or PRBS via rfc_CMD_TX_TEST. Single in-flight cmd. */
static rfc_CMD_TX_TEST_t s_cmd_tx_test;
static RF_CmdHandle s_test_cmd_handle = RF_SCHEDULE_CMD_ERROR;

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
    /* +6 to +10 dBm omitted: Sub-1GHz prop table values, invalid at 2.4 GHz */
    {11, RF_TxPowerTable_DEFAULT_PA_ENTRY(26, 2, 0, 51)},
    {12, RF_TxPowerTable_DEFAULT_PA_ENTRY(16, 0, 0, 82)},
    {13, RF_TxPowerTable_DEFAULT_PA_ENTRY(36, 0, 0, 89)},
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

static RF_TxPowerTable_Entry *RadioIF_getActivePowerTable(void) {
    if (RadioIF_isSub1ghzPhySelected()) {
        return s_tx_power_table_sub1g;
    }
    return s_tx_power_table_24g;
}

static RF_TxPowerTable_Value RadioIF_resolveTxPowerValue(int8_t requested_dbm) {
    RF_TxPowerTable_Entry *table = RadioIF_getActivePowerTable();
    RF_TxPowerTable_Value value = RF_TxPowerTable_findValue(table, requested_dbm);
    size_t i = 0u;
    size_t best_index = 0u;
    int16_t best_diff = 0x7FFF;

    if (value.rawValue != RF_TxPowerTable_INVALID_VALUE) {
        return value;
    }

    while (table[i].value.rawValue != RF_TxPowerTable_INVALID_VALUE) {
        int16_t diff = (int16_t)table[i].power - (int16_t)requested_dbm;
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

    return table[best_index].value;
}

static void RadioIF_applyRfTxPower(RF_Handle rf_handle, RF_TxPowerTable_Value value) {
    if (rf_handle == NULL || value.rawValue == RF_TxPowerTable_INVALID_VALUE) {
        return;
    }
    (void)RF_setTxPower(rf_handle, value);
}

static bool RadioIF_executeTxCommand(RF_Mode *mode, RF_RadioSetup *setup_cmd, RF_Op *fs_cmd,
                                     RF_Op *tx_cmd, RF_TxPowerTable_Value tx_power) {
    RF_EventMask result = 0u;

    /* Cannot TX while jam session is active */
    if (s_jam_session_active) {
        return false;
    }

    /* Reset command status to avoid stale state from previous runs */
    fs_cmd->status = 0x0000;
    tx_cmd->status = 0x0000;

    /* Reuse persistent TX session if same PHY, otherwise open new one */
    if (s_tx_session_handle != NULL && s_tx_session_mode == mode) {
        /* Session already open for this PHY — re-run FS + TX */
        RadioIF_applyRfTxPower(s_tx_session_handle, tx_power);

        /* Always re-run FS before TX */
        result = RF_runCmd(s_tx_session_handle, fs_cmd, RF_PriorityNormal, NULL,
                           RADIO_IF_TX_TERM_EVENTS);
        if ((result & RADIO_IF_TX_SUCCESS_EVENTS) == 0u) {
            return false;
        }

        result = RF_runCmd(s_tx_session_handle, tx_cmd, RF_PriorityNormal, NULL,
                           RADIO_IF_TX_TERM_EVENTS);
        return (result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u;
    }

    /* For 433 MHz: use pristine SysConfig struct directly (no memcpy).
     * For 868+: use copy to avoid loDivider corruption. */
    {
        RF_RadioSetup *actual_setup = setup_cmd;
        if (mode == &Prop0_mode || mode == &Prop0_modeOok) {
            memcpy(&s_prop_setup_copy, setup_cmd, sizeof(s_prop_setup_copy));
            actual_setup = (RF_RadioSetup *)&s_prop_setup_copy;
        }
        /* Prop0_mode433 uses pristine SysConfig struct — no copy needed */
        if (!RadioIF_switchRfMode(mode, actual_setup)) {
            return false;
        }
        s_tx_session_handle = s_rf_handle;
        s_tx_session_mode = mode;
        RadioIF_applyRfTxPower(s_tx_session_handle, tx_power);

        /* FS via RF_postCmd (RF_runCmd(FS) can hang with loDivider=0x0A) */
        fs_cmd->status = 0x0000;
        RF_postCmd(s_tx_session_handle, fs_cmd, RF_PriorityNormal, NULL, 0);
    }

    /* Run TX command */
    result =
        RF_runCmd(s_tx_session_handle, tx_cmd, RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
    s_last_tx_status = tx_cmd->status;
    s_last_tx_event = (uint32_t)result;
    s_last_tx_result = ((result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u) ? 1u : 0u;
    return s_last_tx_result;
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
static void RadioIF_closeTxSession(void);

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

/* BLE 2M Extended Advertising: ADV_EXT_IND(1M, ch37) → AUX_ADV_IND(2M, data ch).
 * Follows Sniffle pattern — BLE spec forbids 2M on primary adv channels. */
#define BLE5_EXT_ADV_AUX_CHANNEL 9u /* fixed secondary channel for OTA */
static bool RadioIF_transmitBle2mExtAdv(uint8_t payload_len, RF_TxPowerTable_Value tx_power) {
    /* All structs on stack (Sniffle pattern) — safe for single-shot TX */
    rfc_CMD_BLE5_ADV_EXT_t adv37;
    rfc_ble5AdvExtPar_t ext_par;
    rfc_ble5ExtAdvEntry_t ext_pkt;
    uint8_t ext_hdr[5]; /* ADI(2) + AuxPtr(3) */

    rfc_CMD_BLE5_ADV_AUX_t adv_aux;
    rfc_ble5AdvAuxPar_t aux_par;
    rfc_ble5ExtAdvEntry_t aux_pkt;

    /* --- Primary: ADV_EXT_IND on ch 37, 1M, with AuxPtr to secondary --- */
    memset(&adv37, 0, sizeof(adv37));
    adv37.commandNo = CMD_BLE5_ADV_EXT;
    adv37.startTrigger.triggerType = TRIG_NOW;
    adv37.startTrigger.pastTrig = 1u;
    adv37.condition.rule = COND_ALWAYS; /* always chain to ADV_AUX */
    adv37.channel = 37u;
    adv37.phyMode.mainMode = 0u; /* 1M on primary */
    adv37.phyMode.coding = 0u;
    adv37.pParams = &ext_par;
    adv37.pOutput = &s_ble_adv_output;
    adv37.pNextOp = (rfc_radioOp_t *)&adv_aux;

    memset(&ext_par, 0, sizeof(ext_par));
    ext_par.advConfig.deviceAddrType = 1u; /* random */
    /* TRIG_REL_PREVEND: AuxPtr offset relative to end of ADV_EXT — no stale absolute time */
    ext_par.auxPtrTargetType = TRIG_REL_PREVEND;
    ext_par.auxPtrTargetTime = 4000u; /* 1ms after ADV_EXT ends */
    ext_par.pAdvPkt = (uint8_t *)&ext_pkt;
    ext_par.pDeviceAddress = (uint16_t *)s_ble_adv_tx_device_addr;

    memset(&ext_pkt, 0, sizeof(ext_pkt));
    ext_pkt.extHdrInfo.length = 6u;  /* flags(1) + ADI(2) + AuxPtr(3) */
    ext_pkt.extHdrInfo.advMode = 0u; /* non-connectable, non-scannable */
    ext_pkt.extHdrFlags = 0x18u;     /* bit3=ADI, bit4=AuxPtr */
    ext_pkt.pExtHeader = ext_hdr;

    /* AuxPtr: channel | offset_units | offset | PHY */
    memset(ext_hdr, 0, sizeof(ext_hdr));
    /* ADI: SID=0, DID=0 */
    ext_hdr[0] = 0x00u;
    ext_hdr[1] = 0x00u;
    /* AuxPtr byte 0: channel(6 bits) | CA(1 bit) | offset_units(1 bit) */
    ext_hdr[2] = BLE5_EXT_ADV_AUX_CHANNEL | 0x40u; /* ch9, CA=1, offset_units=0 (30µs) */
    /* AuxPtr byte 1-2: offset(13 bits) | PHY(3 bits) — RF core fills offset automatically */
    ext_hdr[3] = 0x00u;
    ext_hdr[4] = 1u << 5u; /* PHY=1 (2M) in bits [7:5] */

    /* --- Secondary: AUX_ADV_IND on data channel, 2M, with payload --- */
    memset(&adv_aux, 0, sizeof(adv_aux));
    adv_aux.commandNo = CMD_BLE5_ADV_AUX;
    adv_aux.startTime = 0u;
    adv_aux.startTrigger.triggerType = TRIG_NOW; /* fire immediately after ADV_EXT */
    adv_aux.startTrigger.pastTrig = 1u;
    adv_aux.condition.rule = COND_NEVER;
    adv_aux.channel = BLE5_EXT_ADV_AUX_CHANNEL;
    adv_aux.phyMode.mainMode = 1u; /* 2M */
    adv_aux.phyMode.coding = 0u;
    adv_aux.pParams = &aux_par;
    adv_aux.pOutput = &s_ble_adv_output;

    memset(&aux_par, 0, sizeof(aux_par));
    aux_par.advConfig.deviceAddrType = 1u;
    aux_par.pAdvPkt = (uint8_t *)&aux_pkt;
    aux_par.pRspPkt = NULL;
    aux_par.pDeviceAddress = (uint16_t *)s_ble_adv_tx_device_addr;

    memset(&aux_pkt, 0, sizeof(aux_pkt));
    aux_pkt.extHdrInfo.length = 9u; /* flags(1) + AdvA(6) + ADI(2) */
    aux_pkt.extHdrInfo.advMode = 0u;
    aux_pkt.extHdrFlags = 0x09u;         /* bit0=AdvA, bit3=ADI */
    aux_pkt.extHdrConfig.bSkipAdvA = 1u; /* RF core inserts AdvA from pDeviceAddress */
    aux_pkt.pExtHeader = ext_hdr;        /* reuse ADI bytes */
    aux_pkt.advDataLen = payload_len;
    aux_pkt.pAdvData = s_ble_adv_tx_payload;

    /* bt5 patch required: multi_protocol does NOT support CMD_BLE5_ADV_AUX (hangs). */
    if (!RadioIF_switchRfMode(&Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup)) {
        return false;
    }
    RadioIF_applyRfTxPower(s_rf_handle, tx_power);

    /* Run ADV_EXT(1M,ch37) → ADV_AUX(2M,data ch) chain (Sniffle pattern, RF_runCmd). */
    {
        RF_EventMask result = RF_runCmd(s_rf_handle, (RF_Op *)&adv37, RF_PriorityNormal, NULL,
                                        RF_EventLastCmdDone | RF_EventCmdAborted |
                                            RF_EventCmdStopped | RF_EventCmdCancelled);
        return (result & RF_EventLastCmdDone) != 0u;
    }
}

static bool RadioIF_transmitBleAdvRaw(const uint8_t *payload, uint8_t payload_len) {
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(s_tx_power_dbm);
    uint8_t ble_channel = RadioIF_convertToBleChannel((uint8_t)s_channel);

    if (payload == NULL || payload_len == 0u || payload_len > BLE_ADV_TX_MAX_PAYLOAD_LEN) {
        return false;
    }

    memcpy(s_ble_adv_tx_payload, payload, payload_len);

    /* Configure BLE FS command for this channel */
    RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    RadioIF_applyBlePhyMode(s_selected_phy);

    memset(&s_ble_adv_output, 0, sizeof(s_ble_adv_output));

    /* BLE 2M: extended advertising — ADV_EXT(1M, ch37) → ADV_AUX(2M, data ch).
     * BLE spec forbids 2M on primary channels. Sniffle pattern.
     * Force RadioSetup defaultPhy to 1M — 2M goes in the ADV command phyMode only.
     * Having defaultPhy=2M causes RF core hang on second TX. */
    if (s_selected_phy == PHY_MANAGER_PHY_BLE_2M) {
        Ble5_0_cmdBle5RadioSetup.defaultPhy.mainMode = 0u; /* 1M setup */
        Ble5_0_cmdBle5RadioSetup.defaultPhy.coding = 0u;
        return RadioIF_transmitBle2mExtAdv(payload_len, tx_power);
    }

    if (!RadioIF_isBleAdvChannel(ble_channel)) {
        return false;
    }

    /* Shared ADV params for both BLE4 and BLE5 commands */
    if (Ble5_0_cmdBleAdvNc.pParams == NULL) {
        return false;
    }

    Ble5_0_cmdBleAdvNc.pParams->advLen = payload_len;
    Ble5_0_cmdBleAdvNc.pParams->scanRspLen = 0u;
    Ble5_0_cmdBleAdvNc.pParams->pAdvData = s_ble_adv_tx_payload;
    Ble5_0_cmdBleAdvNc.pParams->pScanRspData = NULL;
    Ble5_0_cmdBleAdvNc.pParams->pDeviceAddress = (uint16_t *)s_ble_adv_tx_device_addr;
    Ble5_0_cmdBleAdvNc.pParams->pWhiteList = NULL;

    /* BLE Coded S8/S2: ADV_NC with phyMode (works on adv channels per BLE 5.0 spec) */
    if (s_selected_phy != PHY_MANAGER_PHY_BLE_1M) {
        Ble5_0_cmdBle5AdvNc.channel = ble_channel;
        Ble5_0_cmdBle5AdvNc.whitening.init = 0u;
        Ble5_0_cmdBle5AdvNc.whitening.bOverride = 0u;
        Ble5_0_cmdBle5AdvNc.startTrigger.triggerType = TRIG_NOW;
        Ble5_0_cmdBle5AdvNc.startTrigger.pastTrig = 1u;
        Ble5_0_cmdBle5AdvNc.condition.rule = COND_NEVER;
        Ble5_0_cmdBle5AdvNc.pNextOp = 0;
        Ble5_0_cmdBle5AdvNc.pOutput = &s_ble_adv_output;

        return RadioIF_executeTxCommand(&Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup,
                                        (RF_Op *)&Ble5_0_cmdFs, (RF_Op *)&Ble5_0_cmdBle5AdvNc,
                                        tx_power);
    }

    /* BLE 1M: use BLE4 ADV_NC (simpler, validated) */
    Ble5_0_cmdBleAdvNc.channel = ble_channel;
    Ble5_0_cmdBleAdvNc.whitening.init = 0u;
    Ble5_0_cmdBleAdvNc.whitening.bOverride = 0u;
    Ble5_0_cmdBleAdvNc.startTrigger.triggerType = TRIG_NOW;
    Ble5_0_cmdBleAdvNc.startTrigger.pastTrig = 1u;
    Ble5_0_cmdBleAdvNc.condition.rule = COND_NEVER;
    Ble5_0_cmdBleAdvNc.pNextOp = 0;
    Ble5_0_cmdBleAdvNc.pOutput = &s_ble_adv_output;

    return RadioIF_executeTxCommand(&Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup,
                                    (RF_Op *)&Ble5_0_cmdFs, (RF_Op *)&Ble5_0_cmdBleAdvNc, tx_power);
}

static bool RadioIF_isBleAdvChannel(uint16_t channel) {
    return (channel >= 37u) && (channel <= 39u);
}

static bool RadioIF_isIeee154PhySelected(void) {
    return s_selected_phy == PHY_MANAGER_PHY_IEEE_802_15_4;
}

static void RadioIF_updateBleHopMode(void) {
    if (s_ble_adv_hop_allowed && PhyManager_isBlePhy(s_selected_phy) &&
        RadioIF_isBleAdvChannel(s_channel)) {
        s_ble_adv_hop_enabled = true;
        s_ble_adv_hop_index = (uint8_t)(s_channel - 37u);
        return;
    }
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
}

void RadioIF_setAdvHopEnabled(bool enabled) {
    s_ble_adv_hop_allowed = enabled;
    RadioIF_updateBleHopMode();
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

void *RadioIF_getRfHandle(void) {
    return (void *)s_rf_handle;
}

void RadioIF_getRfDebug(RadioIF_RfDebug *out) {
    bool is_433 = (s_current_rf_mode == &Prop0_mode433);
    out->rf_handle_ok = (s_rf_handle != NULL) ? 1u : 0u;
    out->rf_mode = (uint8_t)s_rf_mode;
    out->setup_status = is_433 ? (uint16_t)Prop0_cmdPropRadioDivSetup433.status
                               : (uint16_t)Prop0_cmdPropRadioDivSetup.status;
    out->fs_status = is_433 ? (uint16_t)Prop0_cmdFs433.status : (uint16_t)Prop0_cmdFs.status;
    out->rx_status =
        is_433 ? (uint16_t)Prop0_cmdPropRx433.status : (uint16_t)Prop0_cmdPropRx.status;
    out->tx_status = s_last_tx_status;
    out->fs_freq = is_433 ? Prop0_cmdFs433.frequency : Prop0_cmdFs.frequency;
    out->lo_divider =
        is_433 ? Prop0_cmdPropRadioDivSetup433.loDivider : Prop0_cmdPropRadioDivSetup.loDivider;
    out->freq_hz = s_frequency_hz;
    out->tx_result_ok = s_last_tx_result;
    out->tx_event_mask = s_last_tx_event;
    out->fs_frac = is_433 ? Prop0_cmdFs433.fractFreq : Prop0_cmdFs.fractFreq;
    out->setup_center =
        is_433 ? Prop0_cmdPropRadioDivSetup433.centerFreq : Prop0_cmdPropRadioDivSetup.centerFreq;
    out->tx_session_ok = (s_tx_session_handle != NULL) ? 1u : 0u;
    out->tx_session_match = (s_tx_session_mode == &Prop0_mode433) ? 1u : 0u;
    out->tx_cmd_no = is_433 ? Prop0_cmdPropTx433.commandNo : Prop0_cmdPropTx.commandNo;
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
        if (((s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4) ||
             (s_rf_mode == RADIO_IF_RF_MODE_SUB_1GHZ)) &&
            (s_rf_handle != NULL)) {
            RF_cancelCmd(s_rf_handle, s_rf_rx_cmd, 0);
        }
        s_rf_event_flags |= RADIO_IF_RF_EVENT_RX_BUF_FULL;
    }

    /* Wake up the RF processing task via global semaphore */
    extern Semaphore_Handle g_rf_semaphore;
    if (g_rf_semaphore != NULL) {
        Semaphore_post(g_rf_semaphore);
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
        rx_cmd =
            s_ble_active_scan ? (RF_Op *)&Ble5_0_cmdBle5Scanner : (RF_Op *)&Ble5_0_cmdBle5GenericRx;
        break;
    case RADIO_IF_RF_MODE_IEEE_15_4:
        fs_cmd = (RF_Op *)&Ieee154_0_cmdFs;
        rx_cmd = (RF_Op *)&Ieee154_0_cmdIeeeRx;
        event_mask |= RF_EventLastFGCmdDone;
        break;
    case RADIO_IF_RF_MODE_SUB_1GHZ: {
        /* Use 433 SysConfig structs only when active mode is Prop0_mode433 */
        if (s_current_rf_mode == &Prop0_mode433) {
            fs_cmd = (RF_Op *)&Prop0_cmdFs433;
            rx_cmd = (RF_Op *)&Prop0_cmdPropRx433;
        } else {
            fs_cmd = (RF_Op *)&Prop0_cmdFs;
            rx_cmd = (RF_Op *)&Prop0_cmdPropRx;
        }
        break;
    }
    case RADIO_IF_RF_MODE_NONE:
    default:
        return false;
    }

    /* CMD_FS via RF_postCmd (TI-RTOS: RF_runCmd(FS) can hang with loDivider=0x0A) */
    fs_cmd->status = 0x0000;
    (void)RF_postCmd(s_rf_handle, fs_cmd, RF_PriorityNormal, NULL, 0);

    rx_cmd->status = 0x0000;
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

static void RadioIF_applyBlePhyMode(uint8_t phy) {
    uint8_t mainMode = 0u; /* 1M */
    uint8_t coding = 0u;   /* S8 for Coded */

    switch (phy) {
    case PHY_MANAGER_PHY_BLE_2M:
        mainMode = 1u;
        break;
    case PHY_MANAGER_PHY_BLE_CODED_S8:
        mainMode = 2u;
        coding = 0u;
        break;
    case PHY_MANAGER_PHY_BLE_CODED_S2:
        mainMode = 2u;
        coding = 1u;
        break;
    default: /* BLE_1M */
        mainMode = 0u;
        break;
    }

    /* Radio setup default PHY */
    Ble5_0_cmdBle5RadioSetup.defaultPhy.mainMode = mainMode;
    Ble5_0_cmdBle5RadioSetup.defaultPhy.coding = coding;

    /* RX command PHY mode */
    Ble5_0_cmdBle5GenericRx.phyMode.mainMode = mainMode;
    Ble5_0_cmdBle5GenericRx.phyMode.coding = coding;

    /* BLE5 ADV NC TX command PHY mode */
    Ble5_0_cmdBle5AdvNc.phyMode.mainMode = mainMode;
    Ble5_0_cmdBle5AdvNc.phyMode.coding = coding;
}

/* rfDiagnostics pattern: RF_yield → RF_close → RF_open for PHY switch */

static bool RadioIF_switchRfMode(RF_Mode *mode, RF_RadioSetup *setup) {
    /* === 433 path: persistent boot handle, alias only ===
     * Don't close non-433 handle — keep it open for return trip. Both
     * handles can coexist (driver N_MAX_CLIENTS=2). */
    if (mode == &Prop0_mode433) {
        if (s_433_handle == NULL) {
            return false;
        }
        if (s_non433_handle != NULL) {
            RF_flushCmd(s_non433_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
        }
        s_rf_handle = s_433_handle;
        s_current_rf_mode = &Prop0_mode433;
        return true;
    }

    /* === Non-433 path: hot-switch via RadioSetup command ===
     * F9 fix: NEVER RF_close/RF_open between non-433 modes. Per
     * `ti-rtos-rf-cc1352` skill, RF_close+RF_open leaks SemaphoreP/CPE
     * state across cycles and breaks after ~12 PHY switches. Use
     * RF_MODE_MULTIPLE + multi_protocol patch (already configured) so the
     * same handle accepts BLE/IEEE/Prop RadioSetup commands. */
    if (s_non433_handle == NULL) {
        /* First non-433 mode change after init → open the persistent handle. */
        RF_Params rf_params;
        RF_Params_init(&rf_params);
        s_non433_handle = RF_open(&s_rf_object, mode, setup, &rf_params);
        if (s_non433_handle == NULL) {
            s_rf_handle = NULL;
            s_current_rf_mode = NULL;
            return false;
        }
        s_rf_handle = s_non433_handle;
        s_current_rf_mode = mode;
        s_tx_session_handle = s_rf_handle;
        s_tx_session_mode = NULL;
        return true;
    }

    /* Handle already open — alias and re-run setup. Always re-run setup
     * because callers may mutate the setup struct in place (e.g.,
     * setPropConfig switches preset by editing Prop0_cmdPropRadioDivSetup
     * fields), so pointer-identity isn't sufficient to skip. */
    s_rf_handle = s_non433_handle;
    RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
    ((RF_Op *)setup)->status = 0x0000;
    RF_EventMask result = RF_runCmd(s_rf_handle, (RF_Op *)setup, RF_PriorityNormal, NULL,
                                    RF_EventLastCmdDone | RF_EventCmdAborted | RF_EventCmdStopped |
                                        RF_EventCmdCancelled);
    if ((result & RF_EventLastCmdDone) == 0u) {
        return false;
    }
    s_current_rf_mode = mode;
    /* Setup changed — TX session cache must re-run FS+TX from scratch. */
    s_tx_session_mode = NULL;
    return true;
}

static bool RadioIF_startBleRfBackend(void) {
    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return false;
    }

    s_rf_mode = RADIO_IF_RF_MODE_BLE;
    RadioIF_applyBlePhyMode(s_selected_phy);
    RadioIF_applyBleChannelConfig((uint8_t)s_channel);

    /* Switch to BLE mode */
    if (!RadioIF_switchRfMode(&Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup)) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    if (s_ble_active_scan) {
        /* Active scan: use CMD_BLE5_SCANNER (sends SCAN_REQ, captures SCAN_RSP) */
        Ble5_0_cmdBle5Scanner.pParams->pRxQ = &s_rf_data_queue;
        /* bAutoFlushIgnored=1 prevents data-queue saturation under heavy traffic */
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bAutoFlushIgnored = 1u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bAutoFlushCrcErr = 0u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bAutoFlushEmpty = 0u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bIncludeLenByte = 1u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bIncludeCrc = 1u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bAppendRssi = 1u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bAppendStatus = 1u;
        Ble5_0_cmdBle5Scanner.pParams->rxConfig.bAppendTimestamp = 1u;
        Ble5_0_cmdBle5Scanner.pParams->scanConfig.bActiveScan = 1u;
        Ble5_0_cmdBle5Scanner.pParams->scanConfig.scanFilterPolicy = 0u;
        Ble5_0_cmdBle5Scanner.pParams->scanConfig.bEndOnRpt = 0u;
        memset(&s_ble5_scanner_output, 0, sizeof(s_ble5_scanner_output));
        Ble5_0_cmdBle5Scanner.pOutput = &s_ble5_scanner_output;
        Ble5_0_cmdBle5Scanner.phyMode.mainMode = Ble5_0_cmdBle5GenericRx.phyMode.mainMode;
        Ble5_0_cmdBle5Scanner.phyMode.coding = Ble5_0_cmdBle5GenericRx.phyMode.coding;
    } else {
        /* Passive scan: use CMD_BLE5_GENERIC_RX (receive only) */
        Ble5_0_cmdBle5GenericRx.pParams->pRxQ = &s_rf_data_queue;
        /* bAutoFlushIgnored=1 prevents data-queue saturation under heavy traffic */
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAutoFlushIgnored = 1u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAutoFlushCrcErr = 0u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAutoFlushEmpty = 0u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bIncludeLenByte = 1u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bIncludeCrc = 1u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAppendRssi = 1u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAppendStatus = 1u;
        Ble5_0_cmdBle5GenericRx.pParams->rxConfig.bAppendTimestamp = 1u;
    }

    /* RF handle already open from RadioIF_init */
    if (s_rf_handle == NULL) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    if (!RadioIF_runFsAndPostRx()) {
        /* RF_close removed — TI-RTOS */
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    s_rf_event_flags = 0u;
    return true;
}

void RadioIF_setActiveScan(bool enabled) {
    s_ble_active_scan = enabled;
}

void RadioIF_getScannerStats(uint16_t *tx_req, uint16_t *rx_adv_ok, uint16_t *rx_rsp_ok) {
    if (tx_req != NULL) {
        *tx_req = s_ble5_scanner_output.nTxReq;
    }
    if (rx_adv_ok != NULL) {
        *rx_adv_ok = s_ble5_scanner_output.nRxAdvOk;
    }
    if (rx_rsp_ok != NULL) {
        *rx_rsp_ok = s_ble5_scanner_output.nRxRspOk;
    }
}

static bool RadioIF_startIeee154RfBackend(void) {
    /* s_rf_handle may be NULL — switchRfMode will open */

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

    /* Switch to IEEE mode */
    if (!RadioIF_switchRfMode(&Ieee154_0_mode, (RF_RadioSetup *)&Ieee154_0_cmdRadioSetup)) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    if (!RadioIF_runFsAndPostRx()) {
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    s_rf_event_flags = 0u;
    return true;
}

static void RadioIF_stopRfBackend(void) {
    if (s_rf_handle != NULL) {
        /* BLE GenericRx/Scanner use TRIG_NEVER + bRepeat=1 — they never auto-terminate.
         * RF_cancelCmd is required to stop the running command cleanly before flush.
         * Skill pattern: RF_cancelCmd → RF_flushCmd. */
        if (s_rf_rx_cmd >= 0) {
            RF_cancelCmd(s_rf_handle, s_rf_rx_cmd, 0);
        }
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
        s_rx_running = false;
        if (s_prop_ook_active) {
            RF_yield(s_rf_handle);
            ClockP_usleep(50000);
        }
        /* F9 fix: do NOT RF_close. Both s_433_handle and s_non433_handle
         * stay open for the firmware lifetime. Mode switches are handled
         * in switchRfMode by re-running RadioSetup as a command. */
    }

    s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
    s_rf_event_flags = 0u;
    s_rf_mode = RADIO_IF_RF_MODE_NONE;
    /* Keep s_rf_handle and s_current_rf_mode — switchRfMode uses them
     * to decide hot-switch vs first-open. */
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

        /* Smallest plausible entry: 2-byte LL header + 7 appended (no CRC). */
        const uint16_t appended_no_crc =
            (uint16_t)(BLE_APPENDED_RSSI_LEN + BLE_APPENDED_STATUS_LEN +
                       BLE_APPENDED_TIMESTAMP_LEN);
        if (entry_len < (uint16_t)(BLE_ADV_HEADER_LEN + appended_no_crc)) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        /* Try four layouts:
         *   A_crc:    [PDU][CRC(3)][RSSI][STATUS(2)][TIMESTAMP(4)]
         *   A_nocrc:  [PDU][RSSI][STATUS(2)][TIMESTAMP(4)]
         *   B_crc:    [LEN][PDU][CRC(3)][RSSI][STATUS(2)][TIMESTAMP(4)]
         *   B_nocrc:  [LEN][PDU][RSSI][STATUS(2)][TIMESTAMP(4)]
         *
         * Scanner / GenericRx use bIncludeCrc=1 (A_crc / B_crc with our
         * bIncludeLenByte=1 → B_crc). CMD_BLE5_MASTER uses bIncludeCrc=0
         * (B_nocrc); changing the master flag to 1 regresses the initiator
         * (shared data queue), so the parser flexes instead. */
        const uint16_t appended_with_crc = (uint16_t)(appended_no_crc + BLE_APPENDED_CRC_LEN);
        for (uint8_t trial = 0; trial < 4u && !layout_ok; trial++) {
            uint8_t off = (uint8_t)((trial & 1u) ? 1u : 0u);
            uint16_t app = (trial & 2u) ? appended_with_crc : appended_no_crc;
            uint16_t hdr_len_idx = (uint16_t)(off + 1u);
            if (hdr_len_idx >= entry_len) {
                continue;
            }
            uint16_t cand =
                (uint16_t)BLE_ADV_HEADER_LEN + (uint16_t)(entry_data[hdr_len_idx] & 0x3Fu);
            if ((uint16_t)(off + cand + app) == entry_len) {
                pdu_offset = off;
                pdu_len = cand;
                layout_ok = true;
            }
        }

        if (!layout_ok) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        /* RSSI/STATUS/TIMESTAMP always sit at the END of the entry — their
         * indices are independent of whether CRC was appended between them
         * and the PDU. */
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

/* ─── Sub-1GHz Proprietary Backend ─── */

static bool RadioIF_isSub1ghzPhySelected(void) {
    return (s_selected_phy == PHY_MANAGER_PHY_SUB_1GHZ_868) ||
           (s_selected_phy == PHY_MANAGER_PHY_SUB_1GHZ_915) ||
           (s_selected_phy == PHY_MANAGER_PHY_PROPRIETARY_GFSK);
}

static bool RadioIF_isProp24ghzPhySelected(void) {
    return PhyManager_isProp24ghzPhy(s_selected_phy);
}

static void RadioIF_applyProp24ghzChannelConfig(uint32_t frequency_hz) {
    /* CC1352 RF Core CMD_FS frequency field is MHz (uint16_t).
     * Default to 2440 MHz if zero. */
    uint32_t fmhz = frequency_hz / 1000000u;
    if (fmhz < 2400u || fmhz > 2484u) {
        fmhz = 2440u;
    }
    Prop24g_cmdFs.frequency = (uint16_t)fmhz;
    Prop24g_cmdFs.fractFreq = 0u;
    /* fractional part of frequency for sub-MHz tuning could be added here */
}

static void RadioIF_applySub1ghzChannelConfig(uint16_t channel, uint32_t frequency_hz) {
    uint16_t freq_mhz;
    uint16_t frac = 0u;

    /* Only use frequency_hz if it's in the Sub-1GHz range */
    if (frequency_hz != 0u && (frequency_hz / 1000000u) < 1000u) {
        freq_mhz = (uint16_t)(frequency_hz / 1000000u);
        frac = (uint16_t)(((frequency_hz % 1000000u) * 65536u) / 1000000u);
    } else if (s_selected_phy == PHY_MANAGER_PHY_SUB_1GHZ_915) {
        freq_mhz = (uint16_t)(902u + channel);
        frac = 0u;
    } else if (s_selected_phy == PHY_MANAGER_PHY_PROPRIETARY_GFSK) {
        /* GFSK: default 868 MHz, user should set frequency_hz for other bands */
        freq_mhz = 868u;
        frac = 0u;
    } else {
        /* 868 MHz fallback */
        freq_mhz = 868u;
        frac = 0u;
    }

    Prop0_cmdFs.frequency = freq_mhz;
    Prop0_cmdFs.fractFreq = frac;
    Prop0_cmdPropRadioDivSetup.centerFreq = freq_mhz;

    /* loDivider: 0x05 for 779-930 MHz, 0x0A for 431-527 MHz */
    uint8_t lo_div = 0x05;
    if (freq_mhz >= 2400u) {
        lo_div = 0x00;
    } else if (freq_mhz >= 779u && freq_mhz <= 930u) {
        lo_div = 0x05;
    } else if (freq_mhz >= 431u && freq_mhz <= 527u) {
        lo_div = 0x0A;
    }
    Prop0_cmdPropRadioDivSetup.loDivider = lo_div;

    /* 433 SysConfig setup struct IS modified by setPropConfig for modulation
     * changes (FSK/MSK/GFSK). RF_yield forces re-setup on next command.
     * TX/RX paths update Prop0_cmdFs433 directly before each operation. */
}

static bool RadioIF_startSub1ghzRfBackend(void) {
    /* s_rf_handle may be NULL if no RF_open at init — switchRfMode will open */

    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return false;
    }

    s_rf_mode = RADIO_IF_RF_MODE_SUB_1GHZ;
    RadioIF_applySub1ghzChannelConfig(s_channel, s_frequency_hz);

    Prop0_cmdPropRx.pQueue = &s_rf_data_queue;
    Prop0_cmdPropRx.pOutput = NULL;
    Prop0_cmdPropRx.pktConf.bRepeatOk = 1u;
    Prop0_cmdPropRx.pktConf.bRepeatNok = 1u;
    Prop0_cmdPropRx.rxConf.bAutoFlushIgnored = 0u;
    Prop0_cmdPropRx.rxConf.bAutoFlushCrcErr = 0u;
    Prop0_cmdPropRx.rxConf.bIncludeHdr = 1u;
    Prop0_cmdPropRx.rxConf.bIncludeCrc = 1u;
    Prop0_cmdPropRx.rxConf.bAppendRssi = 1u;
    Prop0_cmdPropRx.rxConf.bAppendTimestamp = 1u;
    Prop0_cmdPropRx.rxConf.bAppendStatus = 1u;
    Prop0_cmdPropRx.maxPktLen = 0xFF;

    /* For 433 MHz: use pristine SysConfig-generated structs (commit 2eca110 approach).
     * For 868+: use copy to avoid loDivider corruption. */
    {
        RF_Mode *mode = RadioIF_getPropMode();
        RF_RadioSetup *setup;
        uint32_t freq_mhz = s_frequency_hz / 1000000u;
        /* Use 433 SysConfig structs only when mode is Prop0_mode433 */
        bool use_433 = (mode == &Prop0_mode433);

        if (use_433) {
            /* SysConfig 433 — pristine structs, never modified at runtime */
            setup = (RF_RadioSetup *)&Prop0_cmdPropRadioDivSetup433;

            /* Configure RX on the 433 RX command */
            Prop0_cmdPropRx433.pQueue = &s_rf_data_queue;
            Prop0_cmdPropRx433.pOutput = NULL;
            Prop0_cmdPropRx433.pktConf.bRepeatOk = 1u;
            Prop0_cmdPropRx433.pktConf.bRepeatNok = 1u;
            Prop0_cmdPropRx433.rxConf.bAutoFlushIgnored = 0u;
            Prop0_cmdPropRx433.rxConf.bAutoFlushCrcErr = 0u;
            Prop0_cmdPropRx433.rxConf.bIncludeHdr = 1u;
            Prop0_cmdPropRx433.rxConf.bIncludeCrc = 1u;
            Prop0_cmdPropRx433.rxConf.bAppendRssi = 1u;
            Prop0_cmdPropRx433.rxConf.bAppendTimestamp = 1u;
            Prop0_cmdPropRx433.rxConf.bAppendStatus = 1u;
            Prop0_cmdPropRx433.maxPktLen = 0xFF;
            Prop0_cmdPropRx433.endTrigger.triggerType = TRIG_NEVER;
            Prop0_cmdPropRx433.endTime = 0x00000000;

            /* Set FS frequency for 433 RX (LNA mode) */
            uint16_t frac = (uint16_t)(((s_frequency_hz % 1000000u) * 65536u) / 1000000u);
            Prop0_cmdFs433.frequency = (uint16_t)freq_mhz;
            Prop0_cmdFs433.fractFreq = frac;
            Prop0_cmdFs433.synthConf.bTxMode = 0u; /* LNA mode for RX */
        } else {
            /* 868/915/2.4GHz — use copy of standard struct */
            memcpy(&s_prop_setup_copy, &Prop0_cmdPropRadioDivSetup, sizeof(s_prop_setup_copy));
            setup = (RF_RadioSetup *)&s_prop_setup_copy;
        }

        if (!RadioIF_switchRfMode(mode, setup)) {
            s_rf_mode = RADIO_IF_RF_MODE_NONE;
            return false;
        }
    }

    if (!RadioIF_runFsAndPostRx()) {
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    s_rf_event_flags = 0u;

    /* Pre-set TX session for 433 so executeTxCommand reuses handle */
    if (RadioIF_getPropMode() == &Prop0_mode433) {
        s_tx_session_handle = s_rf_handle;
        s_tx_session_mode = &Prop0_mode433;
    }
    return true;
}

static bool RadioIF_startProp24ghzRfBackend(void) {
    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return false;
    }

    s_rf_mode = RADIO_IF_RF_MODE_PROP_2_4GHZ;
    RadioIF_applyProp24ghzChannelConfig(s_frequency_hz);

    if (!RadioIF_switchRfMode(&Prop24g_mode, (RF_RadioSetup *)&Prop24g_cmdPropRadioSetup)) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    Prop24g_cmdPropRx.pQueue = &s_rf_data_queue;

    if (s_rf_handle == NULL) {
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    if (!RadioIF_runFsAndPostRx()) {
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        s_rf_mode = RADIO_IF_RF_MODE_NONE;
        return false;
    }

    s_rf_event_flags = 0u;
    return true;
}

static void RadioIF_processPropPackets(void) {
    while (RadioIF_rfHasPacket()) {
        RadioIF_RxPacket pkt = {0};
        uint8_t *raw_entry = (uint8_t *)&s_rf_read_entry->data;
        uint16_t entry_len = (uint16_t)raw_entry[0] | ((uint16_t)raw_entry[1] << 8);
        uint8_t *entry_data = raw_entry + RF_QUEUE_ENTRY_LEN_FIELD_SIZE;

        if (entry_len <= PROP_APPENDED_TOTAL_LEN) {
            s_metrics.rx_drop++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        uint16_t pdu_len = (uint16_t)(entry_len - PROP_APPENDED_TOTAL_LEN);
        uint16_t status_idx = pdu_len;
        uint16_t rssi_idx = (uint16_t)(status_idx + PROP_APPENDED_STATUS_LEN);
        uint16_t timestamp_idx = (uint16_t)(rssi_idx + PROP_APPENDED_RSSI_LEN);

        uint8_t status = entry_data[status_idx];
        bool crc_ok = (status & PROP_STATUS_CRC_OK_BIT) != 0u;

        uint32_t rat_timestamp = (uint32_t)entry_data[timestamp_idx] |
                                 ((uint32_t)entry_data[timestamp_idx + 1u] << 8) |
                                 ((uint32_t)entry_data[timestamp_idx + 2u] << 16) |
                                 ((uint32_t)entry_data[timestamp_idx + 3u] << 24);

        pkt.timestamp_us = ((uint64_t)rat_timestamp) / 4u;
        pkt.channel = (uint8_t)s_channel;
        pkt.rssi_dbm = (int8_t)entry_data[rssi_idx];
        pkt.lqi = 0u;
        pkt.crc_ok = crc_ok;

        if (!pkt.crc_ok) {
            s_metrics.rx_crc_err++;
            RadioIF_rfConsumeEntry();
            continue;
        }

        pkt.data_len = (pdu_len > RADIO_IF_MAX_PACKET_DATA) ? (uint8_t)RADIO_IF_MAX_PACKET_DATA
                                                            : (uint8_t)pdu_len;
        memcpy(pkt.data, entry_data, pkt.data_len);

        if (RadioIF_enqueuePacket(&pkt)) {
            s_metrics.rx_ok++;
        } else {
            s_metrics.rx_drop++;
        }
        RadioIF_rfConsumeEntry();
    }
}

static bool RadioIF_transmitPropRaw(const uint8_t *payload, uint8_t payload_len) {
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(s_tx_power_dbm);

    if (payload == NULL || payload_len == 0u) {
        return false;
    }

    memcpy(s_prop_tx_buffer, payload, payload_len);
    RadioIF_applySub1ghzChannelConfig(s_channel, s_frequency_hz);

    RF_Mode *mode = RadioIF_getPropMode();
    if (mode == &Prop0_mode433) {
        /* 433 MHz: handle opened at boot in main_rtos.c, reused forever.
         * NEVER RF_close — TI RF driver bug breaks subsequent RF_open. */
        RF_EventMask result;

        uint32_t freq_mhz = s_frequency_hz / 1000000u;
        uint16_t frac = (uint16_t)(((s_frequency_hz % 1000000u) * 65536u) / 1000000u);

        if (s_433_handle == NULL) {
            return false; /* Handle not opened at boot */
        }

        /* Ensure 433 mode is active — closes any non-433 handle (e.g. BLE/IEEE)
         * that may still be open from a previous PHY. Without this, the stale
         * 2.4 GHz handle blocks the 433 handle on the second round trip. */
        if (!RadioIF_switchRfMode(&Prop0_mode433,
                                  (RF_RadioSetup *)&Prop0_cmdPropRadioDivSetup433)) {
            return false;
        }

        /* Use SAME dedicated 433 structs that the handle was opened with.
         * Driver caches pRadioSetup + first CMD_FS pointers — must match. */
        Prop0_cmdFs433.frequency = (uint16_t)freq_mhz;
        Prop0_cmdFs433.fractFreq = frac;
        Prop0_cmdFs433.synthConf.bTxMode = 1u; /* PA mode */
        Prop0_cmdFs433.status = 0x0000;
        RF_postCmd(s_rf_handle, (RF_Op *)&Prop0_cmdFs433, RF_PriorityNormal, NULL, 0);

        Prop0_cmdPropTx433.pPkt = s_prop_tx_buffer;
        Prop0_cmdPropTx433.pktLen = payload_len;
        Prop0_cmdPropTx433.startTrigger.triggerType = 0x0;
        Prop0_cmdPropTx433.startTrigger.pastTrig = 0x1;
        Prop0_cmdPropTx433.status = 0x0000;

        result = RF_runCmd(s_rf_handle, (RF_Op *)&Prop0_cmdPropTx433, RF_PriorityNormal, NULL, 0);

        s_last_tx_status = Prop0_cmdPropTx433.status;
        s_last_tx_event = (uint32_t)result;
        s_last_tx_result = (Prop0_cmdPropTx433.status == 0x3400) ? 1u : 0u;
        return s_last_tx_result;
    }

    /* 868+ MHz: use standard structs */
    Prop0_cmdPropTx.pPkt = s_prop_tx_buffer;
    Prop0_cmdPropTx.pktLen = payload_len;
    Prop0_cmdPropTx.startTrigger.triggerType = TRIG_NOW;
    Prop0_cmdPropTx.startTrigger.pastTrig = 1u;
    Prop0_cmdPropTx.condition.rule = COND_NEVER;
    Prop0_cmdPropTx.pNextOp = 0;

    return RadioIF_executeTxCommand(mode, (RF_RadioSetup *)&Prop0_cmdPropRadioDivSetup,
                                    (RF_Op *)&Prop0_cmdFs, (RF_Op *)&Prop0_cmdPropTx, tx_power);
}

static bool RadioIF_transmitProp24ghzRaw(const uint8_t *payload, uint8_t payload_len) {
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(s_tx_power_dbm);

    if (payload == NULL || payload_len == 0u || payload_len > PROP_TX_MAX_PAYLOAD_LEN) {
        return false;
    }

    /* Apply frequency in case caller changed s_frequency_hz */
    RadioIF_applyProp24ghzChannelConfig(s_frequency_hz);

    Prop24g_cmdPropTx.pktLen = payload_len;
    Prop24g_cmdPropTx.pPkt = (uint8_t *)payload;
    Prop24g_cmdPropTx.startTrigger.triggerType = TRIG_NOW;
    Prop24g_cmdPropTx.startTrigger.pastTrig = 1u;
    Prop24g_cmdPropTx.condition.rule = COND_NEVER;
    Prop24g_cmdPropTx.pNextOp = 0;

    return RadioIF_executeTxCommand(&Prop24g_mode, (RF_RadioSetup *)&Prop24g_cmdPropRadioSetup,
                                    (RF_Op *)&Prop24g_cmdFs, (RF_Op *)&Prop24g_cmdPropTx, tx_power);
}

static void RadioIF_processRfPackets(void) {
    switch (s_rf_mode) {
    case RADIO_IF_RF_MODE_BLE:
        RadioIF_processBlePackets();
        break;
    case RADIO_IF_RF_MODE_IEEE_15_4:
        RadioIF_processIeee154Packets();
        break;
    case RADIO_IF_RF_MODE_SUB_1GHZ:
        RadioIF_processPropPackets();
        break;
    case RADIO_IF_RF_MODE_NONE:
    default:
        /* No active RF parser. */
        break;
    }
}

void RadioIF_init(void) {
    /* Close ALL RF sessions before resetting state */
    RadioIF_stopJamSession();
    RadioIF_closeTxSession();
    RadioIF_stopRfBackend();

    /* F9: stopRfBackend no longer closes the persistent non-433 handle.
     * If init runs mid-session (CMD_RADIO_INIT after first set_phy), force
     * a real close here so the next set_phy reopens cleanly. Hardware
     * reset / cold boot leave s_non433_handle == NULL so this is a no-op. */
    if (s_non433_handle != NULL) {
        RF_yield(s_non433_handle);
        ClockP_usleep(1000);
        RF_close(s_non433_handle);
        s_non433_handle = NULL;
    }

    s_rx_running = false;
    s_timestamp_us = 0;
    s_selected_phy = 0;
    s_channel = 37;
    s_tx_power_dbm = 0;
    s_frequency_hz = 2402000000u;
    s_prop_ook_active = false;
    s_backend = RADIO_IF_BACKEND_SYNTH;

    /* Restore 868 MHz overrides on the shared prop struct (433 uses its own SysConfig struct) */
    Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverrides;
    Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = Prop0_pOverridesTxStd;
    Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = Prop0_pOverridesTx20;

    RadioIF_resetRxQueue();
    RadioIF_initSyntheticCadence();

    s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
    s_rf_event_flags = 0u;
    s_rf_mode = RADIO_IF_RF_MODE_NONE;
    s_rf_read_entry = NULL;
    RadioIF_resetMetrics();
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
    RadioIF_initBleHopCadence();

    /* Set RF_MODE_MULTIPLE for dual-client operation (rfDualModeRx pattern) */
    Ble5_0_mode.rfMode = RF_MODE_MULTIPLE;
    Prop0_mode.rfMode = RF_MODE_MULTIPLE;
    /* Prop0_mode433 stays RF_MODE_AUTO — matches working hardcoded test */
    Ieee154_0_mode.rfMode = RF_MODE_MULTIPLE;

    /* No RF_open at init — TX path opens from scratch each time */
    s_rf_handle = NULL;
    s_current_rf_mode = NULL;
    s_tx_session_handle = NULL;
    s_tx_session_mode = NULL;
}

static void RadioIF_closeTxSession(void) {
    if (s_tx_session_handle != NULL) {
        RF_flushCmd(s_tx_session_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
        if (s_prop_ook_active) {
            RF_yield(s_tx_session_handle);
            ClockP_usleep(50000); /* 50ms for RF Core full power-down */
        }
        /* RF_close removed — TI-RTOS */
        if (s_prop_ook_active) {
            ClockP_usleep(20000); /* 20ms post-close */
        }
        s_tx_session_handle = NULL;
        s_tx_session_mode = NULL;
    }
}

void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz) {
    /* Close all RF sessions BEFORE resetting OOK flag — RF_yield needs it */
    RadioIF_closeTxSession();
    if (s_rf_handle != NULL) {
        RadioIF_stopRfBackend();
    }
    s_rx_running = false;
    /* Now safe to reset OOK state */
    s_prop_ook_active = false;

    /* Update frequency FIRST so override selection uses new value */
    if (frequency_hz != 0u) {
        s_frequency_hz = frequency_hz;
    }

    /* 433 uses its own SysConfig struct — only set 868 overrides on shared struct */
    {
        uint32_t fmhz = s_frequency_hz / 1000000u;
        if (fmhz > 0u && fmhz < 861u) {
            /* 433 MHz: shared struct overrides don't matter, 433 uses Prop0_cmdPropRadioDivSetup433
             */
            Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverrides433;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = 0;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = 0;
        } else {
            Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverrides;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = Prop0_pOverridesTxStd;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = Prop0_pOverridesTx20;
        }
    }
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

    if (PhyManager_isBlePhy(s_selected_phy)) {
        RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    } else if (RadioIF_isIeee154PhySelected()) {
        RadioIF_applyIeee154ChannelConfig(s_channel);
    } else if (RadioIF_isSub1ghzPhySelected()) {
        RadioIF_applySub1ghzChannelConfig(s_channel, s_frequency_hz);
    }

    RadioIF_updateBleHopMode();
    if (s_ble_adv_hop_enabled) {
        RadioIF_initBleHopCadence();
    }

    if ((s_backend == RADIO_IF_BACKEND_RF) && (s_rf_handle != NULL) &&
        ((PhyManager_isBlePhy(s_selected_phy) && (s_rf_mode == RADIO_IF_RF_MODE_BLE)) ||
         (RadioIF_isIeee154PhySelected() && (s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4)) ||
         (RadioIF_isSub1ghzPhySelected() && (s_rf_mode == RADIO_IF_RF_MODE_SUB_1GHZ)))) {
        (void)RadioIF_restartRfRx();
    }
}

void RadioIF_setChannel(uint8_t channel) {
    RadioIF_closeTxSession();
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
    } else if (RadioIF_isSub1ghzPhySelected()) {
        RadioIF_applySub1ghzChannelConfig(s_channel, s_frequency_hz);
    }

    RadioIF_updateBleHopMode();
    if (s_ble_adv_hop_enabled) {
        RadioIF_initBleHopCadence();
    }

    if ((s_backend == RADIO_IF_BACKEND_RF) && (s_rf_handle != NULL) &&
        ((PhyManager_isBlePhy(s_selected_phy) && (s_rf_mode == RADIO_IF_RF_MODE_BLE)) ||
         (RadioIF_isIeee154PhySelected() && (s_rf_mode == RADIO_IF_RF_MODE_IEEE_15_4)) ||
         (RadioIF_isSub1ghzPhySelected() && (s_rf_mode == RADIO_IF_RF_MODE_SUB_1GHZ)))) {
        (void)RadioIF_restartRfRx();
    }
}

/* Apply formatConf: 0 restores SysConfig defaults (nSwBits=32, bMsbFirst=1) */
static void RadioIF_applyFormatConf(void *dest, uint16_t raw) {
    if (raw != 0u) {
        memcpy(dest, &raw, sizeof(uint16_t));
    } else {
        /* Restore SysConfig defaults: nSwBits=0x20, bMsbFirst=1, rest=0 */
        uint16_t defaults = 0x00A0u; /* bits: nSwBits=0x20(6b), bBitRev=0, bMsb=1, fec=0, wh=0 */
        memcpy(dest, &defaults, sizeof(uint16_t));
    }
}

void RadioIF_setPropConfig(const RadioIF_PropConfig *config) {
    uint32_t freq_mhz;
    uint16_t frac;
    uint8_t lo_div;
    uint32_t rate_word;
    uint8_t pre_scale;

    if (config == NULL) {
        return;
    }

    /* Close RF sessions before reconfiguring.
     * EXCEPTION: if OOK is active, RF_close will hang (TI SDK bug).
     * OOK is locked to its initial frequency — power cycle to change. */
    if (s_prop_ook_active) {
        /* Cannot reconfigure while OOK patches are loaded.
         * Silently update config struct but don't close/reopen RF. */
    } else {
        /* OOK needs stopRfBackend to force RF_close+RF_open with Prop0_modeOok.
         * For <861 MHz: don't stop — 433 handle is persistent, RF_yield forces re-setup.
         * For >=861 MHz: stop to allow mode switch. */
        uint32_t new_freq_mhz = config->frequency_hz / 1000000u;
        if (new_freq_mhz >= 861u || config->mod_type == 2u) {
            RadioIF_closeTxSession();
            if (s_rf_handle != NULL) {
                RadioIF_stopRfBackend();
            }
        }
    }

    /* Frequency */
    freq_mhz = config->frequency_hz / 1000000u;
    frac = (uint16_t)(((config->frequency_hz % 1000000u) * 65536u) / 1000000u);

    /* Always update 868 struct (used for >=861 MHz) */
    Prop0_cmdFs.frequency = (uint16_t)freq_mhz;
    Prop0_cmdFs.fractFreq = frac;
    Prop0_cmdPropRadioDivSetup.centerFreq = (uint16_t)freq_mhz;

    /* Also update 433 SysConfig FS for <861 MHz */
    if (freq_mhz < 861u) {
        Prop0_cmdFs433.frequency = (uint16_t)freq_mhz;
        Prop0_cmdFs433.fractFreq = frac;
        /* Update sync word on 433 commands */
        Prop0_cmdPropTx433.syncWord = config->sync_word;
        Prop0_cmdPropRx433.syncWord = config->sync_word;
    }

    /* loDivider based on frequency */
    if (freq_mhz >= 2360u) {
        lo_div = 0x00u;
    } else if (freq_mhz >= 1076u) {
        lo_div = 0x04u;
    } else if (freq_mhz >= 861u) {
        lo_div = 0x05u;
    } else if (freq_mhz >= 431u) {
        lo_div = 0x0Au;
    } else if (freq_mhz >= 359u) {
        lo_div = 0x0Cu;
    } else if (freq_mhz >= 287u) {
        lo_div = 0x0Fu;
    } else {
        lo_div = 0x1Eu;
    }
    Prop0_cmdPropRadioDivSetup.loDivider = lo_div;

    /* Symbol rate calculation — needed for both 868 and 433 paths */
    pre_scale = 15u;
    rate_word =
        (uint32_t)(((uint64_t)config->symbol_rate * 1048576u * (uint64_t)pre_scale) / 24000000u);
    if (rate_word > 0xFFFFFu) {
        pre_scale = 5u;
        rate_word = (uint32_t)(((uint64_t)config->symbol_rate * 1048576u * (uint64_t)pre_scale) /
                               24000000u);
    }

    /* Apply modulation config to the appropriate setup struct.
     * OOK (mod_type=2) always uses the shared 868 struct + Prop0_modeOok.
     * For >=861 MHz: use shared 868 struct.
     * For <861 MHz non-OOK: update 433 SysConfig struct + yield to force re-setup. */
    if (freq_mhz >= 861u || config->mod_type == 2u) {
        Prop0_cmdPropRadioDivSetup.modulation.modType = config->mod_type;
        Prop0_cmdPropRadioDivSetup.modulation.deviation = config->deviation;
        Prop0_cmdPropRadioDivSetup.symbolRate.preScale = pre_scale;
        Prop0_cmdPropRadioDivSetup.symbolRate.rateWord = rate_word;
        if (config->rx_bw != 0u) {
            Prop0_cmdPropRadioDivSetup.rxBw = config->rx_bw;
        }
        RadioIF_applyFormatConf(&Prop0_cmdPropRadioDivSetup.formatConf, config->format_conf);
    } else if (freq_mhz < 861u) {
        /* Update 433 setup struct so FSK/MSK/GFSK presets all apply */
        Prop0_cmdPropRadioDivSetup433.modulation.modType = config->mod_type;
        Prop0_cmdPropRadioDivSetup433.modulation.deviation = config->deviation;
        Prop0_cmdPropRadioDivSetup433.symbolRate.preScale = pre_scale;
        Prop0_cmdPropRadioDivSetup433.symbolRate.rateWord = rate_word;
        Prop0_cmdPropRadioDivSetup433.loDivider = lo_div;
        Prop0_cmdPropRadioDivSetup433.centerFreq = (uint16_t)freq_mhz;
        if (config->rx_bw != 0u) {
            Prop0_cmdPropRadioDivSetup433.rxBw = config->rx_bw;
        }
        RadioIF_applyFormatConf(&Prop0_cmdPropRadioDivSetup433.formatConf, config->format_conf);
        /* Stop RX before yield — otherwise s_rx_running stays true after radio powers down */
        if (s_rx_running) {
            RadioIF_stopRx();
        }
        /* Yield persistent 433 handle to force RF driver re-setup on next cmd */
        if (s_433_handle != NULL) {
            RF_yield(s_433_handle);
        }
    }

    /* Sync word (applied to both TX and RX commands) */
    Prop0_cmdPropTx.syncWord = config->sync_word;
    Prop0_cmdPropRx.syncWord = config->sync_word;

    /* Select overrides by modulation type and frequency band */
    if (config->mod_type == 2u) {
        /* OOK: use genook patches + band-specific RF tuning */
        if (freq_mhz < 861u) {
            Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverridesOok433;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = Prop0_pOverridesOok433TxStd;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = Prop0_pOverridesOok433Tx20;
        } else {
            Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverridesOok;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = Prop0_pOverridesOokTxStd;
            Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = Prop0_pOverridesOokTx20;
        }
        s_prop_ook_active = true;
    } else if (freq_mhz < 250u) {
        /* 169 MHz band */
        Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverrides169;
        Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = Prop0_pOverrides433TxStd;
        Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = Prop0_pOverrides433Tx20;
        s_prop_ook_active = false;
    } else if (freq_mhz < 861u) {
        /* 287-860 MHz: 433 SysConfig struct handles this — update shared for fallback */
        Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverrides433;
        Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = 0;
        Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = 0;
        s_prop_ook_active = false;
    } else {
        /* 861+ MHz: standard overrides */
        Prop0_cmdPropRadioDivSetup.pRegOverride = Prop0_pOverrides;
        Prop0_cmdPropRadioDivSetup.pRegOverrideTxStd = Prop0_pOverridesTxStd;
        Prop0_cmdPropRadioDivSetup.pRegOverrideTx20 = Prop0_pOverridesTx20;
        s_prop_ook_active = false;
    }

    /* Update frequency_hz state for Sub-1GHz channel config */
    s_frequency_hz = config->frequency_hz;
}

void RadioIF_setBleAdvAddress(const uint8_t *addr) {
    if (addr != NULL) {
        memcpy(s_ble_adv_tx_device_addr, addr, BLE_ADV_TX_DEVICE_ADDR_LEN);
    }
}

void RadioIF_setPower(int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;
}

bool RadioIF_transmitRaw(const uint8_t *data, uint8_t data_len, int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;

    if (s_rx_running) {
        return false;
    }

    /* Each TX path calls executeTxCommand → switchRfMode which opens
     * the RF handle if needed. No s_rf_handle NULL guard here —
     * 433 has its own persistent handle, BLE/IEEE open on demand. */
    if (RadioIF_isSub1ghzPhySelected()) {
        return RadioIF_transmitPropRaw(data, data_len);
    }

    if (PhyManager_isBlePhy(s_selected_phy)) {
        return RadioIF_transmitBleAdvRaw(data, data_len);
    }

    if (RadioIF_isIeee154PhySelected()) {
        return RadioIF_transmitIeee154Raw(data, data_len);
    }

    return false;
}

bool RadioIF_runTxTest(uint8_t mode) {
    if (s_rf_handle == NULL) {
        return false;
    }
    /* Idempotent: if a previous test is running, stop it first. */
    if (s_test_cmd_handle >= 0) {
        RadioIF_stopTxTest();
    }

    memset(&s_cmd_tx_test, 0, sizeof(s_cmd_tx_test));
    s_cmd_tx_test.commandNo = CMD_TX_TEST;
    /* TI CMD_TX_TEST config:
     *   bUseCw    : 1=CW (mode 0), 0=modulated PRBS (mode 1/2)
     *   whitenMode: 2=PRBS-15 (mode 1), 3=PRBS-32 (mode 2)
     * Note: PRBS-9 is BLE DTM via CMD_BLE5_TX_TEST, NOT this command. */
    s_cmd_tx_test.config.bUseCw = (mode == 0u) ? 1u : 0u;
    if (mode == 1u) {
        s_cmd_tx_test.config.whitenMode = 2u; /* PRBS-15 */
    } else if (mode == 2u) {
        s_cmd_tx_test.config.whitenMode = 3u; /* PRBS-32 */
    } else {
        s_cmd_tx_test.config.whitenMode = 0u; /* CW: no whitening */
    }
    s_cmd_tx_test.config.bFsOff = 0u; /* keep FS on */
    s_cmd_tx_test.startTrigger.triggerType = TRIG_NOW;
    s_cmd_tx_test.startTrigger.pastTrig = 1u;
    s_cmd_tx_test.endTrigger.triggerType = TRIG_NEVER; /* runs until cancelled */
    s_cmd_tx_test.endTime = 0u;
    s_cmd_tx_test.condition.rule = COND_NEVER;
    s_cmd_tx_test.status = 0x0000u;

    /* Apply currently-set TX power on the active handle. */
    RadioIF_applyRfTxPower(s_rf_handle, RadioIF_resolveTxPowerValue(s_tx_power_dbm));

    /* Re-tune via FS for current band so the synth is locked at the
     * channel the user picked via set_phy(). */
    if (PhyManager_isBlePhy(s_selected_phy)) {
        Ble5_0_cmdFs.status = 0x0000u;
        RF_postCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdFs, RF_PriorityNormal, NULL, 0);
    } else if (RadioIF_isIeee154PhySelected()) {
        Ieee154_0_cmdFs.status = 0x0000u;
        RF_postCmd(s_rf_handle, (RF_Op *)&Ieee154_0_cmdFs, RF_PriorityNormal, NULL, 0);
    } else if (RadioIF_isSub1ghzPhySelected()) {
        RF_Op *fs = (s_current_rf_mode == &Prop0_mode433) ? (RF_Op *)&Prop0_cmdFs433
                                                          : (RF_Op *)&Prop0_cmdFs;
        fs->status = 0x0000u;
        RF_postCmd(s_rf_handle, fs, RF_PriorityNormal, NULL, 0);
    }

    s_test_cmd_handle =
        RF_postCmd(s_rf_handle, (RF_Op *)&s_cmd_tx_test, RF_PriorityNormal, NULL, 0);
    s_last_tx_status = s_cmd_tx_test.status;
    return s_test_cmd_handle >= 0;
}

void RadioIF_stopTxTest(void) {
    if (s_rf_handle != NULL && s_test_cmd_handle >= 0) {
        RF_cancelCmd(s_rf_handle, s_test_cmd_handle, 0);
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
    }
    s_test_cmd_handle = RF_SCHEDULE_CMD_ERROR;
}

bool RadioIF_startRx(void) {
    RadioIF_closeTxSession();
    s_rx_running = true;
    RadioIF_resetRxQueue();

    if (PhyManager_supportsRfBackendRx(s_selected_phy)) {
        bool rf_started = false;

        if (PhyManager_isBlePhy(s_selected_phy)) {
            rf_started = RadioIF_startBleRfBackend();
        } else if (RadioIF_isIeee154PhySelected()) {
            rf_started = RadioIF_startIeee154RfBackend();
        } else if (RadioIF_isSub1ghzPhySelected()) {
            rf_started = RadioIF_startSub1ghzRfBackend();
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
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const uint8_t s_jam_ieee154_payload[125] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool RadioIF_executeJamTx(RF_Handle rf_handle, uint8_t phy, uint8_t channel,
                                 int8_t power_dbm) {
    RF_TxPowerTable_Value tx_power = RadioIF_resolveTxPowerValue(power_dbm);
    RF_EventMask result;

    if (PhyManager_isBlePhy(phy)) {
        uint8_t ble_channel = RadioIF_convertToBleChannel(channel);
        RadioIF_applyBlePhyMode(phy);
        RadioIF_applyBleChannelConfig(ble_channel);

        Ble5_0_cmdBle5AdvNc.channel = ble_channel;
        Ble5_0_cmdBle5AdvNc.whitening.init = (uint8_t)(0x40u | (ble_channel & 0x3Fu));
        Ble5_0_cmdBle5AdvNc.whitening.bOverride = 1u;
        Ble5_0_cmdBle5AdvNc.startTrigger.triggerType = TRIG_NOW;
        Ble5_0_cmdBle5AdvNc.startTrigger.pastTrig = 1u;
        Ble5_0_cmdBle5AdvNc.condition.rule = COND_NEVER;
        Ble5_0_cmdBle5AdvNc.pNextOp = 0;

        if (tx_power.paType == RF_TxPowerTable_HighPA) {
            Ble5_0_cmdBle5AdvNc.txPower = 0xFFFFu;
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

        result = RF_runCmd(rf_handle, (RF_Op *)&Ble5_0_cmdBle5AdvNc, RF_PriorityNormal, NULL,
                           RADIO_IF_TX_TERM_EVENTS);
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

        result = RF_runCmd(rf_handle, (RF_Op *)&Ieee154_0_cmdIeeeTx, RF_PriorityNormal, NULL,
                           RADIO_IF_TX_TERM_EVENTS);
        return (result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u;
    }

    if (phy == PHY_MANAGER_PHY_SUB_1GHZ_868 || phy == PHY_MANAGER_PHY_SUB_1GHZ_915 ||
        phy == PHY_MANAGER_PHY_PROPRIETARY_GFSK) {
        RadioIF_applySub1ghzChannelConfig((uint16_t)channel, 0u);

        Prop0_cmdPropTx.pPkt = (uint8_t *)s_jam_ieee154_payload; /* reuse 125-byte payload */
        Prop0_cmdPropTx.pktLen = 125u;
        Prop0_cmdPropTx.startTrigger.triggerType = TRIG_NOW;
        Prop0_cmdPropTx.startTrigger.pastTrig = 1u;
        Prop0_cmdPropTx.condition.rule = COND_NEVER;
        Prop0_cmdPropTx.pNextOp = 0;

        result = RF_runCmd(rf_handle, (RF_Op *)&Prop0_cmdPropTx, RF_PriorityNormal, NULL,
                           RADIO_IF_TX_TERM_EVENTS);
        return (result & RADIO_IF_TX_SUCCESS_EVENTS) != 0u;
    }

    return false;
}

int RadioIF_bleInitiate(void) {
    /* Use startBleRfBackend to get into a known-good BLE state,
     * then cancel the GenericRX and post the Initiator. This works
     * around potential issues with RF_open → Initiator directly. */

    /* If not already in BLE RX mode, start it to initialize RF core */
    if (s_rf_mode != RADIO_IF_RF_MODE_BLE || s_rf_handle == NULL) {
        if (!RadioIF_startBleRfBackend()) {
            return -3;
        }
    }

    /* Cancel any active RX command (GenericRX/Scanner) */
    if (s_rf_rx_cmd >= 0) {
        RF_cancelCmd(s_rf_handle, s_rf_rx_cmd, 0);
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
    }
    s_rx_running = false;

    /* Re-create data queue for the initiator */
    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return -3;
    }

    /* Point initiator RX queue to our data queue */
    Ble5_0_cmdBle5Initiator.pParams->pRxQ = &s_rf_data_queue;

    /* Set connectTime just before RF_runCmd — avoids stale RAT timestamps
     * if mode-switch or RX-stop took several ms above. RAT clock is 4 MHz.
     * endTime / endTrigger are owned by ble_conn.c (Sniffle parity:
     * TRIG_NEVER, endTime=0). */
    {
        uint32_t now = RF_getCurrentTime();
        Ble5_0_cmdBle5Initiator.pParams->connectTime = now + 4000u;
    }

    /* Reset command status */
    Ble5_0_cmdBle5Initiator.status = 0;

    /* Run CMD_BLE5_INITIATOR — blocks until CONNECT_IND sent, timeout, or cancel.
     * NO CMD_FS needed for BLE (channel field handles frequency). */
    RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5Initiator, RF_PriorityNormal,
              &RadioIF_rfCallback, RF_EventRxEntryDone);

    /* Map status to result code (Sniffle RadioWrapper.c:719-736) */
    switch (Ble5_0_cmdBle5Initiator.status) {
    case BLE_DONE_CONNECT:
        if (Ble5_0_cmdBle5Initiator.pParams->rxListenTime != 0) {
            return 2; /* AUX connect */
        }
        return 1; /* CSA#2 legacy connect */

    case BLE_DONE_CONNECT_CHSEL0:
        return 0; /* CSA#1 connect */

    case BLE_DONE_RXTIMEOUT:
    case BLE_DONE_ENDED:
    case BLE_DONE_STOPPED:
        return -1; /* timeout / cancelled */

    case BLE_DONE_NOSYNC:
        return -2; /* no sync */

    default:
        return -3; /* RF error */
    }
}

int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit, dataQueue_t *pTxQueue,
                       uint32_t startTime, uint32_t endTime, uint32_t *pNumSent,
                       RadioIF_BleCentralStats *pStats) {
    rfc_bleMasterSlaveOutput_t output = {0};

    if (s_rf_handle == NULL || chan >= 37) {
        return -2;
    }

    Ble5_0_cmdBle5Master.channel = chan;
    Ble5_0_cmdBle5Master.whitening.init = 0x40 + chan;
    Ble5_0_cmdBle5Master.whitening.bOverride = 1;
    Ble5_0_cmdBle5Master.phyMode.mainMode = 0; /* 1M */
    Ble5_0_cmdBle5Master.phyMode.coding = 0;
    Ble5_0_cmdBle5Master.pOutput = &output;

    Ble5_0_cmdBle5Master.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5Master.pParams->pTxQ = pTxQueue;
    Ble5_0_cmdBle5Master.pParams->accessAddress = accessAddr;
    Ble5_0_cmdBle5Master.pParams->crcInit0 = crcInit & 0xFF;
    Ble5_0_cmdBle5Master.pParams->crcInit1 = (crcInit >> 8) & 0xFF;
    Ble5_0_cmdBle5Master.pParams->crcInit2 = (crcInit >> 16) & 0xFF;
    Ble5_0_cmdBle5Master.pParams->maxRxPktLen = 0xFF;

    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushIgnored = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushCrcErr = 1;
    /* Master mode: empties already surface via pOutput.pktStatus.bLastEmpty.
     * Flushing them keeps the 3-entry RX queue from saturating before slave
     * sends a non-empty ATT response. */
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushEmpty = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bIncludeLenByte = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bIncludeCrc = 0;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendRssi = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendStatus = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendTimestamp = 1;

    if (startTime == 0) {
        Ble5_0_cmdBle5Master.startTrigger.triggerType = TRIG_NOW;
    } else {
        Ble5_0_cmdBle5Master.startTrigger.triggerType = TRIG_ABSTIME;
        Ble5_0_cmdBle5Master.startTrigger.pastTrig = 1;
        Ble5_0_cmdBle5Master.startTime = startTime;
    }

    Ble5_0_cmdBle5Master.pParams->endTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Master.pParams->endTime = endTime;

    Ble5_0_cmdBle5Master.status = 0;

    RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5Master, RF_PriorityNormal, &RadioIF_rfCallback,
              RF_EventRxEntryDone);

    *pNumSent = output.nTxEntryDone;
    if (pStats != NULL) {
        pStats->nTx = output.nTx;
        pStats->nRxOk = output.nRxOk;
        pStats->nRxNok = output.nRxNok;
        pStats->nRxIgnored = output.nRxIgnored;
        /* Pack the 7 bitfield bits into one byte explicitly — C doesn't
         * pin bitfield layout, and we want a wire-stable byte the Python
         * parser can interpret unambiguously. Order matches
         * DebugTimingEntry property bitmasks in _responses.py. */
        pStats->pktStatus = (uint8_t)((output.pktStatus.bTimeStampValid ? 0x01u : 0u) |
                                      (output.pktStatus.bLastCrcErr ? 0x02u : 0u) |
                                      (output.pktStatus.bLastIgnored ? 0x04u : 0u) |
                                      (output.pktStatus.bLastEmpty ? 0x08u : 0u) |
                                      (output.pktStatus.bLastCtrl ? 0x10u : 0u) |
                                      (output.pktStatus.bLastMd ? 0x20u : 0u) |
                                      (output.pktStatus.bLastAck ? 0x40u : 0u));
    }

    /* Return raw status for debugging. Caller checks for success codes. */
    return (int)Ble5_0_cmdBle5Master.status;
}

void RadioIF_bleResetSeqStat(void) {
    Ble5_0_cmdBle5Master.pParams->seqStat.lastRxSn = 1;
    Ble5_0_cmdBle5Master.pParams->seqStat.lastTxSn = 1;
    Ble5_0_cmdBle5Master.pParams->seqStat.nextTxSn = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bFirstPkt = 1;
    Ble5_0_cmdBle5Master.pParams->seqStat.bAutoEmpty = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bLlCtrlTx = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bLlCtrlAckRx = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bLlCtrlAckPending = 0;
}

void RadioIF_bleResetRxQueue(void) {
    RadioIF_resetRfDataQueue();
}

void RadioIF_bleDrainRxQueue(void) {
    /* Process any pending RX entries from CMD_BLE5_MASTER.
     * Called from BleConnMgr_poll() — s_rx_running is false during
     * central mode, so RadioIF_poll() skips processing. */
    RadioIF_processBlePackets();
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

    RadioIF_closeTxSession();

    /* Determine mode based on PHY */
    if (PhyManager_isBlePhy(phy)) {
        RadioIF_applyBlePhyMode(phy);
        mode = &Ble5_0_mode;
        setup_cmd = (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup;
    } else if (phy == PHY_MANAGER_PHY_IEEE_802_15_4) {
        mode = &Ieee154_0_mode;
        setup_cmd = (RF_RadioSetup *)&Ieee154_0_cmdRadioSetup;
    } else if (phy == PHY_MANAGER_PHY_SUB_1GHZ_868 || phy == PHY_MANAGER_PHY_SUB_1GHZ_915 ||
               phy == PHY_MANAGER_PHY_PROPRIETARY_GFSK) {
        RadioIF_applySub1ghzChannelConfig((uint16_t)channel, 0u);
        mode = RadioIF_getPropMode();
        setup_cmd = (RF_RadioSetup *)&Prop0_cmdPropRadioDivSetup;
    } else {
        return false;
    }

    /* RF_open for jam session */
    RF_Params_init(&rf_params);
    s_jam_rf_handle = RF_open(&s_jam_rf_object, mode, setup_cmd, &rf_params);
    if (s_jam_rf_handle == NULL) {
        return false;
    }

    /* Run CMD_FS to tune synthesizer before first TX */
    {
        RF_Op *fs_cmd;
        if (PhyManager_isBlePhy(phy)) {
            fs_cmd = (RF_Op *)&Ble5_0_cmdFs;
            RadioIF_applyBleChannelConfig(channel);
        } else if (phy == PHY_MANAGER_PHY_SUB_1GHZ_868 || phy == PHY_MANAGER_PHY_SUB_1GHZ_915 ||
                   phy == PHY_MANAGER_PHY_PROPRIETARY_GFSK) {
            fs_cmd = (RF_Op *)&Prop0_cmdFs;
            RadioIF_applySub1ghzChannelConfig((uint16_t)channel, 0u);
        } else {
            fs_cmd = (RF_Op *)&Ieee154_0_cmdFs;
            RadioIF_applyIeee154ChannelConfig((uint16_t)channel);
        }
        fs_cmd->status = 0x0000;

        RF_EventMask fs_result =
            RF_runCmd(s_jam_rf_handle, fs_cmd, RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
        if ((fs_result & RADIO_IF_TX_SUCCESS_EVENTS) == 0u) {
            /* RF_close removed — TI-RTOS */
            return false;
        }
    }

    /* Execute first TX to start the jam session */
    tx_ok = RadioIF_executeJamTx(s_jam_rf_handle, phy, channel, power_dbm);
    if (!tx_ok) {
        /* RF_close removed — TI-RTOS */
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
        /* RF_close removed — TI-RTOS */
    }

    s_jam_session_active = false;
    s_jam_phy = 0u;
    s_jam_channel = 0u;
    s_jam_power_dbm = 0;
}

bool RadioIF_isJamSessionActive(void) {
    return s_jam_session_active;
}
