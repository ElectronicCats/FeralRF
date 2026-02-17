/*
 * FeralRF CC1352 - Link Layer Manager
 */

#include "ll_manager.h"

#include <stddef.h>

#include "phy_manager.h"

static uint8_t s_selected_profile = LL_MANAGER_DEFAULT;
static uint32_t s_dropped_packets = 0u;
static LLManager_Stats s_stats = {0u, 0u, 0u, 0u, 0u};

#define BLE_LL_HEADER_LEN 2u
#define BLE_LL_LENGTH_MASK 0x3Fu
#define BLE_PDU_TYPE_MASK 0x0Fu
#define BLE_DATA_LLID_MASK 0x03u
#define BLE_ADV_CH_MIN 37u
#define BLE_ADV_CH_MAX 39u
#define BLE_DATA_CH_MAX 36u
#define LL_PDU_TYPE_INVALID 0xFFu
#define BLE_ADV_TYPE_ADV_IND 0x00u
#define BLE_ADV_TYPE_ADV_DIRECT_IND 0x01u
#define BLE_ADV_TYPE_ADV_NONCONN_IND 0x02u
#define BLE_ADV_TYPE_SCAN_REQ 0x03u
#define BLE_ADV_TYPE_SCAN_RSP 0x04u
#define BLE_ADV_TYPE_CONNECT_IND 0x05u
#define BLE_ADV_TYPE_ADV_SCAN_IND 0x06u
#define BLE_ADV_TYPE_ADV_EXT_IND 0x07u

static bool LLManager_isBleAdvChannel(uint8_t channel) {
    return (channel >= BLE_ADV_CH_MIN) && (channel <= BLE_ADV_CH_MAX);
}

static bool LLManager_isBleDataChannel(uint8_t channel) {
    return channel <= BLE_DATA_CH_MAX;
}

static bool LLManager_processBlePacket(RadioIF_RxPacket *pkt) {
    uint8_t ll_payload_len = 0u;
    uint8_t expected_len = 0u;
    uint8_t pdu_type = 0u;

    if (pkt == NULL) {
        return false;
    }

    pkt->ll_pdu_kind = LL_PDU_KIND_UNKNOWN;
    pkt->ll_pdu_type = LL_PDU_TYPE_INVALID;
    pkt->ll_pdu_flags = 0u;

    if (pkt->data_len < BLE_LL_HEADER_LEN) {
        s_dropped_packets++;
        return false;
    }

    ll_payload_len = (uint8_t)(pkt->data[1] & BLE_LL_LENGTH_MASK);
    expected_len = (uint8_t)(BLE_LL_HEADER_LEN + ll_payload_len);

    if (expected_len > pkt->data_len) {
        s_dropped_packets++;
        return false;
    }

    if (expected_len < pkt->data_len) {
        pkt->data_len = expected_len;
    }

    if (LLManager_isBleAdvChannel(pkt->channel)) {
        pdu_type = (uint8_t)(pkt->data[0] & BLE_PDU_TYPE_MASK);
        pkt->ll_pdu_type = pdu_type;
        pkt->ll_pdu_flags |= LL_PDU_FLAG_PRIMARY_ADV_CH;

        switch (pdu_type) {
        case BLE_ADV_TYPE_SCAN_REQ:
        case BLE_ADV_TYPE_SCAN_RSP:
            pkt->ll_pdu_kind = LL_PDU_KIND_SCAN;
            s_stats.kind_scan++;
            break;
        case BLE_ADV_TYPE_CONNECT_IND:
            pkt->ll_pdu_kind = LL_PDU_KIND_CONNECT;
            s_stats.kind_connect++;
            break;
        case BLE_ADV_TYPE_ADV_IND:
        case BLE_ADV_TYPE_ADV_DIRECT_IND:
        case BLE_ADV_TYPE_ADV_NONCONN_IND:
        case BLE_ADV_TYPE_ADV_SCAN_IND:
        case BLE_ADV_TYPE_ADV_EXT_IND:
            pkt->ll_pdu_kind = LL_PDU_KIND_ADV;
            if (pdu_type == BLE_ADV_TYPE_ADV_EXT_IND) {
                pkt->ll_pdu_flags |= LL_PDU_FLAG_EXT_ADV;
            }
            s_stats.kind_adv++;
            break;
        default:
            pkt->ll_pdu_kind = LL_PDU_KIND_UNKNOWN;
            pkt->ll_pdu_flags |= LL_PDU_FLAG_RESERVED;
            s_stats.kind_unknown++;
            break;
        }
        return true;
    }

    if (LLManager_isBleDataChannel(pkt->channel)) {
        uint8_t llid = (uint8_t)(pkt->data[0] & BLE_DATA_LLID_MASK);

        pkt->ll_pdu_kind = LL_PDU_KIND_DATA;
        pkt->ll_pdu_type = llid;
        pkt->ll_pdu_flags |= LL_PDU_FLAG_DATA_CH;
        if (llid == 0u) {
            pkt->ll_pdu_kind = LL_PDU_KIND_UNKNOWN;
            pkt->ll_pdu_flags |= LL_PDU_FLAG_RESERVED;
            s_stats.kind_unknown++;
        } else {
            s_stats.kind_data++;
        }
        return true;
    }

    s_stats.kind_unknown++;
    return true;
}

void LLManager_init(void) {
    s_selected_profile = LL_MANAGER_DEFAULT;
    s_dropped_packets = 0u;
    LLManager_resetStats();
}

void LLManager_select(uint8_t profile) {
    if (profile == PHY_MANAGER_LL_BLE) {
        s_selected_profile = LL_MANAGER_BLE;
        return;
    }

    s_selected_profile = LL_MANAGER_DEFAULT;
}

uint8_t LLManager_getSelected(void) {
    return s_selected_profile;
}

bool LLManager_processRxPacket(RadioIF_RxPacket *pkt) {
    if (pkt == NULL) {
        return false;
    }

    pkt->ll_pdu_kind = LL_PDU_KIND_UNKNOWN;
    pkt->ll_pdu_type = LL_PDU_TYPE_INVALID;
    pkt->ll_pdu_flags = 0u;

    switch (s_selected_profile) {
    case LL_MANAGER_BLE:
        return LLManager_processBlePacket(pkt);
    case LL_MANAGER_DEFAULT:
    default:
        return true;
    }
}

uint32_t LLManager_getDroppedPackets(void) {
    return s_dropped_packets;
}

void LLManager_getStats(LLManager_Stats *out) {
    if (out == NULL) {
        return;
    }

    *out = s_stats;
}

void LLManager_resetStats(void) {
    s_stats.kind_unknown = 0u;
    s_stats.kind_adv = 0u;
    s_stats.kind_scan = 0u;
    s_stats.kind_connect = 0u;
    s_stats.kind_data = 0u;
}
