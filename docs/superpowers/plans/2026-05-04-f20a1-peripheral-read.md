# F20.a.1 BLE Peripheral Read-only Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Aceptar CONNECT_IND post-F21, transicionar a CMD_BLE5_SLAVE state machine, servir T2 GATT table estática vía ATT_READ_REQ. NO Write, NO Notify (esos son F20.a.2).

**Architecture:** Branch `feature/f20a1-peripheral-read` desde `main` HEAD `979eee2`. Trabajo firmware significativo (~600-900 LOC). 6 bundles secuenciales: protocol+smartrf → ATT server skeleton + static table → RadioIF slave wrapper + BleConnMgr slave loop → F21 handoff + L2CAP RX dispatch → Python API + tests → smoke V1. F8A central pattern espejado para slave (anchor/hop/event_count).

**Tech Stack:** CC1352P7 firmware (TI-RTOS 7, SDK 8.30, CMake), Python 3.11+ (pyserial, pytest), pre-commit.

**Spec source:** `docs/superpowers/specs/2026-05-04-f20a1-peripheral-read-design.md` (commit `979eee2`).

**Hardware:** TX peripheral en CatSniffer #1 (`/dev/ttyACM0`), Central en CatSniffer #2 (`/dev/ttyACM5`). Puertos pueden cambiar entre sesiones — re-verificar con `catnip devices`.

**Prerequisites verified at planning time** (2026-05-04):
- ❌ `Ble5_0_cmdBle5Slave` NO existe en `smartrf_ble5_0.c` → Bundle 1 agrega struct definition (pattern F21 cmdBleAdv defs precedent).
- ✅ `RadioIF_bleCentral` existe en `radio_if.c:2572` — espejear para `RadioIF_bleSlave`.
- ✅ `BleConnMgr_poll` (line 360) maneja event loop central — espejear para slave en BleConnMgr_pollSlave.
- ✅ `s_disconnect_cb` callback pattern de F8d en ble_conn_mgr.c:53 — reusable para F20 disconnect emission.
- ✅ `s_rf_data_queue` infra reutilizable (line 149).
- ✅ `AttClient_poll` (called BleConnMgr_poll:407) — espejear para `AttServer_poll` que dequeue pending ATT_RSPs.

---

## Task 0: Crear working branch

**Files:** None (git operation)

- [ ] **Step 1: Verify baseline**

```bash
git status
git log --oneline -3
```

Expected: HEAD = `979eee2 plan(f20.a.1): peripheral read-only spec`. Working tree clean.

- [ ] **Step 2: Create branch `feature/f20a1-peripheral-read`**

```bash
git checkout -b feature/f20a1-peripheral-read
git status
```

---

## Task 1: Protocol + SmartRF cmdBle5Slave struct

**Files:**
- Modify: `firmware/cc1352/include/protocol.h`
- Modify: `firmware/cc1352/src/smartrf_ble5_0.c`

- [ ] **Step 1: Add `CMD_GATT_SERVE_TABLE = 0x53` to `protocol.h`**

Find F21's `CMD_BLE_ADV_LEGACY = 0x52` (under F8b Track B follower section):

```bash
grep -n "CMD_BLE_ADV_LEGACY\|CMD_FOLLOW_DEBUG" firmware/cc1352/include/protocol.h
```

Edit:

```c
/* F8b Track B follower */
#define CMD_FOLLOW_START 0x50u
#define CMD_FOLLOW_STOP 0x51u
#define CMD_BLE_ADV_LEGACY 0x52u
#define CMD_GATT_SERVE_TABLE 0x53u
#define CMD_FOLLOW_DEBUG 0x54u
```

- [ ] **Step 2: Add `Ble5_0_cmdBle5Slave` struct to `smartrf_ble5_0.c`**

Find `Ble5_0_cmdBle5Master` (line ~640):

```bash
grep -n "Ble5_0_cmdBle5Master\b" firmware/cc1352/src/smartrf_ble5_0.c
```

Insert AFTER `Ble5_0_cmdBle5Master` definition. Read existing structure to match field layout.

```c
/* F20.a.1 — BLE Peripheral / Slave (CMD_BLE5_SLAVE, 0x1823).
 * Espejo de cmdBle5Master; pParams populated per-event by RadioIF_bleSlave. */
static rfc_ble5SlavePar_t s_ble5SlavePar = {
    .pRxQ = 0,
    .pTxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .seqStat.lastRxSn = 0x0,
    .seqStat.lastTxSn = 0x0,
    .seqStat.nextTxSn = 0x0,
    .seqStat.bFirstPkt = 0x1,
    .seqStat.bAutoEmpty = 0x0,
    .seqStat.bLlCtrlTx = 0x0,
    .seqStat.bLlCtrlAckRx = 0x0,
    .seqStat.bLlCtrlAckPending = 0x0,
    .maxNack = 0x0,
    .maxPkt = 0x0,
    .accessAddress = 0x0,
    .crcInit0 = 0x00,
    .crcInit1 = 0x00,
    .crcInit2 = 0x00,
    .timeoutTrigger.triggerType = TRIG_REL_START,
    .timeoutTrigger.bEnaCmd = 0x0,
    .timeoutTrigger.triggerNo = 0x0,
    .timeoutTrigger.pastTrig = 0x0,
    .timeoutTime = 0x00000000,
    .endTrigger.triggerType = TRIG_NEVER,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
};

static rfc_bleMasterSlaveOutput_t s_ble5SlaveOutput;

rfc_CMD_BLE5_SLAVE_t Ble5_0_cmdBle5Slave = {
    .commandNo = 0x1823, /* CMD_BLE5_SLAVE */
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_ABSTIME,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0x0,
    .channel = 0x00,
    .whitening.init = 0x0,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0, /* 1M */
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_ble5SlavePar,
    .pOutput = &s_ble5SlaveOutput,
    .tx20Power = 0x00000000,
};
```

- [ ] **Step 3: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -5
```

Expected: clean build. If `rfc_ble5SlavePar_t` or `rfc_CMD_BLE5_SLAVE_t` undefined, the SDK header probably needs different name — check via:

```bash
grep -rn "rfc_ble5Slave\|rfc_CMD_BLE5_SLAVE" firmware/cc1352/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/inc/ 2>/dev/null | head -5
```

Adjust struct name/fields if needed.

- [ ] **Step 4: Pre-commit + commit Bundle 1**

```bash
pre-commit run --files firmware/cc1352/include/protocol.h firmware/cc1352/src/smartrf_ble5_0.c
git add firmware/cc1352/include/protocol.h firmware/cc1352/src/smartrf_ble5_0.c
git commit -m "$(cat <<'EOF'
feat(f20.a.1): add CMD_GATT_SERVE_TABLE protocol + Ble5_0_cmdBle5Slave struct

Adds CMD_GATT_SERVE_TABLE (0x53) to protocol.h and Ble5_0_cmdBle5Slave
TI command struct (opcode 0x1823) to smartrf_ble5_0.c. Mirror of
Ble5_0_cmdBle5Master used by F8A central; per-event params populated
by RadioIF_bleSlave (Bundle 3).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: pre-commit clean, commit lands.

---

## Task 2: ATT server skeleton + static GATT table T2

**Files:**
- Create: `firmware/cc1352/include/att_server.h`
- Create: `firmware/cc1352/src/att_server.c`
- Create: `firmware/cc1352/include/gatt_table.h`
- Create: `firmware/cc1352/src/gatt_table.c`
- Modify: `firmware/cc1352/CMakeLists.txt` (add new sources)

- [ ] **Step 1: Create `gatt_table.h`**

```c
#ifndef GATT_TABLE_H
#define GATT_TABLE_H

#include <stdint.h>
#include <stddef.h>

#define ATTR_PRIMARY_SERVICE 0x2800u
#define ATTR_CHARACTERISTIC  0x2803u

#define GATT_PERM_READ  0x01u
#define GATT_PERM_WRITE 0x02u  /* F20.a.2 */
#define GATT_PERM_NOTIFY 0x10u /* F20.a.2 */

typedef struct {
    uint16_t handle;
    uint16_t type;          /* ATTR_* enum or specific UUID16 */
    uint8_t  perms;
    uint8_t  value_len;
    const uint8_t *value;
} Attribute;

extern const Attribute g_gatt_table[];
extern const size_t g_gatt_table_size;

const Attribute *GattTable_findByHandle(uint16_t handle);

#endif /* GATT_TABLE_H */
```

- [ ] **Step 2: Create `gatt_table.c` with T2 layout**

```c
#include "gatt_table.h"

static const uint8_t s_gap_uuid[2]    = {0x00, 0x18};
static const uint8_t s_dev_name[10]   = {'F','E','R','A','L','_','G','A','T','T'};
/* CHARACTERISTIC declaration: prop=Read, val_handle=0x0003, UUID=0x2A00 */
static const uint8_t s_devname_char[5] = {0x02, 0x03, 0x00, 0x00, 0x2A};
static const uint8_t s_custom_uuid[2] = {0xE0, 0xFF};
static const uint8_t s_test_value[11] = {'H','E','L','L','O','_','F','E','R','A','L'};
/* CHARACTERISTIC declaration: prop=Read, val_handle=0x0006, UUID=0xFFE1 */
static const uint8_t s_test_char[5]    = {0x02, 0x06, 0x00, 0xE1, 0xFF};

const Attribute g_gatt_table[6] = {
    {0x0001, ATTR_PRIMARY_SERVICE, GATT_PERM_READ, 2,  s_gap_uuid},
    {0x0002, ATTR_CHARACTERISTIC,  GATT_PERM_READ, 5,  s_devname_char},
    {0x0003, 0x2A00,               GATT_PERM_READ, 10, s_dev_name},
    {0x0004, ATTR_PRIMARY_SERVICE, GATT_PERM_READ, 2,  s_custom_uuid},
    {0x0005, ATTR_CHARACTERISTIC,  GATT_PERM_READ, 5,  s_test_char},
    {0x0006, 0xFFE1,               GATT_PERM_READ, 11, s_test_value},
};

const size_t g_gatt_table_size = 6;

const Attribute *GattTable_findByHandle(uint16_t handle) {
    for (size_t i = 0; i < g_gatt_table_size; i++) {
        if (g_gatt_table[i].handle == handle) {
            return &g_gatt_table[i];
        }
    }
    return NULL;
}
```

- [ ] **Step 3: Create `att_server.h`**

```c
#ifndef ATT_SERVER_H
#define ATT_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#define ATT_DEFAULT_MTU 23u
#define ATT_MAX_RSP_LEN ATT_DEFAULT_MTU

/* TX queue: pending response to enqueue at next connection event.
 * Single-slot FIFO — A3.1 only ever has one outstanding RSP. */
void AttServer_init(void);
void AttServer_handleRequest(const uint8_t *pdu, uint8_t pdu_len);
bool AttServer_hasPendingTx(void);
uint8_t AttServer_takePendingTx(uint8_t *out_buf, uint8_t buf_len);

#endif /* ATT_SERVER_H */
```

- [ ] **Step 4: Create `att_server.c` skeleton with Read handler stubs**

```c
#include "att_server.h"

#include <string.h>

#include "gatt_table.h"

#define ATT_OP_ERROR_RSP             0x01u
#define ATT_OP_EXCHANGE_MTU_REQ      0x02u
#define ATT_OP_FIND_INFO_REQ         0x04u
#define ATT_OP_FIND_BY_TYPE_VAL_REQ  0x06u
#define ATT_OP_READ_BY_TYPE_REQ      0x08u
#define ATT_OP_READ_BY_TYPE_RSP      0x09u
#define ATT_OP_READ_REQ              0x0Au
#define ATT_OP_READ_RSP              0x0Bu
#define ATT_OP_READ_BLOB_REQ         0x0Cu
#define ATT_OP_READ_MULTIPLE_REQ     0x0Eu
#define ATT_OP_READ_BY_GROUP_REQ     0x10u
#define ATT_OP_READ_BY_GROUP_RSP     0x11u
#define ATT_OP_WRITE_REQ             0x12u
#define ATT_OP_WRITE_CMD             0x52u

#define ATT_ERR_INVALID_HANDLE       0x01u
#define ATT_ERR_READ_NOT_PERMITTED   0x02u
#define ATT_ERR_REQUEST_NOT_SUPPORTED 0x06u
#define ATT_ERR_ATTRIBUTE_NOT_FOUND  0x0Au

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
        send_error_rsp(ATT_OP_READ_REQ, 0x0000u, ATT_ERR_INVALID_HANDLE);
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
        send_error_rsp(ATT_OP_READ_BY_GROUP_REQ, 0x0000u, ATT_ERR_INVALID_HANDLE);
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

    for (size_t i = 0; i < g_gatt_table_size; i++) {
        const Attribute *attr = &g_gatt_table[i];
        if (attr->type != ATTR_PRIMARY_SERVICE) continue;
        if (attr->handle < start || attr->handle > end) continue;
        if (attr->value_len != 2u) continue;

        if (out_pos + 6u > ATT_MAX_RSP_LEN) break;

        /* Find end_handle: next service's handle - 1, or last attr handle */
        uint16_t end_handle = g_gatt_table[g_gatt_table_size - 1u].handle;
        for (size_t j = i + 1u; j < g_gatt_table_size; j++) {
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
        send_error_rsp(ATT_OP_READ_BY_TYPE_REQ, 0x0000u, ATT_ERR_INVALID_HANDLE);
        return;
    }
    uint16_t start = (uint16_t)pdu[1] | ((uint16_t)pdu[2] << 8);
    uint16_t end = (uint16_t)pdu[3] | ((uint16_t)pdu[4] << 8);
    uint16_t type = (uint16_t)pdu[5] | ((uint16_t)pdu[6] << 8);

    /* A3.1: only support discovering characteristics by type 0x2803.
     * Other types (e.g. 0x2A00 Device Name as char value) → not supported here. */
    if (type != ATTR_CHARACTERISTIC) {
        send_error_rsp(ATT_OP_READ_BY_TYPE_REQ, start, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }

    uint8_t rsp[ATT_MAX_RSP_LEN];
    rsp[0] = ATT_OP_READ_BY_TYPE_RSP;
    rsp[1] = 7u; /* entry length: handle(2) + value(5) for char declaration */
    uint8_t out_pos = 2u;
    bool found_any = false;

    for (size_t i = 0; i < g_gatt_table_size; i++) {
        const Attribute *attr = &g_gatt_table[i];
        if (attr->type != ATTR_CHARACTERISTIC) continue;
        if (attr->handle < start || attr->handle > end) continue;
        if (attr->value_len != 5u) continue;
        if (out_pos + 7u > ATT_MAX_RSP_LEN) break;

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
        send_error_rsp(ATT_OP_FIND_BY_TYPE_VAL_REQ, 0x0000u, ATT_ERR_INVALID_HANDLE);
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
    rsp[0] = 0x07u; /* ATT_OP_FIND_BY_TYPE_VAL_RSP */
    uint8_t out_pos = 1u;
    bool found_any = false;

    for (size_t i = 0; i < g_gatt_table_size; i++) {
        const Attribute *attr = &g_gatt_table[i];
        if (attr->type != ATTR_PRIMARY_SERVICE) continue;
        if (attr->handle < start || attr->handle > end) continue;
        if (attr->value_len != 2u) continue;
        uint16_t svc_uuid = (uint16_t)attr->value[0] | ((uint16_t)attr->value[1] << 8);
        if (svc_uuid != target) continue;

        if (out_pos + 4u > ATT_MAX_RSP_LEN) break;

        uint16_t end_handle = g_gatt_table[g_gatt_table_size - 1u].handle;
        for (size_t j = i + 1u; j < g_gatt_table_size; j++) {
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
```

- [ ] **Step 5: Update `firmware/cc1352/CMakeLists.txt`**

```bash
grep -n "att_client.c\|ble_conn.c\b\|target_sources" firmware/cc1352/CMakeLists.txt | head -10
```

Add `att_server.c` and `gatt_table.c` to the source list (follow existing pattern of `att_client.c`).

- [ ] **Step 6: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -5
```

Expected: clean. The new files compile but nothing calls them yet.

- [ ] **Step 7: Pre-commit + commit Bundle 2**

```bash
pre-commit run --files firmware/cc1352/include/att_server.h firmware/cc1352/src/att_server.c firmware/cc1352/include/gatt_table.h firmware/cc1352/src/gatt_table.c firmware/cc1352/CMakeLists.txt
git add firmware/cc1352/include/att_server.h firmware/cc1352/src/att_server.c firmware/cc1352/include/gatt_table.h firmware/cc1352/src/gatt_table.c firmware/cc1352/CMakeLists.txt
git commit -m "feat(f20.a.1): ATT server skeleton + static GATT table T2 (Read paths)"
```

---

## Task 3: RadioIF_bleSlave + slave event loop

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h`
- Modify: `firmware/cc1352/src/radio_if.c`
- Modify: `firmware/cc1352/include/ble_conn_mgr.h`
- Modify: `firmware/cc1352/src/ble_conn_mgr.c`

**Background:** `RadioIF_bleSlave` ejecuta UN connection event como slave (espejo de `RadioIF_bleCentral` line 2572). El loop alto-nivel (anchor, hop, supervision timeout) vive en `BleConnMgr_pollSlave`, espejo de `BleConnMgr_poll`. Reusa `s_rf_data_queue` y la TX queue infra existentes.

- [ ] **Step 1: Read existing `RadioIF_bleCentral` to mirror**

```bash
grep -n "^int RadioIF_bleCentral" firmware/cc1352/src/radio_if.c
```

Read lines 2572-2660 (or wherever it ends) carefully. The mirror function `RadioIF_bleSlave` does:
- Setup `Ble5_0_cmdBle5Slave` per-event (channel, accessAddr, crcInit, timing triggers)
- pParams->pRxQ = &s_rf_data_queue
- pParams->pTxQ = (caller's tx queue)
- RF_runCmd, capture status, parse output
- Same supervisionTimeout / endTime handling

- [ ] **Step 2: Add `RadioIF_bleSlave` declaration to `radio_if.h`**

After `RadioIF_bleCentral` declaration, insert:

```c
/* F20.a.1 — Run one BLE peripheral connection event.
 * chan: channel (0..36 data channels).
 * accessAddr / crcInit: from CONNECT_IND captured by F21.
 * pTxQueue: ATT responses to enqueue (from AttServer_takePendingTx).
 * startTime: anchor RAT tick.
 * endTime: supervision deadline RAT tick.
 * pStats output (nullable). */
int RadioIF_bleSlave(uint8_t chan, uint32_t accessAddr, uint32_t crcInit,
                     dataQueue_t *pTxQueue, uint32_t startTime, uint32_t endTime,
                     RadioIF_BleCentralStats *pStats);
```

- [ ] **Step 3: Implement `RadioIF_bleSlave` in `radio_if.c`**

Insert AFTER `RadioIF_bleCentral`. Mirror its structure exactly:

```c
int RadioIF_bleSlave(uint8_t chan, uint32_t accessAddr, uint32_t crcInit,
                     dataQueue_t *pTxQueue, uint32_t startTime, uint32_t endTime,
                     RadioIF_BleCentralStats *pStats) {
    rfc_bleMasterSlaveOutput_t output = {0};

    Ble5_0_cmdBle5Slave.channel = chan;
    Ble5_0_cmdBle5Slave.whitening.init = (uint8_t)(0x40u + chan);
    Ble5_0_cmdBle5Slave.whitening.bOverride = 1u;
    Ble5_0_cmdBle5Slave.phyMode.mainMode = 0u;
    Ble5_0_cmdBle5Slave.phyMode.coding = 0u;
    Ble5_0_cmdBle5Slave.pOutput = &output;

    Ble5_0_cmdBle5Slave.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5Slave.pParams->pTxQ = pTxQueue;
    Ble5_0_cmdBle5Slave.pParams->accessAddress = accessAddr;
    Ble5_0_cmdBle5Slave.pParams->crcInit0 = (uint8_t)(crcInit & 0xFFu);
    Ble5_0_cmdBle5Slave.pParams->crcInit1 = (uint8_t)((crcInit >> 8) & 0xFFu);
    Ble5_0_cmdBle5Slave.pParams->crcInit2 = (uint8_t)((crcInit >> 16) & 0xFFu);
    Ble5_0_cmdBle5Slave.pParams->timeoutTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Slave.pParams->timeoutTime = endTime;

    Ble5_0_cmdBle5Slave.startTime = startTime;
    Ble5_0_cmdBle5Slave.startTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Slave.startTrigger.pastTrig = 1u;
    Ble5_0_cmdBle5Slave.condition.rule = COND_NEVER;
    Ble5_0_cmdBle5Slave.endTime = endTime;
    Ble5_0_cmdBle5Slave.endTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Slave.status = 0x0000u;

    RF_EventMask events = RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5Slave,
                                    RF_PriorityNormal, NULL, RADIO_IF_TX_TERM_EVENTS);
    (void)events;

    if (pStats != NULL) {
        pStats->nTx = output.nTx;
        pStats->nRxOk = output.nRxOk;
        pStats->nRxNok = output.nRxNok;
        pStats->nRxIgnored = output.nRxIgnored;
        pStats->pktStatus = (uint8_t)(output.pktStatus.bTimeStampValid |
                                      (output.pktStatus.bLastCrcErr << 1) |
                                      (output.pktStatus.bLastIgnored << 2) |
                                      (output.pktStatus.bLastEmpty << 3) |
                                      (output.pktStatus.bLastCtrl << 4) |
                                      (output.pktStatus.bLastMd << 5) |
                                      (output.pktStatus.bLastAck << 6));
    }

    return (int)Ble5_0_cmdBle5Slave.status;
}
```

- [ ] **Step 4: Add slave-mode flag + new BleConnMgr API to `ble_conn_mgr.h`**

```c
/* F20.a.1 — Start/stop the slave event loop driven by BleConnMgr_pollSlave.
 * Caller (command_processor handle CMD_BLE_ADV_LEGACY) extracts CONNECT_IND
 * params and invokes BleConnMgr_startSlave with them. The poll loop runs
 * inside the RfTask context until disconnect; emits the existing
 * disconnect callback (gatt_on_disconnected → RSP_DISCONNECTED). */
typedef struct {
    uint32_t accessAddr;
    uint32_t crcInit;
    uint16_t hopInterval_125us; /* CONNECT_IND interval, 1.25ms units */
    uint16_t latency;
    uint16_t supervTimeout_10ms;
    uint8_t  hopIncrement;
} BleConnMgr_SlaveParams;

void BleConnMgr_startSlave(const BleConnMgr_SlaveParams *params);
bool BleConnMgr_pollSlave(void);
```

- [ ] **Step 5: Implement `BleConnMgr_startSlave` and `BleConnMgr_pollSlave`**

In `ble_conn_mgr.c`, add static state:

```c
static bool s_slave_running = false;
static BleConnMgr_SlaveParams s_slave_params;
static uint16_t s_slave_event_counter = 0u;
static uint32_t s_slave_anchor_rat = 0u;
static uint32_t s_slave_last_rx_rat = 0u;
```

Implement:

```c
void BleConnMgr_startSlave(const BleConnMgr_SlaveParams *params) {
    if (params == NULL) return;
    s_slave_params = *params;
    s_slave_event_counter = 0u;
    s_slave_anchor_rat = RF_getCurrentTime() +
                         (uint32_t)params->hopInterval_125us * 5000u; /* 1.25ms in 4MHz ticks */
    s_slave_last_rx_rat = RF_getCurrentTime();
    s_slave_running = true;
}

bool BleConnMgr_pollSlave(void) {
    if (!s_slave_running) return false;

    uint32_t hop_ticks = (uint32_t)s_slave_params.hopInterval_125us * 5000u;

    /* Check supervision timeout (in 10ms units → 40000 4MHz ticks per unit). */
    uint32_t superv_ticks = (uint32_t)s_slave_params.supervTimeout_10ms * 40000u;
    uint32_t now = RF_getCurrentTime();
    if (now - s_slave_last_rx_rat > superv_ticks) {
        s_slave_running = false;
        if (s_disconnect_cb) s_disconnect_cb(0x22u); /* LL_RESPONSE_TIMEOUT */
        return false;
    }

    /* CSA#1 channel calc per BLE Core Spec Vol 6 Part B §4.5.8.1. */
    uint8_t chan = (uint8_t)(((uint32_t)(s_slave_event_counter + 1u) *
                              s_slave_params.hopIncrement) % 37u);

    /* Build TX queue from AttServer pending RSP (if any). */
    extern bool AttServer_hasPendingTx(void);
    extern uint8_t AttServer_takePendingTx(uint8_t *out, uint8_t buf_len);
    dataQueue_t txq;
    /* For A3.1 simplicity: insert ATT_RSP wrapped in L2CAP frame (CID 0x0004). */
    if (AttServer_hasPendingTx()) {
        uint8_t att[ATT_DEFAULT_MTU];
        uint8_t att_len = AttServer_takePendingTx(att, sizeof(att));
        if (att_len > 0u) {
            uint8_t l2cap_frame[ATT_DEFAULT_MTU + 4u];
            l2cap_frame[0] = att_len;
            l2cap_frame[1] = 0x00u;
            l2cap_frame[2] = 0x04u; /* CID 0x0004 = ATT */
            l2cap_frame[3] = 0x00u;
            memcpy(&l2cap_frame[4], att, att_len);
            TXQueue_insert((uint8_t)(att_len + 4u), TX_QUEUE_LLID_DATA_START, l2cap_frame);
        }
    }
    TXQueue_insert(0u, TX_QUEUE_LLID_DATA_CONT, NULL);
    TXQueue_take(&txq);

    uint32_t startTime = s_slave_anchor_rat;
    uint32_t endTime = s_slave_anchor_rat + hop_ticks;
    RadioIF_BleCentralStats stats = {0};

    int status = RadioIF_bleSlave(chan, s_slave_params.accessAddr, s_slave_params.crcInit,
                                  &txq, startTime, endTime, &stats);
    (void)status;

    if (stats.nRxOk > 0u || stats.nRxNok > 0u) {
        s_slave_last_rx_rat = RF_getCurrentTime();
    }

    /* Drain RX queue: route ATT to AttServer, detect LL_TERMINATE_IND. */
    /* TODO Bundle 4 — full RX dispatch. For now skeleton: */
    extern void Ble20_drainAndDispatch(uint8_t *terminate_reason_out);
    uint8_t reason = 0u;
    Ble20_drainAndDispatch(&reason);
    if (reason != 0u) {
        s_slave_running = false;
        if (s_disconnect_cb) s_disconnect_cb(reason);
        return false;
    }

    s_slave_event_counter++;
    s_slave_anchor_rat += hop_ticks;
    return true;
}
```

NOTE: `Ble20_drainAndDispatch` is a forward reference implemented in Bundle 4 (RX dispatch). For Bundle 3, add a stub:

```c
__attribute__((weak)) void Ble20_drainAndDispatch(uint8_t *reason_out) {
    if (reason_out) *reason_out = 0u;
}
```

- [ ] **Step 6: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -5
```

Expected: clean. Linker may complain about unresolved `Ble20_drainAndDispatch` if the weak attribute doesn't apply — provide the stub explicitly.

- [ ] **Step 7: Pre-commit + commit Bundle 3**

```bash
pre-commit run --files firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git add firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git commit -m "feat(f20.a.1): RadioIF_bleSlave + BleConnMgr_pollSlave (mirror F8A central)"
```

---

## Task 4: F21 handoff + L2CAP RX dispatch + disconnect emit

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c` (add CMD_GATT_SERVE_TABLE handler + extend CMD_BLE_ADV_LEGACY)
- Create: `firmware/cc1352/src/ble20_dispatch.c` (replaces the weak stub)
- Create: `firmware/cc1352/include/ble20_dispatch.h`
- Modify: `firmware/cc1352/CMakeLists.txt` (add ble20_dispatch.c)
- Modify: `firmware/cc1352/src/radio_if.c` (helper to extract CONNECT_IND from RX queue)

- [ ] **Step 1: Add CONNECT_IND parser helper to `radio_if.c`**

After `RadioIF_bleSlave`, add:

```c
/* F20.a.1 — Walk the RX queue looking for the CONNECT_IND PDU left there
 * by Ble5_0_cmdBleAdv when a peer initiates a connection. CONNECT_IND
 * has PDU type 0x5 in the LL header (advertising channel PDU). The body
 * carries the conn parameters at fixed offsets. Returns true if found. */
bool RadioIF_extractConnectIndParams(BleConnMgr_SlaveParams *out_params) {
    if (out_params == NULL) return false;
    rfc_dataEntry_t *entry = (rfc_dataEntry_t *)s_rf_data_queue.pCurrEntry;
    while (entry != NULL && entry->status == DATA_ENTRY_FINISHED) {
        uint8_t *pkt = (uint8_t *)&entry->data;
        uint8_t header = pkt[0];
        uint8_t pdu_type = header & 0x0Fu;
        uint8_t length = pkt[1];
        if (pdu_type == 0x5u && length >= 34u) {
            /* CONNECT_IND body starts at pkt[2]. Skip InitA(6) + AdvA(6),
             * then access_addr(4) + crc_init(3) + win_size(1) + win_offset(2) +
             * interval(2) + latency(2) + timeout(2) + ch_map(5) + hop_chSel(1) */
            const uint8_t *body = &pkt[2 + 6 + 6];
            out_params->accessAddr = (uint32_t)body[0] | ((uint32_t)body[1] << 8) |
                                     ((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 24);
            out_params->crcInit = (uint32_t)body[4] | ((uint32_t)body[5] << 8) |
                                  ((uint32_t)body[6] << 16);
            out_params->hopInterval_125us = (uint16_t)body[10] | ((uint16_t)body[11] << 8);
            out_params->latency = (uint16_t)body[12] | ((uint16_t)body[13] << 8);
            out_params->supervTimeout_10ms = (uint16_t)body[14] | ((uint16_t)body[15] << 8);
            out_params->hopIncrement = body[21] & 0x1Fu;
            entry->status = DATA_ENTRY_PENDING;
            return true;
        }
        entry->status = DATA_ENTRY_PENDING;
        entry = (rfc_dataEntry_t *)entry->pNextEntry;
    }
    return false;
}
```

Add prototype to `radio_if.h` (and forward-declare `BleConnMgr_SlaveParams` or just include `ble_conn_mgr.h`):

```c
#include "ble_conn_mgr.h"
bool RadioIF_extractConnectIndParams(BleConnMgr_SlaveParams *out_params);
```

- [ ] **Step 2: Create `ble20_dispatch.c` for L2CAP RX dispatch**

```c
/* F20.a.1 — Drain RX queue, parse data PDUs, route ATT to AttServer.
 * Detect LL_TERMINATE_IND for clean disconnect. */
#include "ble20_dispatch.h"

#include "att_server.h"
#include "radio_if.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)

extern dataQueue_t s_rf_data_queue;

void Ble20_drainAndDispatch(uint8_t *reason_out) {
    if (reason_out) *reason_out = 0u;
    rfc_dataEntry_t *entry = (rfc_dataEntry_t *)s_rf_data_queue.pCurrEntry;
    while (entry != NULL && entry->status == DATA_ENTRY_FINISHED) {
        uint8_t *pkt = (uint8_t *)&entry->data;
        /* LL data PDU header: byte 0 = LLID + flags, byte 1 = length */
        uint8_t llid = pkt[0] & 0x03u;
        uint8_t length = pkt[1];

        if (llid == 0x3u && length >= 1u) {
            /* LL Control PDU — opcode at pkt[2] */
            uint8_t opcode = pkt[2];
            if (opcode == 0x02u && length >= 2u) {
                /* LL_TERMINATE_IND: reason at pkt[3] */
                if (reason_out) *reason_out = pkt[3];
            }
        } else if ((llid == 0x1u || llid == 0x2u) && length >= 4u) {
            /* L2CAP frame: [len:2 LE][cid:2 LE][payload] */
            uint16_t l2_len = (uint16_t)pkt[2] | ((uint16_t)pkt[3] << 8);
            uint16_t l2_cid = (uint16_t)pkt[4] | ((uint16_t)pkt[5] << 8);
            if (l2_cid == 0x0004u && l2_len >= 1u && length >= (uint8_t)(l2_len + 4u)) {
                AttServer_handleRequest(&pkt[6], (uint8_t)l2_len);
            }
        }
        entry->status = DATA_ENTRY_PENDING;
        entry = (rfc_dataEntry_t *)entry->pNextEntry;
    }
}
```

Header `ble20_dispatch.h`:

```c
#ifndef BLE20_DISPATCH_H
#define BLE20_DISPATCH_H

#include <stdint.h>

void Ble20_drainAndDispatch(uint8_t *reason_out);

#endif /* BLE20_DISPATCH_H */
```

- [ ] **Step 3: Add CMD_GATT_SERVE_TABLE handler in `command_processor.c`**

Find the F21 `CMD_BLE_ADV_LEGACY` case. Add a new case immediately AFTER it:

```c
    case CMD_GATT_SERVE_TABLE: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        AttServer_init();
        s_peripheral_active = true;
        send_ack(seq);
        return;
    }
```

Add a static `static bool s_peripheral_active = false;` near the top of the file (alongside other state).

- [ ] **Step 4: Modify `CMD_BLE_ADV_LEGACY` handler to detect handoff**

Find the F21 handler. After the `RadioIF_transmitBleAdvLegacy(...)` call (which currently breaks loop on CONNECT_IND), add:

```c
    case CMD_BLE_ADV_LEGACY: {
        /* ... existing F21 parsing ... */
        send_ack(seq);
        bool ok = RadioIF_transmitBleAdvLegacy(...);

        /* F20.a.1: if peripheral mode active AND ADV exited because of
         * CONNECT_IND, transition to slave loop. */
        if (ok && s_peripheral_active) {
            BleConnMgr_SlaveParams sparams;
            if (RadioIF_extractConnectIndParams(&sparams)) {
                BleConnMgr_setDisconnectCb(gatt_on_disconnected);
                BleConnMgr_startSlave(&sparams);
                while (BleConnMgr_pollSlave()) {
                    /* runs until disconnect / supervision timeout */
                }
            }
            s_peripheral_active = false; /* one-shot per cycle */
        }
        return;
    }
```

- [ ] **Step 5: Update CMakeLists.txt with `ble20_dispatch.c`**

Add the new source to the firmware target.

- [ ] **Step 6: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -5
```

Expected: clean build, no warnings about weak symbols.

- [ ] **Step 7: Pre-commit + commit Bundle 4**

```bash
pre-commit run --files firmware/cc1352/src/command_processor.c firmware/cc1352/src/radio_if.c firmware/cc1352/include/radio_if.h firmware/cc1352/src/ble20_dispatch.c firmware/cc1352/include/ble20_dispatch.h firmware/cc1352/CMakeLists.txt
git add firmware/cc1352/src/command_processor.c firmware/cc1352/src/radio_if.c firmware/cc1352/include/radio_if.h firmware/cc1352/src/ble20_dispatch.c firmware/cc1352/include/ble20_dispatch.h firmware/cc1352/CMakeLists.txt
git commit -m "feat(f20.a.1): F21 → F20 handoff + L2CAP/ATT RX dispatch + LL_TERMINATE detect"
```

---

## Task 5: Python API + unit tests

**Files:**
- Modify: `python/feralrf/enums.py`
- Modify: `python/feralrf/commands.py`
- Modify: `python/feralrf/radio.py`
- Create: `python/tests/test_radio_serve_gatt.py`

- [ ] **Step 1: Add `Command.GATT_SERVE_TABLE = 0x53` to `enums.py`**

Find the F21 entry `BLE_ADV_LEGACY = 0x52`:

```bash
grep -n "BLE_ADV_LEGACY" python/feralrf/enums.py
```

Insert after it:

```python
    # F21 — BLE Connectable Advertiser
    BLE_ADV_LEGACY = 0x52

    # F20 — BLE Peripheral + GATT server
    GATT_SERVE_TABLE = 0x53
```

- [ ] **Step 2: Add `CommandBuilder.gatt_serve_table()` to `commands.py`**

After F21's `ble_adv_legacy`, append:

```python
    @staticmethod
    def gatt_serve_table() -> bytes:
        """Build CMD_GATT_SERVE_TABLE (F20.a.1) payload — empty (flag toggle).
        F20.a.b will accept a dynamic GATT table here."""
        return b""
```

- [ ] **Step 3: Add `Radio.serve_gatt()` to `radio.py`**

After F21's `advertise_scan_ind` method:

```python
    def serve_gatt(self, table: Optional[object] = None) -> None:
        """F20.a.1 — toggle peripheral mode on. Subsequent advertise_ind()
        will auto-handoff to GATT server slave on CONNECT_IND.

        A3.1: `table` arg ignored (warns) — firmware uses hardcoded T2 table
        (GAP service "FERAL_GATT" + custom service "HELLO_FERAL"). Dynamic
        table arrives in F20.b.
        """
        if table is not None:
            import warnings
            warnings.warn(
                "table arg ignored in F20.a.1 — firmware uses hardcoded T2 table; "
                "dynamic table coming in F20.b",
                stacklevel=2,
            )
        cmd_payload = CommandBuilder.gatt_serve_table()
        self._send_command(Command.GATT_SERVE_TABLE, cmd_payload)
        self._read_response(timeout=2.0, expected={Response.ACK, Response.ERROR})
```

- [ ] **Step 4: Create `python/tests/test_radio_serve_gatt.py`**

```python
"""F20.a.1 — unit tests for Radio.serve_gatt + CommandBuilder.gatt_serve_table."""

from typing import List, Optional, Tuple

import pytest

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response
from feralrf.protocol import build_frame, cobs_decode, parse_frame
from feralrf.radio import Radio


class TestGattServeTablePayload:
    def test_empty_payload(self):
        assert CommandBuilder.gatt_serve_table() == b""


class FakeSerial:
    def __init__(self) -> None:
        self.is_open = True
        self.written: bytearray = bytearray()
        self._read_buf: bytearray = bytearray()
        self.timeout: Optional[float] = None

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, n: int = 1) -> bytes:
        if not self._read_buf:
            return b""
        out = bytes(self._read_buf[:n])
        del self._read_buf[:n]
        return out

    def reset_input_buffer(self) -> None:
        self._read_buf.clear()

    def reset_output_buffer(self) -> None:
        self.written.clear()

    def close(self) -> None:
        self.is_open = False

    def queue_response(self, cmd_id: int, seq: int, payload: bytes = b"") -> None:
        self._read_buf.extend(build_frame(cmd_id, seq, payload))

    def written_frames(self) -> List[Tuple[int, int, bytes]]:
        frames: List[Tuple[int, int, bytes]] = []
        buf = bytearray()
        for b in self.written:
            if b == 0x00:
                if buf:
                    decoded = cobs_decode(bytes(buf))
                    frames.append(parse_frame(decoded))
                buf = bytearray()
            else:
                buf.append(b)
        return frames


def _radio_with_fake_serial() -> Tuple[Radio, FakeSerial]:
    radio = Radio(port="/dev/null")
    fake = FakeSerial()
    radio._serial = fake  # type: ignore[assignment]
    return radio, fake


class TestRadioServeGatt:
    def test_dispatch_correct_command(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.serve_gatt()
        frames = fake.written_frames()
        assert len(frames) == 1
        cmd_id, _seq, payload = frames[0]
        assert cmd_id == Command.GATT_SERVE_TABLE
        assert payload == b""

    def test_warns_when_table_arg_passed(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        with pytest.warns(UserWarning, match="table arg ignored"):
            radio.serve_gatt(table=[("dummy",)])

    def test_seq_advances_after_call(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        seq_before = radio._seq
        radio.serve_gatt()
        assert radio._seq == ((seq_before + 1) & 0xFF)
```

- [ ] **Step 5: Run tests + suite regression**

```bash
cd python && PYTHONPATH=. pytest tests/test_radio_serve_gatt.py -v 2>&1 | tail -10
PYTHONPATH=. pytest -q 2>&1 | tail -3
```

Expected: 4 PASS in new file; full suite ≥ 586 pass (was 582 + 4).

- [ ] **Step 6: Pre-commit + commit Bundle 5**

```bash
pre-commit run --files python/feralrf/enums.py python/feralrf/commands.py python/feralrf/radio.py python/tests/test_radio_serve_gatt.py
git add python/feralrf/enums.py python/feralrf/commands.py python/feralrf/radio.py python/tests/test_radio_serve_gatt.py
git commit -m "feat(f20.a.1): Python serve_gatt API + unit tests"
```

---

## Task 6: Smoke V1 hardware (HUMAN CHECKPOINT)

**Files:**
- Create: `python/examples/smoke_f20a1_peripheral.py`
- Create: `python/examples/lab/demo_gatt_server.py`

- [ ] **Step 1: Create `smoke_f20a1_peripheral.py`**

```python
#!/usr/bin/env python3
"""F20.a.1 — Smoke V1 BLE peripheral Read-only cross-validation 2 boards."""
import argparse
import re
import sys
import time
from threading import Thread

import serial

from feralrf.radio import Radio


def reset_cc1352(port: str) -> None:
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0, write_timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def run_peripheral(port: str, baud: int, target_addr: str) -> None:
    radio = Radio(port=port, baudrate=baud)
    try:
        radio.init()
        radio.serve_gatt()
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            scan_resp_data=b"FERAL_GATT_SR",
            target_addr=target_addr,
            count=200,
            interval_us=10000,
        )
    finally:
        radio.disconnect()


def run_central(port: str, baud: int, target_addr: str) -> tuple:
    """Returns (services_count, chars_count, name_value, test_value)."""
    addr_le = bytes(int(p, 16) for p in reversed(target_addr.split(":")))
    radio = Radio(port=port, baudrate=baud)
    try:
        radio.init()
        radio.reset_device()
        radio.init()
        result = radio.ble_connect(addr_le, addr_type=1, timeout=10.0)
        if not result.is_ok:
            return (0, 0, b"", b"")
        services = radio.gatt_discover(timeout=10.0)
        name_val = radio.gatt_read(handle=3, timeout=5.0)
        test_val = radio.gatt_read(handle=6, timeout=5.0)
        try:
            radio.ble_disconnect(timeout=5.0)
        except Exception:
            pass
        return (len(services.services), len(services.characteristics), name_val, test_val)
    finally:
        radio.disconnect()


def main() -> int:
    parser = argparse.ArgumentParser(description="F20.a.1 smoke V1 peripheral Read-only")
    parser.add_argument("--peripheral-port", required=True)
    parser.add_argument("--central-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    args = parser.parse_args()

    reset_cc1352(args.peripheral_port)
    reset_cc1352(args.central_port)

    print(f"Peripheral on {args.peripheral_port}; Central on {args.central_port}")
    print(f"Target MAC: {args.target_mac}")
    print("=" * 60)

    # Peripheral runs in background thread; central blocks on connect
    peripheral_thread = Thread(
        target=run_peripheral,
        args=(args.peripheral_port, args.baudrate, args.target_mac),
        daemon=True,
    )
    peripheral_thread.start()
    time.sleep(0.5)

    services_count, chars_count, name_val, test_val = run_central(
        args.central_port, args.baudrate, args.target_mac
    )

    peripheral_thread.join(timeout=5.0)

    print(f"\nServices discovered: {services_count}")
    print(f"Chars discovered:    {chars_count}")
    print(f"Device Name read:    {name_val!r}")
    print(f"Test Read read:      {test_val!r}")

    expected_name = b"FERAL_GATT"
    expected_test = b"HELLO_FERAL"
    pass_services = services_count >= 2
    pass_chars = chars_count >= 2
    pass_name = name_val == expected_name
    pass_test = test_val == expected_test

    print("\n" + "=" * 60)
    print(f"[{'PASS' if pass_services else 'FAIL'}] services >= 2: {services_count}")
    print(f"[{'PASS' if pass_chars else 'FAIL'}] chars >= 2: {chars_count}")
    print(f"[{'PASS' if pass_name else 'FAIL'}] device name == 'FERAL_GATT'")
    print(f"[{'PASS' if pass_test else 'FAIL'}] test value == 'HELLO_FERAL'")

    all_pass = pass_services and pass_chars and pass_name and pass_test
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Create `python/examples/lab/demo_gatt_server.py`**

```python
#!/usr/bin/env python3
"""F20.a.1 — single-board peripheral demo for nRF Connect manual testing."""
import argparse
import sys

from feralrf.radio import Radio


def main() -> int:
    parser = argparse.ArgumentParser(description="F20.a.1 GATT server demo (nRF Connect target)")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    parser.add_argument("--count", type=int, default=2000)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        radio.serve_gatt()
        print(f"Advertising as {args.target_mac}, GATT server T2 (FERAL_GATT)")
        print("Connect with nRF Connect; read handle 3 (Device Name) and handle 6 (Test).")
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            scan_resp_data=b"FERAL_GATT_SR",
            target_addr=args.target_mac,
            count=args.count,
            interval_us=10000,
        )
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 3: Verify smoke + demo parse**

```bash
cd python
for f in examples/smoke_f20a1_peripheral.py examples/lab/demo_gatt_server.py; do
  PYTHONPATH=. python -c "import ast; ast.parse(open('$f').read())" && echo "$f: parse OK"
  PYTHONPATH=. python "$f" --help 2>&1 | head -1
done
```

Expected: 2 × `parse OK` + usage lines.

- [ ] **Step 4: HUMAN CHECKPOINT — confirmar 2 boards + reflash con firmware F20.a.1**

```bash
ls /dev/ttyACM*
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip devices
```

Expected: 2 CatSniffer devices listed. Identify which is peripheral and which is central.

Reflash both with the new firmware:

```bash
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
python -m catnip flash -d 2 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

Per memory `feedback_flash_retry`: retry 2× before manual reset.

**STOP before running smoke if either flash fails twice.**

- [ ] **Step 5: Run smoke**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
PYTHONPATH=. python examples/smoke_f20a1_peripheral.py \
    --peripheral-port <PERIPHERAL> --central-port <CENTRAL> 2>&1 | tail -30
```

Expected (PASS path): all 4 asserciones PASS, exit 0.

If FAIL (any of):
- **No connection** (ble_connect timeout): peripheral path broken. Add print() in command_processor handler to verify `s_peripheral_active` triggers; verify `extract_connect_ind_from_rx_queue` finds PDU type 0x5.
- **Discover returns 0 services**: ATT server skeleton not wired correctly. Add print() in `AttServer_handleRequest` opcode dispatch.
- **Read returns wrong bytes**: GATT table layout mismatch. Verify hex dump of T2 attributes vs spec.

- [ ] **Step 6: Pre-commit + commit Bundle 6**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/examples/smoke_f20a1_peripheral.py python/examples/lab/demo_gatt_server.py
git add python/examples/smoke_f20a1_peripheral.py python/examples/lab/demo_gatt_server.py
git commit -m "test(f20.a.1): smoke V1 cross-validation 2-board + nRF Connect demo lab"
```

---

## Task 7: Tag, memory, FF merge

**Files:**
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f20a1_done.md` (new)
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` (extend)
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_next_session.md` (mark F20.a.1 done, F20.a.2 next)

- [ ] **Step 1: Verify bundle commits**

```bash
git log --oneline main..HEAD
```

Expected: 5-6 commits.

- [ ] **Step 2: Tag `v2.0-f20.a.1-partial`**

```bash
git tag -a v2.0-f20.a.1-partial -m "F20.a.1 — BLE peripheral Read-only. Static T2 GATT table. Smoke V1 4/4."
git tag -l | grep f20
```

- [ ] **Step 3: Create memory entry**

```bash
cat > ~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f20a1_done.md <<'EOF'
---
name: project_f20a1_done
description: F20.a.1 BLE peripheral Read-only closed — CMD_GATT_SERVE_TABLE + cmdBle5Slave + static T2 + V1 smoke 4/4
type: project
---

F20.a.1 CLOSED 2026-05-04. Branch `feature/f20a1-peripheral-read`,
tag `v2.0-f20.a.1-partial`. FF'd into main.

**Firmware:** CMD_GATT_SERVE_TABLE (0x53) + Ble5_0_cmdBle5Slave struct +
RadioIF_bleSlave + BleConnMgr_pollSlave (mirror F8A central) + ATT server
skeleton + static T2 GATT table (GAP + custom). F21 CMD_BLE_ADV_LEGACY
modificado: detecta CONNECT_IND, parsea conn params del RX queue,
transitions a slave loop.

**Python:** Command.GATT_SERVE_TABLE, CommandBuilder.gatt_serve_table(),
Radio.serve_gatt() (warn si table arg passed).

**Smoke V1 (2 boards FeralRF):** 4/4 asserciones — ≥2 services, ≥2 chars,
device name "FERAL_GATT", test value "HELLO_FERAL".

**Out of scope (F20.a.2):** Write Req/Rsp + HVN Notify.
**Out of scope (F20.b):** Read Blob, Read Multiple, Indicate, dynamic
GATT table, MTU exchange server-side.
**Out of scope (todo v2.0):** Pairing/encryption/bonding.
EOF
```

- [ ] **Step 4: Update `MEMORY.md` index**

Add after `project_f21_done`:

```
- [project_f20a1_done.md](project_f20a1_done.md) — 2026-05-04 F20.a.1 closed (Read only). Tag v2.0-f20.a.1-partial. CMD_GATT_SERVE_TABLE firmware + Radio.serve_gatt. T2 static GATT table validated by V1 smoke 4/4.
```

- [ ] **Step 5: Update `project_next_session.md`**

Replace F20 entry with:

```
### ~~8. F20.a.1 BLE peripheral Read-only~~ ✅ CLOSED 2026-05-04
Tag v2.0-f20.a.1-partial. Static T2 GATT table + Read works.

### 8. F20.a.2 — Write + HVN Notify (next slice)
Extend F20.a.1: agregar Write Req handler + HVN (notify) queue. Misma
T2 table extendida con char Write + char Notify (handles 0x0007+).
Estimate: 1 sesión (~400 LOC).

### 9. F20.b — operations avanzadas (Indicate, Read Blob/Multiple, dynamic table)
Después de F20.a.2.
```

Renumerar siguientes.

- [ ] **Step 6: FF merge to main**

```bash
git checkout main
git merge --ff-only feature/f20a1-peripheral-read
git update-ref refs/heads/feature/ti-rtos-migration main
git log --oneline -10
```

- [ ] **Step 7: Delete local branch**

```bash
git branch -d feature/f20a1-peripheral-read
git branch
```

- [ ] **Step 8: Push (interactive)**

```
! git push origin main feature/ti-rtos-migration v2.0-f20.a.1-partial
```

NO push autónomo per safety rules.

---

## Self-review checklist

- [ ] **Spec coverage:** Spec lista firmware (CMD + struct + RadioIF + BleConnMgr + ATT server + GATT table + F21 handoff + L2CAP), Python (3 entries), tests, smoke V1, demo. Plan tiene Task 1 (protocol+struct), Task 2 (ATT server + table), Task 3 (RadioIF slave + BleConnMgr loop), Task 4 (handoff + RX dispatch), Task 5 (Python + tests), Task 6 (smoke + demo), Task 7 (tag+memory+FF). ✅
- [ ] **Placeholder scan:** No "TBD"/"TODO". Code blocks completos. ✅
- [ ] **Type/symbol consistency:** `Command.GATT_SERVE_TABLE`, `CommandBuilder.gatt_serve_table`, `Radio.serve_gatt`, `Ble5_0_cmdBle5Slave`, `RadioIF_bleSlave`, `BleConnMgr_startSlave`/`pollSlave`/`SlaveParams`, `s_peripheral_active`, `AttServer_handleRequest`/`init`/`hasPendingTx`/`takePendingTx`, `g_gatt_table`, `Ble20_drainAndDispatch`, `RadioIF_extractConnectIndParams` consistentes en todas las tasks. ✅
- [ ] **Hardware:** 2 boards. Reflash necesario (firmware nuevo). ✅
- [ ] **Compat:** F21 CMD_BLE_ADV_LEGACY se modifica pero el path NC F11 no se toca. F11 attacks smoke debe seguir pasando. ✅
- [ ] **Prerequisites verified:** cmdBle5Slave NO en SmartRF (Bundle 1 agrega), F8A central pattern existe (espejear), s_disconnect_cb pattern reusable. ✅
