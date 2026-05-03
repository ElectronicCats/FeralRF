# F8c — MTU Exchange + Read by UUID + Disconnect Reason Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three GATT-layer features on top of the F8b Track A foundation: explicit ATT MTU Exchange (host-triggered), explicit Read by UUID (find a characteristic value without full discovery), and surface the BT-spec disconnect reason byte to the host via an async response.

**Architecture:** All three features extend existing modules — no new files for the firmware logic. The MTU and Read-by-UUID flows reuse the AttClient state machine and existing GATT callback wiring (`gatt_on_done` → `RSP_GATT_DONE`); both add one new public API and one new callback. Disconnect reason adds a new `BleConnMgr_DisconnectCb` that command_processor registers once at boot and emits as `RSP_DISCONNECTED` whenever the connection terminates (LL_TERMINATE_IND, supervision timeout, host-initiated). New protocol IDs are confined to the 0x4A–0x4B (CMD) and 0xB0–0xB2 (RSP) ranges, both currently unused.

**Tech Stack:** C (TI-RTOS 7, GCC arm-none-eabi), Python 3.11+, pytest, COBS+CRC16 framing.

**Branch:** `feature/ti-rtos-migration` (HEAD = 482a9b1). Do **NOT** modify `firmware/cc1352/include/radio_if.h` — it has WIP whitespace that must stay unstaged.

---

## Protocol ID Allocations (referenced throughout)

| Symbol | Value | Purpose |
|---|---|---|
| `CMD_GATT_EXCHANGE_MTU` | `0x4A` | Host → device: trigger ATT MTU exchange |
| `CMD_GATT_READ_BY_UUID` | `0x4B` | Host → device: ATT Read by Type with arbitrary UUID16 |
| `RSP_GATT_MTU` | `0xB0` | Device → host: negotiated MTU (sync to seq) |
| `RSP_GATT_ATTRIBUTE` | `0xB1` | Device → host: one (handle, value) pair from Read by UUID |
| `RSP_DISCONNECTED` | `0xB2` | Device → host: async, [reason:1] |

`RSP_GATT_DONE` (0xA5, existing) is reused as the terminator for both new flows — same pattern as `gatt_discover`.

---

## File Structure

**Modified:**
- `firmware/cc1352/include/att_client.h` — add states, callbacks, public APIs
- `firmware/cc1352/src/att_client.c` — handlers + start funcs + new state in poll/handle_*
- `firmware/cc1352/include/ble_conn_mgr.h` — add disconnect callback typedef + setter
- `firmware/cc1352/src/ble_conn_mgr.c` — extract reason from LL_TERMINATE_IND, fire callback
- `firmware/cc1352/src/command_processor.c` — new CMDs + new RSPs + DC handler wiring
- `python/feralrf/enums.py` — add Command + Response enum members
- `python/feralrf/commands.py` — add CommandBuilder.gatt_exchange_mtu / gatt_read_by_uuid
- `python/feralrf/radio.py` — add Radio.gatt_exchange_mtu / gatt_read_by_uuid / read_disconnect_events
- `python/tests/test_gatt_api.py` — extend existing test file (do not create new)

**Created:**
- `python/tests/test_disconnect_events.py` — focused tests for the disconnect iterator
- `python/scripts/smoke_f8c.py` — live-board smoke (manual harness)

**Untouched:** `radio_if.{c,h}`, anything under `firmware/cc1352/sdk/`, `python/feralrf/_responses.py` (RSP_DISCONNECTED is parsed inline by Radio).

---

## Pre-Flight (Task 0)

- [ ] **Step 0.1: Verify HEAD and clean working tree (except known WIP)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git status --short
git rev-parse --short HEAD
```

Expected: HEAD = `482a9b1`. Only `M firmware/cc1352/include/radio_if.h` may appear in status. Anything else means you are not on the right base — STOP and ask.

- [ ] **Step 0.2: Confirm baseline build is green**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352
mkdir -p build && cd build
cmake .. >/dev/null
make -j$(nproc) 2>&1 | tail -20
```

Expected: build succeeds, produces `feralrf_cc1352.hex`. No warnings introduced should be tolerated by the time the plan is done.

- [ ] **Step 0.3: Confirm baseline Python tests pass**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
pytest -x -q 2>&1 | tail -10
```

Expected: all green. Note the count for diff at end-of-plan.

---

## Task 1: Add protocol IDs (firmware + Python, no logic yet)

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c` (lines 58–110, the CMD_* / RSP_* `#define` blocks)
- Modify: `python/feralrf/enums.py` (Command + Response enums)
- Modify: `python/feralrf/commands.py` (add two new CommandBuilder methods)
- Modify: `python/tests/test_gatt_api.py` (extend)

- [ ] **Step 1.1: Write the failing test for the new enum values**

Append to `python/tests/test_gatt_api.py`:

```python
# --- F8c protocol IDs ---


def test_f8c_command_ids_are_assigned():
    assert Command.GATT_EXCHANGE_MTU == 0x4A
    assert Command.GATT_READ_BY_UUID == 0x4B


def test_f8c_response_ids_are_assigned():
    assert Response.GATT_MTU == 0xB0
    assert Response.GATT_ATTRIBUTE == 0xB1
    assert Response.DISCONNECTED == 0xB2


def test_gatt_exchange_mtu_payload_is_u16_le_client_mtu():
    assert CommandBuilder.gatt_exchange_mtu(client_mtu=23) == b"\x17\x00"
    assert CommandBuilder.gatt_exchange_mtu(client_mtu=247) == b"\xF7\x00"


def test_gatt_exchange_mtu_rejects_too_small():
    import pytest

    with pytest.raises(ValueError):
        CommandBuilder.gatt_exchange_mtu(client_mtu=22)  # MTU must be >= 23 per spec


def test_gatt_read_by_uuid_uuid16_payload_is_start_end_uuid_le():
    payload = CommandBuilder.gatt_read_by_uuid(start=0x0001, end=0xFFFF, uuid=0x2A00)
    assert payload == b"\x01\x00\xFF\xFF\x00\x2A"


def test_gatt_read_by_uuid_rejects_inverted_range():
    import pytest

    with pytest.raises(ValueError):
        CommandBuilder.gatt_read_by_uuid(start=0x0010, end=0x0001, uuid=0x2A00)
```

- [ ] **Step 1.2: Run tests, verify they fail**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_gatt_api.py -k "f8c or gatt_exchange_mtu or gatt_read_by_uuid" -v
```

Expected: 6 FAIL with `AttributeError: GATT_EXCHANGE_MTU` etc.

- [ ] **Step 1.3: Add Command + Response enum members**

In `python/feralrf/enums.py`, in the `Command` IntEnum block (after `GATT_WRITE = 0x46` at line 84), add:

```python
    GATT_EXCHANGE_MTU = 0x4A
    GATT_READ_BY_UUID = 0x4B
```

In the `Response` IntEnum block (after `FOLLOW_DONE = 0xAC` at line 131), add:

```python
    # F8c — MTU + Read by UUID + Disconnect reason
    GATT_MTU = 0xB0
    GATT_ATTRIBUTE = 0xB1
    DISCONNECTED = 0xB2
```

- [ ] **Step 1.4: Add CommandBuilder methods**

In `python/feralrf/commands.py`, after `gatt_subscribe` (around line 207), add:

```python
    @staticmethod
    def gatt_exchange_mtu(client_mtu: int = 23) -> bytes:
        """Payload for CMD_GATT_EXCHANGE_MTU: 2-byte LE client MTU.

        Args:
            client_mtu: MTU we advertise to the peer. Per BT Core Spec
                Vol 3 Part F §3.2.8, ATT MTU MUST be >= 23.
        """
        if client_mtu < 23 or client_mtu > 0xFFFF:
            raise ValueError(f"client_mtu must be in [23, 65535], got {client_mtu}")
        return struct.pack("<H", client_mtu & 0xFFFF)

    @staticmethod
    def gatt_read_by_uuid(start: int, end: int, uuid: int) -> bytes:
        """Payload for CMD_GATT_READ_BY_UUID: start[2] + end[2] + uuid16[2].

        Only 16-bit UUIDs are supported in this revision (matches firmware
        send_read_by_type_req). 128-bit UUID support deferred.
        """
        if start < 1 or start > 0xFFFF:
            raise ValueError(f"start handle out of range: {start}")
        if end < start or end > 0xFFFF:
            raise ValueError(f"end ({end}) must be >= start ({start}) and <= 0xFFFF")
        return struct.pack("<HHH", start & 0xFFFF, end & 0xFFFF, uuid & 0xFFFF)
```

- [ ] **Step 1.5: Add firmware `#define`s for command/response IDs**

In `firmware/cc1352/src/command_processor.c`, after line 65 (`#define CMD_GATT_WRITE 0x46u`), insert:

```c
#define CMD_GATT_EXCHANGE_MTU 0x4Au
#define CMD_GATT_READ_BY_UUID 0x4Bu
```

After line 110 (`#define RSP_FOLLOW_DEBUG 0xAFu`), insert:

```c
/* F8c — MTU + Read by UUID + Disconnect reason */
#define RSP_GATT_MTU 0xB0u
#define RSP_GATT_ATTRIBUTE 0xB1u
#define RSP_DISCONNECTED 0xB2u
```

- [ ] **Step 1.6: Re-run tests, expect green**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_gatt_api.py -k "f8c or gatt_exchange_mtu or gatt_read_by_uuid" -v
```

Expected: 6 PASS. Run the full suite to confirm no regressions:

```bash
pytest -x -q
```

- [ ] **Step 1.7: Re-build firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -5
```

Expected: clean build, no new warnings (the new `#define`s are unused so far — gcc will not warn).

- [ ] **Step 1.8: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files \
  firmware/cc1352/src/command_processor.c \
  python/feralrf/enums.py \
  python/feralrf/commands.py \
  python/tests/test_gatt_api.py
git add firmware/cc1352/src/command_processor.c \
        python/feralrf/enums.py \
        python/feralrf/commands.py \
        python/tests/test_gatt_api.py
git commit -m "feat(f8c): allocate CMD/RSP IDs for MTU + Read by UUID + DC reason"
```

---

## Task 2: Firmware MTU exchange — public API + callback + handler

**Files:**
- Modify: `firmware/cc1352/include/att_client.h` (add state, callback typedef, public function)
- Modify: `firmware/cc1352/src/att_client.c` (add handler branch, public function, poll branch update)

The current `handle_mtu_rsp` is wired to transition `WAIT_MTU_RSP → WAIT_DISCOVER_RSP` (it was meant to be part of an automatic discovery flow but `AttClient_startDiscover` skips it — see `att_client.c:408-410`). For F8c, MTU exchange becomes a standalone, host-triggered operation. We add a new state `ATT_STATE_WAIT_MTU_EXCHANGE` that is dedicated to the explicit-exchange flow, leaving `WAIT_MTU_RSP` untouched (in case the auto-flow is reactivated later).

- [ ] **Step 2.1: Add new state, callback typedef, and public function declaration to header**

In `firmware/cc1352/include/att_client.h`:

After line 22 (`ATT_STATE_WAIT_WRITE_RSP,`), add inside the `AttClient_State` enum:

```c
    ATT_STATE_WAIT_MTU_EXCHANGE,
```

After line 33 (`typedef void (*AttClient_DoneCb)...`), add:

```c
typedef void (*AttClient_MtuCb)(uint16_t negotiatedMtu);
```

In the `AttClient_Callbacks` struct (lines 35–40), add a new field after `onDone`:

```c
    AttClient_MtuCb onMtu;
```

After line 52 (`bool AttClient_startWrite(...)`), add a new prototype:

```c
/* Trigger an explicit ATT MTU exchange. clientMtu is what we advertise to
 * the peer (must be >= 23). The peer's reply is min(clientMtu, peerMtu);
 * delivered via onMtu(...) and then onDone(0). Returns false if not IDLE. */
bool AttClient_startMtuExchange(uint16_t clientMtu);
```

Also add a debug tag (after `ATT_DBG_TAG_INDICATE_RX = 26,` on line 99):

```c
    ATT_DBG_TAG_START_MTU_ENTER = 27,
    ATT_DBG_TAG_START_MTU_EXIT_OK = 28,
    ATT_DBG_TAG_START_MTU_EXIT_FAIL = 29,
```

- [ ] **Step 2.2: Implement `AttClient_startMtuExchange` in `att_client.c`**

In `firmware/cc1352/src/att_client.c`, the `send_exchange_mtu_req` function (lines 144–150) already exists but only sends the hardcoded `ATT_DEFAULT_MTU`. Refactor to take a parameter, then add the new public API.

Replace lines 144–150:

```c
static bool send_exchange_mtu_req(uint16_t clientMtu) {
    uint8_t pdu[3];
    pdu[0] = ATT_EXCHANGE_MTU_REQ;
    pdu[1] = (uint8_t)(clientMtu & 0xFF);
    pdu[2] = (uint8_t)(clientMtu >> 8);
    return att_send(pdu, 3);
}
```

Update the existing caller at line 528 (inside `AttClient_poll` for the legacy `WAIT_MTU_RSP` branch) to pass `ATT_DEFAULT_MTU`:

```c
    case ATT_STATE_WAIT_MTU_RSP:
        if (!s_request_pending) {
            if (send_exchange_mtu_req(ATT_DEFAULT_MTU)) {
                s_request_pending = true;
                att_dbg(ATT_DBG_TAG_POLL_TX_MTU, old);
            }
        }
        break;
```

Add a new static for the requested MTU near the other discovery state (after line 63, the `s_disc_next_handle` declaration):

```c
/* Client MTU advertised in the most recent explicit exchange request. */
static uint16_t s_requested_mtu;
```

After `AttClient_startWrite` (around line 448), add the new public function:

```c
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
```

Add a poll branch in `AttClient_poll` (the switch starting at line 525), inserting after the existing `WAIT_MTU_RSP` branch:

```c
    case ATT_STATE_WAIT_MTU_EXCHANGE:
        if (send_exchange_mtu_req(s_requested_mtu)) {
            s_request_pending = true;
            att_dbg(ATT_DBG_TAG_POLL_TX_MTU, old);
        }
        break;
```

Update `handle_mtu_rsp` (lines 204–219) to handle BOTH states. Replace the function body:

```c
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
    /* Negotiated MTU = min(client, server). For the legacy auto-flow
     * (WAIT_MTU_RSP) the cap is ATT_DEFAULT_MTU because att_send still
     * uses the static buffer; the explicit-exchange flow records the
     * peer's value verbatim so the host can see it. */
    uint16_t negotiated = (server_mtu < ATT_DEFAULT_MTU) ? server_mtu : ATT_DEFAULT_MTU;
    s_mtu = negotiated;
    s_request_pending = false;

    if (s_state == ATT_STATE_WAIT_MTU_EXCHANGE) {
        if (s_cb.onMtu) {
            /* Surface the *peer-reported* server MTU to the host even
             * though our own buffers are still capped at ATT_DEFAULT_MTU.
             * Lets the host record what the peer would support. */
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

    /* Legacy auto-flow (currently unreachable, kept for compat) */
    s_disc_next_handle = 0x0001;
    s_service_count = 0;
    s_state = ATT_STATE_WAIT_DISCOVER_RSP;
    att_dbg(ATT_DBG_TAG_MTU_RSP, old);
}
```

- [ ] **Step 2.3: Build firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build. Any unused-static warning means you forgot to use `s_requested_mtu` somewhere — fix it now.

- [ ] **Step 2.4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files \
  firmware/cc1352/include/att_client.h \
  firmware/cc1352/src/att_client.c
git add firmware/cc1352/include/att_client.h firmware/cc1352/src/att_client.c
git commit -m "feat(f8c): AttClient_startMtuExchange + onMtu callback"
```

---

## Task 3: Firmware Read by UUID — public API + new state + new handler

**Files:**
- Modify: `firmware/cc1352/include/att_client.h`
- Modify: `firmware/cc1352/src/att_client.c`

The existing `send_read_by_type_req` (line 164) is reused as-is. We add a new state `ATT_STATE_WAIT_READ_BY_UUID_RSP` and a new handler `handle_read_by_uuid_rsp` because the response entries have a different layout for arbitrary UUID16 reads (`[handle:2][value:N]`) than for characteristic discovery (`[handle:2][properties:1][valueHandle:2][uuid:N]`).

- [ ] **Step 3.1: Extend header — new state, callback, public API, debug tags**

In `firmware/cc1352/include/att_client.h`:

In `AttClient_State` enum (after the new `ATT_STATE_WAIT_MTU_EXCHANGE,` from Task 2):

```c
    ATT_STATE_WAIT_READ_BY_UUID_RSP,
```

After the `AttClient_MtuCb` typedef from Task 2:

```c
typedef void (*AttClient_AttributeCb)(uint16_t handle, const uint8_t *value, uint8_t valueLen);
```

In the `AttClient_Callbacks` struct, add field:

```c
    AttClient_AttributeCb onAttribute;
```

After the `AttClient_startMtuExchange` prototype from Task 2:

```c
/* Issue ATT_READ_BY_TYPE_REQ for an arbitrary 16-bit UUID. Each matching
 * attribute is delivered via onAttribute(handle, value, len); the flow
 * terminates with onDone(status) — status 0 means at least one entry was
 * returned, non-zero means the peer replied with ATT_ERROR (typically
 * ATTRIBUTE_NOT_FOUND). Returns false if not IDLE. */
bool AttClient_startReadByUuid(uint16_t startHandle, uint16_t endHandle, uint16_t uuid16);
```

Add debug tags after the MTU tags from Task 2:

```c
    ATT_DBG_TAG_START_READ_UUID_ENTER = 30,
    ATT_DBG_TAG_START_READ_UUID_EXIT_OK = 31,
    ATT_DBG_TAG_START_READ_UUID_EXIT_FAIL = 32,
    ATT_DBG_TAG_READ_UUID_RSP = 33,
    ATT_DBG_TAG_POLL_TX_READ_UUID = 34,
```

- [ ] **Step 3.2: Implement in `att_client.c`**

Add static state for the active query (after `s_requested_mtu` from Task 2):

```c
static uint16_t s_read_uuid_start;
static uint16_t s_read_uuid_end;
static uint16_t s_read_uuid_uuid;
```

Add the public function (after `AttClient_startMtuExchange` from Task 2):

```c
bool AttClient_startReadByUuid(uint16_t startHandle, uint16_t endHandle, uint16_t uuid16) {
    AttClient_State old = s_state;
    att_dbg(ATT_DBG_TAG_START_READ_UUID_ENTER, old);
    if (s_state != ATT_STATE_IDLE) {
        att_dbg(ATT_DBG_TAG_START_READ_UUID_EXIT_FAIL, old);
        return false;
    }
    if (startHandle == 0 || endHandle < startHandle) {
        att_dbg(ATT_DBG_TAG_START_READ_UUID_EXIT_FAIL, old);
        return false;
    }
    s_read_uuid_start = startHandle;
    s_read_uuid_end = endHandle;
    s_read_uuid_uuid = uuid16;
    s_request_pending = false;
    s_state = ATT_STATE_WAIT_READ_BY_UUID_RSP;
    att_dbg(ATT_DBG_TAG_START_READ_UUID_EXIT_OK, old);
    return true;
}
```

Add a new handler `handle_read_by_uuid_rsp` (immediately after `handle_read_by_type_rsp` which ends at line 290). Note the entry layout for non-0x2803 reads is `[handle:2][value:N]`, so `value_len = entry_len - 2`:

```c
static void handle_read_by_uuid_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (len < 4 || s_state != ATT_STATE_WAIT_READ_BY_UUID_RSP) {
        att_dbg(ATT_DBG_TAG_READ_UUID_RSP, old);
        return;
    }
    s_request_pending = false;

    uint8_t entry_len = pdu[1];
    if (entry_len < 3u) {
        /* Malformed — at minimum [handle:2][value:1] */
        s_state = ATT_STATE_IDLE;
        att_dbg(ATT_DBG_TAG_READ_UUID_RSP, old);
        if (s_cb.onDone) {
            s_cb.onDone(1);
        }
        return;
    }
    uint8_t offset = 2;
    uint16_t last_handle = 0;
    bool got_any = false;

    while (offset + entry_len <= len) {
        uint16_t handle = le16(&pdu[offset]);
        uint8_t value_len = entry_len - 2u;
        const uint8_t *value = &pdu[offset + 2];
        if (s_cb.onAttribute) {
            s_cb.onAttribute(handle, value, value_len);
        }
        last_handle = handle;
        got_any = true;
        offset += entry_len;
    }
    (void)last_handle;
    (void)got_any;

    /* Per ATT spec: ReadByType returns up to MTU-2 bytes worth of entries.
     * If the peer has more matches, the host can re-issue with an updated
     * start handle. For F8c we do single-shot — the host gets one batch
     * and onDone(0). Multi-shot continuation is a future enhancement. */
    s_state = ATT_STATE_IDLE;
    att_dbg(ATT_DBG_TAG_READ_UUID_RSP, old);
    if (s_cb.onDone) {
        att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
        s_cb.onDone(0);
    }
}
```

Update the dispatch in `AttClient_onL2capRx` (line 477–479) to route `ATT_READ_BY_TYPE_RSP` based on state:

```c
    case ATT_READ_BY_TYPE_RSP:
        if (s_state == ATT_STATE_WAIT_READ_BY_UUID_RSP) {
            handle_read_by_uuid_rsp(att_pdu, att_len);
        } else {
            handle_read_by_type_rsp(att_pdu, att_len);
        }
        break;
```

Update `handle_error_rsp` (lines 328–375) to also terminate cleanly when the error arrives in the new state. After the existing `if (s_state == ATT_STATE_WAIT_CHAR_RSP)` block (around line 348), add:

```c
        if (s_state == ATT_STATE_WAIT_READ_BY_UUID_RSP) {
            s_state = ATT_STATE_IDLE;
            att_dbg(ATT_DBG_TAG_ERROR_RSP, old);
            if (s_cb.onDone) {
                /* status 1 marks "no entries found" so the host can
                 * distinguish this from "peer didn't reply at all".
                 * Same convention as gatt_discover. */
                s_cb.onDone(1);
            }
            return;
        }
```

Add a poll branch in `AttClient_poll` (after the new `WAIT_MTU_EXCHANGE` branch from Task 2):

```c
    case ATT_STATE_WAIT_READ_BY_UUID_RSP:
        if (send_read_by_type_req(s_read_uuid_start, s_read_uuid_end, s_read_uuid_uuid)) {
            s_request_pending = true;
            att_dbg(ATT_DBG_TAG_POLL_TX_READ_UUID, old);
        }
        break;
```

- [ ] **Step 3.3: Build**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build.

- [ ] **Step 3.4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files \
  firmware/cc1352/include/att_client.h \
  firmware/cc1352/src/att_client.c
git add firmware/cc1352/include/att_client.h firmware/cc1352/src/att_client.c
git commit -m "feat(f8c): AttClient_startReadByUuid + onAttribute callback"
```

---

## Task 4: Firmware DC reason — extract from LL_TERMINATE_IND, expose via callback

**Files:**
- Modify: `firmware/cc1352/include/ble_conn_mgr.h`
- Modify: `firmware/cc1352/src/ble_conn_mgr.c`

Currently `handle_ll_ctrl` for `LL_TERMINATE_IND` (`ble_conn_mgr.c:69-72`) just stops without reading `payload[1]` (the reason byte per BT Core Spec Vol 6 Part B §2.4.2.6). We extract the reason, store it, and add a callback hook so command_processor can emit `RSP_DISCONNECTED`. Three trigger sites must be wired:

1. Peer-initiated: `handle_ll_ctrl` LL_TERMINATE_IND (real reason from peer)
2. Supervision timeout: `BleConnMgr_poll` (synthetic reason 0x22 = LL_RESPONSE_TIMEOUT)
3. Host-initiated: command_processor's `CMD_DISCONNECT` (synthetic reason 0x16 = LOCAL_HOST_TERMINATED) — wired in Task 5

The callback fires *exactly once* per disconnect (idempotent — once `s_running` flips to false, subsequent stop calls are no-ops).

- [ ] **Step 4.1: Extend header**

In `firmware/cc1352/include/ble_conn_mgr.h`:

After the `BleConnMgr_getTotalRxCount(void);` declaration (line 23), add:

```c

/* Disconnect callback. Fires exactly once per connection lifetime when the
 * link terminates, with the BT Core Spec reason byte. Three reason sources:
 *   - Peer LL_TERMINATE_IND payload[1] (e.g., 0x13 REMOTE_USER_TERMINATED)
 *   - Supervision timeout    → 0x22 (LL_RESPONSE_TIMEOUT)
 *   - Host-initiated stop    → 0x16 (LOCAL_HOST_TERMINATED)
 * Setter is idempotent; passing NULL clears the registered callback. */
typedef void (*BleConnMgr_DisconnectCb)(uint8_t reason);
void BleConnMgr_setDisconnectCb(BleConnMgr_DisconnectCb cb);

/* Synthetic reason for host-initiated disconnect. Caller uses this when
 * BleConnMgr_stop() is invoked from a CMD_DISCONNECT path. */
void BleConnMgr_stopWithReason(uint8_t reason);
```

- [ ] **Step 4.2: Implement in `ble_conn_mgr.c`**

Add static state (near the other `static` declarations, after line 53 `s_dbg_total_tx_done`):

```c
static BleConnMgr_DisconnectCb s_disconnect_cb;
/* Sticky reason for the most recent termination — captured on entry into
 * BleConnMgr_stop so the callback always sees the same value the caller
 * intended, even if some other path also calls stop later. */
static uint8_t s_pending_dc_reason;
static bool s_dc_reason_pending;
```

Update `handle_ll_ctrl` for `LL_TERMINATE_IND` (lines 69–72). Replace:

```c
    case LL_TERMINATE_IND:
        BleConnMgr_stop();
        BleConn_disconnect();
        break;
```

with:

```c
    case LL_TERMINATE_IND: {
        /* payload = [opcode:1][reason:1] per BT Core Spec Vol 6 Part B §2.4.2.6 */
        uint8_t reason = (len >= 2) ? payload[1] : 0x13u; /* default REMOTE_USER_TERMINATED */
        BleConnMgr_stopWithReason(reason);
        BleConn_disconnect();
        break;
    }
```

Update the supervision-timeout branch in `BleConnMgr_poll` (lines 270–274). Replace:

```c
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        BleConnMgr_stop();
        BleConn_disconnect();
        return false;
    }
```

with:

```c
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        /* 0x22 = LL_RESPONSE_TIMEOUT per BT Core Spec Vol 1 Part F §1.3.2 */
        BleConnMgr_stopWithReason(0x22u);
        BleConn_disconnect();
        return false;
    }
```

Add the new public functions. After `BleConnMgr_init` (line 157), add:

```c
void BleConnMgr_setDisconnectCb(BleConnMgr_DisconnectCb cb) {
    s_disconnect_cb = cb;
}

void BleConnMgr_stopWithReason(uint8_t reason) {
    if (!s_dc_reason_pending) {
        s_pending_dc_reason = reason;
        s_dc_reason_pending = true;
    }
    BleConnMgr_stop();
}
```

Update `BleConnMgr_stop` (lines 239–243). Replace:

```c
void BleConnMgr_stop(void) {
    s_running = false;
    s_event_counter = 0;
    AttClient_reset();
}
```

with:

```c
void BleConnMgr_stop(void) {
    bool was_running = s_running;
    s_running = false;
    s_event_counter = 0;
    AttClient_reset();
    if (was_running && s_dc_reason_pending) {
        uint8_t reason = s_pending_dc_reason;
        s_dc_reason_pending = false;
        s_pending_dc_reason = 0;
        if (s_disconnect_cb) {
            s_disconnect_cb(reason);
        }
    }
}
```

Update `BleConnMgr_start` (lines 171–237) to clear the pending-reason flag at the top, so a fresh connection cannot inherit stale state. Insert after the `s_event_counter = 0;` line (line 178):

```c
    s_dc_reason_pending = false;
    s_pending_dc_reason = 0;
```

- [ ] **Step 4.3: Build**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build.

- [ ] **Step 4.4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files \
  firmware/cc1352/include/ble_conn_mgr.h \
  firmware/cc1352/src/ble_conn_mgr.c
git add firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git commit -m "feat(f8c): extract LL_TERMINATE_IND reason, expose via DisconnectCb"
```

---

## Task 5: Firmware command_processor — wire CMDs + RSPs + DC callback

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`

This task wires four things:
1. `gatt_on_mtu` callback that emits `RSP_GATT_MTU` with `[mtu:2LE]` payload
2. `gatt_on_attribute` callback that emits `RSP_GATT_ATTRIBUTE` with `[handle:2LE][value:N]`
3. `gatt_on_disconnected` (registered with `BleConnMgr_setDisconnectCb`) that emits async `RSP_DISCONNECTED` with `[reason:1]`
4. Two new switch cases for `CMD_GATT_EXCHANGE_MTU` and `CMD_GATT_READ_BY_UUID`
5. Update the existing `CMD_DISCONNECT` case to route through `BleConnMgr_stopWithReason(0x16)` first

- [ ] **Step 5.1: Add the three new callbacks**

After `gatt_on_done` (around line 238), insert:

```c
static void gatt_on_mtu(uint16_t negotiatedMtu) {
    uint8_t rsp[2];
    rsp[0] = (uint8_t)(negotiatedMtu & 0xFF);
    rsp[1] = (uint8_t)(negotiatedMtu >> 8);
    send_response(RSP_GATT_MTU, s_gatt_seq, rsp, sizeof(rsp));
}

static void gatt_on_attribute(uint16_t handle, const uint8_t *value, uint8_t valueLen) {
    /* Wire format: [handle:2LE][value:N], capped at MTU-2 = 21 bytes. */
    uint8_t rsp[2 + 21];
    rsp[0] = (uint8_t)(handle & 0xFF);
    rsp[1] = (uint8_t)(handle >> 8);
    if (valueLen > 21u) {
        valueLen = 21u;
    }
    for (uint8_t i = 0; i < valueLen; i++) {
        rsp[2 + i] = value[i];
    }
    send_response(RSP_GATT_ATTRIBUTE, s_gatt_seq, rsp, (uint16_t)(2u + valueLen));
}

static void gatt_on_disconnected(uint8_t reason) {
    /* Async — seq=0 since this is unsolicited. Host filters by RSP code. */
    uint8_t rsp[1] = {reason};
    OutputIF_sendResponse(RSP_DISCONNECTED, 0u, rsp, sizeof(rsp));
}
```

- [ ] **Step 5.2: Update `ensure_gatt_callbacks` to include the new callbacks**

Replace the body of `ensure_gatt_callbacks` (lines 242–253) with:

```c
static void ensure_gatt_callbacks(void) {
    if (!gatt_callbacks_installed) {
        AttClient_Callbacks cb = {
            .onService = gatt_on_service,
            .onChar = gatt_on_char,
            .onRead = gatt_on_read,
            .onDone = gatt_on_done,
            .onMtu = gatt_on_mtu,
            .onAttribute = gatt_on_attribute,
        };
        AttClient_setCallbacks(&cb);
        BleConnMgr_setDisconnectCb(gatt_on_disconnected);
        gatt_callbacks_installed = true;
    }
}
```

- [ ] **Step 5.3: Update `CMD_DISCONNECT` to flow through `stopWithReason`**

Find the `CMD_DISCONNECT` case (lines 872–879). Replace its body with:

```c
    case CMD_DISCONNECT:
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ensure_gatt_callbacks();
        /* Mark host-initiated reason BEFORE BleConn_disconnect so the
         * subsequent BleConnMgr_stop callback sees 0x16, not whatever
         * sticks around from the previous session. */
        BleConnMgr_stopWithReason(0x16u);
        BleConn_disconnect();
        send_ack(seq);
        return;
```

- [ ] **Step 5.4: Add the two new switch cases**

After the `CMD_GATT_SUBSCRIBE` case (find its closing brace, near line 1030 — search for `CMD_GATT_SUBSCRIBE`), insert:

```c
    case CMD_GATT_EXCHANGE_MTU: {
        /* Payload: client_mtu[2LE]. */
        if (payload_len != 2u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        s_gatt_seq = seq;
        uint16_t client_mtu = read_u16_le(payload);
        if (!AttClient_startMtuExchange(client_mtu)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;
    }

    case CMD_GATT_READ_BY_UUID: {
        /* Payload: start[2LE] + end[2LE] + uuid16[2LE] = 6 bytes. */
        if (payload_len != 6u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!BleConn_isConnected()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        ensure_gatt_callbacks();
        s_gatt_seq = seq;
        uint16_t start = read_u16_le(&payload[0]);
        uint16_t end = read_u16_le(&payload[2]);
        uint16_t uuid = read_u16_le(&payload[4]);
        if (!AttClient_startReadByUuid(start, end, uuid)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;
    }
```

- [ ] **Step 5.5: Build firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build, `feralrf_cc1352.hex` regenerated.

- [ ] **Step 5.6: Flash the board with `.hex` and run a quick echo check**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -5
```

Expected: `✓ Verified match`. If it fails, retry once. If it still fails, ask the user — do not invoke OpenOCD recovery without explicit go-ahead.

Smoke (no peer needed — just confirm board still boots and ACKs CMD_GET_INFO):

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python -c "
from feralrf import Radio
r = Radio()
r.connect()
print('init:', r.init())
r.disconnect()
"
```

Expected: prints a `DeviceInfo` line. Any timeout means firmware regressed.

- [ ] **Step 5.7: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/src/command_processor.c
git add firmware/cc1352/src/command_processor.c
git commit -m "feat(f8c): wire CMD_GATT_EXCHANGE_MTU + CMD_GATT_READ_BY_UUID + RSP_DISCONNECTED"
```

---

## Task 6: Python — Radio.gatt_exchange_mtu

**Files:**
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_gatt_api.py`

- [ ] **Step 6.1: Write failing test**

Append to `python/tests/test_gatt_api.py`:

```python
# --- Radio.gatt_exchange_mtu ---


def test_radio_has_gatt_exchange_mtu_method():
    from feralrf.radio import Radio

    assert hasattr(Radio, "gatt_exchange_mtu")


def test_radio_public_api_includes_gatt_exchange_mtu():
    from feralrf.radio import Radio

    assert "gatt_exchange_mtu" in Radio.PUBLIC_API
```

(`Radio.PUBLIC_API` is the existing `_PUBLIC_API` tuple at line ~190 — find it and inspect to confirm the attribute name.)

- [ ] **Step 6.2: Run, expect fail**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_gatt_api.py -k "gatt_exchange_mtu" -v
```

Expected: FAIL on `hasattr` check.

- [ ] **Step 6.3: Implement `gatt_exchange_mtu`**

In `python/feralrf/radio.py`, add `"gatt_exchange_mtu"` to the `_PUBLIC_API` tuple (find it near line 207).

After the `gatt_subscribe` method (ends around line 853), insert:

```python
    def gatt_exchange_mtu(self, client_mtu: int = 23, timeout: float = 5.0) -> int:
        """Exchange ATT MTU with the connected peer.

        Note: the firmware's RX/TX buffers are still capped at MTU 23 — this
        method surfaces what the *peer* reports as its server MTU so the host
        can record it for diagnostics. Larger MTUs cannot actually be used
        until firmware buffers are widened (out of scope for F8c).

        Args:
            client_mtu: MTU we advertise. Must be >= 23 per BT spec.
            timeout: Seconds to wait for the negotiated MTU + GATT_DONE.

        Returns:
            Peer-reported server MTU (uint16).

        Raises:
            CommandError: firmware refused the request, peer replied with
                ATT_ERROR, or returned GATT_DONE before GATT_MTU.
            ProtocolError: unexpected response opcode received.
        """
        self._send_command(
            Command.GATT_EXCHANGE_MTU,
            CommandBuilder.gatt_exchange_mtu(client_mtu),
        )
        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_EXCHANGE_MTU failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_EXCHANGE_MTU: 0x{cmd_id:02X}")

        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.GATT_MTU, Response.GATT_DONE, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_EXCHANGE_MTU value error", payload[0] if payload else 0)
        if cmd_id == Response.GATT_DONE:
            status = payload[0] if payload else 0xFF
            raise CommandError("GATT_EXCHANGE_MTU done without value", status)
        if len(payload) < 2:
            raise ProtocolError(f"GATT_MTU payload too short: {len(payload)}")
        peer_mtu = int.from_bytes(payload[0:2], "little")

        # Drain trailing GATT_DONE so the next command does not see it.
        try:
            self._read_response(
                timeout=min(timeout, 1.0),
                expected={Response.GATT_DONE, Response.ERROR},
            )
        except TimeoutError:
            pass
        return peer_mtu
```

- [ ] **Step 6.4: Run tests, expect green**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_gatt_api.py -v -q 2>&1 | tail -10
```

Expected: PASS for the new tests, no regressions.

- [ ] **Step 6.5: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/feralrf/radio.py python/tests/test_gatt_api.py
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "feat(f8c): Radio.gatt_exchange_mtu host API"
```

---

## Task 7: Python — Radio.gatt_read_by_uuid

**Files:**
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_gatt_api.py`

- [ ] **Step 7.1: Write failing test**

Append to `python/tests/test_gatt_api.py`:

```python
# --- Radio.gatt_read_by_uuid ---


def test_radio_has_gatt_read_by_uuid_method():
    from feralrf.radio import Radio

    assert hasattr(Radio, "gatt_read_by_uuid")


def test_gatt_attribute_dataclass_exists():
    from feralrf.radio import GattAttribute

    a = GattAttribute(handle=0x002A, value=b"\x01\x02")
    assert a.handle == 0x002A
    assert a.value == b"\x01\x02"
```

- [ ] **Step 7.2: Run, expect fail**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_gatt_api.py -k "gatt_read_by_uuid or gatt_attribute_dataclass" -v
```

Expected: FAIL.

- [ ] **Step 7.3: Add `GattAttribute` dataclass and method**

In `python/feralrf/radio.py`, find the `GattCharacteristic` dataclass (line ~108–117). Immediately after it, add:

```python
@dataclass
class GattAttribute:
    """Single (handle, value) pair returned by Read by UUID."""

    handle: int
    value: bytes
```

Add `"gatt_read_by_uuid"` to `_PUBLIC_API`.

After `gatt_exchange_mtu` (added in Task 6), add:

```python
    def gatt_read_by_uuid(
        self,
        uuid: int,
        start: int = 0x0001,
        end: int = 0xFFFF,
        timeout: float = 5.0,
    ) -> "list[GattAttribute]":
        """ATT Read by Type with an arbitrary UUID16; return matching attrs.

        Useful when the host already knows the UUID it wants (e.g.,
        Device Name = 0x2A00) and wants to skip a full discovery.

        Args:
            uuid: 16-bit attribute type UUID.
            start: First handle to search (inclusive).
            end: Last handle to search (inclusive).
            timeout: Seconds to wait for ACK + entries + GATT_DONE.

        Returns:
            List of GattAttribute (may be empty if peer replied with
            ATTRIBUTE_NOT_FOUND).

        Raises:
            CommandError: firmware refused or peer returned a non-spec error.
            ProtocolError: unexpected opcode received.
        """
        self._send_command(
            Command.GATT_READ_BY_UUID,
            CommandBuilder.gatt_read_by_uuid(start, end, uuid),
        )
        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_READ_BY_UUID failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_READ_BY_UUID: 0x{cmd_id:02X}")

        attributes: list[GattAttribute] = []
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("GATT_READ_BY_UUID stream timeout")
            cmd_id, _seq, payload = self._read_response(
                timeout=remaining,
                expected={Response.GATT_ATTRIBUTE, Response.GATT_DONE, Response.ERROR},
            )
            if cmd_id == Response.ERROR:
                raise CommandError(
                    "GATT_READ_BY_UUID stream error",
                    payload[0] if payload else 0,
                )
            if cmd_id == Response.GATT_DONE:
                # status byte: 0=ok (entries present), 1=ATTRIBUTE_NOT_FOUND (empty)
                return attributes
            if len(payload) < 2:
                raise ProtocolError(f"GATT_ATTRIBUTE payload too short: {len(payload)}")
            attributes.append(
                GattAttribute(
                    handle=int.from_bytes(payload[0:2], "little"),
                    value=bytes(payload[2:]),
                )
            )
```

- [ ] **Step 7.4: Run tests, expect green**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_gatt_api.py -v -q 2>&1 | tail -10
```

Expected: PASS for all new tests, no regressions.

- [ ] **Step 7.5: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/feralrf/radio.py python/tests/test_gatt_api.py
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "feat(f8c): Radio.gatt_read_by_uuid host API + GattAttribute"
```

---

## Task 8: Python — async disconnect events iterator

**Files:**
- Modify: `python/feralrf/radio.py`
- Create: `python/tests/test_disconnect_events.py`

The pattern matches `read_gatt_notifications`. We expose a `read_disconnect_events` iterator that yields `DisconnectEvent(reason: int, timestamp: float)` items as they arrive. The existing `read_gatt_notifications` already documents (line 862) that `RSP_DISCONNECTED` ends the notification iterator, so consumers of both must call the disconnect iterator separately.

- [ ] **Step 8.1: Write failing test**

Create `python/tests/test_disconnect_events.py`:

```python
"""F8c — async disconnect event iterator (no hardware)."""

from __future__ import annotations

from feralrf.radio import DisconnectEvent, Radio


def test_disconnect_event_dataclass():
    e = DisconnectEvent(reason=0x13, timestamp=1234.5)
    assert e.reason == 0x13
    assert e.timestamp == 1234.5


def test_radio_has_read_disconnect_events_method():
    assert hasattr(Radio, "read_disconnect_events")


def test_disconnect_event_reason_label_is_human_readable():
    # Optional — reason byte → BT spec label.
    e = DisconnectEvent(reason=0x13, timestamp=0.0)
    assert "REMOTE_USER_TERMINATED" in e.reason_label

    e = DisconnectEvent(reason=0x16, timestamp=0.0)
    assert "LOCAL_HOST_TERMINATED" in e.reason_label

    e = DisconnectEvent(reason=0x22, timestamp=0.0)
    assert "RESPONSE_TIMEOUT" in e.reason_label

    e = DisconnectEvent(reason=0xFF, timestamp=0.0)
    # Unknown reasons fall back to a hex string.
    assert "0xFF" in e.reason_label or "UNKNOWN" in e.reason_label
```

- [ ] **Step 8.2: Run, expect fail**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_disconnect_events.py -v
```

Expected: 3 FAIL on import / hasattr.

- [ ] **Step 8.3: Add `DisconnectEvent` dataclass and method**

In `python/feralrf/radio.py`, near the other dataclasses (after `GattAttribute` from Task 7), add:

```python
# Map BT Core Spec Vol 1 Part F §1.3 reason bytes to short labels.
_DISCONNECT_REASON_LABELS = {
    0x05: "AUTHENTICATION_FAILURE",
    0x08: "CONNECTION_TIMEOUT",
    0x13: "REMOTE_USER_TERMINATED",
    0x14: "REMOTE_LOW_RESOURCES",
    0x15: "REMOTE_POWER_OFF",
    0x16: "LOCAL_HOST_TERMINATED",
    0x22: "LL_RESPONSE_TIMEOUT",
    0x28: "INSTANT_PASSED",
    0x3D: "MIC_FAILURE",
    0x3E: "CONNECTION_FAILED_TO_ESTABLISH",
}


@dataclass
class DisconnectEvent:
    """Async disconnect notification surfaced by the firmware."""

    reason: int
    timestamp: float

    @property
    def reason_label(self) -> str:
        return _DISCONNECT_REASON_LABELS.get(self.reason, f"UNKNOWN(0x{self.reason:02X})")
```

Add `"read_disconnect_events"` to `_PUBLIC_API`.

After `read_gatt_notifications` (ends around line 897), insert:

```python
    def read_disconnect_events(
        self,
        timeout: float = 5.0,
    ) -> "Iterator[DisconnectEvent]":
        """Yield DisconnectEvent items as they arrive from the firmware.

        Filters for RSP_DISCONNECTED (0xB2). Other unsolicited frames are
        discarded silently; only DISCONNECTED frames are yielded. Iterator
        ends quietly on timeout — caller can loop and call again.
        """
        while True:
            try:
                cmd_id, _seq, payload = self._read_response(
                    timeout=timeout,
                    expected={Response.DISCONNECTED},
                )
            except TimeoutError:
                return
            if cmd_id != Response.DISCONNECTED:
                return
            reason = payload[0] if payload else 0xFF
            yield DisconnectEvent(reason=reason, timestamp=time.monotonic())
```

- [ ] **Step 8.4: Run tests, expect green**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest tests/test_disconnect_events.py -v
pytest -x -q 2>&1 | tail -10
```

Expected: 3 PASS, full suite green.

- [ ] **Step 8.5: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/feralrf/radio.py python/tests/test_disconnect_events.py
git add python/feralrf/radio.py python/tests/test_disconnect_events.py
git commit -m "feat(f8c): Radio.read_disconnect_events + DisconnectEvent"
```

---

## Task 9: Live-board smoke harness (manual run)

**Files:**
- Create: `python/scripts/smoke_f8c.py`

This script exercises the three new features against a live BLE peripheral. It is run by hand (not pytest) because it requires a peer device. Document the pre-conditions in the script's docstring.

- [ ] **Step 9.1: Create the smoke script**

Create `python/scripts/smoke_f8c.py`:

```python
#!/usr/bin/env python3
"""F8c — live-board smoke for MTU + Read by UUID + DC reason.

Pre-conditions:
  - CC1352 board flashed with the F8c firmware (post-Task 5).
  - One reachable BLE peripheral (advertising) — pass its MAC as argv[1].
  - Address type via argv[2] (0=public, 1=random; default 1).

Pass criteria (printed at end):
  [PASS]  MTU exchange    — peer MTU recorded
  [PASS]  Read by UUID    — at least one entry returned for 0x2A00 (Device Name)
  [PASS]  Disconnect      — host-initiated DC, reason 0x16 received

Usage:
    source .venv/bin/activate
    python scripts/smoke_f8c.py AA:BB:CC:DD:EE:FF 1
"""

from __future__ import annotations

import sys
import time

from feralrf import Radio


def parse_mac(mac: str) -> bytes:
    parts = mac.split(":")
    if len(parts) != 6:
        raise SystemExit(f"bad MAC: {mac}")
    return bytes(int(p, 16) for p in reversed(parts))  # LE


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    addr_le = parse_mac(sys.argv[1])
    addr_type = int(sys.argv[2]) if len(sys.argv) >= 3 else 1

    r = Radio()
    r.connect()
    r.init()
    r.reset_device()

    results = {}

    print(f"Connecting to {sys.argv[1]} (type={addr_type}) ...")
    res = r.ble_connect(addr_le, addr_type=addr_type, timeout=10.0)
    if not res.is_ok:
        print(f"  ble_connect failed: {res}")
        return 1

    # Allow the link to settle.
    time.sleep(0.5)

    # 1) MTU exchange
    try:
        peer_mtu = r.gatt_exchange_mtu(client_mtu=23, timeout=5.0)
        print(f"  MTU exchange: peer reports {peer_mtu}")
        results["mtu"] = peer_mtu >= 23
    except Exception as e:
        print(f"  MTU exchange FAILED: {e}")
        results["mtu"] = False

    # 2) Read by UUID — Device Name (0x2A00)
    try:
        attrs = r.gatt_read_by_uuid(uuid=0x2A00, timeout=5.0)
        print(f"  Read by UUID 0x2A00: {len(attrs)} entry/entries")
        for a in attrs:
            try:
                name = a.value.decode("utf-8", errors="replace")
            except Exception:
                name = a.value.hex()
            print(f"    handle=0x{a.handle:04X}  value={name!r}")
        results["read_by_uuid"] = len(attrs) >= 1
    except Exception as e:
        print(f"  Read by UUID FAILED: {e}")
        results["read_by_uuid"] = False

    # 3) Disconnect — host-initiated, expect reason 0x16
    print("  Disconnecting (host-initiated) ...")
    try:
        r.ble_disconnect(timeout=3.0)
    except Exception as e:
        print(f"  ble_disconnect raised: {e}")

    got_event = None
    for ev in r.read_disconnect_events(timeout=3.0):
        got_event = ev
        break
    if got_event is None:
        print("  Disconnect event: NOT received")
        results["disconnect"] = False
    else:
        print(f"  Disconnect event: reason=0x{got_event.reason:02X} ({got_event.reason_label})")
        results["disconnect"] = got_event.reason == 0x16

    r.disconnect()

    print()
    for name, ok in results.items():
        print(f"  [{'PASS' if ok else 'FAIL'}]  {name}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 9.2: Permissions + lint**

```bash
chmod +x /home/sabas/Documents/electroniccats/FeralRF/python/scripts/smoke_f8c.py
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/scripts/smoke_f8c.py
```

- [ ] **Step 9.3: Document the manual run command (do NOT run unattended)**

The smoke needs a phone or other BLE peripheral nearby. Hand off to the user with this exact command, asking which MAC to target:

```
source python/.venv/bin/activate
python python/scripts/smoke_f8c.py <MAC>  <addr_type>
```

Capture the printed output verbatim into the commit message body. Three [PASS] = wire-level closed for F8c. Any [FAIL] becomes a follow-up bug, not a blocker for the commit.

- [ ] **Step 9.4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add python/scripts/smoke_f8c.py
git commit -m "test(f8c): live-board smoke harness for MTU + Read by UUID + DC reason"
```

---

## Wrap-up

- [ ] **Final: Verify pre-existing radio_if.h WIP is still unstaged**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git status --short
```

Expected: only `M firmware/cc1352/include/radio_if.h` (untouched whitespace WIP).

- [ ] **Final: Run full test suite one more time**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest -x -q 2>&1 | tail -10
```

Expected: green, count higher than baseline by at least 11 tests (the new ones added in Tasks 1, 6, 7, 8).

- [ ] **Final: Tag (only after smoke pass)**

If the user reports `3 [PASS]` from Step 9.3:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git tag v2.0-f8c
```

If smoke is partial (1 or 2 [FAIL]) tag instead with `v2.0-f8c-partial` and file the failures as project memory entries.

---

## Self-Review

**1. Spec coverage:**

- MTU Exchange: Tasks 1 (IDs), 2 (firmware AttClient), 5 (CMD/RSP wiring), 6 (Python). ✓
- Read by UUID: Tasks 1 (IDs), 3 (firmware AttClient), 5 (CMD/RSP wiring), 7 (Python). ✓
- Disconnect Reason: Tasks 1 (IDs), 4 (firmware ble_conn_mgr), 5 (RSP wiring + CMD_DISCONNECT route), 8 (Python iterator). ✓
- Smoke validation: Task 9. ✓

**2. Placeholder scan:**

No `TBD`, no `add appropriate error handling`, no `similar to Task N`. All code blocks contain real code. Step 5.4's "search for `CMD_GATT_SUBSCRIBE`" is acceptable — the case spans many lines and a search is the most reliable locator.

**3. Type consistency:**

- `AttClient_MtuCb`/`onMtu`/`gatt_on_mtu`/`RSP_GATT_MTU`/`Response.GATT_MTU` — consistent.
- `AttClient_AttributeCb`/`onAttribute`/`gatt_on_attribute`/`RSP_GATT_ATTRIBUTE`/`Response.GATT_ATTRIBUTE`/`GattAttribute` — consistent (note Python class is `GattAttribute`, RSP enum is `GATT_ATTRIBUTE`; both intentional).
- `BleConnMgr_DisconnectCb`/`gatt_on_disconnected`/`RSP_DISCONNECTED`/`Response.DISCONNECTED`/`DisconnectEvent` — consistent.
- `gatt_exchange_mtu`/`gatt_read_by_uuid`/`read_disconnect_events` — Python method names match `_PUBLIC_API` registration.
- `BleConnMgr_stopWithReason` — declared in header (Task 4.1), implemented (Task 4.2), called from command_processor (Task 5.3) and from `handle_ll_ctrl` / supervision-timeout (Task 4.2). All sites consistent.

No stray references found.
