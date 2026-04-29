# Capture-and-replay TX timeout — root cause and fix

**Date:** 2026-04-29
**Branch:** `feature/f11-port-ble-attacks-tirtos`
**Filed by:** commit `27bd964` ("test(f11a): capture_and_replay smoke FAIL — TX timeout post-stop_rx")

## TL;DR

The bug has **nothing to do** with `stop_rx`, RF state, or capture traffic load. It is a deterministic Python-side protocol bug: `_next_seq()` in `python/feralrf/radio.py` returns `0xFF` once every 256 commands, but the firmware reserves `0xFF` as the sentinel for asynchronous errors. When a regular command happens to use `seq=0xFF`, its echo response is filtered out by the parser at `radio.py:396` and the caller times out.

Fix is a 5-line change in `_next_seq()` to skip `0xFF`. Hardware fix not required.

## Evidence

### Smoke reproduction
- `python/examples/lab/smoke_ble_attacks.py --attack capture_and_replay` (commit `27bd964`) reports `TimeoutError: Response timeout` in the replay loop.
- Repro on `/dev/ttyACM8` (board #1) without RX board, single-port: `capture_and_replay()` fails after ~13.7s on first replay.

### Index is deterministic and timing-independent
500 sequential `transmit()` calls, no RX cycle, varying `interval_us`:

| `interval_us` | OK count | Fail at | Rate     |
|---------------|----------|---------|----------|
| 0             | 252      | #252    | 108/s    |
| 5_000         | 252      | #252    |  69/s    |
| 20_000        | 252      | #252    |  34/s    |
| 50_000        | 252      | #252    |  17/s    |

Failure index is exactly `252` regardless of TX rate ⇒ it is not a state, queue, or RF-driver issue.

### Index = `(0xFF - seq_at_loop_start)`
Pre-loop commands consumed 3 seq values (`RADIO_INIT=0`, `GET_INFO=1`, `SET_PHY=2`), so the first transmit uses `seq=3`. Failure on the 253rd transmit (i=252) ⇒ that call uses `seq=255=0xFF`. ✓

### Confirmed by direct seq injection
Forcing `radio._seq = 0xFF` before a single `transmit()` ⇒ TimeoutError after 2.0s. Setting `radio._seq = 0xFE` (or `0x00`) ⇒ transmit succeeds in <10ms.

### Firmware confirms 0xFF is the async sentinel
- `firmware/cc1352/src/control_task.c:348-353` — RF async error: `OutputIF_sendResponse(0x81u, 0xFFu, ...)`
- `firmware/cc1352/src/host_if_task.c:96` — host-side busy error: `OutputIF_sendResponse(0x81u, 0xFFu, ...)`
- `firmware/cc1352/src/command_processor.c:121` — regular responses echo the command's seq

### Why the Python parser drops it
`python/feralrf/radio.py:395-401`:
```python
# Skip async errors (seq=0xFF) — log but don't consume as response.
if seq == 0xFF:
    warnings.warn(f"Async RF error: code=0x{err_code:02X}", stacklevel=2)
    continue
```
The 0xFF check fires on the legitimate command echo, the parser keeps reading, the next byte never arrives within the deadline ⇒ `TimeoutError`.

## Fix

`python/feralrf/radio.py:310-314` — make `_next_seq()` skip 0xFF so it never gets used as a regular command seq:

```python
def _next_seq(self) -> int:
    """Get next sequence number. 0xFF is reserved for firmware async errors."""
    seq = self._seq
    nxt = (seq + 1) & 0xFF
    if nxt == 0xFF:
        nxt = 0
    self._seq = nxt
    return seq
```

Range becomes `0..0xFE` (255 values). No firmware change required.

### Why not change the firmware sentinel instead
- The `seq=0xFF=async` convention is encoded in two firmware sites and the Python parser; flipping to a different sentinel (e.g., a payload bit) is a wider change with port impact across the protocol.
- Reserving one seq value out of 256 is invisible from the wire and trivial.
- A future protocol bump (`v2.x`) could move async errors out of the seq field entirely — out of scope here.

## Test plan (TDD)

### Unit test — guarantees the fix is structural, not luck
New `python/tests/test_radio_seq.py`:

1. `test_next_seq_never_returns_0xff` — call `_next_seq()` 1024 times, assert no return value equals 0xFF.
2. `test_next_seq_full_cycle_size` — verify the cycle is exactly 255 distinct values, all in `0..0xFE`.
3. `test_next_seq_initial_state` — fresh `Radio` starts with `_seq=0`, first call returns `0`.

These run in CI under existing `build.yml`. Hardware-free.

### Hardware integration — confirms the fix solves the user-visible bug
Reuse the existing `smoke_ble_attacks.py` harness. Run with both boards:
```
python smoke_ble_attacks.py --tx-port /dev/ttyACM8 --rx-port /dev/ttyACM5 --attack capture_and_replay
```
Pass criterion: smoke prints `[ OK ] capture_and_replay wire smoke PASS`.

Plus a one-liner stress: 1000 sequential `transmit()` calls without disconnect, expect zero exceptions.

## Out of scope (intentional)

These came up during investigation but are *not* required to close the bug. Files for separate followups if Sabas wants:

1. **`stopRfBackend()` violates skill rule "NEVER RF_close"** (`firmware/cc1352/src/radio_if.c:1216-1243`) — for BLE mode, currently does `RF_close` then expects later `RF_open` to succeed. Skill says this can hang TI-RTOS internals. Did not actually trigger this bug, but is latent risk.
2. **BLE GenericRx has `bAutoFlushIgnored = 0`** (`radio_if.c:1135`) — skill says this should be `1` to prevent RF Core data queue saturation under heavy traffic. Latent under high-pkt-rate scans.
3. **`s_rx_rsp_seq` in `data_task.c:61` will at some point send an RX_PACKET frame with `seq=0xFF`**, triggering the harmless `warnings.warn("Async RF error...")` even though no error happened. Cosmetic — fix when convenient (skip 0xFF on that counter too).

## Done criteria

- [ ] Unit tests in `python/tests/test_radio_seq.py` PASS.
- [ ] `smoke_ble_attacks.py --attack capture_and_replay` PASS on both boards.
- [ ] 1000-transmit stress passes with zero timeouts.
- [ ] Commit on `feature/f11-port-ble-attacks-tirtos` with body summarizing root cause and pointing to this plan.
- [ ] Update `commit message of 27bd964` correction (or note in this plan + memory) — original "RX queue overflow" hypothesis was wrong.
