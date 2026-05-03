# Code Review: TI-RTOS Migration Branch

Scope: `feature/ti-rtos-migration`, focused on `firmware/cc1352` and `python/feralrf`.

## Follow-up Review After Fixes

Notes:

- F8c is now treated as finished and included in this review.
- `CMD_CONNECT` no longer appears to be a blocker: `BleConn_initiate()` now configures a bounded `TRIG_REL_START` timeout (`firmware/cc1352/src/ble_conn.c:187-207`).
- The main `CMD_DISCONNECT` path no longer calls the sleeping legacy helper: it ACKs first and uses `BleConnMgr_initiateGracefulDisconnect()` (`firmware/cc1352/src/command_processor.c:916-937`).
- `_read_response()` now lets `seq=0` async events bypass the command sequence check and buffers unexpected async frames (`python/feralrf/radio.py:511-530`), so the original async-drop issue is partially addressed.

## Current Findings

1. High: passive follower can still overflow the protocol frame for long LL PDUs.
   - `follower_on_packet()` allocates `uint8_t buf[5 + 257]` and sends `payload_len = 5 + pdu_len` (`firmware/cc1352/src/command_processor.c:327-340`).
   - `pdu_len` is `uint8_t`, so the check `pdu_len > sizeof(buf) - 5u` can never reject a value above 257, while `PROTOCOL_MAX_PAYLOAD` is 255 (`firmware/cc1352/include/protocol.h:15-22`).
   - `OutputIF_sendResponse()` builds into `raw_frame[PROTOCOL_MAX_FRAME]` before checking encoded size (`firmware/cc1352/src/output_if.c:15-24`).
   - Recommended fix: cap emitted follower PDU bytes to `PROTOCOL_MAX_PAYLOAD - 5`, return/drop with an explicit reason when too large, and add a defensive payload-length guard before `protocol_build_frame()`.

2. High: F8c MTU exchange can advertise MTUs larger than the firmware can actually receive.
   - Python explicitly allows `gatt_exchange_mtu(client_mtu=247)` and encodes it on the wire (`python/feralrf/commands.py:210-219`).
   - Firmware stores that requested value and sends it verbatim in `ATT_EXCHANGE_MTU_REQ` (`firmware/cc1352/src/att_client.c:151-156,541-553`).
   - The same ATT stack still caps outgoing ATT PDUs to `ATT_DEFAULT_MTU` and has no L2CAP continuation reassembly: it treats every LL data PDU as a complete L2CAP frame (`firmware/cc1352/src/att_client.c:112-127,578-593`; routing from `firmware/cc1352/src/ble_conn_mgr.c:172-181`).
   - After advertising a large client MTU, a compliant peer may send fragmented or >23-byte ATT responses that this stack drops, leaving F8c reads/flows to time out.
   - Recommended fix: until L2CAP reassembly and larger ATT buffers exist, reject or clamp `client_mtu > ATT_DEFAULT_MTU` at the Python and firmware command boundary. If the goal is only diagnostics, expose it as a clearly non-negotiating probe.

3. High: F8c disconnect events are not registered until a GATT-touching command runs.
   - `BleConnMgr_setDisconnectCb(gatt_on_disconnected)` is called only from `ensure_gatt_callbacks()` (`firmware/cc1352/src/command_processor.c:271-295`).
   - `CMD_CONNECT` completes without installing that callback (`firmware/cc1352/src/command_processor.c:900-912`).
   - If the peer terminates immediately after connect, before `CMD_GATT_*` or `CMD_DISCONNECT`, `BleConnMgr_stop()` has no callback to emit `RSP_DISCONNECTED` (`firmware/cc1352/src/ble_conn_mgr.c:340-357`).
   - Recommended fix: register the disconnect callback during connection-manager init/start or immediately after successful `CMD_CONNECT`, not lazily through GATT setup.

4. Medium: GATT command rejection can corrupt the sequence used by an in-flight GATT operation.
   - GATT handlers assign `s_gatt_seq = seq` before checking whether `AttClient_start*()` accepted the operation (`firmware/cc1352/src/command_processor.c:992-996,1012-1018,1033-1039,1065-1075,1096-1102,1117-1125`).
   - If a second GATT command arrives while an earlier transaction is still active, the second command is rejected, but `s_gatt_seq` now points at the rejected command. The completion callback for the first command then emits `RSP_GATT_*`/`RSP_GATT_DONE` with the wrong sequence.
   - Python waits for the original command sequence and drops mismatched nonzero sequences in `_read_response()` (`python/feralrf/radio.py:511-519`), so the original caller can time out even though firmware completed the transaction.
   - Recommended fix: move `s_gatt_seq = seq` until after `AttClient_start*()` returns true, or store sequence in the AttClient transaction object rather than a shared global.

5. Medium: ATT discovery parsers can still loop or read invalid fields on malformed peer responses.
   - `handle_read_by_group_type_rsp()` uses `entry_len = pdu[1]` without validating `entry_len >= 4` (`firmware/cc1352/src/att_client.c:253-284`).
   - `handle_read_by_type_rsp()` uses `entry_len = pdu[1]` without validating `entry_len >= 5` (`firmware/cc1352/src/att_client.c:296-321`).
   - `entry_len == 0` leaves `offset` unchanged, and too-small values underflow `uuidLen`.
   - Recommended fix: reject malformed `entry_len` before the loop and fail/advance discovery cleanly.

6. Medium: BLE follower supervision timestamp is stale before the first data packet.
   - The transition from ADV scan to `LL_FOLLOWER_STATE_FOLLOWING` does not initialize `s_last_rx_rat` (`firmware/cc1352/src/ll_follower.c:302-305`).
   - The supervision check later compares the current RAT time against that stale value even when no data packet has been captured (`firmware/cc1352/src/ll_follower.c:335-345`).
   - Recommended fix: initialize `s_last_rx_rat` when entering `FOLLOWING`, probably to the computed anchor/current RAT time.

7. Medium: async RF errors are still inconsistent and can be swallowed by Python stream APIs.
   - RX backend start failure sends `RSP_ERROR` with `seq=0` (`firmware/cc1352/src/data_task.c:117-125`).
   - TX execution failure sends `RSP_ERROR` with `seq=0xFF` (`firmware/cc1352/src/control_task.c:348-355`).
   - Python warns and discards `seq=0xFF` errors inside `_read_response()` (`python/feralrf/radio.py:503-509`).
   - `read_packets()` only yields `Response.RX_PACKET`; other responses returned by `_read_response()` are ignored by falling through the loop (`python/feralrf/radio.py:1491-1494`).
   - Recommended fix: standardize async error sequencing and surface async errors as exceptions or state events in `start_rx()`/`read_packets()` and TX helpers.

8. Medium: ATT parser for follower captures mis-decodes read responses as handle-bearing PDUs.
   - `python/feralrf/_ll_parser.py:160-164` includes `0x0B` (`ATT_READ_RSP`) and `0x0D` (`ATT_READ_BLOB_RSP`) in `has_handle`.
   - Those response PDUs carry only `value`, so the parser consumes the first two value bytes as a fake handle.
   - Recommended fix: split ATT parsing by opcode shape and add tests for `ATT_READ_RSP` and `ATT_READ_BLOB_RSP`.

9. Medium: pending spectrum command IDs collide with BLE connection IDs.
   - `Command.CONNECT`, `DISCONNECT`, and `CONN_STATUS` occupy `0x40`, `0x41`, and `0x42` (`python/feralrf/enums.py:75-78`).
   - `PENDING_COMMAND_IDS` still maps `SPECTRUM_SCAN`, `SPECTRUM_MONITOR`, and `SPECTRUM_STOP` to those same values (`python/feralrf/enums.py:180-185`).
   - Recommended fix: reserve non-conflicting IDs or remove pending spectrum IDs until the firmware protocol has real allocations.

10. Medium: protocol constants are duplicated across firmware modules instead of using one source of truth.
   - Crypto command/response IDs are defined in both `firmware/cc1352/include/protocol.h:35-54` and `firmware/cc1352/src/command_processor.c:41-50,84-91`.
   - Error/response IDs are also repeated in local modules, e.g. `RSP_ERROR` and `ERR_RF_INIT_FAILED` in `firmware/cc1352/src/data_task.c:22-24`, plus `RSP_GATT_NOTIFY` in both `firmware/cc1352/src/att_client.c:22` and `firmware/cc1352/src/command_processor.c:102`.
   - This already risks redefinition warnings and future drift between files.
   - Recommended fix: expose all command/response/error IDs from `protocol.h` and remove local duplicate `#define`s.

11. Low: deprecated `BleConn_disconnect()` remains exported with the old unsafe behavior.
   - The command path no longer calls it, and current in-tree references show only the declaration, definition, and comments.
   - The helper still queues `LL_TERMINATE_IND`, sleeps, then stops the connection manager (`firmware/cc1352/src/ble_conn.c:262-289`), and it remains declared in the public header (`firmware/cc1352/include/ble_conn.h:61-73`).
   - Recommended fix: make it `static`/remove it if no out-of-tree caller needs it, or rename it to mark the unsafe legacy semantics.

## Verification Snapshot

- Firmware build: `cmake --build firmware/cc1352/build -j2` passed.
- F8c Python focused tests: `PYTHONPATH=python pytest -q python/tests/test_gatt_api.py python/tests/test_disconnect_events.py python/tests/test_gatt_notifications.py` produced `62 passed`.
- Focused Python tests: `PYTHONPATH=python pytest -q python/tests/test_radio_strict_responses.py python/tests/test_follow_connection.py python/tests/test_gatt_notifications.py python/tests/test_ll_parser.py python/tests/test_gatt_api.py` produced `85 passed, 1 failed`.
- Failing test: `python/tests/test_radio_strict_responses.py::test_read_response_ignores_echoed_command_frames`. Current code ignores the echoed command frame, but the test calls `_read_response()` directly without setting `_last_seq`, so the following ACK with `seq=0x10` is treated as stale by `python/feralrf/radio.py:516`.

## Local Worktree Note

Current uncommitted local change observed during this follow-up:

- Modified: `firmware/cc1352/include/radio_if.h`

## F8f Cleanup Branch Review

Snapshot: `feature/f8f-cleanup` at/after `fix(f8f): #7a — TX async error uses seq=0, align with RX-side contract`.

The earlier `Current Findings` section above is now partly historical. Current code has addressed several items:

- Follower payload cap and `OutputIF_sendResponse()` defensive guard are present (`firmware/cc1352/src/command_processor.c:330-344`, `firmware/cc1352/src/output_if.c:21-29`).
- Python clamps `gatt_exchange_mtu()` to `client_mtu == 23` (`python/feralrf/commands.py:210-227`).
- Disconnect callback is installed from `CMD_CONNECT` via `ensure_gatt_callbacks()` (`firmware/cc1352/src/command_processor.c:910-922`).
- GATT handlers now assign `s_gatt_seq` only after `AttClient_start*()` succeeds (`firmware/cc1352/src/command_processor.c:991-1042`).
- ATT discovery malformed `entry_len` checks are present (`firmware/cc1352/src/att_client.c:253-333`).
- Passive follower initializes `s_last_rx_rat` when entering `FOLLOWING` (`firmware/cc1352/src/ll_follower.c:302-310`).
- Python enum collisions and ATT read-response parsing are fixed in the current Python code.
- Follow-up `fix(f8f): follow-up — seq=0xFF synchronous bypass + host_if_task contract + test helper` resolves the post-Bundle-2 findings for `_read_response(expected={Response.ERROR})`, `HostIFTask_poll()` busy errors, and the duplicate-delimiter async-error test helper.

### Current F8f Notes

1. Deferred / accepted risk: firmware still accepts oversized `CMD_GATT_EXCHANGE_MTU` from non-current hosts.
   - Python now rejects anything except 23 (`python/feralrf/commands.py:222-227`), but firmware still accepts any 2-byte `client_mtu` and passes it through (`firmware/cc1352/src/command_processor.c:1000-1018`).
   - `AttClient_startMtuExchange()` only floors values below `ATT_DEFAULT_MTU`; values above 23 are stored and later advertised (`firmware/cc1352/src/att_client.c:571-581`).
   - A direct serial client or older Python host can still send `client_mtu=247` and recreate the large-MTU/no-reassembly failure.
   - Not treated as an F8f blocker because the implementation plan scoped this mitigation to the Python boundary until L2CAP reassembly exists.

2. Resolved: `seq=0xFF` async-error compatibility now works for `_read_response(expected={...ERROR...})`.
   - `_read_response()` returns `RSP_ERROR` with `seq in (0, 0xFF)` before the stale-sequence filter when the caller is streaming or explicitly expects `Response.ERROR` (`python/feralrf/radio.py:516-531`).
   - Regression coverage was added for synchronous `seq=0xFF` delivery (`python/tests/test_async_error_surfacing.py:90-105`).

3. Resolved: host busy error path now follows the async-error/protocol contract.
   - `HostIFTask_poll()` includes `output_if.h` and emits `OutputIF_sendResponse(RSP_ERROR, 0u, {ERR_INVALID_STATE}, 1u)` instead of local literals and `seq=0xFF` (`firmware/cc1352/src/host_if_task.c:13-17,92-100`).

4. Separate cleanup: firmware build is not warning-clean.
   - `cmake --build firmware/cc1352/build -j2` passes, but emits `unused variable 'l2cap_rx'` from `CMD_CONN_STATUS` (`firmware/cc1352/src/command_processor.c:856`).
   - This appears pre-existing and is not treated as an F8f blocker.

5. Resolved: async-error test helper no longer duplicates the COBS delimiter.
   - `python/feralrf/protocol.py:46-63` shows `build_frame()` already returns COBS-encoded bytes with delimiter.
   - `python/tests/test_async_error_surfacing.py:49-52` now extends the read buffer directly with `build_frame()` and documents that the delimiter is already included.

6. Low / cosmetic: `control_task.c` still has a local `extern` for `OutputIF_sendResponse()`.
   - The behavior is correct (`RSP_ERROR`, `seq=0`, `ERR_INVALID_STATE`), but the declaration style differs from the cleaned-up `host_if_task.c` path (`firmware/cc1352/src/control_task.c:357-361`).
   - This is not a functional finding for the follow-up; it is a small consistency cleanup if the file is touched again.

### Current Verification

- Focused follow-up Python tests: `PYTHONPATH=python pytest -q python/tests/test_async_error_surfacing.py python/tests/test_radio_strict_responses.py python/tests/test_gatt_api.py` produced `53 passed`.
- Full Python suite: `PYTHONPATH=python pytest -q python/tests` produced `454 passed, 5 skipped`.
- Firmware build: `cmake --build firmware/cc1352/build -j2` passed.

## Additional Review Pass After `a6b3f5c`

Scope: current `feature/ti-rtos-migration` at `a6b3f5c`, with unresolved local `radio_if.h` work left untouched.

### New Findings

1. High: oversized but delimiter-bounded COBS input can overflow `CommandProcessor_processEncodedFrame()`'s decoded stack buffer.
   - `PROTOCOL_MAX_FRAME` is 261 bytes and `COBS_MAX_ENCODED` is 264 including the delimiter (`firmware/cc1352/include/protocol.h:15-28`).
   - `HostIFTask_poll()` stores encoded bytes before the `0x00` delimiter in `s_encoded_frame[COBS_MAX_ENCODED]` and accepts bytes while `s_encoded_len < sizeof(s_encoded_frame)`, so it can pass 264 encoded bytes excluding the delimiter to the RF task (`firmware/cc1352/src/host_if_task.c:22-28,76-119`).
   - A 262-byte decoded COBS payload of nonzero bytes encodes to 264 bytes excluding the delimiter; Python check: `len(cobs.encode(b"\x01" * 262)) == 264`.
   - `CommandProcessor_processEncodedFrame()` then decodes into `uint8_t frame[PROTOCOL_MAX_FRAME]` without any output capacity parameter or post-decode length guard before writes happen (`firmware/cc1352/src/command_processor.c:1252-1266`; decoder writes at `firmware/cc1352/src/protocol.c:80-113`).
   - Recommended fix: either make the RX encoded buffer/acceptance limit exclude the delimiter (`COBS_MAX_ENCODED - 1`) and reject `encoded_len > COBS_MAX_ENCODED - 1`, or better add a bounded `cobs_decode(input, input_len, output, output_cap)` that fails before `write_idx` exceeds `PROTOCOL_MAX_FRAME`.

2. Medium: explicit `GATT_READ_BY_UUID` treats malformed `ATT_READ_BY_TYPE_RSP` with impossible `entry_len` as an empty success.
   - `handle_read_by_uuid_rsp()` validates only `entry_len < 3`, then loops while `offset + entry_len <= len` (`firmware/cc1352/src/att_client.c:354-385`).
   - If a peer returns `len=4` and `entry_len=10`, the loop never runs, the state moves to `IDLE`, and `onDone(0)` reports success (`firmware/cc1352/src/att_client.c:387-396`).
   - Python then receives only `RSP_GATT_DONE status=0` and returns an empty list, which is indistinguishable from the valid `ATTRIBUTE_NOT_FOUND` path (`python/feralrf/radio.py:1065-1091`).
   - Recommended fix: mirror the existing discovery guards and reject `entry_len > len - 2` and non-integral trailing bytes for `READ_BY_UUID`, returning the malformed-response status instead of `onDone(0)`.

3. Medium: ATT transactions have no firmware-side timeout, so one lost/malformed peer response can wedge all future GATT commands until disconnect/reset.
   - The public callback contract already reserves `onDone(..., 2=timeout)` (`firmware/cc1352/include/att_client.h:35`), but `AttClient_poll()` only returns early while `s_request_pending` is true and has no deadline/event counter watchdog (`firmware/cc1352/src/att_client.c:681-753`).
   - All start paths reject new operations unless `s_state == ATT_STATE_IDLE` (`firmware/cc1352/src/att_client.c:518-605`), so after Python times out waiting for a `GATT_*` completion, later `CMD_GATT_*` commands return `ERR_INVALID_STATE` until disconnect resets the client.
   - This is especially visible for `CMD_GATT_SUBSCRIBE`, whose ACK is deferred until `ATT_WRITE_RSP`; if the peer never answers, the host sees a timeout but firmware remains in `WAIT_WRITE_RSP` (`firmware/cc1352/src/command_processor.c:957-997`, `firmware/cc1352/src/att_client.c:551-568`).
   - Recommended fix: track the event counter or RAT time when an ATT request is queued, fail with `onDone(2)` after a bounded number of connection events, clear `s_request_pending`, and return to `IDLE`.

4. Low/Medium: `read_packets()` consumes unrelated async events instead of preserving them for their dedicated iterators.
   - `_read_response(expected={...})` buffers unexpected `seq=0` async frames for later consumers, but `read_packets()` calls `_read_response()` with `expected=None` (`python/feralrf/radio.py:441-557`, `python/feralrf/radio.py:1524-1536`).
   - With `expected=None`, frames like `RSP_DISCONNECTED` or `RSP_GATT_NOTIFY` are returned to `read_packets()` and then ignored because only `RSP_ERROR` and `RSP_RX_PACKET` are handled (`python/feralrf/radio.py:1524-1571`).
   - A caller that runs `read_packets()` during mixed BLE activity can permanently lose the disconnect/notification event before `read_disconnect_events()` or `read_gatt_notifications()` has a chance to drain it.
   - Recommended fix: have `read_packets()` re-buffer recognized async events it does not yield, or add a stream mode to `_read_response()` that allows `RSP_RX_PACKET` regardless of sequence while still buffering other `seq=0` async frames.

### Verification

- Rechecked focused follow-up tests: `PYTHONPATH=python pytest -q python/tests/test_async_error_surfacing.py python/tests/test_radio_strict_responses.py python/tests/test_gatt_api.py` produced `53 passed`.
- Rechecked firmware build: `cmake --build firmware/cc1352/build -j2` passed.
