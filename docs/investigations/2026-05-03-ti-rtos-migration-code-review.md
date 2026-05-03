# Code Review: TI-RTOS Migration Branch

Scope: `feature/ti-rtos-migration`, focused on `firmware/cc1352` and `python/feralrf`.

## Current Findings

1. Critical: `CMD_CONNECT` can block firmware indefinitely.
   - `firmware/cc1352/src/ble_conn.c:180-188` disables both `endTrigger` and `timeoutTrigger` with `TRIG_NEVER`.
   - `firmware/cc1352/src/radio_if.c:2408-2411` then runs `CMD_BLE5_INITIATOR` through blocking `RF_runCmd()`.
   - `firmware/cc1352/src/command_processor.c:862-876` sends `RSP_CONN_RESULT` only after `BleConn_initiate()` returns.
   - If the target peer never appears, the host times out but the RF task remains blocked and cannot process recovery commands.
   - Recommended fix: reintroduce a bounded RF timeout, or post the initiator command and cancel it from a watchdog/control path.

2. High: `CMD_DISCONNECT` sleeps in the RF task before the queued terminate PDU can be sent.
   - `firmware/cc1352/src/command_processor.c:879-885` runs `BleConn_disconnect()` from the command-processing path.
   - In the TI-RTOS migration, command processing happens in the RF task via `DataTask_poll()` (`firmware/cc1352/src/data_task.c:78-80`), and `BleConnMgr_poll()` also runs later in that same RF task loop (`firmware/cc1352/src/main_rtos.c:179-188`).
   - `BleConn_disconnect()` queues `LL_TERMINATE_IND`, then calls `Task_sleep()` for three connection intervals before calling `BleConnMgr_stop()` (`firmware/cc1352/src/ble_conn.c:242-261`).
   - Because the only task that can run `BleConnMgr_poll()` is asleep, the queued terminate PDU is not transmitted during that wait; the code then stops the manager and drops the queue.
   - Recommended fix: make disconnect a state transition handled by `BleConnMgr_poll()` (queue terminate, mark pending disconnect, stop after a TX event or timeout), or remove the sleep and keep the manager alive until it has one event to send the control PDU.

3. High: Python drops valid async responses because `_read_response()` always enforces `_last_seq`.
   - `python/feralrf/radio.py:450-455` discards frames when `seq != self._last_seq` whenever `expected` is provided.
   - Firmware emits unsolicited `GATT_NOTIFY`, `LL_PACKET`, and `FOLLOW_DONE` with `seq=0` (`firmware/cc1352/src/att_client.c:125-129`, `firmware/cc1352/src/command_processor.c:290-311`).
   - After any command with sequence other than zero, `read_gatt_notifications()` and `read_ll_packets()` can time out even though valid frames arrived.
   - Current failing test: `python/tests/test_radio_strict_responses.py:39-51` also demonstrates the seq-filter issue for echoed command handling.
   - Recommended fix: add a stream/async read mode that disables seq matching, or allow `seq=0` for known unsolicited response IDs.

4. High: passive follower can overflow the protocol frame for long LL PDUs.
   - `firmware/cc1352/src/command_processor.c:293-303` accepts `pdu_len` up to 257 and then sends `payload_len = 5 + pdu_len`.
   - `PROTOCOL_MAX_PAYLOAD` is 255 (`firmware/cc1352/include/protocol.h:15-22`), and `OutputIF_sendResponse()` calls `protocol_build_frame()` before checking encoded size (`firmware/cc1352/src/output_if.c:15-24`).
   - A captured DLE-sized LL data PDU can therefore build a response payload larger than the raw frame buffer.
   - Recommended fix: cap follower payloads to `PROTOCOL_MAX_PAYLOAD - 5`, and add a defensive `payload_len <= PROTOCOL_MAX_PAYLOAD` guard in `OutputIF_sendResponse()`/`protocol_build_frame()`.

5. Medium: ATT discovery parsers can loop or read invalid fields on malformed peer responses.
   - `handle_read_by_group_type_rsp()` uses `entry_len = pdu[1]` without validating `entry_len >= 4` (`firmware/cc1352/src/att_client.c:221-252`).
   - `handle_read_by_type_rsp()` similarly needs `entry_len >= 5` (`firmware/cc1352/src/att_client.c:264-288`).
   - `entry_len == 0` creates an infinite loop; too-small values underflow `uuidLen`.
   - Recommended fix: validate entry size and fail discovery cleanly on malformed responses.

6. Medium: BLE follower supervision timestamp is stale/uninitialized before first data packet.
   - `s_last_rx_rat` is declared as session state at `firmware/cc1352/src/ll_follower.c:47-50`.
   - It is only set when a data packet is received (`firmware/cc1352/src/ll_follower.c:165-174`).
   - The supervision check runs even when no data packet has ever been captured (`firmware/cc1352/src/ll_follower.c:335-345`).
   - Recommended fix: initialize `s_last_rx_rat` when transitioning from ADV scan to FOLLOWING.

7. Medium: async RF errors are not delivered consistently to Python callers.
   - `RX_START` is ACKed before the RF backend actually starts (`firmware/cc1352/src/command_processor.c:431-442`), and `DataTask_poll()` reports backend start failure later with `RSP_ERROR` using `seq=0` (`firmware/cc1352/src/data_task.c:117-126`).
   - TX execution failures use `RSP_ERROR` with `seq=0xFF` (`firmware/cc1352/src/control_task.c:348-355`).
   - Python only special-cases async errors when `seq == 0xFF`, and even then it warns and discards them (`python/feralrf/radio.py:442-448`).
   - `read_packets()` calls `_read_response()` with no expected set and ignores non-`RX_PACKET` responses (`python/feralrf/radio.py:1237-1241`), so a post-ACK `ERR_RF_INIT_FAILED` can be silently swallowed.
   - Recommended fix: standardize async errors on one sequence value and expose them as exceptions or state transitions in the affected high-level API, especially `start_rx()`/`read_packets()` and TX helpers.

8. Medium: ATT parser for follower captures mis-decodes read responses as handle-bearing PDUs.
   - `python/feralrf/_ll_parser.py:160-164` includes opcodes `0x0B` (`ATT_READ_RSP`) and `0x0D` (`ATT_READ_BLOB_RSP`) in the generic `has_handle` set.
   - Per ATT, those response PDUs carry only `value`, not `[handle:2][value]`; the current parser consumes the first two value bytes as a fake handle and shifts the displayed value.
   - This affects Python-side analysis/export workflows for `Radio.read_ll_packets()` captures, especially when inspecting GATT reads from a followed connection.
   - Recommended fix: split opcode parsing by PDU shape instead of using one `has_handle` set; add tests for `ATT_READ_RSP` and `ATT_READ_BLOB_RSP`.

9. Medium: pending spectrum command IDs now collide with BLE connection IDs.
   - `python/feralrf/enums.py:75-78` assigns `CONNECT=0x40`, `DISCONNECT=0x41`, and `CONN_STATUS=0x42`.
   - `PENDING_COMMAND_IDS` still maps `SPECTRUM_SCAN=0x40`, `SPECTRUM_MONITOR=0x41`, and `SPECTRUM_STOP=0x42` (`python/feralrf/enums.py:180-185`), and docs repeat the stale mapping (`docs/protocol.md:75-85`).
   - Because `PENDING_COMMAND_IDS` is exported from `python/feralrf/__init__.py`, code that probes pending IDs can accidentally send BLE connection commands.
   - Recommended fix: reserve new non-conflicting pending IDs or remove spectrum IDs until a real allocation is chosen; update docs in the same change.

## Verification Snapshot

- Firmware build after excluding active F8c work: `cmake --build firmware/cc1352/build -j2` passed.
- Current worktree contains active F8c edits and was intentionally excluded from build-failure findings.
- Python tests from `python/`: `python -m pytest -q tests` produced `424 passed, 5 skipped, 1 failed`.
- Failing test: `tests/test_radio_strict_responses.py::test_read_response_ignores_echoed_command_frames`, tied to `_read_response()` sequence filtering.
- Focused Python check: `PYTHONPATH=python pytest -q python/tests/test_radio_strict_responses.py python/tests/test_follow_connection.py python/tests/test_gatt_notifications.py` produced `24 passed, 1 failed`; same `_read_response()` failure.

## Local Worktree Note

Existing local changes observed before this note was created:

- Modified: `firmware/cc1352/include/radio_if.h`
- Untracked: `docs/superpowers/plans/2026-05-02-f8c-mtu-readbyuuid-dc-reason.md`

Current uncommitted local changes after excluding active F8c work:

- Modified: `firmware/cc1352/include/att_client.h` (active F8c work)
- Modified: `firmware/cc1352/include/radio_if.h`
- Modified: `firmware/cc1352/src/att_client.c` (active F8c work)

F8c-related edits are treated as active work in progress and excluded from findings.
