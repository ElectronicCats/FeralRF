# F22 — Test modes CW + PRBS — Design Spec

> **Date:** 2026-04-29
> **Branch:** `feature/f22-test-modes`
> **Phase:** F22 (Bloque D — chip API completeness) of plan-v2
> **Author:** Sabas + Claude (brainstorming session)

---

## 1. Goal & scope

Expose the CC1352P7's `rfc_CMD_TX_TEST` capability via three new commands so the Python API can emit:

- An unmodulated carrier (CW) at the current PHY / channel / frequency.
- A PRBS-modulated continuous test signal (PRBS-9 or PRBS-15).
- A clean stop for either.

Primary value: a debug primitive every future RF / lab task can lean on. CW lets you check whether the transmitter is on independent of the modulator; PRBS lets you check that the modulator runs without needing a peer that decodes a real protocol.

Secondary value: closes one phase of Bloque D (chip API completeness).

### In scope

- 3 new firmware commands and matching Python methods.
- The test signal runs on whatever PHY is currently active (`set_phy(...)` first). No new PHY enum, no new RadioSetup.
- Validation via firmware telemetry (CW) + 2-board RX bump (PRBS). No spectrum analyzer required.

### Out of scope

- Spectrum analyzer validation — optional manual checkpoint per §F22 ("if disponible").
- TX power calibration measurements (covered by F23 High PA).
- Frequency override via the test method itself — use `set_phy(..., frequency_hz=...)` for custom frequencies.
- Auto-stop timer / duration limits — caller is responsible for `tx_test_stop()`.

## 2. Open design decisions (resolved during brainstorming)

| Decision | Resolution | Why |
|---|---|---|
| API frequency: take `freq_hz` arg vs use current PHY/freq | **Use current PHY/freq** | Consistent with rest of API (`set_phy → operate`); no auto-band-detection magic; cleaner firmware. |
| PRBS pattern coverage | **Both PRBS-9 and PRBS-15** | ~5 LOC each. PRBS-9 default for spec compliance (BLE DTM). PRBS-15 available for cleaner spectral analysis. |
| Validation approach | **CW: firmware tx_status check; PRBS: scan rx_count differential** | Empirical for PRBS (RX board sees PRBS bytes occasionally trip sync → rx_count up). For CW, telemetry confirms RF Core executed CMD_TX_TEST without error (DONE_OK bit). Manual spectrum analyzer optional. |

## 3. API

### 3.1 Python (radio.py)

```python
def tx_cw(self, power_dbm: int = 0) -> None:
    """Emit unmodulated carrier on current PHY/channel.

    Requires set_phy(...) first to select band + channel/frequency.
    Stop with tx_test_stop(). Test signal runs until cancelled.

    Raises:
        CommandError: if no PHY is set or RF Core rejects the command.
    """

def tx_prbs(self, power_dbm: int = 0, pattern: str = "prbs9") -> None:
    """Emit PRBS-modulated test signal on current PHY/channel.

    Args:
        power_dbm: TX power, -20 to +5 dBm (std-PA cap).
        pattern: 'prbs9' (default, BLE DTM compliant) or 'prbs15'.

    Raises:
        ValueError: if pattern not 'prbs9' or 'prbs15'.
        CommandError: as tx_cw.
    """

def tx_test_stop(self) -> None:
    """Stop any active CW or PRBS test signal. Idempotent."""
```

### 3.2 Firmware commands (command_processor.c)

| Command ID | Name | Payload | Response |
|---|---|---|---|
| `0x55` | `CMD_TX_CW` | (none) | `RSP_ACK` or `RSP_ERROR` |
| `0x56` | `CMD_TX_PRBS` | `[mode: u8]` (1=PRBS9, 2=PRBS15) | `RSP_ACK` or `RSP_ERROR` |
| `0x57` | `CMD_TX_TEST_STOP` | (none) | `RSP_ACK` |

Errors (CMD_TX_CW / CMD_TX_PRBS):
- `ERR_RF_NOT_READY (0x07)` — `set_phy` was not called first; `s_rf_handle` is NULL.
- `ERR_INVALID_PAYLOAD (0x05)` — PRBS mode byte not 1 or 2.
- `ERR_TX_FAILED (0x08)` — RF Core rejected `CMD_TX_TEST` (e.g., synthesizer not locked).

## 4. Firmware implementation

### 4.1 Static state in radio_if.c

```c
static rfc_CMD_TX_TEST_t s_cmd_tx_test;
static RF_CmdHandle s_test_cmd_handle = RF_SCHEDULE_CMD_ERROR;
```

### 4.2 New functions

```c
bool RadioIF_runTxTest(uint8_t mode);   /* 0=CW, 1=PRBS9, 2=PRBS15 */
void RadioIF_stopTxTest(void);
```

Implementation outline:

```c
bool RadioIF_runTxTest(uint8_t mode) {
    if (s_rf_handle == NULL) return false;
    if (s_test_cmd_handle >= 0) RadioIF_stopTxTest();   /* idempotent */

    /* SmartRF defaults: bOverrideDefault=0 (use synth defaults).
     * bFsOff=0 keeps FS on after TX_TEST so we don't lose the lock. */
    memset(&s_cmd_tx_test, 0, sizeof(s_cmd_tx_test));
    s_cmd_tx_test.commandNo = CMD_TX_TEST;
    s_cmd_tx_test.config.bUsePrbs9  = (mode == 1) ? 1 : 0;
    s_cmd_tx_test.config.bUsePrbs15 = (mode == 2) ? 1 : 0;
    s_cmd_tx_test.config.bFsOff = 0;
    s_cmd_tx_test.startTrigger.triggerType = TRIG_NOW;
    s_cmd_tx_test.startTrigger.pastTrig = 1;
    s_cmd_tx_test.endTrigger.triggerType = TRIG_NEVER;   /* runs until RF_cancelCmd */
    s_cmd_tx_test.endTime = 0;
    s_cmd_tx_test.condition.rule = COND_NEVER;
    s_cmd_tx_test.status = 0x0000;

    /* TX power: same path as RadioIF_transmitRaw — applies via RF_setTxPower */
    RadioIF_applyRfTxPower(s_rf_handle, RadioIF_resolveTxPowerValue(s_tx_power_dbm));

    /* Re-tune to currently selected channel (tunes synthesizer to target freq).
     * Each band has its own FS path; reuse what set_phy already configured. */
    if (PhyManager_isBlePhy(s_selected_phy)) {
        RF_postCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdFs, RF_PriorityNormal, NULL, 0);
    } else if (RadioIF_isIeee154PhySelected()) {
        RF_postCmd(s_rf_handle, (RF_Op *)&Ieee154_0_cmdFs, RF_PriorityNormal, NULL, 0);
    } else if (RadioIF_isSub1ghzPhySelected()) {
        RF_Op *fs = (s_current_rf_mode == &Prop0_mode433)
                    ? (RF_Op *)&Prop0_cmdFs433
                    : (RF_Op *)&Prop0_cmdFs;
        RF_postCmd(s_rf_handle, fs, RF_PriorityNormal, NULL, 0);
    }

    s_test_cmd_handle = RF_postCmd(s_rf_handle, (RF_Op *)&s_cmd_tx_test,
                                   RF_PriorityNormal, NULL, 0);
    s_last_tx_status = s_cmd_tx_test.status;
    return s_test_cmd_handle >= 0;
}

void RadioIF_stopTxTest(void) {
    if (s_rf_handle != NULL && s_test_cmd_handle >= 0) {
        RF_cancelCmd(s_rf_handle, s_test_cmd_handle, 0);
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
    }
    s_test_cmd_handle = RF_SCHEDULE_CMD_ERROR;
}
```

### 4.3 Telemetry

`s_last_tx_status` is already exposed via `RadioIF_getRfDebug` and the `CMD_DEBUG_TIMING` (0x47) command. No new telemetry path needed — `radio.debug_timing().tx_status` reads the field.

CMD_TX_TEST status values of interest:
- `0x0400` `IDLE` (or queued)
- `0x0001` `PENDING`
- `0x0002` `ACTIVE` ← what we expect after `tx_cw` / `tx_prbs`
- `0x0400` `DONE_OK` ← after `tx_test_stop` (cancelled cleanly)
- `0x0401`+ `ERROR_*` ← failure modes

The validation script asserts `status` in `{ACTIVE, DONE_OK}` after invoking, not in any error state.

## 5. Validation

### 5.1 Wire-level smoke — `python/examples/lab/smoke_f22_tx_test.py`

Auto-runs on 2 boards (TX = `/dev/ttyACM5`, RX = `/dev/ttyACM8` per project_hardware.md).

```python
# Test 1: CW on Sub-1GHz 868 MHz
tx.set_phy(PHY.SUB_1GHZ_868)
tx.tx_cw(power_dbm=5)
time.sleep(0.5)
dbg = tx.debug_timing()
tx.tx_test_stop()
assert dbg.tx_status not in ERROR_RANGE, f"CW failed: status=0x{dbg.tx_status:04X}"

# Test 2: CW on BLE 1M ch37 (verifies BLE band path)
tx.set_phy(PHY.BLE_1M, channel=37)
tx.tx_cw(power_dbm=5)
time.sleep(0.5)
dbg = tx.debug_timing()
tx.tx_test_stop()
assert dbg.tx_status not in ERROR_RANGE

# Test 3: PRBS-9 on Sub-1GHz 868, RX scan_count differential
rx.set_phy(PHY.SUB_1GHZ_868)
rx.start_rx(); time.sleep(1.0)
n_idle = len(list(rx.read_packets(timeout=0.5)))
rx.stop_rx()

tx.set_phy(PHY.SUB_1GHZ_868)
tx.tx_prbs(power_dbm=5, pattern="prbs9")
rx.start_rx(); time.sleep(1.0)
n_prbs = len(list(rx.read_packets(timeout=0.5)))
rx.stop_rx()
tx.tx_test_stop()
assert n_prbs > n_idle + 5, f"PRBS not detected: idle={n_idle} prbs={n_prbs}"

# Test 4: PRBS-15 same as test 3 with pattern='prbs15'

# Test 5: tx_test_stop is idempotent
tx.tx_test_stop()  # already stopped — should not raise
```

Pass criteria:
- All 5 tests pass without exception.
- CW on both Sub-1GHz and BLE bands sets a non-error tx_status.
- PRBS-9 and PRBS-15 increase RX packet count by ≥5 vs idle baseline.
- `tx_test_stop()` is safe to call when nothing is running.

### 5.2 Unit tests — `python/tests/test_tx_test.py`

Hardware-free, run in CI:

1. `test_tx_cw_command_id` — `Command.TX_CW == 0x55`.
2. `test_tx_prbs_command_id` — `Command.TX_PRBS == 0x56`.
3. `test_tx_test_stop_command_id` — `Command.TX_TEST_STOP == 0x57`.
4. `test_tx_prbs_payload_prbs9` — payload byte `0x01`.
5. `test_tx_prbs_payload_prbs15` — payload byte `0x02`.
6. `test_tx_prbs_invalid_pattern_raises` — `pattern='prbs99'` raises `ValueError`.
7. `test_tx_test_stop_no_payload` — empty payload bytes.

(7 unit tests covering protocol contract.)

### 5.3 Manual checkpoint (deferred)

Optional spectrum analyzer test:
- Connect spectrum analyzer to TX board's antenna port.
- `tx.set_phy(PHY.BLE_1M, channel=37); tx.tx_cw(power_dbm=5)` → verify single-tone at 2402 MHz on analyzer trace.
- `tx.tx_prbs(...)` → verify spread spectrum without nulls.

Tag `v2.0-f22` lands when wire-level smoke (§5.1) PASSES. Manual spectrum check is informational, not gating.

## 6. File layout

| File | Change | LOC est. |
|---|---|---|
| `firmware/cc1352/include/radio_if.h` | declarations for new functions | +2 |
| `firmware/cc1352/src/radio_if.c` | new state + 2 new functions | +60 |
| `firmware/cc1352/src/command_processor.c` | 3 CMD ids + 3 handlers | +30 |
| `python/feralrf/commands.py` | 3 enum entries | +3 |
| `python/feralrf/radio.py` | 3 new methods | +50 |
| `python/tests/test_tx_test.py` | 7 unit tests | +50 |
| `python/examples/lab/smoke_f22_tx_test.py` | hardware smoke | +90 |
| `docs/superpowers/specs/2026-04-29-f22-test-modes-design.md` | this file | (already counted) |
| `docs/superpowers/plans/2026-04-29-f22-test-modes-plan.md` | next: writing-plans output | (next step) |

Total: ~285 LOC new code, all isolated, no protocol changes.

## 7. Risks

| # | Risk | Mitigation |
|---|---|---|
| f22-r1 | `RF_postCmd(CMD_TX_TEST)` fails because no FS lock → no TX, no error visible | The implementation runs CMD_FS first (per band). Validation script checks `tx_status` after invocation. |
| f22-r2 | TX_TEST keeps RF Core busy and blocks subsequent operations | `RadioIF_stopTxTest()` cancels + flushes; tested in unit/smoke. F9 hot-switch path means subsequent `set_phy` / `start_rx` work without close+open. |
| f22-r3 | Idempotency: caller calls `tx_cw` while CW is already running, or `tx_test_stop` when nothing runs | `RadioIF_runTxTest` calls `RadioIF_stopTxTest` if `s_test_cmd_handle >= 0`. `RadioIF_stopTxTest` is no-op if not running. |
| f22-r4 | Power validation: `set_power(5)` exceeds std-PA range | Existing `set_power` enforces range; reusing it. High PA path (DIO29 fix, F23) future. |

All mitigated within F22 scope.

## 8. Closure criteria

- [ ] 7/7 unit tests PASS.
- [ ] 5/5 wire-level smoke tests PASS on 2 boards.
- [ ] Pre-commit clean.
- [ ] No regression on Python suite (322 baseline + 7 new = 329).
- [ ] No regression on F9 6/6 PHY switch matrix.
- [ ] Plan in `docs/superpowers/plans/2026-04-29-f22-test-modes-plan.md` covered checkbox by checkbox.
- [ ] `project_f22_done.md` memory entry written.
- [ ] Commit on `feature/f22-test-modes`. Tag `v2.0-f22` after smoke PASSES.

## 9. Open questions

None — all resolved in brainstorming.

---

**Next step:** writing-plans skill → step-by-step implementation plan in `docs/superpowers/plans/2026-04-29-f22-test-modes-plan.md`.
