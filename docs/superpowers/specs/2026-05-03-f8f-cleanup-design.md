# F8f — Code Review Cleanup (Python + Firmware Bundles)

**Date:** 2026-05-03
**Branch (target):** `feature/f8f-cleanup` cut from `feature/ti-rtos-migration` HEAD=`12e76ae`
**Tags (target):** `v2.0-f8f-py` (Bundle 1 mid-branch), `v2.0-f8f-fw` (Bundle 2 end)
**Source:** `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md` — findings #1, #2, #3, #4, #7, #8, #9, #10, #11
**Closed earlier (not in scope):** #5, #6 (already landed: 36e77fe, 99d8f63, 12e76ae)
**Sibling sub-projects:** none — F8f absorbs prior F8f / F8g / F8h / F8i splits into one branch with two tags.

## Goal

Address all 9 actionable findings from the post-F8e code review audit in a single
focused branch, split into a Python bundle (host-only) and a firmware bundle
(CC1352, two flashes). No new feature work. No protocol additions beyond
consolidation. The async-error contract (#7) is unified across both bundles.

## Bundle layout

| Bundle | Findings | Flash | Tag |
|--------|----------|-------|-----|
| 1 — Python | #9, #8, #2, #7b | none | `v2.0-f8f-py` |
| 2A — Firmware runtime | #1, #3, #4 | yes | (no intermediate tag) |
| 2B — Firmware contract+cleanup | #10, #7a, #11 | yes | `v2.0-f8f-fw` |

**Why #10 before #7a in 2B:** `RSP_ERROR` currently has no copy in
`protocol.h` (only locally in `data_task.c:23` as `#define RSP_ERROR 0x81u`).
`control_task.c:355` uses the literal `0x81u`. Consolidating first lets #7a
reference the `protocol.h` symbol cleanly without temporary local defines.

One commit per finding. Format: `fix(f8f): #N — descripción`. Pre-commit clean
on every commit (`pre-commit run --files <files>`, never `--all-files`, never
`--no-verify`).

## Findings recap and fixes

### #9 — `PENDING_COMMAND_IDS` collide with BLE connection IDs

`python/feralrf/enums.py`:

- `Command.CONNECT = 0x40`, `DISCONNECT = 0x41`, `CONN_STATUS = 0x42` (lines 76-78).
- `PENDING_COMMAND_IDS` maps `SPECTRUM_SCAN`, `SPECTRUM_MONITOR`, `SPECTRUM_STOP`
  to those exact values (lines 183-185).

**Fix:** remove the spectrum entries from `PENDING_COMMAND_IDS` until firmware
allocates real, non-conflicting IDs. Removing is preferred over reassignment
because there's no live spectrum command in firmware yet — keeping placeholder
IDs invites future collision noise.

**External callers check:** `PENDING_COMMAND_IDS` is re-exported from
`python/feralrf/__init__.py:60`. Grep the workspace for dict-style key access
(`PENDING_COMMAND_IDS["SPECTRUM_*"]`) before removing; replace with
non-conflicting IDs only if external callers exist (none observed in
in-tree code at spec time).

**Test:** add `test_no_command_id_collisions` asserting that every value in
`PENDING_COMMAND_IDS` is disjoint from every value in `Command`.

### #8 — ATT parser mis-decodes `READ_RSP` / `READ_BLOB_RSP` as handle-bearing

`python/feralrf/_ll_parser.py:161`:

- `has_handle` set includes `0x0B` (`ATT_READ_RSP`) and `0x0D` (`ATT_READ_BLOB_RSP`).
- Per BT Core Spec, both PDUs carry only `value` bytes — no handle field.
- Parser consumes bytes 1-2 of `value` as fake handle.

**Fix:** remove `0x0B` and `0x0D` from `has_handle`.

**Tests:** add unit tests for `ATT_READ_RSP` and `ATT_READ_BLOB_RSP` parse,
asserting full value bytes preserved and no spurious handle field set.

### #2 — MTU exchange advertises larger MTU than firmware can receive

`python/feralrf/commands.py:217-219`:

- `gatt_exchange_mtu(client_mtu=247)` accepts any value `>= 23` with no upper
  cap tied to firmware capability.
- Firmware ATT stack caps outgoing PDUs to `ATT_DEFAULT_MTU` (23) and has no
  L2CAP reassembly (per audit, `att_client.c:114-128` and `:573-587`).
- Risk path is only `CMD_GATT_EXCHANGE_MTU` (audit clarified `startDiscover`
  skips MTU exchange in current code, contrary to original review claim).

**Fix:** clamp `client_mtu > 23` at the host boundary. Reject with
`ValueError` if explicitly larger; document that L2CAP reassembly is a future
work item. Firmware-side cap is out of scope (deferred until L2CAP exists).

**Test:** validation test asserts `ValueError` on `client_mtu=247`, accepts
`client_mtu=23`.

### #7b — Surface async errors in Python stream APIs (consumer side)

`python/feralrf/radio.py`:

- `_read_response()` warns and **discards** `RSP_ERROR seq=0xFF` (lines 503-509).
- `read_packets()` only yields `Response.RX_PACKET`; other responses fall
  through (lines 1491-1494).

**Fix (Bundle 1, Python consumer side):**
- `_read_response()` accepts `RSP_ERROR` with `seq=0` (already does via async
  bypass at lines 511-530) AND with `seq=0xFF` during compat window. Stop
  silently discarding `seq=0xFF` errors.
- `read_packets()` and `start_rx()` yield/raise async errors so the caller
  doesn't time out on a dead RX backend. New variant `RxStreamError(error_code,
  context)` yielded by `read_packets()`. TX helpers (`tx_packet`,
  `tx_test_*`) raise `RadioError(error_code, context)` if async error arrives
  mid-TX.
- The obsolete test `test_read_response_ignores_echoed_command_frames` (audit
  noted as already failing) is replaced by tests of the new contract.

**Tests:** unit tests for async-error injection through both `read_packets()`
yield and TX helper raise paths.

### #1 — Follower frame overflow in `follower_on_packet`

`firmware/cc1352/src/command_processor.c:330-340`:

- `uint8_t buf[5 + 257]` with guard `pdu_len > sizeof(buf) - 5u` allows up to
  257 bytes through.
- `PROTOCOL_MAX_PAYLOAD = 255` (`include/protocol.h:16`); a 256/257-byte PDU
  builds a frame over `PROTOCOL_MAX_FRAME`.
- `OutputIF_sendResponse()` builds into `raw_frame[PROTOCOL_MAX_FRAME]` and
  only checks encoded size after building (`output_if.c:24`) — pre-COBS
  overflow risks stack buffer overwrite.

**Fix:** in `follower_on_packet`:
- Cap `pdu_len > PROTOCOL_MAX_PAYLOAD - 5u`, drop with explicit reason via
  `dbg`/telemetry tag (e.g. `FOLL_DBG_TAG_PDU_TOO_LARGE`).
- Add defensive `payload_len <= PROTOCOL_MAX_PAYLOAD` check before
  `protocol_build_frame()`.

**Note:** `PROTOCOL_MAX_PAYLOAD - 5 = 250` exceeds real BLE LL data MTU (37
without DLE, 257 with DLE — but we never enable DLE on the follower path), so
the cap does not affect legitimate traffic.

**Smoke (2A):** mock or stress with crafted 256+ byte PDU; verify drop tag
emitted, no crash.

### #3 — Disconnect callback not registered until a GATT command runs

`firmware/cc1352/src/command_processor.c:899-913` (`CMD_CONNECT`),
`:283-296` (`ensure_gatt_callbacks`), `:916-937` (`CMD_DISCONNECT`):

- `gatt_on_disconnected` is installed only via `ensure_gatt_callbacks()`,
  called lazily from each GATT handler and from `CMD_DISCONNECT`.
- If peer terminates immediately after `CMD_CONNECT` (before any
  `CMD_GATT_*` or `CMD_DISCONNECT`), `BleConnMgr_stop()` has a null
  `s_disconnect_cb` and `RSP_DISCONNECTED` is never emitted to host.

**Fix:** register `gatt_on_disconnected` (or a non-GATT-coupled equivalent if
that callback assumes GATT state) at `CMD_CONNECT` success path, immediately
after `BleConn_initiate()` returns true. Verify `BleConnMgr_stop()` at
`ble_conn_mgr.c:340-357` already null-checks the callback.

**Smoke (2A):** issue `CMD_CONNECT`, force peer-side immediate DC (kill
script-side), verify host receives `RSP_DISCONNECTED` with the matching seq
within supervision timeout.

### #4 — `s_gatt_seq` corrupted by rejected second GATT command

`firmware/cc1352/src/command_processor.c` GATT handlers at lines 992, 1012,
1033, 1065, 1096, 1117:

- Each handler writes `s_gatt_seq = seq` BEFORE calling `AttClient_start*()`.
- If `AttClient_start*()` returns false (operation in progress), handler sends
  `ERR_INVALID_STATE` but `s_gatt_seq` is now overwritten.
- Original in-flight transaction's completion fires with the wrong sequence;
  Python `_read_response()` drops the response as stale mismatch (radio.py:516).

**Fix (minimal):** move `s_gatt_seq = seq` to AFTER the `AttClient_start*()`
guard returns true. Apply to all 6 handlers.

**Out of scope (preferred but invasive):** moving seq into the AttClient
transaction object — defer until a GATT pipelining feature actually needs it.

**Smoke (2A):** issue back-to-back `CMD_GATT_DISCOVER` + `CMD_GATT_READ` on
Sony with the second command racing the first; verify both responses arrive
with matching seqs.

### #7a — Async error seq standardization (firmware producer side)

`firmware/cc1352/src/data_task.c:122-124` and `control_task.c:351-355`:

- RX failure sends `RSP_ERROR seq=0`.
- TX failure sends `RSP_ERROR seq=0xFF` (hardcoded `0x81u`).

**Contract decision (cross-bundle):**

| Aspect | Value |
|--------|-------|
| Async error code | `RSP_ERROR` (existing; do NOT add `RSP_ASYNC_ERROR`) |
| `seq` | `0` always (matches `_read_response()` async bypass at radio.py:511-530) |
| Payload | `[error_code: u8, context: u8]` (same layout as sync errors) |
| Why `seq=0` and not `0xFF` | `0xFF` collides with `_read_response`'s "command echo skipped" sentinel — root of `feedback_async_event_drop_bug`. RX side already uses `seq=0`; TX aligns to RX, not the reverse. |

**Fix (firmware):** change `control_task.c:355` from
`OutputIF_sendResponse(0x81u, 0xFFu, ...)` to
`OutputIF_sendResponse(RSP_ERROR, 0u, ...)`. The `RSP_ERROR` symbol is
expected to be visible from `protocol.h` after #10 (which runs first in 2B).

**Compat window:** Bundle 1 Python accepts BOTH `seq=0` and `seq=0xFF` so it
runs against pre-Bundle-2 firmware builds without losing async errors. After
Bundle 2 lands, a small follow-up commit in Bundle 2B (or a tagged-on commit)
removes the `seq=0xFF` compat branch from Python.

**Smoke (2B):** force RX backend init failure (config error path) and force
TX execution failure; verify Python receives async error event in
`read_packets()` / TX helper raise, no timeout.

### #10 — Protocol constants duplicated across firmware modules

- `firmware/cc1352/include/protocol.h:35-54` — crypto cmd/rsp IDs.
- `firmware/cc1352/src/command_processor.c:41-50, 84-91` — same crypto IDs
  re-defined locally.
- `firmware/cc1352/src/data_task.c:22-24` — `RSP_ERROR`, `ERR_RF_INIT_FAILED`.
- `firmware/cc1352/src/att_client.c:22` — `RSP_GATT_NOTIFY`.

**Fix:**
- Move all duplicated IDs into `protocol.h`.
- Remove local `#define`s from the three .c files.
- Build after each header touch (`cmake --build firmware/cc1352/build -j2`).

**Smoke (2B):** any BLE smoke command exercises the consolidated symbols (no
runtime change expected).

### #11 — Deprecated `BleConn_disconnect()` still exported

- Function still exists at `firmware/cc1352/src/ble_conn.c:262-289`,
  declared in `firmware/cc1352/include/ble_conn.h:61-73`.
- `CMD_DISCONNECT` no longer calls it (uses `BleConnMgr_initiateGracefulDisconnect`).
- Header comment (added by F8d) warns about unsafe legacy semantics.

**Fix:** make `BleConn_disconnect` `static` in `ble_conn.c` and remove the
declaration from `ble_conn.h`. If a build error surfaces an out-of-tree
caller, defer instead and document.

**Smoke (2B):** build clean; no runtime exercise needed.

## Smoke baseline (Bundle 2)

Sub-bundle 2A (after #1, #3, #4):
- BLE OTA RX 15 packets from Sony advertising — no regression.
- BLE active scanner sees 3 peripherals — no regression.
- GATT discover + read primary services on Sony (paired peer A8:E6:E8:8A:7D:F8) — validates #4.
- `CMD_CONNECT` followed by peer immediate DC — validates #3 (`RSP_DISCONNECTED` emitted).
- Follower overflow stress with crafted 256+ byte PDU — validates #1 (drop with reason, no crash).

Sub-bundle 2B (after #7a, #10, #11):
- Re-run 2A smoke (no regression from #10 / #11).
- Force RX backend failure → host receives `RxStreamError` — validates #7a.
- Force TX execution failure → host helper raises `RadioError` — validates #7a.

Hardware: CatSniffer #1 on `/dev/ttyACM2`, board #2 as second BLE-2.4 endpoint
if needed. Board #1 Sub-1GHz HW fault is irrelevant to BLE smoke. Flash with
`.hex` only (`feedback_flash_hex`), retry 2× before asking for manual reset
(`feedback_flash_retry`).

## Risks and mitigations

| Risk | Mitigation |
|------|-----------|
| Bundle 1 merged, firmware still emits `seq=0xFF` | Compat window: Bundle 1 Python accepts both `seq=0` and `seq=0xFF` |
| #10 consolidation triggers redefinition errors | Build after each header change; pre-commit runs `cmake --build` |
| #4 fix breaks happy path (single GATT command flow) | 2A smoke includes functional GATT discover+read on Sony |
| #3 registers cb but `BleConnMgr_stop()` skips it | Verify null-check at `ble_conn_mgr.c:340-357` before relying on cb |
| #1 cap rejects legitimate traffic | `PROTOCOL_MAX_PAYLOAD - 5 = 250` >> real LL data MTU; no impact on real traffic |
| Pre-commit slow on full repo | Always `pre-commit run --files <files>`, never `--all-files` |
| Board #1 Sub-1GHz HW fault mid-bundle | All smoke is BLE 2.4 GHz; fault is irrelevant |

## Out of scope

- L2CAP reassembly / large ATT buffers (#2 is host-side clamp, not feature work).
- Spectrum command real allocations (#9 removes placeholders, doesn't add features).
- F8c MTU "diagnostic non-negotiating probe" alternative (deferred).
- Refactor of `s_gatt_seq` shared global to AttClient transaction object
  (preferred but invasive — minimal reorder fix used instead).
- Findings already closed: #5, #6 (committed in 36e77fe / 99d8f63 / 12e76ae).
- Findings deferred: none — all 9 actionable items in scope.

## Acceptance criteria

- 9/9 actionable findings resolved.
- One commit per finding, format `fix(f8f): #N — descripción`.
- Pre-commit clean on every commit.
- `pytest python/tests/` passes (no regressions; obsolete
  `test_read_response_ignores_echoed_command_frames` replaced by new contract
  tests).
- `cmake --build firmware/cc1352/build -j2` clean.
- Smoke baseline 2A and 2B 100% pass.
- Tags `v2.0-f8f-py` (Bundle 1 boundary) and `v2.0-f8f-fw` (Bundle 2 end)
  with release notes.
- FF-merge `feature/f8f-cleanup` into `feature/ti-rtos-migration`, no merge
  commits.
