# Remove BLE Protocol Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the BLE protocol/application layer (connection, GATT/ATT, link-layer following, advertising, active scanning) from the CC1352 firmware and the `feralrf` Python stack, while keeping the BLE PHY for raw capture, the shared `ll_manager` RX classifier, crypto, and jamming.

**Architecture:** This is a deletion/refactor, not a feature build. It proceeds Python-first (each task keeps `pytest` green and the package importable), then firmware (a single atomic edit-and-delete that must build as one unit), then examples and docs. Every task ends in a verifiable state: kept tests pass, removed symbols grep-clean, firmware still builds.

**Tech Stack:** Python 3.9+ (`feralrf`, pytest), C / TI-RTOS 7 / TI SimpleLink SDK 8.30.01.01 (CC1352P7 firmware), CMake.

**Source spec:** `docs/superpowers/specs/2026-07-20-remove-ble-protocol-layer-design.md`

## Global Constraints

- Output is plain ASCII: no emojis, no em/en dashes, no fancy Unicode. Applies to code, comments, and commit messages.
- Commits are authored as the user's own work: no AI/Claude attribution, co-author trailer, or "generated with" line.
- Do NOT renumber command/response IDs. Removed IDs become unused and return `ERR_INVALID_CMD (0x01)`.
- KEEP the BLE PHY: `PHY.BLE_1M/BLE_2M/BLE_CODED_S8/BLE_CODED_S2`, `smartrf_ble5_0.c`, and the `radio_if.c`/`phy_manager.c` BLE-PHY code stay.
- KEEP `ll_manager.c/.h` (shared RX classifier), `crypto_engine.c`, and jamming.
- `feralrf.__version__` becomes `0.3.0` (removing public API is a breaking change).
- Commit style follows the repo: scoped conventional commits (e.g. `refactor(ble): ...`).
- This work lands on a dedicated branch (see Task 0). `git revert`/branch delete restores everything.

---

### Task 0: Create the working branch

**Files:** none (git only).

- [ ] **Step 1: Branch off the current state**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git checkout -b feature/remove-ble-protocol
```
Expected: `Switched to a new branch 'feature/remove-ble-protocol'`

- [ ] **Step 2: Confirm baseline is green before removing anything**

Run:
```bash
cd python && python -m pytest -q
```
Expected: all tests pass (this is the pre-change baseline; note the count).

---

### Task 1: Delete BLE tests and trim mixed tests

Remove the tests for the code we are about to delete, first, so the suite stays green at every later step.

**Files:**
- Delete: `python/tests/test_attacks_ble.py`, `python/tests/test_ble_scan.py`, `python/tests/test_connect_ind_pdu.py`, `python/tests/test_disconnect_events.py`, `python/tests/test_follow_connection.py`, `python/tests/test_gatt_api.py`, `python/tests/test_gatt_integration.py`, `python/tests/test_gatt_notifications.py`, `python/tests/test_ll_parser.py`, `python/tests/test_radio_advertise.py`, `python/tests/test_debug_timing.py`
- Modify: `python/tests/test_emulation.py`, `python/tests/test_commands_contract.py`, and any test importing a removed symbol (`test_enums_no_collisions.py`, `test_radio_seq.py`, `test_async_error_surfacing.py`)

- [ ] **Step 1: Delete the pure-BLE test files**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python/tests
git rm test_attacks_ble.py test_ble_scan.py test_connect_ind_pdu.py \
  test_disconnect_events.py test_follow_connection.py test_gatt_api.py \
  test_gatt_integration.py test_gatt_notifications.py test_ll_parser.py \
  test_radio_advertise.py test_debug_timing.py
```
Expected: `rm 'test_attacks_ble.py' ...` (11 files staged for removal).

- [ ] **Step 2: Trim BLE cases from mixed tests**

In `test_emulation.py`: remove any test function or parametrization that imports/uses `ble_peripheral`, `emulate_ble`, `BlePersonality`, `SOUNDCORE_BOOM_2`, `APPLE_AIRPODS_PRO`, `GOOGLE_FASTPAIR_GENERIC`, or `BLE_PERSONALITIES`. Keep the IEEE154 / OOK / Sub-1GHz cases.

In `test_commands_contract.py`: remove assertions referencing any removed `Command`/`Response` member (see Task 4 lists) — e.g. `Command.CONNECT`, `Command.GATT_*`, `Command.FOLLOW_*`, `Command.BLE_ADV_LEGACY`, `Command.SET_BLE_ADDR`, `Command.SET_BLE_SCAN_MODE`, `Response.GATT_*`, `Response.CONN_*`, `Response.DEBUG_*`, `Response.LL_PACKET`, `Response.FOLLOW_DONE`, `Response.DISCONNECTED`.

In `test_enums_no_collisions.py`, `test_radio_seq.py`, `test_async_error_surfacing.py`: grep each for the removed symbol names and delete/adjust only the lines that reference them. (Run `grep -nE "gatt|conn_status|advertise|follow|ble_scan|set_ble_addr|DisconnectEvent|GattService|ConnectionResult" test_*.py` to locate.)

- [ ] **Step 3: Run the suite**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python && python -m pytest -q
```
Expected: PASS, with the BLE test files gone. No collection/import errors.

- [ ] **Step 4: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A python/tests
git commit -m "test(ble): remove BLE protocol tests ahead of stack removal"
```

---

### Task 2: Remove BLE from the Python API surface

Strip BLE methods, dataclasses, command builders, and response parsers so the package stays importable with no BLE surface. This is one coordinated task because `radio.py`, `__init__.py`, `commands.py`, and `_responses.py` are mutually referencing.

**Files:**
- Modify: `python/feralrf/radio.py`
- Modify: `python/feralrf/commands.py`
- Modify: `python/feralrf/_responses.py`
- Modify: `python/feralrf/__init__.py`

**Interfaces:**
- Produces: a `Radio` class with only non-BLE methods; `feralrf` exports without BLE dataclasses; `CommandBuilder` without BLE payload builders. Task 4 (enums) relies on no remaining references to the removed `Command`/`Response` members after this task.

- [ ] **Step 1: Remove BLE imports at the top of `radio.py`**

Delete these two import lines (`radio.py:12-13`):
```python
from feralrf._ble_scan import BleScanResult, extract_pdu_header
from feralrf._responses import DebugConnParamsResponse, DebugTimingResponse
```
Keep the other imports. (`DebugTimingResponse`/`DebugConnParamsResponse` are removed from `_responses.py` in Step 4, and their only consumers — `debug_timing`/`debug_conn_params` — are removed in Step 2.)

- [ ] **Step 2: Remove the BLE methods from the `Radio` class**

Delete these methods in full (bodies included): `set_ble_addr`, `set_ble_addr_str`, `advertise_ind`, `advertise_direct`, `advertise_scan_ind`, `ble_connect`, `ble_disconnect`, `conn_status`, `debug_timing`, `debug_conn_params`, `gatt_discover`, `gatt_read`, `gatt_write`, `gatt_subscribe`, `gatt_exchange_mtu`, `gatt_read_by_uuid`, `read_gatt_notifications`, `read_disconnect_events`, `follow_connection`, `stop_follow_connection`, `read_ll_packets`, `set_ble_scan_mode`, `scan_ble_active`.

KEEP: `set_adv_hop`, `set_phy`, `set_channel`, `set_power`, `init`, `connect`, `disconnect`, `reset_device`, `start_rx`/`stop_rx`/`read_packets`/`read_one_packet`, all `transmit*`, `tx_cw`/`tx_prbs`/`tx_test_stop`, `configure_prop`, `get_stats`, and all crypto methods (`random_bytes`, `aes_*`, `sha256`, `ecdsa_*`, `_aes_*`, `_resolve_curve_id`).

- [ ] **Step 3: Remove the BLE dataclasses from `radio.py`**

Delete these dataclass definitions: `ConnectionResult`, `ConnectionStatus`, `GattService`, `GattCharacteristic`, `GattAttribute`, `GattDiscoveryResult`, `DisconnectEvent`, `GattNotification`, `LLPacket`. KEEP `Packet`, `DeviceInfo`, `DeviceStats`, `RxStreamError`.

- [ ] **Step 4: Remove BLE parsers from `_responses.py`**

Delete the dataclasses `DebugTimingEntry`, `DebugTimingResponse`, `DebugConnParamsResponse`. KEEP `RxPacketResponse` (including its optional `ll_pdu_*` fields — the firmware still emits them via the kept `ll_manager`), `SpectrumDataResponse`, `InfoResponse`.

- [ ] **Step 5: Remove BLE payload builders from `commands.py`**

In the `CommandBuilder` class delete: `set_ble_addr`, `ble_connect`, `ble_disconnect`, `conn_status`, `gatt_discover`, `gatt_read`, `debug_conn_params`, `gatt_write`, `gatt_subscribe`, `gatt_exchange_mtu`, `gatt_read_by_uuid`, `follow_start`, `follow_stop`, and the `ble_adv_legacy` builder. KEEP the `set_adv_hop` payload builder and `spectrum_scan`.

- [ ] **Step 6: Update `__init__.py` exports, docstring, and version**

Remove `ConnectionResult`, `ConnectionStatus`, `GattService`, `GattCharacteristic`, `GattDiscoveryResult` from both the `from feralrf.radio import (...)` block and `__all__`. Set `__version__ = "0.3.0"`. Edit the module docstring to drop BLE scan/GATT/initiator wording, e.g. replace the "Public API status" lines with:
```python
"""
FeralRF - Python API for CatSniffer RF pentesting

Supports: IEEE 802.15.4 (Zigbee/Thread), Sub-1GHz, OOK, GFSK/FSK, plus raw
BLE-PHY capture (no BLE protocol stack; use Sniffle for BLE).

Public API status:
- Stable: session control, RX/TX multi-PHY, proprietary configuration, OOK recovery.
- Experimental: jamming helpers.
- Pending: spectrum helpers.
"""
```

- [ ] **Step 7: Verify import and run the suite**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python
python -c "import feralrf; from feralrf import Radio, PHY; print(feralrf.__version__)"
python -m pytest -q
```
Expected: prints `0.3.0`; pytest PASS with no import errors.

- [ ] **Step 8: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A python/feralrf/radio.py python/feralrf/commands.py python/feralrf/_responses.py python/feralrf/__init__.py
git commit -m "refactor(ble): remove BLE methods, dataclasses, and builders from Python API"
```

---

### Task 3: Delete BLE-only Python modules

Now that nothing imports them, delete the BLE modules and trim the emulation package.

**Files:**
- Delete: `python/feralrf/ble/` (dir), `python/feralrf/attacks/` (dir), `python/feralrf/emulation/ble_peripheral.py`, `python/feralrf/_ble_scan.py`, `python/feralrf/_ll_parser.py`
- Modify: `python/feralrf/emulation/__init__.py`

- [ ] **Step 1: Edit `emulation/__init__.py`**

Remove the two `from feralrf.emulation.ble_peripheral import ...` import blocks and the BLE entries from `__all__`: `BlePersonality`, `SOUNDCORE_BOOM_2`, `APPLE_AIRPODS_PRO`, `GOOGLE_FASTPAIR_GENERIC`, `BLE_PERSONALITIES`, `emulate_ble`. Keep the IEEE154 / OOK / Sub-1GHz imports and `__all__` entries.

- [ ] **Step 2: Delete the modules**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python/feralrf
git rm -r ble attacks
git rm emulation/ble_peripheral.py _ble_scan.py _ll_parser.py
```
Expected: files staged for removal.

- [ ] **Step 3: Verify import, suite, and grep-clean**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python
python -c "import feralrf, feralrf.emulation; print('ok')"
python -m pytest -q
grep -rInE "gatt|ble_conn|ll_follower|att_client|advertise|scan_ble|connect_ind|ble_peripheral" feralrf/ || echo "CLEAN"
```
Expected: prints `ok`; pytest PASS; grep prints `CLEAN` (BLE-PHY enum names in `enums.py` and `set_adv_hop` are allowed and won't match these patterns).

- [ ] **Step 4: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A python/feralrf
git commit -m "refactor(ble): delete BLE Python modules (ble, attacks, ble_peripheral, scanners)"
```

---

### Task 4: Remove BLE command/response IDs from `enums.py`

**Files:**
- Modify: `python/feralrf/enums.py`

- [ ] **Step 1: Remove BLE `Command` members**

Delete these from the `Command` IntEnum: `SET_BLE_ADDR (0x09)`, `SET_BLE_SCAN_MODE (0x0B)`, `CONNECT (0x40)`, `DISCONNECT (0x41)`, `CONN_STATUS (0x42)`, `GATT_DISCOVER (0x43)`, `GATT_SUBSCRIBE (0x44)`, `GATT_READ (0x45)`, `GATT_WRITE (0x46)`, `DEBUG_TIMING (0x47)`, `DEBUG_CONN_PARAMS (0x48)`, `GATT_EXCHANGE_MTU (0x4A)`, `GATT_READ_BY_UUID (0x4B)`, `BLE_ADV_LEGACY (0x52)`, `FOLLOW_START (0x50)`, `FOLLOW_STOP (0x51)`. KEEP `SET_ADV_HOP (0x07)` and all others.

- [ ] **Step 2: Remove BLE `Response` members**

Delete from the `Response` IntEnum: `CONN_RESULT (0xA0)`, `CONN_STATUS (0xA1)`, `GATT_SERVICE (0xA2)`, `GATT_CHAR (0xA3)`, `GATT_READ_VALUE (0xA4)`, `GATT_DONE (0xA5)`, `GATT_NOTIFY (0xA6)`, `DEBUG_TIMING (0xA8)`, `DEBUG_CONN_PARAMS (0xA9)`, `LL_PACKET (0xAB)`, `FOLLOW_DONE (0xAC)`, `GATT_MTU (0xB0)`, `GATT_ATTRIBUTE (0xB1)`, `DISCONNECTED (0xB2)`. KEEP `ACK`, `ERROR`, `RX_PACKET`, `STATS`, `INFO`, and the crypto responses `0x95-0x9C`.

- [ ] **Step 3: Fix the `STABLE_COMMANDS` tuple**

Remove `Command.SET_BLE_ADDR`, `Command.SET_BLE_SCAN_MODE`, and `Command.GATT_SUBSCRIBE` from `STABLE_COMMANDS`. Keep `Command.SET_ADV_HOP`. Leave `EXPERIMENTAL_COMMANDS` (jamming) and `PENDING_COMMAND_IDS` as-is.

- [ ] **Step 4: Verify**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python
python -c "from feralrf.enums import Command, Response, STABLE_COMMANDS; print(len(Command), len(Response))"
python -m pytest -q
```
Expected: prints reduced counts; pytest PASS.

- [ ] **Step 5: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A python/feralrf/enums.py
git commit -m "refactor(ble): drop BLE command and response IDs from the wire enum"
```

---

### Task 5: Remove BLE from the CC1352 firmware

One atomic firmware change: edit the integration points, delete the 6 BLE file pairs, and rewire CMake. The firmware only builds once all steps are done, so verify with a single build (or grep + CMake-configure where the TI SDK/toolchain is unavailable).

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`, `firmware/cc1352/src/control_task.c`, `firmware/cc1352/src/main_rtos.c`, `firmware/cc1352/src/radio_if.c`, `firmware/cc1352/include/protocol.h`, `firmware/cc1352/CMakeLists.txt`
- Delete: `src/ble_conn.c`+`include/ble_conn.h`, `src/ble_conn_mgr.c`+`include/ble_conn_mgr.h`, `src/ble_conn_pdu.c`+`include/ble_conn_pdu.h`, `src/ll_follower.c`+`include/ll_follower.h`, `src/att_client.c`+`include/att_client.h`, `src/csa2.c`+`include/csa2.h`

- [ ] **Step 1: Edit `include/protocol.h`**

Delete the `CMD_*` defines: `CMD_SET_BLE_ADDR (0x09)`, `CMD_SET_BLE_SCAN_MODE (0x0B)`, `CMD_CONNECT (0x40)`, `CMD_DISCONNECT (0x41)`, `CMD_CONN_STATUS (0x42)`, `CMD_GATT_DISCOVER (0x43)`, `CMD_GATT_SUBSCRIBE (0x44)`, `CMD_GATT_READ (0x45)`, `CMD_GATT_WRITE (0x46)`, `CMD_GATT_EXCHANGE_MTU (0x4A)`, `CMD_GATT_READ_BY_UUID (0x4B)`, `CMD_DEBUG_TIMING (0x47)`, `CMD_DEBUG_CONN_PARAMS (0x48)`, `CMD_ATT_DEBUG (0x49)`, `CMD_FOLLOW_START (0x50)`, `CMD_FOLLOW_STOP (0x51)`, `CMD_BLE_ADV_LEGACY (0x52)`, `CMD_FOLLOW_DEBUG (0x54)`.
Delete the `RSP_*` defines: `RSP_CONN_RESULT (0xA0)`, `RSP_CONN_STATUS_R (0xA1)`, `RSP_GATT_SERVICE (0xA2)`, `RSP_GATT_CHAR (0xA3)`, `RSP_GATT_READ_R (0xA4)`, `RSP_GATT_DONE (0xA5)`, `RSP_GATT_NOTIFY (0xA6)`, `RSP_DEBUG_TIMING (0xA8)`, `RSP_DEBUG_CONN_PARAMS (0xA9)`, `RSP_ATT_DEBUG (0xAA)`, `RSP_LL_PACKET (0xAB)`, `RSP_FOLLOW_DONE (0xAC)`, `RSP_FOLLOW_DEBUG (0xAF)`, `RSP_GATT_MTU (0xB0)`, `RSP_GATT_ATTRIBUTE (0xB1)`, `RSP_DISCONNECTED (0xB2)`.
Keep `SET_ADV_HOP (0x07)` and all kept IDs. Add one comment where the BLE block was:
```c
/* 0x09,0x0B,0x40-0x54 and RSP 0xA0-0xB2 retired: BLE protocol stack removed.
 * Do not reuse these IDs. BLE PHY raw capture remains via SET_PHY + RX_START. */
```

- [ ] **Step 2: Edit `src/command_processor.c`**

Remove the `#include` lines for `att_client.h`, `ble_conn.h`, `ble_conn_mgr.h`, `ll_follower.h`. KEEP `#include "ll_manager.h"`.
Remove the GATT ATT callback block (`s_gatt_seq`, `s_gatt_subscribe_pending`, `gatt_on_service`/`gatt_on_char`/`gatt_on_read`/`gatt_on_done`/`gatt_on_mtu`/`gatt_on_attribute`/`gatt_on_disconnected`, `gatt_callbacks_installed`, `ensure_gatt_callbacks`).
Remove the follower helpers (`follower_on_done` and the `LlFollower_setCallbacks` wiring) and the `RSP_FOLLOW_DONE` sender.
Remove each `case CMD_*:` handler for the commands deleted in Step 1.
KEEP the `GET_STATS` handler including its `LLManager_getStats()` / `ll_stats` usage.
Locate each block with: `grep -nE "gatt_on_|CMD_CONNECT|CMD_GATT|CMD_FOLLOW|CMD_DEBUG_|CMD_BLE_ADV|CMD_SET_BLE|BleConn|LlFollower|AttClient" firmware/cc1352/src/command_processor.c`.

- [ ] **Step 3: Edit `src/control_task.c`**

Remove `#include "ble_conn.h"` and `#include "ll_follower.h"`; KEEP `#include "ll_manager.h"`.
Remove `BleConn_init();` (in `ControlTask_init`, ~L211, and `ControlTask_onRadioInit`, ~L227) and `LlFollower_init();` (~L212).
Replace the body of `ControlTask_poll` (~L459-463) so it is an empty no-op:
```c
void ControlTask_poll(void) {
    /* BLE connection follower removed; nothing to pump here. */
}
```
KEEP `LLManager_resetStats();` (~L226) and `LLManager_select(...)` (~L235).

- [ ] **Step 4: Edit `src/main_rtos.c`**

Remove `#include "ble_conn_mgr.h"` (~L42). In `RfTask_taskFxn`, remove the block (~L183-188):
```c
        /* Run BLE central connection events here ... */
        if (BleConnMgr_isRunning()) {
            BleConnMgr_poll();
        }
```
Trim the two stale comments mentioning `BleConnMgr_poll()` (~L126-128 in the UART loop and the one just removed).

- [ ] **Step 5: Fix the stale comment in `src/radio_if.c`**

At ~L2663 change the comment that reads "Called from BleConnMgr_poll()..." to reflect that BLE central polling is gone, e.g. "Called during RX teardown; s_rx_running is false during ...". Comment only.

- [ ] **Step 6: Delete the 6 BLE file pairs and rewire CMake**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/firmware/cc1352
git rm src/ble_conn.c include/ble_conn.h \
       src/ble_conn_mgr.c include/ble_conn_mgr.h \
       src/ble_conn_pdu.c include/ble_conn_pdu.h \
       src/ll_follower.c include/ll_follower.h \
       src/att_client.c include/att_client.h \
       src/csa2.c include/csa2.h
```
Then in `CMakeLists.txt`, remove these lines from the `APP_SOURCES` list: `src/ble_conn.c`, `src/ble_conn_mgr.c`, `src/ble_conn_pdu.c`, `src/csa2.c`, `src/ll_follower.c`, `src/att_client.c`. Leave `src/ll_manager.c` in `APP_SOURCES`.

- [ ] **Step 7: Verify grep-clean, then build**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/firmware/cc1352
grep -rInE "BleConn|BleConnMgr|BleConnPdu|LlFollower|AttClient|Csa2|CMD_GATT|CMD_CONNECT|CMD_FOLLOW" src include && echo "STILL REFERENCED" || echo "CLEAN"
```
Expected: `CLEAN` (the `useCsa2` struct field lived only in removed CONN_STATUS handlers).

Then, if the TI SDK + ARM toolchain are available:
```bash
rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)
```
Expected: builds to `feralrf_cc1352.elf`. If the SDK/toolchain is not present in this environment, record that the build could not be run here and must pass on a machine with the SDK before merge; at minimum run `cmake ..` to confirm the source list resolves.

- [ ] **Step 8: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A firmware/cc1352
git commit -m "refactor(ble): remove BLE protocol stack from CC1352 firmware (keep BLE PHY + ll_manager)"
```

---

### Task 6: Delete BLE example scripts and update the README example list

**Files:**
- Delete (top-level): `python/examples/ble_sniffer.py`, `release_gate_ble.py`, `smoke_ble_scan_mode.py`, `smoke_f21_advertise.py`, `smoke_f8c.py`, `smoke_f8d_connect_timeout.py`, `smoke_f8d_graceful_dc.py`, `smoke_tx_ble_phase1.py`
- Delete (lab): `python/examples/lab/demo_advertise_connectable.py`, `demo_ble_analyzer.py`, `demo_ble_clone.py`, `demo_ble_connect_gatt.py`, `demo_ble_scan_active.py`, `demo_emulate_soundcore.py`, `diag_attclient_dump.py`, `diag_attclient_repro.py`, `f8a_session3_capture.py`, `f8a_session3_offset_analysis.py`, `smoke_ble_attacks.py`, `smoke_f12_scan_active.py`, `smoke_f8b_follower.py`, `smoke_f8b_notifications.py`, `soak_ble_30min.py`
- Modify: `python/examples/release_gate_multi_phy.py`, `python/examples/smoke_f17_emulation.py`, `python/examples/lab/canary_regression.py`, `python/examples/lab/smoke_f9_phy_matrix_ota.py`, `python/examples/lab/ota_*.py` (only if they reference removed methods), `README.md`

- [ ] **Step 1: Delete the BLE-only scripts**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python/examples
git rm ble_sniffer.py release_gate_ble.py smoke_ble_scan_mode.py smoke_f21_advertise.py \
  smoke_f8c.py smoke_f8d_connect_timeout.py smoke_f8d_graceful_dc.py smoke_tx_ble_phase1.py
git rm lab/demo_advertise_connectable.py lab/demo_ble_analyzer.py lab/demo_ble_clone.py \
  lab/demo_ble_connect_gatt.py lab/demo_ble_scan_active.py lab/demo_emulate_soundcore.py \
  lab/diag_attclient_dump.py lab/diag_attclient_repro.py lab/f8a_session3_capture.py \
  lab/f8a_session3_offset_analysis.py lab/smoke_ble_attacks.py lab/smoke_f12_scan_active.py \
  lab/smoke_f8b_follower.py lab/smoke_f8b_notifications.py lab/soak_ble_30min.py
```

- [ ] **Step 2: Audit the mixed scripts**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python/examples
grep -lnE "scan_ble_active|ble_connect|gatt_|advertise_|follow_connection|read_ll_packets|set_ble_scan_mode|set_ble_addr|emulate_ble" \
  release_gate_multi_phy.py smoke_f17_emulation.py lab/canary_regression.py lab/smoke_f9_phy_matrix_ota.py lab/ota_*.py
```
For each file that matches: if BLE is incidental, delete the offending lines/branches (keep the non-BLE flow); if the script is entirely BLE, `git rm` it. If nothing matches, leave them.

- [ ] **Step 3: Update `README.md`**

In the "Examples" list, remove the deleted script names (`ble_sniffer.py`, `smoke_tx_ble_phase1.py`, etc.). In "Features", drop the BLE 5.x sniffing/attack bullets, or reword to "raw BLE-PHY capture (protocol handling via Sniffle)".

- [ ] **Step 4: Verify kept examples import**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python
python -m py_compile examples/smoke_phase2.py examples/smoke_phy4_ieee154.py examples/smoke_f17_emulation.py examples/release_gate_multi_phy.py && echo "COMPILE OK"
```
Expected: `COMPILE OK` (no import of removed symbols).

- [ ] **Step 5: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A python/examples README.md
git commit -m "docs(ble): remove BLE example scripts and update README example list"
```

---

### Task 7: Update documentation

**Files:**
- Modify: `docs/protocol.md`, `docs/ARCHITECTURE.md`, `docs/PYTHON_API.md`, `CLAUDE.md`

- [ ] **Step 1: `docs/protocol.md`**

Remove the payload sections for the removed BLE commands/responses (CONNECT, DISCONNECT, CONN_STATUS, GATT_*, DEBUG_*, FOLLOW_*, BLE_ADV_LEGACY, SET_BLE_ADDR, SET_BLE_SCAN_MODE). Keep the frame format, crypto, RX/TX, jamming, `SET_ADV_HOP`, `SET_PROP_CONFIG`, and the kept command/response tables.

- [ ] **Step 2: `docs/ARCHITECTURE.md`**

In section 2 remove the "L4 ble" firmware row (`ll_manager.c`, `csa2.c`, `ble_conn.c`, `ble_conn_mgr.c`, `att_client.c`) and re-add `ll_manager.c` under an RX/data row (it is the kept packet classifier). In section 3 remove the BLE-specific Python rows. Keep the RF-driver lifecycle rules (section 5). Add one line: "BLE PHY retained for raw capture; BLE protocol stack removed (2026-07-20), Sniffle handles BLE."

- [ ] **Step 3: `docs/PYTHON_API.md`**

Remove BLE from the exported-objects list and any BLE scan/GATT usage sections.

- [ ] **Step 4: `CLAUDE.md`**

In the "CC1352 firmware layout" section drop `ble_conn*`/`att_client`/`csa2`/`ll_follower` from the module list; note `ll_manager.c` is the kept RX classifier. In the "Python API structure" section remove the BLE method enumeration; keep the "BLE PHY raw capture stays" note and the RF invariants. In "Wire Protocol" update the command range note (`0x01`-`0x62` minus the retired BLE IDs).

- [ ] **Step 5: Commit**

```bash
cd /Users/wero1414/zigbeepollo/FeralRF
git add -A docs CLAUDE.md
git commit -m "docs(ble): update architecture, protocol, and CLAUDE docs for BLE protocol removal"
```

---

### Task 8: Final verification

**Files:** none (verification only).

- [ ] **Step 1: Full acceptance run**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF/python
pip install -e ".[dev]" >/dev/null && python -m pytest -q
python -c "import feralrf; from feralrf import Radio, PHY; print('version', feralrf.__version__); print('BLE PHY still present:', PHY.BLE_1M)"
python -c "import feralrf; assert not hasattr(feralrf, 'GattService') and not hasattr(feralrf, 'ConnectionResult'); print('BLE exports gone')"
```
Expected: pytest PASS; version `0.3.0`; `PHY.BLE_1M` prints; "BLE exports gone".

- [ ] **Step 2: Repo-wide grep for stragglers**

Run:
```bash
cd /Users/wero1414/zigbeepollo/FeralRF
grep -rInE "gatt|ble_conn|ll_follower|att_client|scan_ble|advertise_|follow_connection" python/feralrf firmware/cc1352/src firmware/cc1352/include | grep -viE "ll_manager|adv_hop|BLE_1M|BLE_2M|BLE_CODED|smartrf_ble5" || echo "CLEAN"
```
Expected: `CLEAN`.

- [ ] **Step 3: Confirm the branch diff is coherent**

Run:
```bash
git --no-pager diff --stat main...feature/remove-ble-protocol
```
Expected: deletions dominate; touched files match this plan (firmware BLE files, Python BLE modules, tests, examples, docs). No stray edits to `radio_if.c` beyond the comment, none to `data_task.c`, `smartrf_ble5_0.c`, `ll_manager.c`.

---

## Self-Review notes

- **Spec coverage:** every spec section (3.1-3.3 IDs, 4 firmware, 5 Python, 6 tests, 7 examples, 8 docs, 9 acceptance) maps to a task (4, 5, 2/3/4, 1, 6, 7, 8). The corrected firmware scope (keep `ll_manager.c`; edit `control_task.c`/`main_rtos.c`) is in Task 5.
- **`commands.py`** was under-specified in the spec; Task 2 Step 5 covers it.
- **Ordering keeps green:** tests deleted first (Task 1), then API surface (Task 2), then module deletion (Task 3), then enum IDs (Task 4) — no step references a symbol removed in a later step.
- **Firmware is one atomic task** (Task 5) because the build only passes after all edits + deletions land together.
