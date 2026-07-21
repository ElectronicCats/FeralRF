# Remove the BLE protocol layer (keep BLE PHY for raw capture)

Date: 2026-07-20
Status: Draft for review

## 1. Goal and motivation

BLE is now handled by Sniffle (an external, mature BLE sniffer). This firmware
should stop carrying its own hand-rolled BLE protocol stack (link-layer
following, connection management, GATT/ATT client, advertising, active
scanning) and focus on the RF areas where the CC1352P7 differentiates:
IEEE 802.15.4 (Zigbee/Thread), Sub-1GHz (868/915 MHz), and proprietary
GFSK/OOK, plus raw TX/RX, jamming, spectrum, and the HW crypto engine.

The BLE protocol stack is the largest and most bug-prone part of the codebase
(see the f8a/f8b/f8c/f8d history). Removing it eliminates that maintenance
burden without losing any capability that Sniffle already covers.

## 2. Locked scope decisions

1. **Remove the BLE protocol/application layer only.** Delete connection,
   GATT/ATT, link-layer following, advertising, and active-scan code and their
   host-facing commands.
2. **Keep the BLE PHY as a raw radio capability.** `SET_PHY(BLE_1M/BLE_2M/
   BLE_CODED_S8/BLE_CODED_S2)` followed by `RX_START` still captures raw
   BLE-channel PDUs. No changes to `radio_if.c`, `phy_manager.c`, or
   `smartrf_ble5_0.c` (this deliberately avoids surgery on the ~599 BLE
   references braided through the shared radio file).
3. **Full stack.** Firmware + Python package + tests + examples + docs.
4. **Keep crypto, jamming, and TI-RTOS as-is.** AES-CCM is core Zigbee
   security; jamming stays a generic RF capability; the RTOS question is out of
   scope for this change.
5. **Do NOT renumber command/response IDs.** Removed IDs simply become unused;
   the firmware answers them with `ERR_INVALID_CMD (0x01)`. Kept commands retain
   their current wire values so existing non-BLE tooling is unaffected.

Non-goals: no NoRTOS migration, no `radio_if.c` refactor, no PHY-enum
renumbering, no removal of the BLE PHY, no changes to the RP2040 firmware.

## 3. Command/response contract changes

### 3.1 Commands removed (firmware `protocol.h` + Python `Command`)

| ID | Name | Reason |
|----|------|--------|
| 0x09 | SET_BLE_ADDR | TX/scan participation (own address) |
| 0x0B | SET_BLE_SCAN_MODE | active scan (TX SCAN_REQ) |
| 0x40 | CONNECT | BLE central connection |
| 0x41 | DISCONNECT | BLE connection |
| 0x42 | CONN_STATUS | BLE connection |
| 0x43 | GATT_DISCOVER | GATT client |
| 0x44 | GATT_SUBSCRIBE | GATT client |
| 0x45 | GATT_READ | GATT client |
| 0x46 | GATT_WRITE | GATT client |
| 0x47 | DEBUG_TIMING | BLE connection timing |
| 0x48 | DEBUG_CONN_PARAMS | BLE connection params |
| 0x49 | ATT_DEBUG | ATT client debug |
| 0x4A | GATT_EXCHANGE_MTU | GATT client |
| 0x4B | GATT_READ_BY_UUID | GATT client |
| 0x50 | FOLLOW_START | LL connection follower |
| 0x51 | FOLLOW_STOP | LL connection follower |
| 0x52 | BLE_ADV_LEGACY | BLE advertising TX |
| 0x54 | FOLLOW_DEBUG | LL follower debug |

### 3.2 Commands kept (unchanged)

`RADIO_INIT(0x01)`, `SET_CHANNEL(0x02)`, `SET_POWER(0x03)`, `SET_PHY(0x04)`,
`GET_INFO(0x05)`, `GET_STATS(0x06)`, `SET_ADV_HOP(0x07)`,
`SET_PROP_CONFIG(0x08)`, `RX_START(0x10)`, `RX_STOP(0x11)`, `TX_RAW(0x20)`,
`TX_CONTINUOUS(0x21)`, `TX_BURST(0x22)`, `TX_FRAME(0x23)`, `TX_STOP(0x24)`,
`JAM_CONTINUOUS(0x30)`, `JAM_STOP(0x33)`, `TX_CW(0x55)`, `TX_PRBS(0x56)`,
`TX_TEST_STOP(0x57)`, crypto `0x59-0x62`.

Note on `SET_ADV_HOP(0x07)`: kept. It is a passive radio aid (dwell-time retune
across BLE advertising channels 37/38/39 during RX), not protocol
participation, and its firmware handler calls into the untouched radio layer.
It supports raw BLE capture. This is the one borderline item; flag for review.

### 3.3 Responses removed (firmware `protocol.h` + Python `Response`)

`CONN_RESULT(0xA0)`, `CONN_STATUS_R(0xA1)`, `GATT_SERVICE(0xA2)`,
`GATT_CHAR(0xA3)`, `GATT_READ_R(0xA4)`, `GATT_DONE(0xA5)`, `GATT_NOTIFY(0xA6)`,
`DEBUG_TIMING(0xA8)`, `DEBUG_CONN_PARAMS(0xA9)`, `ATT_DEBUG(0xAA)`,
`LL_PACKET(0xAB)`, `FOLLOW_DONE(0xAC)`, `FOLLOW_DEBUG(0xAF)`, `GATT_MTU(0xB0)`,
`GATT_ATTRIBUTE(0xB1)`, `DISCONNECTED(0xB2)`.

Responses kept: `ACK(0x80)`, `ERROR(0x81)`, `RX_PACKET(0x90)`, `STATS(0x93)`,
`INFO(0x94)`, crypto `0x95-0x9C`.

## 4. Firmware changes (`firmware/cc1352/`)

### 4.1 Delete (BLE protocol stack, sources + headers) — 6 file pairs

- `src/ble_conn.c` + `include/ble_conn.h`
- `src/ble_conn_mgr.c` + `include/ble_conn_mgr.h`
- `src/ble_conn_pdu.c` + `include/ble_conn_pdu.h`
- `src/ll_follower.c` + `include/ll_follower.h`
- `src/att_client.c` + `include/att_client.h`
- `src/csa2.c` + `include/csa2.h`

DO NOT delete `ll_manager.c` / `ll_manager.h` (correction to earlier draft).
`LLManager_processRxPacket()` is the shared RX classifier: a no-op passthrough
for IEEE154/Sub-1GHz/Prop (`LL_MANAGER_DEFAULT`) and BLE PDU classification
(ADV/SCAN/CONNECT/DATA) for BLE PHYs. `data_task.c` feeds every RX packet
through it and `GET_STATS` reports its counters. It stays and continues to
enrich raw BLE captures — aligned with "keep BLE PHY raw capture."

### 4.2 Edit

- `src/command_processor.c`: remove the GATT ATT callback block, the
  `follower_on_done`/`LlFollower_setCallbacks` helpers, the `s_gatt_*` state, and
  the `case CMD_*` handlers for every command in section 3.1. Remove the
  `#include` of `att_client.h`, `ble_conn.h`, `ble_conn_mgr.h`, `ll_follower.h`.
  KEEP `#include "ll_manager.h"` and the `LLManager_getStats()` usage in the
  `GET_STATS` handler. Keep all kept-command cases.
- `src/control_task.c`: remove `#include "ble_conn.h"` and `"ll_follower.h"`
  (KEEP `"ll_manager.h"`); remove the `BleConn_init()` calls (in
  `ControlTask_init` ~L211 and `ControlTask_onRadioInit` ~L227), the
  `LlFollower_init()` call (~L212), and the follower block in `ControlTask_poll`
  (~L459-463) — leave `ControlTask_poll` as an empty no-op so its `data_task.c`
  caller stays valid. KEEP `LLManager_resetStats()` (~L226) and
  `LLManager_select(...)` (~L235).
- `src/main_rtos.c`: remove `#include "ble_conn_mgr.h"` (~L42) and the
  `if (BleConnMgr_isRunning()) BleConnMgr_poll();` block in `RfTask_taskFxn`
  (~L186-188); trim the two stale `BleConnMgr_poll()` comments (~L126-128,
  ~L183-185).
- `src/radio_if.c`: cosmetic only — update the stale comment at ~L2663 that
  references `BleConnMgr_poll()`. No code change; not required for the build.
- `include/protocol.h`: delete the removed `CMD_*` and `RSP_*` defines from
  sections 3.1 and 3.3. Leave a short comment noting the gaps are reserved/retired
  so nobody reuses the IDs.
- `CMakeLists.txt`: remove the six deleted `.c` files (section 4.1) from
  `APP_SOURCES`. Leave `src/ll_manager.c` in `APP_SOURCES`.

### 4.3 Explicitly untouched

`radio_if.c` (code), `phy_manager.c`, `smartrf_ble5_0.c`, `ll_manager.c`,
`crypto_engine.c`, `data_task.c`, `host_if*.c`, `protocol.c`, `output_if.c`,
`packet_queue.c`, `tx_queue.c`, the IEEE154/prop SmartRF configs, and all
startup/linker/RTOS glue. `data_task.c` is untouched because it only uses the
kept `ll_manager.h`. The BLE PHY RX path in `radio_if.c` stays live; with
`SET_BLE_SCAN_MODE` gone, BLE RX defaults to passive raw capture, and
`LLManager` still classifies captured BLE PDUs.

## 5. Python changes (`python/feralrf/`)

### 5.1 Delete modules

- `ble/` package (`ble/__init__.py`, `ble/connect_ind.py`)
- `attacks/` package (only contains `attacks/ble.py` + `__init__.py` that imports it)
- `emulation/ble_peripheral.py`
- `_ble_scan.py` (BLE active-scan dataclass/AD parser)
- `_ll_parser.py` (BLE LL/ATT PDU parser for the follower)

### 5.2 Edit modules

- `radio.py`: remove BLE dataclasses (`ConnectionResult`, `ConnectionStatus`,
  `GattService`, `GattCharacteristic`, `GattAttribute`, `GattDiscoveryResult`,
  `DisconnectEvent`, `GattNotification`, `LLPacket`) and every BLE method:
  `set_ble_addr`, `set_ble_addr_str`, `advertise_ind`, `advertise_direct`,
  `advertise_scan_ind`, `ble_connect`, `ble_disconnect`, `conn_status`,
  `debug_timing`, `debug_conn_params`, `gatt_discover`, `gatt_read`,
  `gatt_write`, `gatt_subscribe`, `gatt_exchange_mtu`, `gatt_read_by_uuid`,
  `read_gatt_notifications`, `read_disconnect_events`, `follow_connection`,
  `stop_follow_connection`, `read_ll_packets`, `set_ble_scan_mode`,
  `scan_ble_active`. Keep `set_adv_hop` (see 3.2). Keep session control,
  `set_phy` (incl. BLE PHYs), `set_channel`, `set_power`, RX/TX/TX-test,
  `configure_prop`, `get_stats`, and all crypto methods.
- `__init__.py`: drop the BLE dataclass exports from imports and `__all__`
  (`ConnectionResult`, `ConnectionStatus`, `GattService`, `GattCharacteristic`,
  `GattDiscoveryResult`). Update the module docstring's API-status lines to
  remove BLE scan/GATT/initiator wording. Bump `__version__` to `0.3.0`
  (removed public API is a breaking change).
- `enums.py`: remove the `Command` and `Response` members in 3.1/3.3; remove
  `SET_BLE_ADDR`, `SET_BLE_SCAN_MODE`, `GATT_SUBSCRIBE` from `STABLE_COMMANDS`.
- `emulation/__init__.py`: remove the `ble_peripheral` imports, the BLE
  personality exports (`BlePersonality`, `SOUNDCORE_BOOM_2`, `APPLE_AIRPODS_PRO`,
  `GOOGLE_FASTPAIR_GENERIC`, `BLE_PERSONALITIES`, `emulate_ble`), and their
  `__all__` entries. Keep IEEE154/OOK/Sub-1GHz personalities.
- `_responses.py`: keep the file and its non-BLE parsers `RxPacketResponse`,
  `SpectrumDataResponse`, `InfoResponse`. Remove the BLE-connection diagnostic
  classes `DebugTimingEntry`, `DebugTimingResponse`, `DebugConnParamsResponse`
  (used only by the removed `debug_timing`/`debug_conn_params` methods).
  `RxPacketResponse`'s optional `ll_pdu_*` fields simply stay `None` once the
  follower is gone; leave them to avoid touching the RX packet format the
  firmware still emits.

## 6. Tests (`python/tests/`)

### 6.1 Delete (pure BLE)

`test_attacks_ble.py`, `test_ble_scan.py`, `test_connect_ind_pdu.py`,
`test_disconnect_events.py`, `test_follow_connection.py`, `test_gatt_api.py`,
`test_gatt_integration.py`, `test_gatt_notifications.py`, `test_ll_parser.py`,
`test_radio_advertise.py`, `test_debug_timing.py`.

### 6.2 Edit (mixed)

- `test_emulation.py`: drop the BLE-peripheral cases, keep IEEE154/OOK/Sub-1GHz.
- `test_commands_contract.py`: remove assertions for the removed command IDs;
  keep/adjust the kept set.
- Any test importing a removed symbol (`test_enums_no_collisions.py`,
  `test_radio_seq.py`, `test_async_error_surfacing.py`): update imports/assertions.

### 6.3 Keep (non-BLE)

`test_crypto.py`, `test_crypto_vectors.py`, `test_protocol.py`, `test_props.py`,
`test_tx_test.py`, `test_read_one_packet.py`, `test_shell_port.py`,
`test_killerbee_dispatch.py`, `test_killerbee_integration.py`.

Acceptance: `cd python && pytest` passes with zero collection/import errors.

## 7. Examples (`python/examples/`)

Delete BLE-only scripts: `ble_sniffer.py`, `release_gate_ble.py`,
`smoke_ble_scan_mode.py`, `smoke_f21_advertise.py`, `smoke_f8c.py`,
`smoke_f8d_connect_timeout.py`, `smoke_f8d_graceful_dc.py`,
`smoke_tx_ble_phase1.py`, and under `lab/`:
`demo_advertise_connectable.py`, `demo_ble_analyzer.py`, `demo_ble_clone.py`,
`demo_ble_connect_gatt.py`, `demo_ble_scan_active.py`, `demo_emulate_soundcore.py`,
`diag_attclient_dump.py`, `diag_attclient_repro.py`, `f8a_session3_capture.py`,
`f8a_session3_offset_analysis.py`, `smoke_ble_attacks.py`,
`smoke_f12_scan_active.py`, `smoke_f8b_follower.py`, `smoke_f8b_notifications.py`,
`soak_ble_30min.py`.

Audit and, if they reference removed BLE methods, either trim or delete:
`release_gate_multi_phy.py`, `smoke_f17_emulation.py`, `lab/canary_regression.py`,
`lab/smoke_f9_phy_matrix_ota.py`, `lab/ota_*`. Keep TX/RX/jam/prop/IEEE154/crypto
scripts. Update `README.md`'s example list to match.

## 8. Docs

- `docs/protocol.md`: remove the BLE command/response payload sections; keep
  the frame format, kept commands, crypto, and RX/TX sections.
- `docs/ARCHITECTURE.md`: remove the L4 `ble` firmware row and BLE-specific
  Python rows; keep the RF-driver lifecycle rules (they still govern the shared
  radio layer) and add a one-line note that the BLE PHY remains for raw capture
  while the protocol stack is gone.
- `CLAUDE.md`: update the CC1352 firmware layout and Python API sections to drop
  the removed BLE modules/methods; keep the "BLE PHY raw capture stays" note and
  the RF invariants.
- `README.md`, `docs/PYTHON_API.md`: remove BLE from feature/API lists.

## 9. Verification / acceptance criteria

1. `cd python && pip install -e ".[dev]" && pytest` -> all pass, no import errors.
2. `python -c "import feralrf; from feralrf import Radio, PHY"` succeeds; BLE
   symbols (`GattService`, `ConnectionResult`, ...) are gone from `feralrf`.
3. `grep -rIn -E "gatt|ble_conn|ll_follower|att_client|advertise|scan_ble|csa2" python/feralrf` returns nothing (except allowed BLE-PHY enum names in `enums.py` and any kept `set_adv_hop`).
4. CC1352 firmware still builds (`firmware/cc1352/build` via CMake with the TI
   SDK + precompiled libs), and `grep -n CMD_GATT firmware/cc1352/src` is empty.
5. `SET_PHY(BLE_1M)` + `RX_START` path still compiles and dispatches (BLE PHY
   retained); a removed command ID returns `ERR_INVALID_CMD`.

## 10. Risks and rollback

- **Risk: hidden coupling from a kept file into a deleted BLE file.** Mitigation:
  after deleting, a firmware build surfaces any dangling reference from
  `command_processor.c` or elsewhere; fix by removing the caller (it will be BLE
  code by construction). `radio_if.c` is untouched and does not depend on the
  deleted protocol files (it owns only the PHY layer).
- **Risk: a mixed test/example imports a removed symbol.** Mitigation: pytest
  collection and a grep for removed names catch these before completion.
- **Rollback:** the whole change lands on a feature branch; git revert restores
  the BLE stack in full. No data migration involved.

## 11. Suggested implementation order

1. Python `enums.py` (command/response IDs) + `__init__.py` exports.
2. Python module deletions + `radio.py` / `emulation/__init__.py` edits.
3. Python test deletions/edits -> `pytest` green.
4. Firmware `protocol.h` + `command_processor.c` + `CMakeLists.txt` + file
   deletions -> firmware builds.
5. Examples + docs.
6. Final verification (section 9) and CLAUDE.md refresh.
