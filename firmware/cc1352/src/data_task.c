/*
 * FeralRF CC1352 - Data Task (polling variant)
 *
 * Consumes control events and drains RX packets produced by radio_if.
 */

#include "data_task.h"

#include <stdint.h>

#include "control_task.h"
#include "host_if_task.h"
#include "ll_manager.h"
#include "output_if.h"
#include "protocol.h"
#include "radio_if.h"
#include "task_event.h"

static bool s_rx_active = false;
static uint8_t s_rx_rsp_seq = 0;

#define RSP_RX_PACKET 0x90u
#define RSP_ERROR 0x81u
#define ERR_RF_INIT_FAILED 0x06u
#define RX_PACKET_BASE_META_LEN 13u
#define RX_PACKET_LL_META_LEN 3u
#define RX_PACKET_MAX_EMIT_DATA_LEN \
    (PROTOCOL_MAX_PAYLOAD - RX_PACKET_BASE_META_LEN - RX_PACKET_LL_META_LEN)

static void DataTask_emitRxPacket(const RadioIF_RxPacket *pkt) {
    uint8_t payload[RX_PACKET_BASE_META_LEN + RADIO_IF_MAX_PACKET_DATA + RX_PACKET_LL_META_LEN];
    uint8_t emit_data_len = pkt->data_len;
    uint16_t payload_len = 0u;

    if (emit_data_len > RX_PACKET_MAX_EMIT_DATA_LEN) {
        emit_data_len = (uint8_t)RX_PACKET_MAX_EMIT_DATA_LEN;
    }

    payload_len = RX_PACKET_BASE_META_LEN + emit_data_len + RX_PACKET_LL_META_LEN;

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
    payload[12] = emit_data_len;
    for (uint16_t i = 0; i < emit_data_len; i++) {
        payload[13u + i] = pkt->data[i];
    }
    payload[13u + emit_data_len] = pkt->ll_pdu_kind;
    payload[13u + emit_data_len + 1u] = pkt->ll_pdu_type;
    payload[13u + emit_data_len + 2u] = pkt->ll_pdu_flags;

    OutputIF_sendResponse(RSP_RX_PACKET, s_rx_rsp_seq++, payload, payload_len);
}

void DataTask_init(void) {
    s_rx_active = false;
    s_rx_rsp_seq = 0;
    /* RadioIF_init only touches shared 868 structs, not dedicated 433 SysConfig structs.
     * 433 handle was opened at boot with its own structs — safe to init. */
    RadioIF_init();
}

void DataTask_poll(void) {
    /* Process deferred commands from UART task (runs in RF task context) */
    HostIFTask_processPendingCommand();

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_RX_STOP)) {
        s_rx_active = false;
        RadioIF_stopRx();
        TaskEvent_clear(TASK_EVENT_DATA_RX_ACTIVE);
        TaskEvent_clear(TASK_EVENT_CONTROL_RX_STOP);
    }

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_TX_RAW)) {
        ControlTask_processTxRaw();
        TaskEvent_clear(TASK_EVENT_CONTROL_TX_RAW);
    }

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_TX_BURST)) {
        ControlTask_processTxBurst();
        if (!ControlTask_isTxBurstPending()) {
            TaskEvent_clear(TASK_EVENT_CONTROL_TX_BURST);
        }
    }

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_TX_CONTINUOUS)) {
        ControlTask_processTxContinuous();
        if (!ControlTask_isTxContinuousPending()) {
            TaskEvent_clear(TASK_EVENT_CONTROL_TX_CONTINUOUS);
        }
    }

    /* Poll jam session timeout (runs independently from TX continuous) */
    ControlTask_processJamTimeout();

    /* Keep jam session transmitting */
    RadioIF_pollJamSession();

    if (TaskEvent_isSet(TASK_EVENT_CONTROL_RX_START)) {
        s_rx_active = RadioIF_startRx();
        if (s_rx_active) {
            TaskEvent_set(TASK_EVENT_DATA_RX_ACTIVE);
        } else {
            /* RF backend failed — notify host so failure is not silent. */
            uint8_t err_payload[1] = {ERR_RF_INIT_FAILED};
            OutputIF_sendResponse(RSP_ERROR, 0, err_payload, 1u);
        }
        TaskEvent_clear(TASK_EVENT_CONTROL_RX_START);
    }

    RadioIF_poll();

    if (s_rx_active) {
        RadioIF_RxPacket pkt;
        uint8_t max_pkts = 8u; /* Limit per poll to prevent UART starvation */
        while (max_pkts > 0u && RadioIF_popRxPacket(&pkt)) {
            if (LLManager_processRxPacket(&pkt)) {
                DataTask_emitRxPacket(&pkt);
            }
            max_pkts--;
        }
    }
}
