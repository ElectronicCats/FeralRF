/* FeralRF CC1352 - ATT server (F20.a.1).
 * Handles discovery + Read paths over L2CAP CID 0x0004.
 * Wired into BleConnMgr_pollSlave at Bundle 4. */
#include "att_server.h"

#include <string.h>

#include "gatt_table.h"

#define ATT_OP_ERROR_RSP 0x01u
#define ATT_OP_EXCHANGE_MTU_REQ 0x02u
#define ATT_OP_FIND_INFO_REQ 0x04u
#define ATT_OP_FIND_BY_TYPE_VAL_REQ 0x06u
#define ATT_OP_FIND_BY_TYPE_VAL_RSP 0x07u
#define ATT_OP_READ_BY_TYPE_REQ 0x08u
#define ATT_OP_READ_BY_TYPE_RSP 0x09u
#define ATT_OP_READ_REQ 0x0Au
#define ATT_OP_READ_RSP 0x0Bu
#define ATT_OP_READ_BLOB_REQ 0x0Cu
#define ATT_OP_READ_MULTIPLE_REQ 0x0Eu
#define ATT_OP_READ_BY_GROUP_REQ 0x10u
#define ATT_OP_READ_BY_GROUP_RSP 0x11u
#define ATT_OP_WRITE_REQ 0x12u
#define ATT_OP_WRITE_CMD 0x52u

#define ATT_ERR_INVALID_HANDLE 0x01u
#define ATT_ERR_READ_NOT_PERMITTED 0x02u
#define ATT_ERR_INVALID_PDU 0x04u
#define ATT_ERR_REQUEST_NOT_SUPPORTED 0x06u
#define ATT_ERR_ATTRIBUTE_NOT_FOUND 0x0Au

static uint8_t s_pending_tx[ATT_MAX_RSP_LEN];
static uint8_t s_pending_tx_len = 0u;

void AttServer_init(void) {
    s_pending_tx_len = 0u;
}

bool AttServer_hasPendingTx(void) {
    return s_pending_tx_len > 0u;
}

uint8_t AttServer_takePendingTx(uint8_t *out_buf, uint8_t buf_len) {
    if (s_pending_tx_len == 0u || buf_len < s_pending_tx_len) {
        return 0u;
    }
    memcpy(out_buf, s_pending_tx, s_pending_tx_len);
    uint8_t taken = s_pending_tx_len;
    s_pending_tx_len = 0u;
    return taken;
}

static void enqueue_tx(const uint8_t *pdu, uint8_t len) {
    if (len > ATT_MAX_RSP_LEN) {
        return;
    }
    memcpy(s_pending_tx, pdu, len);
    s_pending_tx_len = len;
}

static void send_error_rsp(uint8_t opcode_in_error, uint16_t handle, uint8_t error_code) {
    uint8_t rsp[5];
    rsp[0] = ATT_OP_ERROR_RSP;
    rsp[1] = opcode_in_error;
    rsp[2] = (uint8_t)(handle & 0xFFu);
    rsp[3] = (uint8_t)(handle >> 8);
    rsp[4] = error_code;
    enqueue_tx(rsp, 5u);
}

static void handle_read_req(const uint8_t *pdu, uint8_t pdu_len) {
    if (pdu_len != 3u) {
        send_error_rsp(ATT_OP_READ_REQ, 0x0000u, ATT_ERR_INVALID_PDU);
        return;
    }
    uint16_t handle = (uint16_t)pdu[1] | ((uint16_t)pdu[2] << 8);
    const Attribute *attr = GattTable_findByHandle(handle);
    if (attr == NULL) {
        send_error_rsp(ATT_OP_READ_REQ, handle, ATT_ERR_INVALID_HANDLE);
        return;
    }
    if ((attr->perms & GATT_PERM_READ) == 0u) {
        send_error_rsp(ATT_OP_READ_REQ, handle, ATT_ERR_READ_NOT_PERMITTED);
        return;
    }
    uint8_t rsp[ATT_MAX_RSP_LEN];
    rsp[0] = ATT_OP_READ_RSP;
    uint8_t copy_len = attr->value_len;
    if (copy_len > ATT_MAX_RSP_LEN - 1u) {
        copy_len = ATT_MAX_RSP_LEN - 1u;
    }
    memcpy(&rsp[1], attr->value, copy_len);
    enqueue_tx(rsp, (uint8_t)(1u + copy_len));
}

/* Discover Primary Services by Group Type (UUID 0x2800).
 * Returns list of (start_handle, end_handle, service_uuid). */
static void handle_read_by_group_type_req(const uint8_t *pdu, uint8_t pdu_len) {
    if (pdu_len != 7u) {
        send_error_rsp(ATT_OP_READ_BY_GROUP_REQ, 0x0000u, ATT_ERR_INVALID_PDU);
        return;
    }
    uint16_t start = (uint16_t)pdu[1] | ((uint16_t)pdu[2] << 8);
    uint16_t end = (uint16_t)pdu[3] | ((uint16_t)pdu[4] << 8);
    uint16_t group_type = (uint16_t)pdu[5] | ((uint16_t)pdu[6] << 8);

    if (group_type != ATTR_PRIMARY_SERVICE) {
        send_error_rsp(ATT_OP_READ_BY_GROUP_REQ, start, ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }

    /* Build response: opcode(1) + length_per_entry(1) + entries...
     * Each entry: start_handle(2) + end_handle(2) + service_uuid(2) = 6 bytes */
    uint8_t rsp[ATT_MAX_RSP_LEN];
    rsp[0] = ATT_OP_READ_BY_GROUP_RSP;
    rsp[1] = 6u; /* entry length: 2 handles + 2-byte UUID */
    uint8_t out_pos = 2u;
    bool found_any = false;

    for (size_t i = 0; i < GATT_TABLE_SIZE; i++) {
        const Attribute *attr = &g_gatt_table[i];
        if (attr->type != ATTR_PRIMARY_SERVICE)
            continue;
        if (attr->handle < start || attr->handle > end)
            continue;
        if (attr->value_len != 2u)
            continue;

        if (out_pos + 6u > ATT_MAX_RSP_LEN)
            break;

        /* Find end_handle: next service's handle - 1, or last attr handle */
        uint16_t end_handle = g_gatt_table[GATT_TABLE_SIZE - 1u].handle;
        for (size_t j = i + 1u; j < GATT_TABLE_SIZE; j++) {
            if (g_gatt_table[j].type == ATTR_PRIMARY_SERVICE) {
                end_handle = (uint16_t)(g_gatt_table[j].handle - 1u);
                break;
            }
        }

        rsp[out_pos++] = (uint8_t)(attr->handle & 0xFFu);
        rsp[out_pos++] = (uint8_t)(attr->handle >> 8);
        rsp[out_pos++] = (uint8_t)(end_handle & 0xFFu);
        rsp[out_pos++] = (uint8_t)(end_handle >> 8);
        rsp[out_pos++] = attr->value[0];
        rsp[out_pos++] = attr->value[1];
        found_any = true;
    }

    if (!found_any) {
        send_error_rsp(ATT_OP_READ_BY_GROUP_REQ, start, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }
    enqueue_tx(rsp, out_pos);
}

/* Discover Characteristics within a service range (Read By Type with type=0x2803). */
static void handle_read_by_type_req(const uint8_t *pdu, uint8_t pdu_len) {
    if (pdu_len != 7u) {
        send_error_rsp(ATT_OP_READ_BY_TYPE_REQ, 0x0000u, ATT_ERR_INVALID_PDU);
        return;
    }
    uint16_t start = (uint16_t)pdu[1] | ((uint16_t)pdu[2] << 8);
    uint16_t end = (uint16_t)pdu[3] | ((uint16_t)pdu[4] << 8);
    uint16_t type = (uint16_t)pdu[5] | ((uint16_t)pdu[6] << 8);

    /* A3.1: only support discovering characteristics by type 0x2803.
     * Other types (e.g. 0x2A00 Device Name as char value) → not supported here. */
    if (type != ATTR_CHARACTERISTIC) {
        send_error_rsp(ATT_OP_READ_BY_TYPE_REQ, start, ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }

    uint8_t rsp[ATT_MAX_RSP_LEN];
    rsp[0] = ATT_OP_READ_BY_TYPE_RSP;
    rsp[1] = 7u; /* entry length: handle(2) + value(5) for char declaration */
    uint8_t out_pos = 2u;
    bool found_any = false;

    for (size_t i = 0; i < GATT_TABLE_SIZE; i++) {
        const Attribute *attr = &g_gatt_table[i];
        if (attr->type != ATTR_CHARACTERISTIC)
            continue;
        if (attr->handle < start || attr->handle > end)
            continue;
        if (attr->value_len != 5u)
            continue;
        if (out_pos + 7u > ATT_MAX_RSP_LEN)
            break;

        rsp[out_pos++] = (uint8_t)(attr->handle & 0xFFu);
        rsp[out_pos++] = (uint8_t)(attr->handle >> 8);
        memcpy(&rsp[out_pos], attr->value, 5u);
        out_pos = (uint8_t)(out_pos + 5u);
        found_any = true;
    }

    if (!found_any) {
        send_error_rsp(ATT_OP_READ_BY_TYPE_REQ, start, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }
    enqueue_tx(rsp, out_pos);
}

/* Discover Primary Services by 16-bit UUID (Find By Type Value with type=0x2800). */
static void handle_find_by_type_value_req(const uint8_t *pdu, uint8_t pdu_len) {
    if (pdu_len < 7u) {
        send_error_rsp(ATT_OP_FIND_BY_TYPE_VAL_REQ, 0x0000u, ATT_ERR_INVALID_PDU);
        return;
    }
    uint16_t start = (uint16_t)pdu[1] | ((uint16_t)pdu[2] << 8);
    uint16_t end = (uint16_t)pdu[3] | ((uint16_t)pdu[4] << 8);
    uint16_t type = (uint16_t)pdu[5] | ((uint16_t)pdu[6] << 8);

    if (type != ATTR_PRIMARY_SERVICE || pdu_len != 9u) {
        send_error_rsp(ATT_OP_FIND_BY_TYPE_VAL_REQ, start, ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    /* Lookup target UUID16 from request bytes 7-8 */
    uint16_t target = (uint16_t)pdu[7] | ((uint16_t)pdu[8] << 8);

    uint8_t rsp[ATT_MAX_RSP_LEN];
    rsp[0] = ATT_OP_FIND_BY_TYPE_VAL_RSP;
    uint8_t out_pos = 1u;
    bool found_any = false;

    for (size_t i = 0; i < GATT_TABLE_SIZE; i++) {
        const Attribute *attr = &g_gatt_table[i];
        if (attr->type != ATTR_PRIMARY_SERVICE)
            continue;
        if (attr->handle < start || attr->handle > end)
            continue;
        if (attr->value_len != 2u)
            continue;
        uint16_t svc_uuid = (uint16_t)attr->value[0] | ((uint16_t)attr->value[1] << 8);
        if (svc_uuid != target)
            continue;

        if (out_pos + 4u > ATT_MAX_RSP_LEN)
            break;

        uint16_t end_handle = g_gatt_table[GATT_TABLE_SIZE - 1u].handle;
        for (size_t j = i + 1u; j < GATT_TABLE_SIZE; j++) {
            if (g_gatt_table[j].type == ATTR_PRIMARY_SERVICE) {
                end_handle = (uint16_t)(g_gatt_table[j].handle - 1u);
                break;
            }
        }

        rsp[out_pos++] = (uint8_t)(attr->handle & 0xFFu);
        rsp[out_pos++] = (uint8_t)(attr->handle >> 8);
        rsp[out_pos++] = (uint8_t)(end_handle & 0xFFu);
        rsp[out_pos++] = (uint8_t)(end_handle >> 8);
        found_any = true;
    }

    if (!found_any) {
        send_error_rsp(ATT_OP_FIND_BY_TYPE_VAL_REQ, start, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }
    enqueue_tx(rsp, out_pos);
}

void AttServer_handleRequest(const uint8_t *pdu, uint8_t pdu_len) {
    if (pdu == NULL || pdu_len < 1u) {
        return;
    }
    switch (pdu[0]) {
    case ATT_OP_FIND_BY_TYPE_VAL_REQ:
        handle_find_by_type_value_req(pdu, pdu_len);
        break;
    case ATT_OP_READ_BY_TYPE_REQ:
        handle_read_by_type_req(pdu, pdu_len);
        break;
    case ATT_OP_READ_REQ:
        handle_read_req(pdu, pdu_len);
        break;
    case ATT_OP_READ_BY_GROUP_REQ:
        handle_read_by_group_type_req(pdu, pdu_len);
        break;
    default:
        send_error_rsp(pdu[0], 0x0000u, ATT_ERR_REQUEST_NOT_SUPPORTED);
        break;
    }
}
