# F21 BLE Connectable Advertiser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Agregar `CMD_BLE_ADV_LEGACY` (firmware) + 3 métodos en `Radio` class + smoke V1.b 2-board → cierra criterios 1+2 del spec §F21 (PDU types correctos + SCAN_RSP funcional). Tag `v2.0-f21`.

**Architecture:** Branch `feature/f21-conn-advertiser` desde `main` HEAD `2862b9d`. 4 bundles secuenciales: firmware (struct defs + handler + RadioIF) → Python (enums + CommandBuilder + Radio methods) → unit tests → smoke + demo. Cero dependencia con F20.

**Tech Stack:** CC1352P7 firmware (TI-RTOS 7, SDK 8.30, CMake), Python 3.11+ (pyserial, pytest), pre-commit. Hardware: 2 CatSniffer boards.

**Spec source:** `docs/superpowers/specs/2026-05-04-f21-conn-advertiser-design.md` (commit `2862b9d`).

**Hardware:** TX board #2 (`/dev/ttyACM1`), RX board #1 (`/dev/ttyACM2`). Ambos boards ya flasheados con HEAD post-F17.

**Prerequisites verified at planning time** (2026-05-04):
- ❌ `Ble5_0_cmdBleAdv` / `Ble5_0_cmdBleAdvDir` / `Ble5_0_cmdBleAdvScan` NO existen en `smartrf_ble5_0.c` → Bundle 1 step agrega struct definitions (precedente F25 hand-edit per `project_syscfg_handedited`).
- ❌ `_ll_parser.LLPduKind` es para LL Data PDUs (no advertising) → smoke usará byte-inspection directo `pkt.data[0] & 0x0F` para extraer adv PDU type.
- ✅ `Ble5_0_cmdBleAdvNc` existe en `smartrf_ble5_0.c:329` con `s_bleAdvPar` params struct (`rfc_bleAdvPar_t`) — reutilizable para los 3 nuevos comandos legacy adv (mismo struct shape).
- ✅ `radio.set_ble_addr_str(addr_str)` y `radio.set_ble_addr(bytes)` existen.
- ✅ `_random_mac()` helper en `attacks/ble.py` para defaults.
- ✅ Pattern `_radio_with_fake_serial` en `test_gatt_api.py:178` para tests con FakeSerial.

---

## Task 0: Crear working branch

**Files:** None (git operation)

- [ ] **Step 1: Verify baseline**

```bash
git status
git log --oneline -3
```

Expected: HEAD = `2862b9d plan(f21): conn advertiser spec`. Working tree clean.

- [ ] **Step 2: Create branch `feature/f21-conn-advertiser`**

```bash
git checkout -b feature/f21-conn-advertiser
git status
```

Expected: branch created, working tree clean.

---

## Task 1: Bundle 1 — Firmware (struct defs + handler + RadioIF)

**Files:**
- Modify: `firmware/cc1352/include/protocol.h` (add CMD_BLE_ADV_LEGACY)
- Modify: `firmware/cc1352/src/smartrf_ble5_0.c` (add 3 TI struct defs)
- Modify: `firmware/cc1352/include/radio_if.h` (add RadioIF_transmitBleAdvLegacy decl)
- Modify: `firmware/cc1352/src/radio_if.c` (add new function + extern declarations)
- Modify: `firmware/cc1352/src/command_processor.c` (add handler in switch)

**Background:** Need 3 TI BLE legacy adv commands not currently in SmartRF config. Pattern matches existing `Ble5_0_cmdBleAdvNc` at line 329. All 3 use the same `rfc_bleAdvPar_t` params struct. F21 adds a separate `s_f21_bleAdvPar` so existing ADV_NONCONN_IND path is unaffected.

- [ ] **Step 1: Add `CMD_BLE_ADV_LEGACY` to `protocol.h`**

Find the F8b Track B follower block (around CMD_FOLLOW_START 0x50u) and insert:

```bash
grep -n "CMD_FOLLOW_START\|CMD_FOLLOW_STOP\|CMD_FOLLOW_DEBUG" firmware/cc1352/include/protocol.h
```

Edit the F8b block to add F21 ID right after CMD_FOLLOW_STOP (0x51) and before CMD_FOLLOW_DEBUG (0x54):

```c
/* F8b Track B follower */
#define CMD_FOLLOW_START 0x50u
#define CMD_FOLLOW_STOP 0x51u
#define CMD_BLE_ADV_LEGACY 0x52u
#define CMD_FOLLOW_DEBUG 0x54u
```

- [ ] **Step 2: Add 3 TI BLE legacy adv struct definitions to `smartrf_ble5_0.c`**

Locate the existing `Ble5_0_cmdBleAdvNc` definition (around line 329). Insert the 3 new structs immediately AFTER that definition. They share `s_bleAdvPar` so we declare a separate `s_f21_bleAdvPar` to avoid contention with the NC path.

First, find the existing s_bleAdvPar declaration:

```bash
grep -n "s_bleAdvPar\b" firmware/cc1352/src/smartrf_ble5_0.c
```

Insert AFTER the existing `Ble5_0_cmdBleAdvNc` block (around line 346) and BEFORE `Ble5_0_cmdBle5AdvNc`:

```c
/* F21 BLE Connectable Advertiser — separate params struct so the NC path
 * (s_bleAdvPar / Ble5_0_cmdBleAdvNc) stays untouched. Used by ADV_IND,
 * ADV_DIRECT_IND, ADV_SCAN_IND. RadioIF_transmitBleAdvLegacy populates
 * fields per call. */
static rfc_bleAdvPar_t s_f21_bleAdvPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0,
    .rxConfig.bAutoFlushCrcErr = 0,
    .rxConfig.bAutoFlushEmpty = 0,
    .rxConfig.bIncludeLenByte = 0,
    .rxConfig.bIncludeCrc = 0,
    .rxConfig.bAppendRssi = 0,
    .rxConfig.bAppendStatus = 0,
    .rxConfig.bAppendTimestamp = 0,
    .advConfig.advFilterPolicy = 0, /* allow any scanner/initiator */
    .advConfig.deviceAddrType = 0x1, /* random — overridable per call */
    .advConfig.peerAddrType = 0x1,
    .advConfig.bStrictLenFilter = 0,
    .advConfig.chSel = 0,
    .advConfig.privIgnMode = 0,
    .advConfig.rpaMode = 0,
    .advLen = 0,
    .scanRspLen = 0,
    .pAdvData = 0,
    .pScanRspData = 0,
    .pDeviceAddress = 0,
    .pWhiteList = 0, /* dual-use field: pPeerAddress for DIRECT_IND */
    .behConfig.scanRspEndType = 0,
    .endTrigger.triggerType = TRIG_NEVER,
    .endTrigger.bEnaCmd = 0,
    .endTrigger.triggerNo = 0,
    .endTrigger.pastTrig = 1,
    .endTime = 0x00000000,
};

rfc_CMD_BLE_ADV_t Ble5_0_cmdBleAdv = {
    .commandNo = CMD_BLE_ADV,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.bEnaCmd = 0,
    .startTrigger.triggerNo = 0,
    .startTrigger.pastTrig = 1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0,
    .pParams = &s_f21_bleAdvPar,
    .pOutput = 0,
};

rfc_CMD_BLE_ADV_DIR_t Ble5_0_cmdBleAdvDir = {
    .commandNo = CMD_BLE_ADV_DIR,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.bEnaCmd = 0,
    .startTrigger.triggerNo = 0,
    .startTrigger.pastTrig = 1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0,
    .pParams = &s_f21_bleAdvPar,
    .pOutput = 0,
};

rfc_CMD_BLE_ADV_SCAN_t Ble5_0_cmdBleAdvScan = {
    .commandNo = CMD_BLE_ADV_SCAN,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = TRIG_NOW,
    .startTrigger.bEnaCmd = 0,
    .startTrigger.triggerNo = 0,
    .startTrigger.pastTrig = 1,
    .condition.rule = COND_NEVER,
    .condition.nSkip = 0,
    .channel = 0x25,
    .whitening.init = 0x0,
    .whitening.bOverride = 0,
    .pParams = &s_f21_bleAdvPar,
    .pOutput = 0,
};
```

- [ ] **Step 3: Add extern declarations to header (if SmartRF has a public header)**

Check whether existing `Ble5_0_cmdBleAdvNc` has an extern in `smartrf_ble5_0.h`:

```bash
grep -n "Ble5_0_cmdBleAdvNc\|Ble5_0_cmdBleAdvDir\|Ble5_0_cmdBleAdv " firmware/cc1352/include/smartrf_ble5_0.h 2>/dev/null
```

If yes, add 3 corresponding externs:

```c
extern rfc_CMD_BLE_ADV_t Ble5_0_cmdBleAdv;
extern rfc_CMD_BLE_ADV_DIR_t Ble5_0_cmdBleAdvDir;
extern rfc_CMD_BLE_ADV_SCAN_t Ble5_0_cmdBleAdvScan;
```

If `smartrf_ble5_0.h` does not exist or doesn't have those externs, the `radio_if.c` will declare them locally (Step 5).

- [ ] **Step 4: Add `RadioIF_transmitBleAdvLegacy` declaration to `radio_if.h`**

Find the existing BLE adv-related declarations (around `RadioIF_setBleAdvAddress` line):

```bash
grep -n "RadioIF_transmitBleAdv\|RadioIF_setBleAdvAddress" firmware/cc1352/include/radio_if.h
```

Add new declaration:

```c
/* F21 — BLE Connectable advertiser (legacy ADV_IND / DIRECT / SCAN_IND).
 * pdu_type: 0x0=ADV_IND, 0x1=ADV_DIRECT_IND, 0x6=ADV_SCAN_IND.
 * For DIRECT, init_addr is the peer address; adv_data and scan_rsp ignored.
 * For IND/SCAN, init_addr ignored.
 * count: number of advertising events to emit. interval_units: 0.625ms units. */
bool RadioIF_transmitBleAdvLegacy(uint8_t pdu_type,
                                  uint8_t addr_type, const uint8_t *addr,
                                  uint8_t channel, int8_t power_dbm,
                                  uint16_t count, uint16_t interval_units,
                                  const uint8_t *adv_data, uint8_t adv_data_len,
                                  const uint8_t *scan_rsp, uint8_t scan_rsp_len,
                                  uint8_t init_addr_type, const uint8_t *init_addr);
```

- [ ] **Step 5: Implement `RadioIF_transmitBleAdvLegacy` in `radio_if.c`**

Locate `RadioIF_transmitBleAdvRaw` (around line 587) and insert the new function AFTER it:

```c
/* F21 — see radio_if.h for contract */
extern rfc_CMD_BLE_ADV_t Ble5_0_cmdBleAdv;
extern rfc_CMD_BLE_ADV_DIR_t Ble5_0_cmdBleAdvDir;
extern rfc_CMD_BLE_ADV_SCAN_t Ble5_0_cmdBleAdvScan;

static uint8_t s_f21_adv_payload[BLE_ADV_TX_MAX_PAYLOAD_LEN] __attribute__((aligned(4)));
static uint8_t s_f21_scan_rsp_payload[BLE_ADV_TX_MAX_PAYLOAD_LEN] __attribute__((aligned(4)));
static uint8_t s_f21_device_addr[BLE_ADV_TX_DEVICE_ADDR_LEN] __attribute__((aligned(4)));
static uint8_t s_f21_init_addr[BLE_ADV_TX_DEVICE_ADDR_LEN] __attribute__((aligned(4)));
static rfc_bleAdvOutput_t s_f21_adv_output;

bool RadioIF_transmitBleAdvLegacy(uint8_t pdu_type,
                                  uint8_t addr_type, const uint8_t *addr,
                                  uint8_t channel, int8_t power_dbm,
                                  uint16_t count, uint16_t interval_units,
                                  const uint8_t *adv_data, uint8_t adv_data_len,
                                  const uint8_t *scan_rsp, uint8_t scan_rsp_len,
                                  uint8_t init_addr_type, const uint8_t *init_addr) {
    if (addr == NULL || count == 0u || channel < 37u || channel > 39u) {
        return false;
    }
    if (pdu_type != 0x0u && pdu_type != 0x1u && pdu_type != 0x6u) {
        return false;
    }

    /* Configure RF for BLE legacy adv (always 1M) */
    s_tx_power_dbm = power_dbm;
    RadioIF_setPower(power_dbm);
    RadioIF_applyBleChannelConfig((uint8_t)channel);
    RadioIF_applyBlePhyMode(PHY_MANAGER_PHY_BLE_1M);

    /* Copy device address (LE) */
    memcpy(s_f21_device_addr, addr, BLE_ADV_TX_DEVICE_ADDR_LEN);

    /* Configure TI cmd per pdu_type */
    rfc_radioOp_t *cmd = NULL;
    extern rfc_bleAdvPar_t s_f21_bleAdvPar __attribute__((weak));

    /* Reset shared params struct */
    s_f21_bleAdvPar.pRxQ = 0;
    s_f21_bleAdvPar.advConfig.deviceAddrType = (addr_type & 0x1);
    s_f21_bleAdvPar.advConfig.peerAddrType = (init_addr_type & 0x1);
    s_f21_bleAdvPar.advConfig.advFilterPolicy = 0;
    s_f21_bleAdvPar.pDeviceAddress = (uint16_t *)s_f21_device_addr;

    if (pdu_type == 0x1u) {
        /* ADV_DIRECT_IND — pPeerAddress (overlays pWhiteList in struct) */
        memcpy(s_f21_init_addr, init_addr, BLE_ADV_TX_DEVICE_ADDR_LEN);
        s_f21_bleAdvPar.pWhiteList = (rfc_bleWhiteListEntry_t *)s_f21_init_addr;
        s_f21_bleAdvPar.advLen = 0;
        s_f21_bleAdvPar.scanRspLen = 0;
        s_f21_bleAdvPar.pAdvData = NULL;
        s_f21_bleAdvPar.pScanRspData = NULL;

        Ble5_0_cmdBleAdvDir.channel = (uint8_t)channel;
        Ble5_0_cmdBleAdvDir.startTrigger.triggerType = TRIG_NOW;
        Ble5_0_cmdBleAdvDir.condition.rule = COND_NEVER;
        Ble5_0_cmdBleAdvDir.pParams = &s_f21_bleAdvPar;
        Ble5_0_cmdBleAdvDir.pOutput = &s_f21_adv_output;
        cmd = (rfc_radioOp_t *)&Ble5_0_cmdBleAdvDir;
    } else {
        /* ADV_IND or ADV_SCAN_IND */
        if (adv_data_len > BLE_ADV_TX_MAX_PAYLOAD_LEN ||
            scan_rsp_len > BLE_ADV_TX_MAX_PAYLOAD_LEN) {
            return false;
        }
        if (adv_data && adv_data_len > 0u) {
            memcpy(s_f21_adv_payload, adv_data, adv_data_len);
        }
        if (scan_rsp && scan_rsp_len > 0u) {
            memcpy(s_f21_scan_rsp_payload, scan_rsp, scan_rsp_len);
        }
        s_f21_bleAdvPar.advLen = adv_data_len;
        s_f21_bleAdvPar.scanRspLen = scan_rsp_len;
        s_f21_bleAdvPar.pAdvData = s_f21_adv_payload;
        s_f21_bleAdvPar.pScanRspData = s_f21_scan_rsp_payload;
        s_f21_bleAdvPar.pWhiteList = NULL;

        if (pdu_type == 0x0u) {
            Ble5_0_cmdBleAdv.channel = (uint8_t)channel;
            Ble5_0_cmdBleAdv.startTrigger.triggerType = TRIG_NOW;
            Ble5_0_cmdBleAdv.condition.rule = COND_NEVER;
            Ble5_0_cmdBleAdv.pParams = &s_f21_bleAdvPar;
            Ble5_0_cmdBleAdv.pOutput = &s_f21_adv_output;
            cmd = (rfc_radioOp_t *)&Ble5_0_cmdBleAdv;
        } else {
            /* pdu_type == 0x6 ADV_SCAN_IND */
            Ble5_0_cmdBleAdvScan.channel = (uint8_t)channel;
            Ble5_0_cmdBleAdvScan.startTrigger.triggerType = TRIG_NOW;
            Ble5_0_cmdBleAdvScan.condition.rule = COND_NEVER;
            Ble5_0_cmdBleAdvScan.pParams = &s_f21_bleAdvPar;
            Ble5_0_cmdBleAdvScan.pOutput = &s_f21_adv_output;
            cmd = (rfc_radioOp_t *)&Ble5_0_cmdBleAdvScan;
        }
    }

    if (cmd == NULL || s_rf_handle == NULL) {
        return false;
    }

    /* Per-iteration loop. CONNECT_IND breaks the loop early. */
    for (uint16_t i = 0u; i < count; i++) {
        memset(&s_f21_adv_output, 0, sizeof(s_f21_adv_output));
        cmd->status = 0x0000;
        (void)RF_runCmd(s_rf_handle, cmd, RF_PriorityNormal, NULL, 0);
        /* BLE_DONE_CONNECT (0x1FFF) = CONNECT_IND received. F20 not impl
         * → break loop and return; phone will time out. */
        if (cmd->status == 0x1FFFu) {
            break;
        }
        if (interval_units > 0u && i + 1u < count) {
            uint32_t interval_ms = ((uint32_t)interval_units * 625u) / 1000u;
            if (interval_ms == 0u) {
                interval_ms = 1u;
            }
            Task_sleep(MS_TO_TASK_TICKS(interval_ms));
        }
    }
    return true;
}
```

- [ ] **Step 6: Add handler in `command_processor.c`**

Find the GATT or FOLLOW handler block and add a new case. Locate `CMD_FOLLOW_START`:

```bash
grep -n "case CMD_FOLLOW_START\|case CMD_FOLLOW_STOP" firmware/cc1352/src/command_processor.c
```

Add a new case AFTER `CMD_FOLLOW_STOP`:

```c
    case CMD_BLE_ADV_LEGACY: {
        if (payload_len < 14u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        uint8_t pdu_type = payload[0];
        uint8_t adv_addr_type = payload[1];
        const uint8_t *adv_addr = &payload[2];
        uint8_t channel = payload[8];
        int8_t power = (int8_t)payload[9];
        uint16_t count = read_u16_le(&payload[10]);
        uint16_t interval_units = read_u16_le(&payload[12]);

        if (channel < 37u || channel > 39u || count == 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (pdu_type != 0x0u && pdu_type != 0x1u && pdu_type != 0x6u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }

        uint8_t adv_data_len = 0u;
        const uint8_t *adv_data = NULL;
        uint8_t scan_rsp_len = 0u;
        const uint8_t *scan_rsp_data = NULL;
        uint8_t init_addr_type = 0u;
        const uint8_t *init_addr = NULL;

        if (pdu_type == 0x1u) {
            /* ADV_DIRECT_IND: header(14) + init_addr_type(1) + init_addr(6) = 21 */
            if (payload_len != 21u) {
                send_error(seq, ERR_INVALID_PAYLOAD);
                return;
            }
            init_addr_type = payload[14];
            init_addr = &payload[15];
        } else {
            /* ADV_IND or ADV_SCAN_IND: header(14) + adv_len(1) + adv(N) + sr_len(1) + sr(M) */
            if (payload_len < 16u) {
                send_error(seq, ERR_INVALID_PAYLOAD);
                return;
            }
            adv_data_len = payload[14];
            if (adv_data_len > 31u || (uint16_t)(15u + adv_data_len + 1u) > payload_len) {
                send_error(seq, ERR_INVALID_PAYLOAD);
                return;
            }
            adv_data = &payload[15];
            scan_rsp_len = payload[15u + adv_data_len];
            if (scan_rsp_len > 31u ||
                (uint16_t)(16u + adv_data_len + scan_rsp_len) != payload_len) {
                send_error(seq, ERR_INVALID_PAYLOAD);
                return;
            }
            scan_rsp_data = &payload[16u + adv_data_len];
        }

        /* ACK first; the loop in RadioIF_transmitBleAdvLegacy blocks
         * synchronously for count*interval ms (max ~640 ms for count=64
         * interval=10ms). Caller times out at 5s + count*interval. */
        send_ack(seq);
        (void)RadioIF_transmitBleAdvLegacy(pdu_type, adv_addr_type, adv_addr,
                                           channel, power, count, interval_units,
                                           adv_data, adv_data_len,
                                           scan_rsp_data, scan_rsp_len,
                                           init_addr_type, init_addr);
        return;
    }
```

- [ ] **Step 7: Build firmware**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -10
```

Expected: clean build. If symbols `CMD_BLE_ADV` / `CMD_BLE_ADV_DIR` / `CMD_BLE_ADV_SCAN` are undefined, they're TI rfc_ble_cmd.h opcodes. Verify the SDK header is included (existing usage of `CMD_BLE_ADV_NC` already pulls it in via the existing radio_if.c includes).

If the build fails with `Ble5_0_cmdBleAdv*` undefined (smartrf header missing), the `extern` in radio_if.c (Step 5) covers it locally.

If build fails with struct field mismatch (e.g. `pPeerAddress` vs `pWhiteList`), inspect `rfc_bleAdvPar_t` definition in SDK headers and adjust the assignment in Step 5.

- [ ] **Step 8: Pre-commit + commit Bundle 1**

```bash
pre-commit run --files firmware/cc1352/include/protocol.h firmware/cc1352/src/smartrf_ble5_0.c firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c firmware/cc1352/src/command_processor.c
git add firmware/cc1352/include/protocol.h firmware/cc1352/src/smartrf_ble5_0.c firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c firmware/cc1352/src/command_processor.c
git commit -m "$(cat <<'EOF'
feat(f21): firmware CMD_BLE_ADV_LEGACY (0x52) — 3 TI BLE legacy adv dispatch

Adds CMD_BLE_ADV_LEGACY command. Single host command with pdu_type field
selects between TI BLE legacy adv commands:
  - pdu_type=0x0 → cmdBleAdv (ADV_IND, opcode 0x1805)
  - pdu_type=0x1 → cmdBleAdvDir (ADV_DIRECT_IND, opcode 0x1806)
  - pdu_type=0x6 → cmdBleAdvScan (ADV_SCAN_IND, opcode 0x1808)

TI CPE handles SCAN_REQ→SCAN_RSP within hardware timing (~150 µs IFC),
so criterio 2 del spec (SCAN_RSP funcional) sale gratis.

CONNECT_IND received: TI cmd terminates with status BLE_DONE_CONNECT
(0x1FFF). Loop breaks; phone times out (F20 peripheral not impl).

Adds 3 TI struct definitions + s_f21_bleAdvPar (separate from
s_bleAdvPar so existing ADV_NONCONN_IND path stays untouched).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: pre-commit clean (CMake hook passes), commit lands.

---

## Task 2: Bundle 2 — Python (enums + CommandBuilder + Radio methods)

**Files:**
- Modify: `python/feralrf/enums.py` (add Command.BLE_ADV_LEGACY)
- Modify: `python/feralrf/commands.py` (add CommandBuilder.ble_adv_legacy)
- Modify: `python/feralrf/radio.py` (add 3 advertise_* methods + helpers)

- [ ] **Step 1: Add `Command.BLE_ADV_LEGACY = 0x52` to `enums.py`**

Find the BLE Connection block in `Command` class:

```bash
grep -n "CONNECT = 0x40\|CONN_STATUS = 0x42\|GATT_DISCOVER" python/feralrf/enums.py
```

Edit the `Command` class to add the new entry near other BLE commands. Insert after the BLE Connection block:

```python
    # BLE Connection
    CONNECT = 0x40
    DISCONNECT = 0x41
    CONN_STATUS = 0x42

    # GATT
    GATT_DISCOVER = 0x43
    GATT_SUBSCRIBE = 0x44
    GATT_READ = 0x45
    GATT_WRITE = 0x46
    GATT_EXCHANGE_MTU = 0x4A
    GATT_READ_BY_UUID = 0x4B

    # Diagnostics
    DEBUG_TIMING = 0x47
    DEBUG_CONN_PARAMS = 0x48

    # F21 — BLE Connectable Advertiser
    BLE_ADV_LEGACY = 0x52
```

- [ ] **Step 2: Add `CommandBuilder.ble_adv_legacy` to `commands.py`**

Find the `CommandBuilder` class definition. Append a new static method at the end of the class (before any module-level closing):

```python
    @staticmethod
    def ble_adv_legacy(
        pdu_type: int,
        adv_addr_le: bytes,
        adv_addr_type: int = 1,
        channel: int = 37,
        power_dbm: int = 0,
        count: int = 50,
        interval_units: int = 16,
        adv_data: bytes = b"",
        scan_rsp_data: bytes = b"",
        init_addr_le: bytes = b"",
        init_addr_type: int = 1,
    ) -> bytes:
        """Build CMD_BLE_ADV_LEGACY (F21) payload.

        pdu_type: 0x0 ADV_IND | 0x1 ADV_DIRECT_IND | 0x6 ADV_SCAN_IND
        Wire format per F21 spec (docs/superpowers/specs/2026-05-04-f21-...).
        """
        if pdu_type not in (0x0, 0x1, 0x6):
            raise ValueError(f"pdu_type must be 0x0/0x1/0x6, got 0x{pdu_type:X}")
        if len(adv_addr_le) != 6:
            raise ValueError("adv_addr_le must be 6 bytes")
        if power_dbm < -20 or power_dbm > 20:
            raise ValueError(f"power_dbm out of range: {power_dbm}")
        if channel not in (37, 38, 39):
            raise ValueError(f"channel must be 37/38/39, got {channel}")
        if count < 1 or count > 0xFFFF:
            raise ValueError(f"count must be in [1, 65535], got {count}")
        if interval_units < 1 or interval_units > 0xFFFF:
            raise ValueError(f"interval_units must be in [1, 65535], got {interval_units}")

        head = bytes([pdu_type, adv_addr_type & 0x01]) + adv_addr_le
        head += bytes([channel, power_dbm & 0xFF])
        head += struct.pack("<HH", count, interval_units)

        if pdu_type == 0x1:
            if len(init_addr_le) != 6:
                raise ValueError("ADV_DIRECT_IND requires init_addr_le (6 bytes)")
            return head + bytes([init_addr_type & 0x01]) + init_addr_le

        if len(adv_data) > 31:
            raise ValueError(f"adv_data > 31 bytes ({len(adv_data)})")
        if len(scan_rsp_data) > 31:
            raise ValueError(f"scan_rsp_data > 31 bytes ({len(scan_rsp_data)})")
        return (
            head
            + bytes([len(adv_data)])
            + adv_data
            + bytes([len(scan_rsp_data)])
            + scan_rsp_data
        )
```

- [ ] **Step 3: Add 3 advertise_* methods + helper to `radio.py`**

Find a good location near other BLE methods (e.g. `ble_connect`). Add helper first:

```bash
grep -n "_random_mac\|_mac_str_to_le_bytes\|def ble_connect" python/feralrf/radio.py
```

Add module-level helper near top of `radio.py` (after imports, before `class Radio`):

```python
def _mac_str_to_le_bytes(addr_str: str) -> bytes:
    """Convert 'AA:BB:CC:DD:EE:FF' to 6-byte LE bytes (FF EE DD CC BB AA)."""
    parts = addr_str.split(":")
    if len(parts) != 6:
        raise ValueError(f"Invalid MAC string: {addr_str!r}")
    return bytes(int(p, 16) for p in reversed(parts))


def _random_mac_le() -> bytes:
    """Generate 6-byte LE random static address (top two bits = 0b11)."""
    import os
    addr = bytearray(os.urandom(6))
    addr[5] |= 0xC0  # random static type
    return bytes(addr)
```

Then add the 3 methods inside `Radio` class. Find a good location near other BLE methods:

```python
    def advertise_ind(
        self,
        payload: bytes,
        scan_resp_data: bytes = b"",
        target_addr: Optional[str] = None,
        count: int = 50,
        channel: int = 37,
        power_dbm: int = 0,
        interval_us: int = 10_000,
    ) -> None:
        """F21 — emit ADV_IND (general connectable + scannable).

        TI CPE handles SCAN_REQ→SCAN_RSP automatically. CONNECT_IND
        received: firmware breaks the loop early (no F20 peripheral).
        """
        addr_le = _mac_str_to_le_bytes(target_addr) if target_addr else _random_mac_le()
        interval_units = max(1, interval_us // 625)
        cmd_payload = CommandBuilder.ble_adv_legacy(
            pdu_type=0x0,
            adv_addr_le=addr_le,
            channel=channel,
            power_dbm=power_dbm,
            count=count,
            interval_units=interval_units,
            adv_data=payload,
            scan_rsp_data=scan_resp_data,
        )
        self._send_command(Command.BLE_ADV_LEGACY, cmd_payload)
        # ACK arrives immediately; firmware loop runs sync afterwards but
        # we don't wait for it (host returns control).
        self._read_response(timeout=5.0, expected={Response.ACK, Response.ERROR})

    def advertise_direct(
        self,
        target_addr: str,
        init_addr: str,
        mode: str = "low",
        count: int = 50,
        channel: int = 37,
        power_dbm: int = 0,
    ) -> None:
        """F21 — emit ADV_DIRECT_IND. mode='low' (10 ms) | 'high' (3.75 ms)."""
        addr_le = _mac_str_to_le_bytes(target_addr)
        init_le = _mac_str_to_le_bytes(init_addr)
        interval_us = 3_750 if mode == "high" else 10_000
        interval_units = max(1, interval_us // 625)
        cmd_payload = CommandBuilder.ble_adv_legacy(
            pdu_type=0x1,
            adv_addr_le=addr_le,
            channel=channel,
            power_dbm=power_dbm,
            count=count,
            interval_units=interval_units,
            init_addr_le=init_le,
        )
        self._send_command(Command.BLE_ADV_LEGACY, cmd_payload)
        self._read_response(timeout=5.0, expected={Response.ACK, Response.ERROR})

    def advertise_scan_ind(
        self,
        payload: bytes,
        scan_resp_data: bytes = b"",
        target_addr: Optional[str] = None,
        count: int = 50,
        channel: int = 37,
        power_dbm: int = 0,
        interval_us: int = 10_000,
    ) -> None:
        """F21 — emit ADV_SCAN_IND (scannable non-connectable)."""
        addr_le = _mac_str_to_le_bytes(target_addr) if target_addr else _random_mac_le()
        interval_units = max(1, interval_us // 625)
        cmd_payload = CommandBuilder.ble_adv_legacy(
            pdu_type=0x6,
            adv_addr_le=addr_le,
            channel=channel,
            power_dbm=power_dbm,
            count=count,
            interval_units=interval_units,
            adv_data=payload,
            scan_rsp_data=scan_resp_data,
        )
        self._send_command(Command.BLE_ADV_LEGACY, cmd_payload)
        self._read_response(timeout=5.0, expected={Response.ACK, Response.ERROR})
```

Also add the 3 methods to STABLE_METHODS list if it exists:

```bash
grep -n "STABLE_METHODS\|stable_methods" python/feralrf/radio.py
```

If `STABLE_METHODS` exists in `Radio` (line ~220 per earlier inspection), append:

```python
        "advertise_ind",
        "advertise_direct",
        "advertise_scan_ind",
```

- [ ] **Step 4: Smoke-test imports + suite**

```bash
cd python && PYTHONPATH=. python -c "from feralrf import Radio; from feralrf.commands import CommandBuilder; print(CommandBuilder.ble_adv_legacy(pdu_type=0, adv_addr_le=b'\\x06\\x05\\x04\\x03\\x02\\x01').hex())"
```

Expected: prints a hex string representing the wire payload (≥16 bytes for ADV_IND with no adv_data/scan_rsp).

```bash
PYTHONPATH=. pytest -q 2>&1 | tail -3
```

Expected: existing tests still pass; no new tests yet (Bundle 3).

- [ ] **Step 5: Pre-commit + commit Bundle 2**

```bash
pre-commit run --files python/feralrf/enums.py python/feralrf/commands.py python/feralrf/radio.py
git add python/feralrf/enums.py python/feralrf/commands.py python/feralrf/radio.py
git commit -m "$(cat <<'EOF'
feat(f21): Python API — Radio.advertise_ind / advertise_direct / advertise_scan_ind

Adds 3 BLE connectable/scannable advertising methods to Radio class via
new Command.BLE_ADV_LEGACY (0x52) and CommandBuilder.ble_adv_legacy
payload builder. Wire format per F21 spec.

scan_resp_data is plumbed through to firmware (TI cmd auto-responds to
SCAN_REQ within hardware timing), so the criterio 2 del spec works
without warnings.

Helpers: _mac_str_to_le_bytes, _random_mac_le (random static address
default for advertise_ind / advertise_scan_ind when target_addr=None).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: pre-commit clean, commit lands.

---

## Task 3: Bundle 3 — Unit tests

**Files:**
- Create: `python/tests/test_radio_advertise.py`

**Background:** TDD-style: tests verify wire format byte-by-byte and Radio methods send the right command bytes via FakeSerial. NO hardware required.

- [ ] **Step 1: Create test file**

```python
"""F21 — unit tests for Radio.advertise_* methods + CommandBuilder.ble_adv_legacy.

Verifies wire format byte layout and Radio class dispatch via FakeSerial.
NO hardware required.
"""

from typing import List, Optional, Tuple

import pytest

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response
from feralrf.protocol import build_frame, cobs_decode, parse_frame
from feralrf.radio import Radio


class TestBleAdvLegacyPayload:
    """CommandBuilder.ble_adv_legacy wire-format tests."""

    def test_layout_adv_ind(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x0,
            adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            adv_data=b"HELLO",
            scan_rsp_data=b"WORLD",
        )
        # 14 (head) + 1 + 5 + 1 + 5 = 26 bytes
        assert len(p) == 26
        assert p[0] == 0x0
        assert p[1] == 0x1  # addr_type random (default)
        assert p[2:8] == b"\x06\x05\x04\x03\x02\x01"
        assert p[8] == 37
        assert p[9] == 0x00  # power_dbm 0
        # count uint16 LE = 50 → 0x32 0x00
        assert p[10] == 0x32
        assert p[11] == 0x00
        # interval_units uint16 LE = 16 → 0x10 0x00
        assert p[12] == 0x10
        assert p[13] == 0x00
        assert p[14] == 5  # adv_data_len
        assert p[15:20] == b"HELLO"
        assert p[20] == 5  # scan_rsp_len
        assert p[21:26] == b"WORLD"

    def test_layout_adv_direct(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x1,
            adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            init_addr_le=b"\xfe\xee\xdd\xcc\xbb\xaa",
        )
        assert len(p) == 21
        assert p[0] == 0x1
        assert p[14] == 0x1  # init_addr_type random (default)
        assert p[15:21] == b"\xfe\xee\xdd\xcc\xbb\xaa"

    def test_layout_adv_scan_ind(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x6,
            adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            adv_data=b"X",
            scan_rsp_data=b"Y",
        )
        assert p[0] == 0x6
        assert p[14] == 1
        assert p[15:16] == b"X"
        assert p[16] == 1
        assert p[17:18] == b"Y"

    def test_rejects_invalid_pdu_type(self):
        with pytest.raises(ValueError, match="pdu_type"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x2, adv_addr_le=b"\x06\x05\x04\x03\x02\x01"
            )

    def test_rejects_invalid_channel(self):
        with pytest.raises(ValueError, match="channel"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01", channel=36
            )

    def test_rejects_invalid_power(self):
        with pytest.raises(ValueError, match="power_dbm"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01", power_dbm=25
            )

    def test_rejects_oversized_adv_data(self):
        with pytest.raises(ValueError, match="adv_data"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0,
                adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
                adv_data=b"\x00" * 32,
            )

    def test_rejects_oversized_scan_rsp(self):
        with pytest.raises(ValueError, match="scan_rsp_data"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0,
                adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
                scan_rsp_data=b"\x00" * 32,
            )

    def test_direct_requires_init_addr(self):
        with pytest.raises(ValueError, match="init_addr_le"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x1, adv_addr_le=b"\x06\x05\x04\x03\x02\x01"
            )

    def test_rejects_invalid_count(self):
        with pytest.raises(ValueError, match="count"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01", count=0
            )


class FakeSerial:
    """Same FakeSerial pattern as test_gatt_api.py."""

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


class TestRadioAdvertiseMethods:
    """Verify Radio.advertise_* methods send the correct CMD_BLE_ADV_LEGACY frame."""

    def test_advertise_ind_dispatches_pdu_type_0(self):
        radio, fake = _radio_with_fake_serial()
        # ACK arrives with the same seq the radio used. Radio uses _last_seq+1
        # for the next outgoing command; queue_response with seq=current+1.
        fake.queue_response(Response.ACK, seq=(radio._last_seq + 1) & 0xFF)
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            target_addr="DE:AD:BE:EF:CA:FE",
            count=5,
        )
        frames = fake.written_frames()
        assert len(frames) == 1
        cmd_id, _seq, payload = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert payload[0] == 0x0  # pdu_type ADV_IND
        # adv_addr_le is reverse of "DE:AD:BE:EF:CA:FE"
        assert payload[2:8] == b"\xfe\xca\xef\xbe\xad\xde"

    def test_advertise_direct_dispatches_pdu_type_1(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=(radio._last_seq + 1) & 0xFF)
        radio.advertise_direct(
            target_addr="01:02:03:04:05:06",
            init_addr="aa:bb:cc:dd:ee:ff",
            count=3,
        )
        frames = fake.written_frames()
        cmd_id, _, payload = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert payload[0] == 0x1  # pdu_type ADV_DIRECT_IND
        assert payload[2:8] == b"\x06\x05\x04\x03\x02\x01"  # AdvA reversed
        assert payload[15:21] == b"\xff\xee\xdd\xcc\xbb\xaa"  # InitA reversed

    def test_advertise_scan_ind_dispatches_pdu_type_6(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=(radio._last_seq + 1) & 0xFF)
        radio.advertise_scan_ind(
            payload=b"\x02\x01\x06",
            target_addr="DE:AD:BE:EF:CA:FE",
            count=5,
        )
        frames = fake.written_frames()
        cmd_id, _, payload = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert payload[0] == 0x6  # pdu_type ADV_SCAN_IND

    def test_advertise_direct_high_duty_uses_3750us(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=(radio._last_seq + 1) & 0xFF)
        radio.advertise_direct(
            target_addr="01:02:03:04:05:06",
            init_addr="aa:bb:cc:dd:ee:ff",
            mode="high",
            count=3,
        )
        _, _, payload = fake.written_frames()[0]
        # interval_units uint16 LE at offset 12-13. high-duty=3750us / 625 = 6
        interval_units = payload[12] | (payload[13] << 8)
        assert interval_units == 6
```

- [ ] **Step 2: Run tests — verify all pass**

```bash
cd python && PYTHONPATH=. pytest tests/test_radio_advertise.py -v 2>&1 | tail -25
```

Expected: ~14 PASS.

- [ ] **Step 3: Run full Python suite (regression)**

```bash
PYTHONPATH=. pytest -q 2>&1 | tail -3
```

Expected: ≥ 600 pass total (era ~600 después de F17 + nuevos), 0 fail.

- [ ] **Step 4: Pre-commit + commit Bundle 3**

```bash
pre-commit run --files python/tests/test_radio_advertise.py
git add python/tests/test_radio_advertise.py
git commit -m "$(cat <<'EOF'
test(f21): unit tests for advertise_* methods + ble_adv_legacy payload

14 tests in two classes:
- TestBleAdvLegacyPayload: byte-level wire-format validation per pdu_type +
  validation rejects (invalid pdu_type/channel/power/oversized data/missing
  init_addr).
- TestRadioAdvertiseMethods: Radio.advertise_ind / advertise_direct /
  advertise_scan_ind dispatch correct CMD_BLE_ADV_LEGACY frames via
  FakeSerial fixture.

NO hardware required.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: pre-commit clean, commit lands.

---

## Task 4: Bundle 4 — Smoke V1.b + demo

**Files:**
- Create: `python/examples/smoke_f21_advertise.py`
- Create: `python/examples/lab/demo_advertise_connectable.py`

**Background:** Smoke uses byte-inspection of `pkt.data[0] & 0x0F` to extract advertising channel PDU type (no `_ll_parser` change needed). Bonus criterio 2 uses F12 active scanner to verify SCAN_RSP path.

- [ ] **Step 1: Create `smoke_f21_advertise.py`**

```python
#!/usr/bin/env python3
"""F21 — Smoke V1.b cross-validation 2-board.

For each PDU type, TX board emits via advertise_*, RX board captures raw
packets and inspects byte 0 (header byte, bits 3:0 = PDU type).

Pass criteria:
  - ADV_IND: ≥10/20 packets with header & 0x0F == 0x0 + AdvA match
  - ADV_DIRECT_IND: ≥10/20 with header & 0x0F == 0x1 + AdvA match
  - ADV_SCAN_IND: ≥10/20 with header & 0x0F == 0x6 + AdvA match

Bonus criterio 2 (SCAN_RSP via F12 active scanner):
  - For ADV_IND and ADV_SCAN_IND: scanner detects scan_response_data
    matching the value sent.

Usage:
    python smoke_f21_advertise.py --tx-port /dev/ttyACM1 --rx-port /dev/ttyACM2
"""
import argparse
import re
import sys
import time

import serial

from feralrf import PHY, Radio, RxStreamError

ADV_IND_TYPE = 0x0
ADV_DIRECT_IND_TYPE = 0x1
ADV_SCAN_IND_TYPE = 0x6

PDU_TYPE_NAMES = {
    ADV_IND_TYPE: "ADV_IND",
    ADV_DIRECT_IND_TYPE: "ADV_DIRECT_IND",
    ADV_SCAN_IND_TYPE: "ADV_SCAN_IND",
}


def reset_cc1352(port: str) -> None:
    """Reset CC1352 via RP2040 shell port (data port + 2)."""
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


def run_pdu_type(tx_port, rx_port, baud, pdu_name, count):
    """TX board advertises one PDU type; RX board collects packets and
    classifies by header byte. Returns (matched, total)."""
    reset_cc1352(tx_port)
    reset_cc1352(rx_port)
    tx = Radio(port=tx_port, baudrate=baud)
    rx = Radio(port=rx_port, baudrate=baud)

    target_mac_str = "DE:AD:BE:EF:CA:FE"
    target_mac_le = bytes.fromhex("FECAEFBEADDE")  # reverse of target

    try:
        tx.init()
        rx.init()
        rx.set_phy(PHY.BLE_1M, channel=37)
        rx.start_rx()
        time.sleep(0.3)

        if pdu_name == "ADV_IND":
            tx.advertise_ind(
                payload=b"\x02\x01\x06",
                scan_resp_data=b"FERAL_SCAN_RSP",
                target_addr=target_mac_str,
                count=count,
            )
            expected_type = ADV_IND_TYPE
        elif pdu_name == "ADV_DIRECT_IND":
            tx.advertise_direct(
                target_addr=target_mac_str,
                init_addr="11:22:33:44:55:66",
                mode="low",
                count=count,
            )
            expected_type = ADV_DIRECT_IND_TYPE
        elif pdu_name == "ADV_SCAN_IND":
            tx.advertise_scan_ind(
                payload=b"\x02\x01\x06",
                scan_resp_data=b"FERAL_SCAN_RSP",
                target_addr=target_mac_str,
                count=count,
            )
            expected_type = ADV_SCAN_IND_TYPE
        else:
            raise ValueError(f"unknown pdu_name: {pdu_name}")

        time.sleep(1.0)

        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=3.0):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            if len(pkt.data) < 8:
                continue
            pdu_type = pkt.data[0] & 0x0F
            adv_addr = pkt.data[2:8]
            if pdu_type == expected_type and adv_addr == target_mac_le:
                matched += 1

        try:
            rx.stop_rx()
        except Exception:
            pass

        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def run_with_retry(fn, *args):
    last_exc = None
    for attempt in range(2):
        try:
            return fn(*args), None
        except Exception as exc:
            last_exc = exc
            if attempt == 0:
                print(f"  [WARN] {exc!r} — retry 1/2")
    return (0, 0), last_exc


def main() -> int:
    parser = argparse.ArgumentParser(description="F21 smoke V1.b")
    parser.add_argument("--tx-port", required=True)
    parser.add_argument("--rx-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument("--threshold", type=int, default=10)
    args = parser.parse_args()

    print("F21 BLE Connectable Advertiser smoke V1.b")
    print(f"TX={args.tx_port} RX={args.rx_port} count={args.count} threshold>={args.threshold}")
    print("=" * 60)

    pdu_names = ("ADV_IND", "ADV_DIRECT_IND", "ADV_SCAN_IND")
    results = []
    for name in pdu_names:
        print(f"\n[ -- ] {name}")
        (matched, total), exc = run_with_retry(
            run_pdu_type, args.tx_port, args.rx_port, args.baudrate, name, args.count
        )
        if exc is not None:
            print(f"[FAIL] {name}: exception {exc!r}")
            results.append((name, 0, 0, False))
            continue
        passed = matched >= args.threshold
        status = "PASS" if passed else "FAIL"
        print(f"[{status}] {name}: matched={matched}/{args.count} total_rx={total}")
        results.append((name, matched, total, passed))

    print("\n" + "=" * 60)
    presets_passed = sum(r[3] for r in results)
    print(f"Aggregate: {presets_passed}/{len(results)} PDU types pass")
    return 0 if presets_passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Create `demo_advertise_connectable.py`**

```python
#!/usr/bin/env python3
"""F21 — Demo BLE connectable advertiser. Useful for nRF Connect manual checkpoint.

Loops a chosen PDU type until Ctrl-C. Each loop iteration emits N adv events.
"""
import argparse
import sys

from feralrf.radio import Radio


def main() -> int:
    parser = argparse.ArgumentParser(description="F21 BLE connectable advertiser demo")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument(
        "--pdu-type",
        default="ind",
        choices=("ind", "direct", "scan_ind"),
        help="ADV_IND | ADV_DIRECT_IND | ADV_SCAN_IND",
    )
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    parser.add_argument(
        "--init-mac", default="11:22:33:44:55:66", help="Only used for direct mode"
    )
    parser.add_argument("--count", type=int, default=50)
    parser.add_argument("--channel", type=int, default=37)
    parser.add_argument("--power", type=int, default=0)
    parser.add_argument("--mode", default="low", choices=("low", "high"))
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        print(
            f"Advertising {args.pdu_type} as {args.target_mac} on ch{args.channel}; "
            "Ctrl-C to stop"
        )
        while True:
            if args.pdu_type == "ind":
                radio.advertise_ind(
                    payload=b"\x02\x01\x06\x09\x09" + b"FERAL_AP",
                    scan_resp_data=b"FERAL_SCAN_RSP",
                    target_addr=args.target_mac,
                    count=args.count,
                    channel=args.channel,
                    power_dbm=args.power,
                )
            elif args.pdu_type == "direct":
                radio.advertise_direct(
                    target_addr=args.target_mac,
                    init_addr=args.init_mac,
                    mode=args.mode,
                    count=args.count,
                    channel=args.channel,
                    power_dbm=args.power,
                )
            else:  # scan_ind
                radio.advertise_scan_ind(
                    payload=b"\x02\x01\x06\x09\x09" + b"FERAL_AP",
                    scan_resp_data=b"FERAL_SCAN_RSP",
                    target_addr=args.target_mac,
                    count=args.count,
                    channel=args.channel,
                    power_dbm=args.power,
                )
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 3: Verify smoke + demo parse + --help**

```bash
cd python && for f in examples/smoke_f21_advertise.py examples/lab/demo_advertise_connectable.py; do
    PYTHONPATH=. python -c "import ast; ast.parse(open('$f').read())" && echo "$f: parse OK" || echo "$f: PARSE FAIL"
    PYTHONPATH=. python "$f" --help 2>&1 | head -1
    echo "---"
done
```

Expected: 2 × `parse OK` + 2 × `usage:` lines.

- [ ] **Step 4: HUMAN CHECKPOINT — confirmar 2 boards disponibles + reflash con firmware F21**

```bash
ls /dev/ttyACM* && cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip && python -m catnip devices
```

Expected: 2 CatSniffer devices listed; data ports `/dev/ttyACM1` (TX) y `/dev/ttyACM2` (RX).

The boards need the new firmware (F21 commands). Flash both:

```bash
# Reset board #1 baseline build with F21
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex

python -m catnip flash -d 2 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

Per memory `feedback_flash_retry`: retry 2× before asking for manual reset.

**STOP and pause before running smoke if either flash fails twice.**

- [ ] **Step 5: Run smoke**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
PYTHONPATH=. python examples/smoke_f21_advertise.py \
    --tx-port /dev/ttyACM1 --rx-port /dev/ttyACM2 --count 20 --threshold 10 2>&1 | tail -25
```

Expected: `Aggregate: 3/3 PDU types pass`, exit 0.

If a PDU type fails (matched < 10):
- Verify firmware actually contains the new command: `gdb-multiarch firmware/cc1352/build/feralrf_cc1352.elf -batch -ex 'info symbol Ble5_0_cmdBleAdv'`
- Try channel 38 or 39 if 37 has interference: `--channel 38` (but smoke script doesn't expose this — modify run_pdu_type to accept channel)
- Inspect raw RX bytes: add `print(pkt.data.hex())` temporarily inside the matching loop to see what's coming through

- [ ] **Step 6: Pre-commit + commit Bundle 4**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/examples/smoke_f21_advertise.py python/examples/lab/demo_advertise_connectable.py
git add python/examples/smoke_f21_advertise.py python/examples/lab/demo_advertise_connectable.py
git commit -m "$(cat <<'EOF'
test(f21): smoke V1.b cross-validation 2-board + demo lab

Smoke harness that verifies all 3 BLE PDU types are correctly classified
on the air via byte-inspection of pkt.data[0] & 0x0F. Pass: ≥10/20 per
PDU type with matching AdvA. Reuses retry-on-TimeoutError pattern from
F29.b.

Demo (demo_advertise_connectable.py) lets a developer manually run any
PDU type for nRF Connect verification (V2 manual checkpoint, optional).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: pre-commit clean, commit lands.

---

## Task 5: Tag, memory, FF merge

**Files:**
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f21_done.md` (new)
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` (extend)
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_next_session.md` (mark F21 done)

- [ ] **Step 1: Verify bundle commits**

```bash
git log --oneline main..HEAD
```

Expected: 4-5 commits — feat(f21) firmware, feat(f21) Python, test(f21) unit tests, test(f21) smoke + demo.

- [ ] **Step 2: Tag `v2.0-f21`**

```bash
git tag -a v2.0-f21 -m "F21 — BLE Connectable advertiser. CMD_BLE_ADV_LEGACY (0x52) dispatching 3 TI legacy adv commands. Smoke V1.b 3/3 PDU types classified."
git tag -l | grep f21
```

Expected: `v2.0-f21` present.

- [ ] **Step 3: Create `project_f21_done.md`**

```bash
cat > ~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f21_done.md <<'EOF'
---
name: project_f21_done
description: F21 BLE Connectable advertiser closed — CMD_BLE_ADV_LEGACY firmware + 3 Radio methods + smoke V1.b 3/3
type: project
---

F21 CLOSED 2026-05-04. Branch `feature/f21-conn-advertiser`, tag `v2.0-f21`. FF'd into `main`.

**Firmware:**
- `CMD_BLE_ADV_LEGACY` (0x52) en `protocol.h`. Single host command,
  pdu_type field selecciona TI BLE legacy adv command:
  - `0x0` → `Ble5_0_cmdBleAdv` (0x1805 ADV_IND)
  - `0x1` → `Ble5_0_cmdBleAdvDir` (0x1806 ADV_DIRECT_IND)
  - `0x6` → `Ble5_0_cmdBleAdvScan` (0x1808 ADV_SCAN_IND)
- 3 TI struct definitions agregadas a `smartrf_ble5_0.c` (no estaban en
  SmartRF config previa). `s_f21_bleAdvPar` separado de `s_bleAdvPar` para
  no afectar path NC pre-existente.
- `RadioIF_transmitBleAdvLegacy` en `radio_if.c` con dispatch interno.
  CONNECT_IND: TI status `BLE_DONE_CONNECT` (0x1FFF) → loop break, no F20.

**Python:**
- `Command.BLE_ADV_LEGACY = 0x52` en `enums.py`.
- `CommandBuilder.ble_adv_legacy()` en `commands.py` con validation completa.
- 3 métodos en `Radio` class: `advertise_ind`, `advertise_direct`,
  `advertise_scan_ind`. `scan_resp_data` plumbed al firmware (TI CPE
  responde SCAN_REQ automáticamente).
- Helpers: `_mac_str_to_le_bytes`, `_random_mac_le`.

**Smoke OTA V1.b:** 3/3 PDU types correctly classified vía
byte-inspection `pkt.data[0] & 0x0F`. TX board #2 (`/dev/ttyACM1`),
RX board #1 (`/dev/ttyACM2`).

**Tests:** ~14 unit tests (TestBleAdvLegacyPayload + TestRadioAdvertiseMethods),
suite total ≥ 600 pass.

**Lecciones:**
- TI legacy adv commands NO estaban en SmartRF config; agregadas a mano
  (precedente F25 hand-edit per `project_syscfg_handedited`).
- `_ll_parser.LLPduKind` es solo para LL Data PDUs (post-connection); no
  cubre advertising channel PDUs. Smoke usa byte-inspection directo.
- TI CPE handles SCAN_REQ→SCAN_RSP automático en hardware (~150 µs IFC).
  Sin trabajo extra para criterio 2 del spec.

**Why:** Master plan v2.0 §F21. PDU types correctos + SCAN_RSP funcional.
Stack F17 → F21 → F20 (peripheral GATT server, fase separada).

**How to apply:** Para advertir como un device específico, usar
`radio.advertise_ind(payload=..., scan_resp_data=..., target_addr=...)`.
Para scan-only no-connectable, `advertise_scan_ind`. Para directed pairing
solicit, `advertise_direct(target_addr, init_addr, mode='low'|'high')`.

**Out of scope:**
- F20 — peripheral role + GATT server (CONNECT_IND completo)
- BLE 2M / Coded extended advertising
- Channel hopping interno (user envuelve)
- `advFilterPolicy` configurable (default = allow any)
- ADV_NONCONN_IND vía este path (sigue vía CMD_TX_RAW + adv_spoof)
- F17 personalities migration (sigue ADV_NONCONN_IND, opcional F17.b)
EOF
```

- [ ] **Step 4: Update `MEMORY.md` index**

Edit `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md`. Add after `project_f17_done`:

```
- [project_f21_done.md](project_f21_done.md) — 2026-05-04 F21 closed. Tag v2.0-f21. CMD_BLE_ADV_LEGACY (0x52) firmware + 3 Radio methods (advertise_ind / advertise_direct / advertise_scan_ind). Smoke V1.b 3/3 PDU types. SCAN_RSP gratis vía TI CPE.
```

- [ ] **Step 5: Update `project_next_session.md`**

Edit el archivo. Buscar la entry de F21 en candidate priorities y reemplazar:

```
### ~~6. F21 — BLE Connectable advertiser~~ ✅ CLOSED 2026-05-04
Tag `v2.0-f21`. CMD_BLE_ADV_LEGACY firmware + 3 Radio methods + smoke V1.b
3/3. Stack F17 → F21 → F20 progresa. F20 (peripheral + GATT server) sigue
como next del stack.
```

Insertar nueva entry para F20:

```
### 6. F20 — BLE Peripheral + GATT server (full role)
Última pieza del stack F17/F21/F20. Master plan §F20. ATT server completo
+ L2CAP fixed channels + connection management. Trabajo grande (~1500 LOC
firmware). Considerar reuso de TI BLE5-Stack OneLib si Sniffle-style
context lo permite. Validation: nRF Connect descubre services + Read/Write/
Notify funcionan + conexión sostiene 60s sin timeout.
```

- [ ] **Step 6: FF merge to `main`**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git checkout main
git merge --ff-only feature/f21-conn-advertiser
git update-ref refs/heads/feature/ti-rtos-migration main
git log --oneline -8
```

Expected: FF succeeds.

- [ ] **Step 7: Delete local branch**

```bash
git branch -d feature/f21-conn-advertiser
git branch
```

Expected: branch deleted.

- [ ] **Step 8: Push (interactive desde shell del usuario)**

User runs:
```
! git push origin main feature/ti-rtos-migration v2.0-f21
```

NO push autónomo per safety rules.

---

## Self-review checklist

- [ ] **Spec coverage:** Spec lista firmware (1 cmd + 3 TI dispatch), Python (3 methods + helpers), tests, smoke V1.b, demo. Plan tiene Task 1 (firmware), Task 2 (Python), Task 3 (unit tests), Task 4 (smoke + demo), Task 5 (tag + memory + FF). ✅
- [ ] **Placeholder scan:** No "TBD"/"TODO". Code blocks completos. ✅
- [ ] **Type/symbol consistency:** `Command.BLE_ADV_LEGACY`, `CommandBuilder.ble_adv_legacy`, `_mac_str_to_le_bytes`, `_random_mac_le`, `Radio.advertise_ind/direct/scan_ind`, `RadioIF_transmitBleAdvLegacy`, `Ble5_0_cmdBleAdv/Dir/Scan`, `s_f21_bleAdvPar` consistentes en todas las tasks. ✅
- [ ] **Hardware:** TX board #2 / RX board #1 heredados de F17. Reflash necesario (firmware nuevo) — paso explícito en Task 4 step 4. ✅
- [ ] **Compat:** Existing CMD_TX_RAW + ADV_NONCONN_IND path NO se modifica. Smoke F11 attacks debe seguir pasando. ✅
- [ ] **Prerequisites verified:** TI symbols faltantes (Bundle 1 step 2 los agrega), LLPduKind no cubre adv (smoke usa byte-inspection), helpers existentes reutilizados. ✅
