# Historical command audit

Source baseline checked: `main@0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`, Python `0.3.0`, TI SDK `8.30.01.01` (`5b31d0a...`). This audit preserves what was actually recorded; it does not silently replace historical commands with corrected commands.

## Current implementation findings

- `firmware/cc1352/src/control_task.c:249` queues RAW. `ControlTask_onTxFrame()` at line 262 delegates directly to `ControlTask_onTxRaw()`.
- `firmware/cc1352/src/radio_if.c`, `RadioIF_transmitRaw()`, dispatches by the selected PHY: proprietary to `RadioIF_transmitPropRaw()`, BLE to `RadioIF_transmitBleAdvRaw()`, IEEE to `RadioIF_transmitIeee154Raw()`. RAW/FRAME/BURST/CONTINUOUS are FeralRF operations, not BLE operations.
- BURST and CONTINUOUS repeatedly call `RadioIF_transmitRaw()` from the scheduler. ACK occurs when scheduling/state is accepted, before all RF work completes.
- `python/examples/lab/ota_rx_probe.py` and `python/examples/smoke_phy4_ieee154.py` currently filter `RxStreamError` objects out of their packet lists and do not print/count them. They are useful packet observers but cannot prove absence of asynchronous errors.
- The current Python API describes proprietary `mod_type=4/5/6` as MSK/4-FSK/4-GFSK, but TI SDK `cc13x2x7_cc26x2x7/driverlib/rf_prop_cmd.h` defines 0=FSK, 1=GFSK, 2=OOK, all others reserved for the audited setup structure.
- `RadioIF_setPropConfig()` is `void`; the command handler sends ACK after calling it. That ACK is control evidence, not confirmation that the effective physical configuration or RF operation succeeded.

## Per-experiment audit

| Historical test | Actual command/procedure preserved | Guide-prescribed procedure | Current implementation checked | Classification | Reason | Repeated now |
|---|---|---|---|---|---|---|
| IEEE RX preflight, both boards | Exact shell commands not preserved in report; counts 16 and 15 recorded | `smoke_phy4_ieee154.py --port <bridge> --channel 25 --duration 10` | Script inspected; it discards `RxStreamError` | `CANNOT DETERMINE` exact command; equivalent observable | Parameters/results exist, literal command/stdout are incomplete | Yes, with custom error-visible harness |
| IEEE RAW COM33→COM88 | Exact shell command not preserved; 20 calls were accidentally made | RX first, negative, then 10 one-shot RAW calls | RAW PHY dispatch inspected | `INTENTIONAL EXPERIMENTAL VARIANT` (count deviation) | 20 was the actual run and must remain 20/20, not be rewritten as 10/10 | Yes, 10-request current baseline |
| IEEE RAW COM88→COM33 | Exact shell command not preserved; parameters/results recorded | Same in reverse | RAW PHY dispatch inspected | `CANNOT DETERMINE` exact command; result is valid | Literal command line missing | Yes, 10-request current baseline |
| FRAME COM33→COM88 | Exact shell command not preserved; 10 requests/results recorded | `ota_tx_frame.py`, RX first, 10 calls | `ControlTask_onTxFrame()` delegates to RAW | `CANNOT DETERMINE` exact command; API contract valid | RF backend is shared; no need to infer a separate RF path | No; historical 10/10 is sufficient |
| BURST 5×250000 | Parameters and observer output recorded; exact shell command not preserved | RX first for 12 s, `ota_tx_burst.py --count 5 --interval-us 250000` | Scheduler and current helper inspected | `EQUIVALENT PROCEDURE` | 1/5 observed; helper ACK means scheduled, not completed | No; same repeat would not discriminate cause |
| BURST host-open variant | Custom `init/config/transmit_burst/ACK/wait 3 s/disconnect` sequence preserved | Not the base guide step | Scheduler inspected | `INTENTIONAL EXPERIMENTAL VARIANT` | Validly tests the early-disconnect hypothesis; again 1/5 | No |
| CONTINUOUS E2 RX window | RX command/stdout preserved; no simultaneous TX stdout | Guide requires RX/TX overlap | Scripts/scheduler inspected | `PARTIAL EXECUTION` | Only one side of a two-terminal procedure is demonstrated; zero hits are not TX failure evidence | No; later coordinated executions supersede it |
| CONTINUOUS 250000 E3 | Both commands/stdout preserved and coordinated | Matches guide | Helper and scheduler inspected | `EXACT PROCEDURE MATCH` | 1 hit vs threshold 5; PARTIAL | No; unchanged repetition adds little without scheduler instrumentation |
| Post-STOP E4 | Later independent RX window preserved | Guide prescribed later post window | Stop path inspected | `EXACT PROCEDURE MATCH` for guide, but limited observable | Demonstrates no marker in that later window, not immediate cessation | No |
| CONTINUOUS interval 0 E5 | Both commands/stdout preserved | Matches guide except marker-specific negative was absent | Helper and scheduler inspected | `PARTIAL EXECUTION` | Repetition passed; gate incomplete because RX_STOP timed out and negative was not marker-specific | No; already repeated by directed campaign |
| Directed STOP helper v1 | Full custom helper preserved | Experimental improvement over guide | Python timing and queue path reviewed | `PROCEDURAL ERROR` for immediate RF ordering | Host print time was incorrectly treated as RF receive time; device timestamps disproved that assumption | No |
| Directed STOP anchors B | Full custom helper(s) preserved | Intentional discriminating variant | TX/STOP/RAW and RX stream paths reviewed | `INTENTIONAL EXPERIMENTAL VARIANT` | A observed; B 0/3 and 0/10×2; RX_STOP timeout; cause remains unresolved | No; needs implementation/physical instrumentation |
| RAW recovery invalid command | `smoke_tx_phase1.py ... --count 10 --interval-ms 100` preserved | Loop ten supported one-shot calls | Current CLI inspected | `PROCEDURAL ERROR` / `COMMAND ONLY` | Parser rejected unsupported options; no TX occurred | No |
| RAW recovery corrected | PowerShell loop of ten supported invocations preserved | Equivalent to guide loop | Current CLI inspected | `EQUIVALENT PROCEDURE` | 9/10 OTA and RX_STOP ACK; strict gate PARTIAL, recovery physically demonstrated | No |
| Proprietary first gfsk_868 forward | Helper invocation recorded; first literal stdout missing | `Invoke-FeralPresetOta` | Helper/API/backend inspected | `PARTIAL EXECUTION` evidence package | Summary says 10 returns/0 hits; literal first-run stdout and exit code missing | No; broad campaign and hardware blocker make serial-only rerun low-value |
| Proprietary original-helper runs | Exact invocations/results preserved for most runs | Same helper in guide | Helper and RX buffering inspected | `EXACT PROCEDURE MATCH` | RX starts before TX, but positive consumption occurs after the TX loop; later concurrent controls addressed this concern | No |
| Proprietary concurrent forward | Separate RX/TX stdout preserved without common timestamps | Guide permits concurrent child processes with evidence | Current API inspected | `PARTIAL EXECUTION` | Concurrency intended, exact overlap not demonstrated | No; inverse timestamped control exists |
| Proprietary concurrent inverse | RX ready and all TX ACK timestamps preserved | Safe synchronization requirement | Current API inspected | `EQUIVALENT PROCEDURE` | RX active 4.064 s before first ACK and 24.690 s after last; zero events | No |
| Named MSK/4FSK/4GFSK presets | Helper invocations exact | Historical guide allowed presets | TI `modType` audited | `INTENTIONAL EXPERIMENTAL VARIANT` but taxonomy invalid if called physical modulation tests | Values 4/5/6 are reserved in audited TI structure; only command acceptance/zero delivery are supported | No |
| Sub-GHz band preparation | `band1→band2` described; first transition literal outputs incomplete; later transitions literal | Explicit forced transition | Shell behavior/context inspected | `PARTIAL EXECUTION` evidence for first transition; exact later | Shell response/cache is not GPIO/U2 readback | No; next step requires measurement hardware |
| 2.4-GHz band preparation | Later `band2→band1` commands/stdout preserved | Explicit forced transition | Shell behavior/context inspected | `EXACT PROCEDURE MATCH` at control level | Does not prove electrical switch state, though IEEE OTA later demonstrates a functional path | Yes, current control baseline |

## Unsafe or obsolete helpers

`run_validation.py`, `Radio.reset_device()`, `smoke_ota_txrx.py`, `smoke_f29_subg_915.py`, `smoke_f9_phy_matrix_ota.py`, `smoke_f22_tx_test.py`, and `smoke_f17_emulation.py` retain automatic reset and/or arithmetic Shell derivation (`Bridge + 2`) in the audited checkout. They are classified `OBSOLETE/UNSAFE HELPER` for this campaign and were not executed. No `--auto-reset`, Catnip reset/sniff/flash, bootloader, or firmware operation was used.

## Procedurally incomplete historical evidence

1. CONTINUOUS E2: RX ran with a hit threshold, but no simultaneous TX execution is demonstrated.
2. Directed STOP helper v1: captured both sides but used host arrival time as RF ordering, invalidated by buffering.
3. Proprietary first run: literal stdout and exit code are missing.
4. Proprietary concurrent forward control: no common timestamps, so overlap is not proven by that transcript alone.
5. Invalid RAW recovery attempt: RX ran while the TX CLI rejected its arguments; zero hits cannot be assigned to RAW.
6. Historical IEEE preflight/RAW/FRAME/BURST command lines are not all literal in report 01; results are preserved, but command correctness cannot always be upgraded beyond equivalent/unknown.
7. `ota_rx_probe.py` output cannot prove zero asynchronous errors because the current script filters `RxStreamError` without reporting it.
