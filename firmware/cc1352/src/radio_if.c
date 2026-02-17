/*
 * FeralRF CC1352 - Radio Interface (hybrid backend)
 *
 * Primary backend: RF Core BLE5 RX (based on sniffer_fw_cc1252P_7).
 * Fallback backend: synthetic stream (keeps host pipeline alive if RF init fails).
 */

#include "radio_if.h"

#include "config.h"
#include "phy_manager.h"
#include "smartrf_ble5_0.h"

#include <stddef.h>
#include <string.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
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

typedef enum {
    RADIO_IF_BACKEND_SYNTH = 0,
    RADIO_IF_BACKEND_RF = 1,
} RadioIF_Backend;

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

/* RF backend state */
static RF_Object s_rf_object;
static RF_Handle s_rf_handle = NULL;
static RF_CmdHandle s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;

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

#if defined(__GNUC__)
static uint8_t s_rf_rx_data_buffer[RF_QUEUE_DATA_BUFFER_SIZE] __attribute__((aligned(4)));
#else
static uint8_t s_rf_rx_data_buffer[RF_QUEUE_DATA_BUFFER_SIZE];
#endif

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

static void RadioIF_applyBleChannelConfig(uint8_t channel) {
    uint8_t ble_channel = RadioIF_convertToBleChannel(channel);

    Ble5_0_cmdBle5GenericRx.channel = ble_channel;
    Ble5_0_cmdBle5GenericRx.whitening.init = (uint8_t)(0x40u | (ble_channel & 0x3Fu));
    Ble5_0_cmdBle5GenericRx.whitening.bOverride = 1u;

    Ble5_0_cmdFs.frequency = RadioIF_bleChannelToFrequency(ble_channel);
    Ble5_0_cmdFs.fractFreq = 0u;
}

static bool RadioIF_isBleAdvChannel(uint16_t channel) {
    return (channel >= 37u) && (channel <= 39u);
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
        RadioIF_RxPacket pkt;

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
        s_rf_event_flags |= RADIO_IF_RF_EVENT_RX_BUF_FULL;
    }
}

static bool RadioIF_runFsAndPostRx(void) {
    if (s_rf_handle == NULL) {
        return false;
    }

    (void)RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdFs, RF_PriorityNormal, NULL, 0);
    s_rf_rx_cmd = RF_postCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5GenericRx, RF_PriorityNormal,
                             &RadioIF_rfCallback, RF_EventRxEntryDone | RF_EventRxBufFull);
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

static bool RadioIF_advanceBleHopChannel(void) {
    if (!s_ble_adv_hop_enabled) {
        return false;
    }

    s_ble_adv_hop_index = (uint8_t)((s_ble_adv_hop_index + 1u) % 3u);
    s_channel = s_ble_adv_hop_channels[s_ble_adv_hop_index];
    RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    return RadioIF_restartRfRx();
}

static bool RadioIF_startRfBackend(void) {
    RF_Params rf_params;

    if (!RadioIF_createRfDataQueue(&s_rf_data_queue, s_rf_rx_data_buffer,
                                   (uint16_t)sizeof(s_rf_rx_data_buffer), RF_QUEUE_NUM_DATA_ENTRIES,
                                   RF_QUEUE_ENTRY_PAYLOAD_LEN)) {
        return false;
    }

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
        return false;
    }

    if (!RadioIF_runFsAndPostRx()) {
        RF_close(s_rf_handle);
        s_rf_handle = NULL;
        s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
        return false;
    }

    s_rf_event_flags = 0u;
    return true;
}

static void RadioIF_stopRfBackend(void) {
    if (s_rf_handle != NULL) {
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
        RF_close(s_rf_handle);
        s_rf_handle = NULL;
    }

    s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
    s_rf_event_flags = 0u;
    RadioIF_resetRfDataQueue();
}

static void RadioIF_processRfPackets(void) {
    while (RadioIF_rfHasPacket()) {
        RadioIF_RxPacket pkt;
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
    s_rf_read_entry = NULL;
    RadioIF_resetMetrics();
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
    RadioIF_initBleHopCadence();
}

void RadioIF_setPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz) {
    s_selected_phy = phy;

    if (channel != 0u) {
        s_channel =
            PhyManager_isBlePhy(phy) ? RadioIF_convertToBleChannel((uint8_t)channel) : channel;
    }

    if (frequency_hz != 0u) {
        s_frequency_hz = frequency_hz;
    }

    if (PhyManager_isBlePhy(s_selected_phy)) {
        RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    }

    RadioIF_updateBleHopMode();
    if (s_ble_adv_hop_enabled) {
        RadioIF_initBleHopCadence();
    }

    if ((s_backend == RADIO_IF_BACKEND_RF) && (s_rf_handle != NULL) &&
        PhyManager_isBlePhy(s_selected_phy)) {
        (void)RadioIF_restartRfRx();
    }
}

void RadioIF_setChannel(uint8_t channel) {
    s_channel =
        PhyManager_isBlePhy(s_selected_phy) ? RadioIF_convertToBleChannel(channel) : channel;

    if (PhyManager_isBlePhy(s_selected_phy)) {
        RadioIF_applyBleChannelConfig((uint8_t)s_channel);
    }

    RadioIF_updateBleHopMode();
    if (s_ble_adv_hop_enabled) {
        RadioIF_initBleHopCadence();
    }

    if ((s_backend == RADIO_IF_BACKEND_RF) && (s_rf_handle != NULL) &&
        PhyManager_isBlePhy(s_selected_phy)) {
        (void)RadioIF_restartRfRx();
    }
}

void RadioIF_setPower(int8_t power_dbm) {
    s_tx_power_dbm = power_dbm;
}

bool RadioIF_startRx(void) {
    s_rx_running = true;
    RadioIF_resetRxQueue();

    /* Real RF backend currently supports BLE 1M only; others use synthetic fallback. */
    if (PhyManager_supportsRfBackendRx(s_selected_phy) && RadioIF_startRfBackend()) {
        s_backend = RADIO_IF_BACKEND_RF;
        RadioIF_updateBleHopMode();
        if (s_ble_adv_hop_enabled) {
            RadioIF_initBleHopCadence();
        }
        return true;
    }

    /* Fallback: synthetic stream keeps command path operational. */
    s_backend = RADIO_IF_BACKEND_SYNTH;
    RadioIF_initSyntheticCadence();
    return true;
}

void RadioIF_stopRx(void) {
    s_rx_running = false;
    RadioIF_resetRxQueue();

    if (s_backend == RADIO_IF_BACKEND_RF) {
        RadioIF_stopRfBackend();
    }

    s_backend = RADIO_IF_BACKEND_SYNTH;
    s_ble_adv_hop_enabled = false;
    s_ble_adv_hop_index = 0u;
    RadioIF_initBleHopCadence();
    RadioIF_initSyntheticCadence();
}

bool RadioIF_isRxRunning(void) {
    return s_rx_running;
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
            (void)RadioIF_restartRfRx();
        }

        if ((events & RADIO_IF_RF_EVENT_RX_ENTRY_DONE) != 0u) {
            RadioIF_processRfPackets();
        }

        /* Also drain if callback raced and queue already has entries. */
        if (RadioIF_rfHasPacket()) {
            RadioIF_processRfPackets();
        }

        if (RadioIF_shouldHopBleChannel()) {
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
