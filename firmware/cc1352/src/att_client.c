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
#include "ble_conn.h"
#include "output_if.h"
#include "tx_queue.h"

#include <string.h>

/* Response code for GATT notifications/indications streamed to the host.
 * The canonical definition lives in command_processor.c; duplicated here
 * because that file's response codes are file-local #defines. Keep in sync. */
#define RSP_GATT_NOTIFY 0xA6u

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
#define ATT_HANDLE_VALUE_NOTIFICATION 0x1Bu
#define ATT_HANDLE_VALUE_INDICATION 0x1Du
#define ATT_HANDLE_VALUE_CONFIRMATION 0x1Eu

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

/* Client MTU advertised in the most recent explicit exchange request. */
static uint16_t s_requested_mtu;

/* Service table for char discovery */
static struct {
    uint16_t startHandle;
    uint16_t endHandle;
} s_services[MAX_SERVICES];
static uint8_t s_service_count;
static uint8_t s_service_idx;

/* For read/write requests */
static uint16_t s_rw_handle;

/* ── Debug ring buffer (Task 0.2) ── */
static AttClient_DbgEntry s_dbg_log[ATT_DBG_LOG_DEPTH];
static uint8_t s_dbg_head;  /* next write slot */
static uint8_t s_dbg_count; /* valid entries, saturates at depth */
static uint16_t s_dbg_seq;  /* monotonic sequence */

/* ── Notification/indication metrics (F8b Track A) ──
 * Counters are not currently surfaced through a dedicated CMD_*; they exist
 * for instrumentation and can be exposed later via the debug log path. */
static struct {
    uint16_t notif_rx;  /* total 0x1B + 0x1D PDUs accepted and emitted */
    uint16_t notif_bad; /* malformed (len < 3) PDUs dropped */
} s_metrics;

static void att_dbg(uint8_t tag, AttClient_State oldState) {
    AttClient_DbgEntry *e = &s_dbg_log[s_dbg_head];
    e->seq = s_dbg_seq++;
    e->tag = tag;
    e->oldState = (uint8_t)oldState;
    e->newState = (uint8_t)s_state;
    e->connAlive = BleConn_isConnected() ? 1u : 0u;
    e->mtu = (uint8_t)s_mtu;
    e->reqPending = s_request_pending ? 1u : 0u;
    s_dbg_head = (uint8_t)((s_dbg_head + 1u) % ATT_DBG_LOG_DEPTH);
    if (s_dbg_count < ATT_DBG_LOG_DEPTH) {
        s_dbg_count++;
    }
}

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

/* ── Notification/indication helpers (F8b Track A) ── */

static void AttClient_emitNotification(const uint8_t *handle_and_value, uint8_t len) {
    /* Build RSP_GATT_NOTIFY payload = [handle:2LE][value:N], passing the ATT
     * payload bytes through unchanged. seq=0 because notifications are
     * unsolicited streaming responses (no host-side request to correlate). */
    OutputIF_sendResponse(RSP_GATT_NOTIFY, 0u, handle_and_value, (uint16_t)len);
}

static void AttClient_sendCfm(void) {
    /* ATT_HANDLE_VALUE_CONFIRMATION is a single-byte PDU sent on the ATT
     * L2CAP channel. Per BT Core Spec Vol 3 Part F §3.4.7.3, the client must
     * acknowledge every received indication; failure to do so will stall
     * further indications from the peer. Reuse att_send (the same TX path
     * all ATT requests already use). */
    uint8_t cfm = ATT_HANDLE_VALUE_CONFIRMATION;
    (void)att_send(&cfm, 1u);
}

/* ── ATT Request builders ── */

static bool send_exchange_mtu_req(uint16_t clientMtu) {
    uint8_t pdu[3];
    pdu[0] = ATT_EXCHANGE_MTU_REQ;
    pdu[1] = (uint8_t)(clientMtu & 0xFF);
    pdu[2] = (uint8_t)(clientMtu >> 8);
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
    AttClient_State old = s_state;
    if (len < 3) {
        att_dbg(ATT_DBG_TAG_MTU_RSP, old);
        return;
    }
    if (s_state != ATT_STATE_WAIT_MTU_RSP && s_state != ATT_STATE_WAIT_MTU_EXCHANGE) {
        att_dbg(ATT_DBG_TAG_MTU_RSP, old);
        return;
    }
    uint16_t server_mtu = le16(&pdu[1]);
    /* `s_mtu` is the firmware's TX/RX buffer cap — it is always
     * min(server_mtu, ATT_DEFAULT_MTU) regardless of which path we are on.
     * The explicit-exchange path additionally surfaces the peer's raw
     * advertised value through onMtu so the host can record it. */
    s_mtu = (server_mtu < ATT_DEFAULT_MTU) ? server_mtu : ATT_DEFAULT_MTU;
    s_request_pending = false;

    if (s_state == ATT_STATE_WAIT_MTU_EXCHANGE) {
        if (s_cb.onMtu) {
            /* Pass the peer's raw advertised MTU. The firmware's own buffer
             * cap (s_mtu) is unchanged. */
            s_cb.onMtu(server_mtu);
        }
        s_state = ATT_STATE_IDLE;
        att_dbg(ATT_DBG_TAG_MTU_RSP, old);
        if (s_cb.onDone) {
            att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
            s_cb.onDone(0);
        }
        return;
    }

    /* Legacy auto-flow: this path runs only if a future caller re-enables
     * MTU exchange in AttClient_startDiscover (currently skipped — see
     * AttClient_startDiscover comment). Keep behavior unchanged. */
    s_disc_next_handle = 0x0001;
    s_service_count = 0;
    s_state = ATT_STATE_WAIT_DISCOVER_RSP;
    att_dbg(ATT_DBG_TAG_MTU_RSP, old);
}

static void handle_read_by_group_type_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (len < 4 || s_state != ATT_STATE_WAIT_DISCOVER_RSP) {
        att_dbg(ATT_DBG_TAG_GROUP_RSP, old);
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
    att_dbg(ATT_DBG_TAG_GROUP_RSP, old);
}

static void handle_read_by_type_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (len < 4 || s_state != ATT_STATE_WAIT_CHAR_RSP) {
        att_dbg(ATT_DBG_TAG_TYPE_RSP, old);
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
    att_dbg(ATT_DBG_TAG_TYPE_RSP, old);
}

static void handle_read_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (s_state != ATT_STATE_WAIT_READ_RSP) {
        att_dbg(ATT_DBG_TAG_READ_RSP, old);
        return;
    }
    s_request_pending = false;

    if (s_cb.onRead) {
        s_cb.onRead(s_rw_handle, &pdu[1], len - 1u);
    }
    s_state = ATT_STATE_IDLE;
    att_dbg(ATT_DBG_TAG_READ_RSP, old);
    if (s_cb.onDone) {
        att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
        s_cb.onDone(0);
    }
}

static void handle_write_rsp(const uint8_t *pdu, uint8_t len) {
    (void)pdu;
    (void)len;
    AttClient_State old = s_state;
    if (s_state != ATT_STATE_WAIT_WRITE_RSP) {
        att_dbg(ATT_DBG_TAG_WRITE_RSP, old);
        return;
    }
    s_request_pending = false;
    s_state = ATT_STATE_IDLE;
    att_dbg(ATT_DBG_TAG_WRITE_RSP, old);
    if (s_cb.onDone) {
        att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
        s_cb.onDone(0);
    }
}

static void handle_error_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (len < 5) {
        att_dbg(ATT_DBG_TAG_ERROR_RSP, old);
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
            att_dbg(ATT_DBG_TAG_ERROR_RSP, old);
            return;
        }
        if (s_state == ATT_STATE_WAIT_CHAR_RSP) {
            /* Chars done for this service, move to next */
            s_service_idx++;
            s_disc_next_handle = 0;
            if (s_service_idx >= s_service_count) {
                /* All done */
                s_state = ATT_STATE_IDLE;
                att_dbg(ATT_DBG_TAG_ERROR_RSP, old);
                if (s_cb.onDone) {
                    att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
                    s_cb.onDone(0);
                }
                return;
            }
            att_dbg(ATT_DBG_TAG_ERROR_RSP, old);
            return;
        }
    }

    /* Actual error */
    (void)req_opcode;
    s_state = ATT_STATE_IDLE;
    att_dbg(ATT_DBG_TAG_ERROR_RSP, old);
    if (s_cb.onDone) {
        att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
        s_cb.onDone(1);
    }
}

/* ── Public API ── */

void AttClient_init(void) {
    AttClient_State old = s_state;
    s_state = ATT_STATE_IDLE;
    s_mtu = ATT_DEFAULT_MTU;
    s_request_pending = false;
    s_service_count = 0;
    s_service_idx = 0;
    memset(&s_cb, 0, sizeof(s_cb));
    /* NB: dbg ring is intentionally NOT reset on init/reset — we want to
     * see the last events of cycle N to diagnose cycle N+1's failure.
     * It is only reset on chip boot via static initialization. */
    att_dbg(ATT_DBG_TAG_INIT, old);
}

void AttClient_setCallbacks(const AttClient_Callbacks *cb) {
    s_cb = *cb;
}

bool AttClient_startDiscover(void) {
    AttClient_State old = s_state;
    att_dbg(ATT_DBG_TAG_START_DISC_ENTER, old);
    if (s_state != ATT_STATE_IDLE) {
        att_dbg(ATT_DBG_TAG_START_DISC_EXIT_FAIL, old);
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
    att_dbg(ATT_DBG_TAG_START_DISC_EXIT_OK, old);
    return true;
}

bool AttClient_startRead(uint16_t handle) {
    AttClient_State old = s_state;
    att_dbg(ATT_DBG_TAG_START_READ_ENTER, old);
    if (s_state != ATT_STATE_IDLE) {
        att_dbg(ATT_DBG_TAG_START_READ_EXIT_FAIL, old);
        return false;
    }
    s_rw_handle = handle;
    s_request_pending = false;
    s_state = ATT_STATE_WAIT_READ_RSP;
    att_dbg(ATT_DBG_TAG_START_READ_EXIT_OK, old);
    return true;
}

bool AttClient_startWrite(uint16_t handle, const uint8_t *data, uint8_t len) {
    AttClient_State old = s_state;
    att_dbg(ATT_DBG_TAG_START_WRITE_ENTER, old);
    if (s_state != ATT_STATE_IDLE) {
        att_dbg(ATT_DBG_TAG_START_WRITE_EXIT_FAIL, old);
        return false;
    }
    s_rw_handle = handle;
    s_request_pending = false;
    /* Queue the write immediately since we have the data now */
    if (!send_write_req(handle, data, len)) {
        att_dbg(ATT_DBG_TAG_START_WRITE_EXIT_FAIL, old);
        return false;
    }
    s_request_pending = true;
    s_state = ATT_STATE_WAIT_WRITE_RSP;
    att_dbg(ATT_DBG_TAG_START_WRITE_EXIT_OK, old);
    return true;
}

bool AttClient_startMtuExchange(uint16_t clientMtu) {
    AttClient_State old = s_state;
    att_dbg(ATT_DBG_TAG_START_MTU_ENTER, old);
    if (s_state != ATT_STATE_IDLE) {
        att_dbg(ATT_DBG_TAG_START_MTU_EXIT_FAIL, old);
        return false;
    }
    if (clientMtu < ATT_DEFAULT_MTU) {
        clientMtu = ATT_DEFAULT_MTU;
    }
    s_requested_mtu = clientMtu;
    s_request_pending = false;
    s_state = ATT_STATE_WAIT_MTU_EXCHANGE;
    att_dbg(ATT_DBG_TAG_START_MTU_EXIT_OK, old);
    return true;
}

void AttClient_onL2capRx(const uint8_t *l2capPayload, uint8_t len) {
    AttClient_State old = s_state;
    att_dbg(ATT_DBG_TAG_L2CAP_RX, old);
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
    case ATT_HANDLE_VALUE_NOTIFICATION:
        /* Payload after the opcode: handle[2] + value[N]. Minimum
         * well-formed PDU is opcode + handle = 3 bytes. */
        if (att_len < 3u) {
            s_metrics.notif_bad++;
            att_dbg(ATT_DBG_TAG_NOTIFY_RX, old);
            return;
        }
        AttClient_emitNotification(&att_pdu[1], (uint8_t)(att_len - 1u));
        s_metrics.notif_rx++;
        att_dbg(ATT_DBG_TAG_NOTIFY_RX, old);
        return;
    case ATT_HANDLE_VALUE_INDICATION:
        /* Same shape as 0x1B (handle[2] + value[N]) but requires a
         * CONFIRMATION (0x1E) reply or the peer will stall. */
        if (att_len < 3u) {
            s_metrics.notif_bad++;
            att_dbg(ATT_DBG_TAG_INDICATE_RX, old);
            return;
        }
        AttClient_emitNotification(&att_pdu[1], (uint8_t)(att_len - 1u));
        s_metrics.notif_rx++;
        AttClient_sendCfm();
        att_dbg(ATT_DBG_TAG_INDICATE_RX, old);
        return;
    default:
        break;
    }
}

void AttClient_poll(void) {
    if (s_state == ATT_STATE_IDLE || s_request_pending) {
        return;
    }
    AttClient_State old = s_state;

    switch (s_state) {
    case ATT_STATE_WAIT_MTU_RSP:
        if (!s_request_pending) {
            if (send_exchange_mtu_req(ATT_DEFAULT_MTU)) {
                s_request_pending = true;
                att_dbg(ATT_DBG_TAG_POLL_TX_MTU, old);
            }
        }
        break;

    case ATT_STATE_WAIT_MTU_EXCHANGE:
        if (send_exchange_mtu_req(s_requested_mtu)) {
            s_request_pending = true;
            att_dbg(ATT_DBG_TAG_POLL_TX_MTU, old);
        }
        break;

    case ATT_STATE_WAIT_DISCOVER_RSP:
        if (s_disc_next_handle == 0) {
            s_disc_next_handle = 0x0001;
        }
        if (send_read_by_group_type_req(s_disc_next_handle, 0xFFFF, GATT_PRIMARY_SERVICE_UUID)) {
            s_request_pending = true;
            att_dbg(ATT_DBG_TAG_POLL_TX_GROUP, old);
        }
        break;

    case ATT_STATE_WAIT_CHAR_RSP:
        if (s_service_idx >= s_service_count) {
            s_state = ATT_STATE_IDLE;
            att_dbg(ATT_DBG_TAG_POLL_DONE, old);
            if (s_cb.onDone) {
                att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
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
                att_dbg(ATT_DBG_TAG_POLL_TX_TYPE, old);
            }
        }
        break;

    case ATT_STATE_WAIT_READ_RSP:
        if (send_read_req(s_rw_handle)) {
            s_request_pending = true;
            att_dbg(ATT_DBG_TAG_POLL_TX_READ, old);
        }
        break;

    default:
        break;
    }
}

void AttClient_reset(void) {
    AttClient_State old = s_state;
    s_state = ATT_STATE_IDLE;
    s_request_pending = false;
    s_mtu = ATT_DEFAULT_MTU;
    s_service_count = 0;
    s_service_idx = 0;
    att_dbg(ATT_DBG_TAG_RESET, old);
}

AttClient_State AttClient_getState(void) {
    return s_state;
}

uint8_t AttClient_getDebugLog(AttClient_DbgEntry *out, uint8_t maxEntries) {
    if (out == NULL || maxEntries == 0u) {
        return 0u;
    }
    uint8_t n = (s_dbg_count < maxEntries) ? s_dbg_count : maxEntries;
    /* Walk oldest-to-newest. With saturating ring, oldest is (head-count) mod depth. */
    uint8_t start = (uint8_t)((ATT_DBG_LOG_DEPTH + s_dbg_head - s_dbg_count) % ATT_DBG_LOG_DEPTH);
    for (uint8_t i = 0; i < n; i++) {
        out[i] = s_dbg_log[(start + i) % ATT_DBG_LOG_DEPTH];
    }
    return n;
}
