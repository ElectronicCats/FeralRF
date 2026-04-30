/*
 * FeralRF CC1352 - ATT/GATT Client (Phase 3)
 *
 * State machine for GATT discovery over raw RF connection.
 * Queues ATT requests via TXQueue, processes responses from BleConnMgr.
 *
 * L2CAP framing: [Length:2LE][CID:2LE][ATT PDU]
 * ATT CID = 0x0004
 * Default MTU = 23 (no fragmentation needed)
 */

#include "att_client.h"
#include "tx_queue.h"

#include <string.h>

/* ── ATT Opcodes (Core Spec Vol 3, Part F, 3.4) ── */
#define ATT_ERROR_RSP 0x01u
#define ATT_EXCHANGE_MTU_REQ 0x02u
#define ATT_EXCHANGE_MTU_RSP 0x03u
#define ATT_FIND_INFO_REQ 0x04u
#define ATT_FIND_INFO_RSP 0x05u
#define ATT_READ_BY_TYPE_REQ 0x08u
#define ATT_READ_BY_TYPE_RSP 0x09u
#define ATT_READ_REQ 0x0Au
#define ATT_READ_RSP 0x0Bu
#define ATT_WRITE_REQ 0x12u
#define ATT_WRITE_RSP 0x13u
#define ATT_READ_BY_GROUP_TYPE_REQ 0x10u
#define ATT_READ_BY_GROUP_TYPE_RSP 0x11u

/* ATT Error codes */
#define ATT_ERR_ATTRIBUTE_NOT_FOUND 0x0Au

/* GATT UUIDs */
#define GATT_PRIMARY_SERVICE_UUID 0x2800u
#define GATT_CHARACTERISTIC_UUID 0x2803u

/* L2CAP */
#define L2CAP_CID_ATT 0x0004u
#define ATT_DEFAULT_MTU 23u

/* Max services/characteristics we can track for auto-discovery */
#define MAX_SERVICES 16u

/* ── State ── */
static AttClient_State s_state;
static AttClient_Callbacks s_cb;
static uint16_t s_mtu;
static bool s_request_pending;

/* Discovery state */
static uint16_t s_disc_next_handle;

/* Service table for char discovery */
static struct {
    uint16_t startHandle;
    uint16_t endHandle;
} s_services[MAX_SERVICES];
static uint8_t s_service_count;
static uint8_t s_service_idx;

/* For read/write requests */
static uint16_t s_rw_handle;

/* ── L2CAP + ATT TX helper ── */

static bool att_send(const uint8_t *att_pdu, uint8_t att_len) {
    /* L2CAP frame: [att_len:2LE][CID_ATT:2LE][att_pdu] */
    uint8_t l2cap[4 + ATT_DEFAULT_MTU];
    if (att_len > ATT_DEFAULT_MTU - 0u) {
        return false;
    }
    uint8_t total = 4u + att_len;
    l2cap[0] = att_len;
    l2cap[1] = 0;
    l2cap[2] = (uint8_t)(L2CAP_CID_ATT & 0xFF);
    l2cap[3] = (uint8_t)(L2CAP_CID_ATT >> 8);
    memcpy(&l2cap[4], att_pdu, att_len);

    return TXQueue_insert(total, TX_QUEUE_LLID_DATA_START, l2cap);
}

/* ── ATT Request builders ── */

static bool send_exchange_mtu_req(void) {
    uint8_t pdu[3];
    pdu[0] = ATT_EXCHANGE_MTU_REQ;
    pdu[1] = (uint8_t)(ATT_DEFAULT_MTU & 0xFF);
    pdu[2] = (uint8_t)(ATT_DEFAULT_MTU >> 8);
    return att_send(pdu, 3);
}

static bool send_read_by_group_type_req(uint16_t startHandle, uint16_t endHandle, uint16_t uuid16) {
    uint8_t pdu[7];
    pdu[0] = ATT_READ_BY_GROUP_TYPE_REQ;
    pdu[1] = (uint8_t)(startHandle & 0xFF);
    pdu[2] = (uint8_t)(startHandle >> 8);
    pdu[3] = (uint8_t)(endHandle & 0xFF);
    pdu[4] = (uint8_t)(endHandle >> 8);
    pdu[5] = (uint8_t)(uuid16 & 0xFF);
    pdu[6] = (uint8_t)(uuid16 >> 8);
    return att_send(pdu, 7);
}

static bool send_read_by_type_req(uint16_t startHandle, uint16_t endHandle, uint16_t uuid16) {
    uint8_t pdu[7];
    pdu[0] = ATT_READ_BY_TYPE_REQ;
    pdu[1] = (uint8_t)(startHandle & 0xFF);
    pdu[2] = (uint8_t)(startHandle >> 8);
    pdu[3] = (uint8_t)(endHandle & 0xFF);
    pdu[4] = (uint8_t)(endHandle >> 8);
    pdu[5] = (uint8_t)(uuid16 & 0xFF);
    pdu[6] = (uint8_t)(uuid16 >> 8);
    return att_send(pdu, 7);
}

static bool send_read_req(uint16_t handle) {
    uint8_t pdu[3];
    pdu[0] = ATT_READ_REQ;
    pdu[1] = (uint8_t)(handle & 0xFF);
    pdu[2] = (uint8_t)(handle >> 8);
    return att_send(pdu, 3);
}

static bool send_write_req(uint16_t handle, const uint8_t *data, uint8_t len) {
    uint8_t pdu[3 + ATT_DEFAULT_MTU];
    if (len > ATT_DEFAULT_MTU - 3u) {
        return false;
    }
    pdu[0] = ATT_WRITE_REQ;
    pdu[1] = (uint8_t)(handle & 0xFF);
    pdu[2] = (uint8_t)(handle >> 8);
    memcpy(&pdu[3], data, len);
    return att_send(pdu, 3u + len);
}

/* ── Inline LE helpers ── */

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* ── ATT Response handlers ── */

static void handle_mtu_rsp(const uint8_t *pdu, uint8_t len) {
    if (len < 3 || s_state != ATT_STATE_WAIT_MTU_RSP) {
        return;
    }
    uint16_t server_mtu = le16(&pdu[1]);
    s_mtu = (server_mtu < ATT_DEFAULT_MTU) ? server_mtu : ATT_DEFAULT_MTU;
    s_request_pending = false;

    /* Start service discovery */
    s_disc_next_handle = 0x0001;
    s_service_count = 0;
    s_state = ATT_STATE_WAIT_DISCOVER_RSP;
}

static void handle_read_by_group_type_rsp(const uint8_t *pdu, uint8_t len) {
    if (len < 4 || s_state != ATT_STATE_WAIT_DISCOVER_RSP) {
        return;
    }
    s_request_pending = false;

    uint8_t entry_len = pdu[1]; /* length of each attribute data */
    uint8_t offset = 2;

    while (offset + entry_len <= len) {
        uint16_t startH = le16(&pdu[offset]);
        uint16_t endH = le16(&pdu[offset + 2]);
        uint8_t uuidLen = entry_len - 4u;
        const uint8_t *uuid = &pdu[offset + 4];

        /* Store for characteristic discovery */
        if (s_service_count < MAX_SERVICES) {
            s_services[s_service_count].startHandle = startH;
            s_services[s_service_count].endHandle = endH;
            s_service_count++;
        }

        /* Notify host */
        if (s_cb.onService) {
            s_cb.onService(startH, endH, uuid, uuidLen);
        }

        s_disc_next_handle = endH + 1u;
        offset += entry_len;
    }

    /* If endHandle was 0xFFFF or we wrapped, service discovery is done */
    if (s_disc_next_handle == 0x0000 || s_disc_next_handle == 0xFFFF) {
        /* Move to characteristic discovery */
        s_service_idx = 0;
        s_state = ATT_STATE_WAIT_CHAR_RSP;
    }
    /* Otherwise poll() will send next request */
}

static void handle_read_by_type_rsp(const uint8_t *pdu, uint8_t len) {
    if (len < 4 || s_state != ATT_STATE_WAIT_CHAR_RSP) {
        return;
    }
    s_request_pending = false;

    uint8_t entry_len = pdu[1];
    uint8_t offset = 2;

    while (offset + entry_len <= len) {
        uint16_t handle = le16(&pdu[offset]);
        uint8_t properties = pdu[offset + 2];
        uint16_t valueHandle = le16(&pdu[offset + 3]);
        uint8_t uuidLen = entry_len - 5u;
        const uint8_t *uuid = &pdu[offset + 5];

        if (s_cb.onChar) {
            s_cb.onChar(handle, properties, valueHandle, uuid, uuidLen);
        }

        s_disc_next_handle = handle + 1u;
        offset += entry_len;
    }
}

static void handle_read_rsp(const uint8_t *pdu, uint8_t len) {
    if (s_state != ATT_STATE_WAIT_READ_RSP) {
        return;
    }
    s_request_pending = false;

    if (s_cb.onRead) {
        s_cb.onRead(s_rw_handle, &pdu[1], len - 1u);
    }
    s_state = ATT_STATE_IDLE;
    if (s_cb.onDone) {
        s_cb.onDone(0);
    }
}

static void handle_write_rsp(const uint8_t *pdu, uint8_t len) {
    (void)pdu;
    (void)len;
    if (s_state != ATT_STATE_WAIT_WRITE_RSP) {
        return;
    }
    s_request_pending = false;
    s_state = ATT_STATE_IDLE;
    if (s_cb.onDone) {
        s_cb.onDone(0);
    }
}

static void handle_error_rsp(const uint8_t *pdu, uint8_t len) {
    if (len < 5) {
        return;
    }
    s_request_pending = false;
    uint8_t req_opcode = pdu[1];
    uint8_t error_code = pdu[4];

    if (error_code == ATT_ERR_ATTRIBUTE_NOT_FOUND) {
        /* Normal end of discovery for current request type */
        if (s_state == ATT_STATE_WAIT_DISCOVER_RSP) {
            /* Service discovery complete, start char discovery */
            s_service_idx = 0;
            s_disc_next_handle = 0;
            s_state = ATT_STATE_WAIT_CHAR_RSP;
            return;
        }
        if (s_state == ATT_STATE_WAIT_CHAR_RSP) {
            /* Chars done for this service, move to next */
            s_service_idx++;
            s_disc_next_handle = 0;
            if (s_service_idx >= s_service_count) {
                /* All done */
                s_state = ATT_STATE_IDLE;
                if (s_cb.onDone) {
                    s_cb.onDone(0);
                }
            }
            return;
        }
    }

    /* Actual error */
    (void)req_opcode;
    s_state = ATT_STATE_IDLE;
    if (s_cb.onDone) {
        s_cb.onDone(1);
    }
}

/* ── Public API ── */

void AttClient_init(void) {
    s_state = ATT_STATE_IDLE;
    s_mtu = ATT_DEFAULT_MTU;
    s_request_pending = false;
    s_service_count = 0;
    s_service_idx = 0;
    memset(&s_cb, 0, sizeof(s_cb));
}

void AttClient_setCallbacks(const AttClient_Callbacks *cb) {
    s_cb = *cb;
}

bool AttClient_startDiscover(void) {
    if (s_state != ATT_STATE_IDLE) {
        return false;
    }
    s_service_count = 0;
    s_service_idx = 0;
    s_disc_next_handle = 0x0001;
    s_request_pending = false;
    /* Skip MTU exchange — default MTU 23 is fine, some peripherals
     * ignore MTU requests from raw RF connections. Go straight to
     * service discovery. */
    s_state = ATT_STATE_WAIT_DISCOVER_RSP;
    return true;
}

bool AttClient_startRead(uint16_t handle) {
    if (s_state != ATT_STATE_IDLE) {
        return false;
    }
    s_rw_handle = handle;
    s_request_pending = false;
    s_state = ATT_STATE_WAIT_READ_RSP;
    return true;
}

bool AttClient_startWrite(uint16_t handle, const uint8_t *data, uint8_t len) {
    if (s_state != ATT_STATE_IDLE) {
        return false;
    }
    s_rw_handle = handle;
    s_request_pending = false;
    /* Queue the write immediately since we have the data now */
    if (!send_write_req(handle, data, len)) {
        return false;
    }
    s_request_pending = true;
    s_state = ATT_STATE_WAIT_WRITE_RSP;
    return true;
}

void AttClient_onL2capRx(const uint8_t *l2capPayload, uint8_t len) {
    /* l2capPayload = [L2CAP_Length:2][CID:2][ATT PDU] */
    if (len < 5) {
        return;
    }
    uint16_t cid = le16(&l2capPayload[2]);
    if (cid != L2CAP_CID_ATT) {
        return;
    }
    uint16_t att_len = le16(&l2capPayload[0]);
    const uint8_t *att_pdu = &l2capPayload[4];

    if (att_len == 0 || att_len > len - 4u) {
        return;
    }

    uint8_t opcode = att_pdu[0];

    switch (opcode) {
    case ATT_EXCHANGE_MTU_RSP:
        handle_mtu_rsp(att_pdu, att_len);
        break;
    case ATT_READ_BY_GROUP_TYPE_RSP:
        handle_read_by_group_type_rsp(att_pdu, att_len);
        break;
    case ATT_READ_BY_TYPE_RSP:
        handle_read_by_type_rsp(att_pdu, att_len);
        break;
    case ATT_READ_RSP:
        handle_read_rsp(att_pdu, att_len);
        break;
    case ATT_WRITE_RSP:
        handle_write_rsp(att_pdu, att_len);
        break;
    case ATT_ERROR_RSP:
        handle_error_rsp(att_pdu, att_len);
        break;
    default:
        break;
    }
}

void AttClient_poll(void) {
    if (s_state == ATT_STATE_IDLE || s_request_pending) {
        return;
    }

    switch (s_state) {
    case ATT_STATE_WAIT_MTU_RSP:
        if (!s_request_pending) {
            if (send_exchange_mtu_req()) {
                s_request_pending = true;
            }
        }
        break;

    case ATT_STATE_WAIT_DISCOVER_RSP:
        if (s_disc_next_handle == 0) {
            s_disc_next_handle = 0x0001;
        }
        if (send_read_by_group_type_req(s_disc_next_handle, 0xFFFF, GATT_PRIMARY_SERVICE_UUID)) {
            s_request_pending = true;
        }
        break;

    case ATT_STATE_WAIT_CHAR_RSP:
        if (s_service_idx >= s_service_count) {
            s_state = ATT_STATE_IDLE;
            if (s_cb.onDone) {
                s_cb.onDone(0);
            }
            break;
        }
        {
            uint16_t start = s_disc_next_handle;
            if (start == 0) {
                start = s_services[s_service_idx].startHandle;
            }
            uint16_t end = s_services[s_service_idx].endHandle;
            if (send_read_by_type_req(start, end, GATT_CHARACTERISTIC_UUID)) {
                s_request_pending = true;
            }
        }
        break;

    case ATT_STATE_WAIT_READ_RSP:
        if (send_read_req(s_rw_handle)) {
            s_request_pending = true;
        }
        break;

    default:
        break;
    }
}

void AttClient_reset(void) {
    s_state = ATT_STATE_IDLE;
    s_request_pending = false;
    s_mtu = ATT_DEFAULT_MTU;
    s_service_count = 0;
    s_service_idx = 0;
}

AttClient_State AttClient_getState(void) {
    return s_state;
}
