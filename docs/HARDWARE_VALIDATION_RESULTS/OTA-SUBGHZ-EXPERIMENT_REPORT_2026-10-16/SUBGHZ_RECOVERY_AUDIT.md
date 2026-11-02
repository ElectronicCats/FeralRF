# Sub-GHz Historical Validation and Recovery Audit

## 1. Scope and baseline

Audit date: 2026-10-15 (America/Mexico_City). This was a static, read-only investigation of Git history, source, committed reports, and repository hardware design files. No serial port was opened; no device was reset, flashed, or exercised; no RF test was run.

| Item | Audited value | Evidence |
|---|---|---|
| Repository | `https://github.com/ElectronicCats/FeralRF.git` | DIRECT CODE EVIDENCE — local `origin` |
| Branch | `feature/hardware-validation` | DIRECT CODE EVIDENCE — `git branch --show-current` |
| HEAD | `ba27fbeb8c5683a31cca4339982cadeaaa406444` | DIRECT CODE EVIDENCE — `git rev-parse HEAD` |
| `main` | `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea` | DIRECT CODE EVIDENCE — local ref |
| Initial worktree | Clean | DIRECT CODE EVIDENCE — initial `git status --short` had no output |
| Branch delta from `main` | Eleven added files, all under `docs/EXPERIMENT_REPORT_2026-10-12/` | DIRECT CODE EVIDENCE — `git diff --name-status main...HEAD` |
| F29 tags | No local tag contains the F29 closing commit | DIRECT CODE EVIDENCE — `git tag --contains f5863c3` had no output |

Source hierarchy used in this audit, strongest first: direct execution artifact; source at the relevant commit; current source; committed result report; commit message; inference. A committed result statement is not promoted to raw evidence. Likewise, a successful command return is not treated as RF emission, and RF emission is not treated as peer reception or stack interoperability.

## 2. Executive findings

1. **DOCUMENTED CLAIM:** F29 is reported as seven PHY/raw presets, ten CRC-good marker matches each, aggregate 70/70, on 2026-05-03. **70/70 is a documented historical result, but raw execution evidence was not found.** The strongest corroboration is the closing commit message, followed by the later validation matrix; neither is a raw transcript.
2. **HISTORICAL CODE EVIDENCE:** `smoke_f29_subg_915.py` reset both named TX and RX CC1352s before every preset, then initialized fresh host objects, configured both ends, started RX, transmitted ten `DE AD BE EF` markers, and required ten CRC-good marker-containing packets. It retried a whole preset once after *any* exception, despite the comment naming `TimeoutError`.
3. **HISTORICAL CODE EVIDENCE:** F29's reset helper derived Shell as Bridge COM/tty suffix plus two, returned silently when no numeric suffix existed, swallowed every exception, and continued after 3.5 seconds. A failed reset therefore did not necessarily fail the test.
4. **DIRECT CODE EVIDENCE:** Current mappings disprove `Shell = Bridge + 2` as a general discovery rule: only COM33→COM35 matches; COM38→COM40 and COM88→COM90 miss their actual Shell ports COM37 and COM87.
5. **HISTORICAL CODE EVIDENCE:** `boot`/`exit` in the then-present RP2040 bridge firmware reset the CC1352, not the RP2040. A CC reset recreated CC firmware/RAM/TI RF Driver state, but RP2040 GPIO and cached external-band state could survive.
6. **HISTORICAL CODE EVIDENCE:** The persistent non-433 RF-handle fix (`b6814b8`) predates F29 by three days, while the F29 harness still performs resets. **OPEN / UNKNOWN:** Neither code nor an A/B artifact proves reset was required by the F29-era implementation.
7. **DIRECT CODE EVIDENCE:** Current source lazily opens one persistent non-433 handle and hot-switches its RadioSetup; it also keeps a separate 433 handle opened at boot. Current comments/docs that reduce this to “one RF client” or say all RF opens are lazy are incomplete. `RADIO_INIT` is a guarded exception that closes the non-433 handle; ordinary non-433 switching avoids close/reopen.
8. **DIRECT CODE EVIDENCE:** An ACK for configuration confirms parsing/call dispatch, not successful RF setup. `RX_START` ACKs after queueing work. `TX_RAW` ACKs after enqueueing and before `RadioIF_transmitRaw()` runs; a later asynchronous error is possible.
9. **DIRECT CODE EVIDENCE:** Five presets named `wisun_915_*` actually request 902.2 MHz. That is historical and current, intentional in comments as NA-1 channel 0, but the name alone is misleading. Sidewalk presets request 915 MHz.
10. **HARDWARE DOCUMENTATION:** The repository PCB connects RP2040 GPIO8/9/10 to U2 RFSW8006 CTF1/2/3 while CC1352 DIO28/29/30 are unconnected. The active CC1352 SysConfig still contains LaunchPad-style DIO28/29/30 antenna switching. Repository evidence therefore indicates separate internal CC RF selection and external CatSniffer path ownership. Installed v3 hardware equivalence remains unverified.
11. **OPEN / UNKNOWN:** F29 contains no `band1`, `band2`, or equivalent external path selection. The key unanswered question is: **How was the external CatSniffer RF path placed into the Sub-GHz position during the historically reported successful 70/70 run?**
12. **DIRECT TEST ARTIFACT:** The recent committed campaign reports the same seven names at 0/70, within an accumulated 24 runs/240 successful `transmit()` returns/0 delivered packets, with no automatic reset. It explicitly performed logical RP2040 band transitions; the missing fact is electrical switch/path state, not merely whether a shell command was sent.
13. **INFERENCE:** H1 (different CC reset/state) is **plausible**, not established as root cause. H2 (external RF path not physically selected) is independently **plausible**. The minimum next work is one preset with explicit Bridge/Shell pairing and a verified reset, coupled to an ordering A/B and electrical path observation—not another blind seven-preset sweep.

## 3. Historical Sub-GHz validation timeline

| Date / commit | Change or result | Recoverable conditions | Evidence class |
|---|---|---|---|
| 2026-04-03 `685ec952` | Added `Radio.reset_device()` via RP2040 shell; numeric `+2` Shell derivation | `boot`, 0.5 s, `exit`, 0.3 s, extra wait default 1.5 s, reconnect/init when previously connected; serial errors propagate as `ConnectionError` | HISTORICAL CODE EVIDENCE |
| 2026-04-28 `236e476d` | Added 200 ms after RF close because Sub-1GHz→BLE could retain stale patches | Earlier close/reopen lifecycle; IEEE→BLE still described broken | HISTORICAL CODE EVIDENCE |
| 2026-04-30 `b6814b8a` | Replaced normal non-433 close/reopen with persistent handle plus RadioSetup hot-switch | Six-of-six PHY switch matrix claimed by commit; implementation predates F29 | HISTORICAL CODE EVIDENCE for implementation; DOCUMENTED CLAIM for commit result |
| 2026-04-30 `f8d7921a` | Added manual two-board F9 harness | Commit says three of three switch cycles passed at +5 dBm | DOCUMENTED CLAIM — commit message; no raw artifact found in this audit |
| Matrix, provenance consolidated 2026-07-22 | GFSK 868/915/2440 10/10; GFSK 433 6–10/10; FSK/MSK 433 1/10; MSK and 4-FSK/4-GFSK 868 10/10; OOK 868 10/10 with reset; W-MBus S/T/C 10/10 | Exact boards, direction, binary hashes, delays, markers, and raw logs are not recoverable from matrix rows | DOCUMENTED CLAIM |
| 2026-05-03 `c45114b5` | Introduced three F29 presets | Sidewalk 50/250 k and Wi-SUN 50 k | HISTORICAL CODE EVIDENCE |
| 2026-05-03 `68ed18f2` | Introduced three-preset F29 smoke | Ten markers/preset intended; reset both endpoints | HISTORICAL CODE EVIDENCE |
| 2026-05-03 `10fc47a7` | Sidewalk 250 k RX BW changed `0x56`→`0x5A` after “wire-level smoke” | Parameter change is source evidence; stated smoke is not a raw artifact | HISTORICAL CODE EVIDENCE for change; DOCUMENTED CLAIM for commit message |
| 2026-05-03 `04caba92` | Acceptance tightened to `crc_ok` plus marker | Commit says same hardware revalidated 30/30 | HISTORICAL CODE EVIDENCE for source; DOCUMENTED CLAIM for result |
| 2026-05-03 `2595276a` | Added Wi-SUN 100/150/200/300 k presets | All at 902.2 MHz | HISTORICAL CODE EVIDENCE |
| 2026-05-03 `f5863c32` | Extended smoke to seven presets and one retry | Commit says final 70/70 and retry made all seven consistent | HISTORICAL CODE EVIDENCE for source; DOCUMENTED CLAIM for result |
| 2026-07-22 `7ad0a1ab`, corrected by `cd958c3f` | Matrix states “F29, OTA 70/70” and later individual 10/10 rows | No raw F29 output, capture, or machine-readable run record found | DOCUMENTED CLAIM |
| 2026-10-08 report, committed at current branch HEAD | Recent campaign | COM33↔COM88; 0 dBm; no auto-reset; logical band changes; 24 total runs, 240 accepted TX requests, 0 delivered proprietary packets; F29 subset 0/70 | DIRECT TEST ARTIFACT for the recorded transcript/table, not proof of RF emission |

F29 planning documents identify TX `/dev/ttyACM1` as the board “without SubGHz fault” and RX `/dev/ttyACM2` as previously 30/30, and mention firmware at `04caba9`; another F29 specification says `a0e3e07` or newer. These statements do not supply an independently hashed `.elf`, build manifest, board serials, or complete raw run. Historical binary and board provenance are therefore **NOT RECOVERABLE**.

## 4. F29 reconstruction

The closing version is `python/examples/smoke_f29_subg_915.py` at `f5863c32`. Current HEAD retains the material procedure.

### Exact sequence per preset

1. Call `reset_cc1352(tx_port)`, then `reset_cc1352(rx_port)`.
2. For each reset, take the trailing decimal suffix, add two, open that inferred shell at 115200, send `boot\r\n`, wait 0.5 s, send `exit\r\n`, wait 0.3 s, close, then wait 3.5 s. No response is read or validated.
3. Construct new `Radio` objects at default 921600 for TX and RX.
4. `tx.init()` and `rx.init()`.
5. Both endpoints call `set_phy(PHY.PROPRIETARY_GFSK, channel=0)`.
6. Both call `configure_prop(**PROP_PRESETS[name])`.
7. TX calls `set_power(0)` by default.
8. RX calls `start_rx()`; wait 0.3 s.
9. TX sends ten identical four-byte `DE AD BE EF` payloads, sleeping 0.1 s after each call.
10. Wait 1.0 s, then consume `read_packets(timeout=2.0)`.
11. Ignore `RxStreamError`; count every normal RX item as `total`; count a match only when `pkt.crc_ok` and the marker occurs in `pkt.data`.
12. Try `stop_rx()`, swallowing its exception, and always disconnect both endpoints.

Pass is `matched >= --min-markers` (default 10) for every preset; aggregate expected is `count × 7` (70). A completed 0/10 run is not retried. Any exception is retried once with the full reset/setup procedure; the code catches `Exception`, broader than the “TimeoutError” comment. If both attempts throw, that preset fails. **HISTORICAL CODE EVIDENCE.**

### Preset order

1. `sidewalk_915_fsk_50k`
2. `sidewalk_915_fsk_250k`
3. `wisun_915_fsk_50k`
4. `wisun_915_fsk_100k`
5. `wisun_915_fsk_150k`
6. `wisun_915_fsk_200k`
7. `wisun_915_fsk_300k`

MIOTY was intentionally excluded after its preset was marked pending native support. TX/RX direction was whatever the operator assigned to the CLI ports; planning text suggests `/dev/ttyACM1` TX and `/dev/ttyACM2` RX, but serial identity and exact boards are **OPEN / UNKNOWN**.

Crucially, `reset_cc1352()` has both a silent nonnumeric-port return and `except Exception: pass`. A wrong Shell, unavailable Shell, permission error, write timeout, or other exception can leave the CC unreset while the smoke continues after the same 3.5 s delay. That is permitted behavior, not evidence it happened in the successful run. **HISTORICAL CODE EVIDENCE.**

## 5. Cat-Bridge / Cat-Shell reset audit

### Current devices

| Unit | Bridge | LoRa | Shell | `Bridge + 2` | Result |
|---|---:|---:|---:|---:|---|
| #1 | COM38 | COM36 | COM37 | COM40 | Wrong |
| #2 | COM33 | COM34 | COM35 | COM35 | Matches by coincidence/current enumeration |
| #3 | COM88 | COM86 | COM87 | COM90 | Wrong |

The robust conceptual relation is common USB device instance plus interface: MI_00 Bridge, MI_02 LoRa, MI_04 Shell. COM values are independently assigned labels, not an identity relation. Manual Windows COM reassignment could temporarily create COM40/COM35/COM90 Shell labels and make the arithmetic work, subject to availability/collisions, but it is a workaround and remains brittle after re-enumeration.

| Mechanism | Who/target | Discovery and commands | Failure behavior | Evidence |
|---|---|---|---|---|
| F29 `reset_cc1352` | Standalone Python→RP2040 Shell→CC reset pins | Numeric Bridge suffix +2; `boot`, then `exit` | Nonnumeric returns; all exceptions swallowed; no response check | HISTORICAL CODE EVIDENCE (`f5863c32`) |
| `Radio.reset_device()` | Connected host API→RP2040 Shell→CC | Numeric data port +2; same commands | Serial errors become `ConnectionError`; reconnect/init if previously connected | HISTORICAL CODE EVIDENCE at `685ec952`; DIRECT CODE EVIDENCE at current HEAD |
| `smoke_ota_txrx`, F9/F17/F22 helpers | Script→RP Shell→CC | Same +2 convention | Broad catch/pass, then wait | DIRECT CODE EVIDENCE |
| `run_validation_baseline.sh` | Shell helper | tty numeric +2 | Reports reset failure rather than proving reset; suite control depends on caller | DIRECT CODE EVIDENCE |
| `smoke_prop_phase1 --auto-reset` | Calls host API | Inherits `Radio.reset_device()` | Error reaches outer script handling | DIRECT CODE EVIDENCE |
| KillerBee optional reset | Integration layer | Calls reset path | Broad catch permits continuation | DIRECT CODE EVIDENCE |
| `5e0f9d5` macOS branch only | Proposed host API improvement | Finds a USB-location sibling; falls back to +2 on Linux | Not in audited HEAD; “one sibling” is insufficient to choose among three CDC interfaces without interface identity | HISTORICAL CODE EVIDENCE |

The historical tools do not fundamentally require arithmetic: the reset only needs the correct explicit Shell interface. The historical sequence is safely reproducible without renumbering by accepting/providing explicit Bridge and Shell pairs or resolving MI_00→MI_04 within the same USB device instance. No Windows configuration change is needed.

## 6. Reset semantics

The RP2040 bridge source still present at F29 (removed later by `ebdfcf31`) implements `reset_cc1352()` by driving the CC reset pin low 100 ms and high 100 ms. `boot_mode()` holds the boot pin low and resets once; `change_mode(BOOT)` calls `boot_mode()`, then resets again, waits 200 ms, and changes bridge UART to 500000. `change_mode(PASSTHROUGH)` releases boot with pull-up, resets once, and restores 921600. Thus the F29 `boot`→`exit` sequence may produce multiple CC reset pulses, ending in normal firmware startup. **HISTORICAL CODE EVIDENCE.**

| State domain | Effect of historical F29 `boot`→`exit` | Evidence/classification |
|---|---|---|
| RP2040 MCU/process | Not reset | HISTORICAL CODE EVIDENCE |
| CC1352 MCU/firmware | Hardware-reset and restarted; final `exit` returns it to normal boot | HISTORICAL CODE EVIDENCE |
| CC UART endpoint | CC firmware/UART restarts; RP bridge UART changes 921600→500000→921600 | HISTORICAL CODE EVIDENCE |
| TI RF Driver/RF Core | Destroyed/recreated as part of CC reboot | INFERENCE directly following MCU reset/startup semantics |
| CC static RAM/config structs | Reinitialized from image startup values | INFERENCE directly following hardware reset |
| RP cached band and GPIO state | Preserved unless the shell mode code changes it; `boot`/`exit` do not call `change_band()` | HISTORICAL CODE EVIDENCE |
| External U2 path | May remain at the RP-driven pre-reset selection | INFERENCE; actual voltage/path was not measured |
| Wait | 0.5 s after `boot`, 0.3 s after `exit`, then 3.5 s; roughly 4.3 s scripted delay total | HISTORICAL CODE EVIDENCE |

`RADIO_INIT` is not the same reset. Current `RadioIF_init()` stops activity, clears queues/metrics/state and, if present, yields/closes the persistent non-433 handle so it can reopen. It does not reboot CC or RP, reinitialize UART hardware by reboot, or necessarily recreate all C static objects from link-time initializers. **DIRECT CODE EVIDENCE.**

Neither a successful shell write nor elapsed wait proves that the intended device reset. F29 did not read the shell response. Likewise, a CC reset is separate from external band selection because the external control belongs to RP state in the historical bridge and repository PCB.

## 7. Current Sub-GHz software call path

Concrete case: `wisun_915_fsk_50k` (despite its name, 902.2 MHz).

| Stage | Processor/interface | Symbol and important behavior | Persistent state |
|---|---|---|---|
| Preset | Host | `PROP_PRESETS` in `python/feralrf/presets.py`: 902200000, mod 0, 50000, deviation register 100, BW `0x52`, sync `0x930B51DE` | Python dictionary |
| Host API | Host | `Radio.set_phy(PHY.PROPRIETARY_GFSK, 0)` then `Radio.configure_prop(**preset)` | Radio sequence number, serial buffers |
| Serialization | Host | `CommandBuilder.set_prop_config()` packs 18 bytes `<IBIHBIH>`; `format_conf` defaults zero | None beyond command/sequence |
| Framing | Host→RP→CC UART | command ID `SET_PROP_CONFIG` (`0x08`) in COBS-delimited frame containing command, sequence, little-endian length, payload, CRC16 | RP bridge queues/buffers can affect delivery |
| Parser | CC | `command_processor.c`: validates payload, builds `RadioIF_PropConfig`, calls `RadioIF_setPropConfig()`, then ACKs | ACK does not inspect physical RF outcome |
| Configuration | CC | `RadioIF_setPropConfig()`: stops relevant session; sets FS 902 + fraction 13107, center 902, LO divider `0x05`, mod 0, rate prescale 15/rateWord 32768, deviation 100, BW `0x52`, default format `0x00A0`, sync word and band overrides | Mutates global TI command structures and `s_frequency_hz` |
| RX start | CC tasks | `CMD_RX_START` calls `ControlTask_onRxStart()` and ACKs; `data_task` later calls `RadioIF_startRx()` and sends async error sequence 0 on failure | Task-event and RX flags/queue |
| RF RX | CC→TI driver/core | `RadioIF_startSub1ghzRfBackend()` prepares queue/setup; `RadioIF_switchRfMode()` opens/reuses non-433 handle; `RadioIF_runFsAndPostRx()` posts FS then RX and receives callbacks | Persistent non-433 handle, active setup/mode, command structs |
| TX enqueue | Host→CC | `CMD_TX_RAW` validates/copies payload, queues event, ACKs | Static TX buffer/pending flag |
| RF TX | CC→TI driver/core | `ControlTask_processTxRaw()` later calls `RadioIF_transmitRaw()`; `RadioIF_executeTxCommand()` applies setup/power and executes FS/TX; failure is an async error because ACK already occurred | Persistent handle and TX session alias |
| Delivery | RF core→CC→RP→host | RX queue/callback parsed into response stream; host counts only normal packet items meeting CRC+marker | Queue overflow, UART and host buffering remain possible |

Meaningful variants:

- `sidewalk_915_fsk_50k`: same path/rate/registers, but FS is exactly 915 MHz (`frequency=915`, `fractFreq=0`, center 915).
- `gfsk_868_50k`: exactly 868 MHz and `mod_type=1` (GFSK); otherwise 50 k settings match.
- `gfsk_915_50k`: exactly 915 MHz and `mod_type=1`; otherwise 50 k settings match.

The initial PHY enum selects the proprietary family/backend. The following proprietary config supplies the effective modulation/frequency. The seven F29 entries use `mod_type=0` FSK even though the generic enum is named `PROPRIETARY_GFSK`. **DIRECT CODE EVIDENCE.**

## 8. Effective preset configuration

All seven entries were materially the same at `f5863c32` and current HEAD. `deviation` and `rx_bw` below are raw TI setup register/config values; this repository alone does not establish a safe physical-Hz interpretation for every value.

| Preset | Requested / effective synth | Mod | Rate / rateWord | Dev | RX BW | Other effective common values |
|---|---|---:|---:|---:|---:|---|
| `sidewalk_915_fsk_50k` | 915000000 Hz; FS 915 + 0/65536 | 0 FSK | 50k / 32768 | 100 | `0x52` | LO div `0x05` |
| `sidewalk_915_fsk_250k` | 915000000 Hz; FS 915 + 0/65536 | 0 FSK | 250k / 163840 | 200 | `0x5A` | LO div `0x05` |
| `wisun_915_fsk_50k` | 902200000 requested; FS 902 + 13107/65536 ≈ 902199996.95 Hz | 0 FSK | 50k / 32768 | 100 | `0x52` | LO div `0x05` |
| `wisun_915_fsk_100k` | same 902.2 MHz quantization | 0 FSK | 100k / 65536 | 150 | `0x56` | LO div `0x05` |
| `wisun_915_fsk_150k` | same | 0 FSK | 150k / 98304 | 200 | `0x5A` | LO div `0x05` |
| `wisun_915_fsk_200k` | same | 0 FSK | 200k / 131072 | 200 | `0x5A` | LO div `0x05` |
| `wisun_915_fsk_300k` | same | 0 FSK | 300k / 196608 | 200 | `0x5E` | LO div `0x05` |

Common effective format: four-byte preamble, mode 0; 32-bit sync `0x930B51DE`; MSB first; no whitening; no FEC; no bit reversal; variable-length packet; CRC enabled; receiver repeats on good and bad CRC, includes header and CRC, appends RSSI/timestamp/status, maximum 255. Host `format_conf=0` is explicitly translated back to TI defaults `0x00A0`, not to an all-zero live format. **DIRECT CODE EVIDENCE.**

Discrepancies:

- The five `wisun_915_*` names configure 902.2 MHz. This is **both historical and current**, and source comments identify Wi-SUN NA-1 channel 0; it is a naming shorthand, not a silent later override.
- “Sidewalk” and “Wi-SUN” describe parameter groupings only. The test sends four raw bytes and supplies no Sidewalk/Wi-SUN stack, authentication, framing, routing, or certification. **DIRECT CODE EVIDENCE.**
- Exact historical binary realization is **UNVERIFIED** because no independently hashed firmware artifact was tied to the 70/70 run.

## 9. RF-driver lifecycle history

1. Before `236e476d` (2026-04-28), mode changes closed and reopened non-433 RF handles; a stale-patch/switch failure was diagnosed. That commit added a 200 ms settle after close. **HISTORICAL CODE EVIDENCE.**
2. `b6814b8a` (2026-04-30) made the non-433 handle persistent and changed mode transitions to flush and execute a new RadioSetup on the same multi-protocol handle. `RadioIF_init()` retained a guarded yield/close path. This is the major pre-F29 transition. **HISTORICAL CODE EVIDENCE.**
3. **INFERENCE:** F29 closed at `f5863c32` (2026-05-03), after that fix, so it did not run against the older close/reopen source if its asserted provenance was accurate. **OPEN / UNKNOWN:** Binary provenance is not independently proven.
4. `e910fd52` (2026-07-02) changed IEEE/jam use to reuse the shared handle rather than opening another RF object. **HISTORICAL CODE EVIDENCE.**
5. Current ordinary stop cancels/flushes commands and leaves persistent 433 and non-433 handles open. `RADIO_INIT` deliberately yields/closes non-433; OOK has special guarded behavior. Current source can keep the boot-opened 433 handle and one non-433 handle simultaneously (`N_MAX_CLIENTS=2`). **DIRECT CODE EVIDENCE.**
6. Current RX `CMD_FS` is posted before RX. In the current TX reuse path, first/mode-change setup and FS/TX mechanics are not accurately summarized by a blanket claim that every FS always uses `RF_postCmd`; source contains both run and post patterns. **DIRECT CODE EVIDENCE.**

Answers to the lifecycle questions:

- **A — Was reset genuinely required by the F29-era implementation? OPEN / UNKNOWN.** It was always performed by the harness, so no within-F29 no-reset control survives. The persistent-handle fix had already landed, which argues against assuming the older lifecycle failure still mandated it.
- **B — Was reset a workaround for older lifecycle behavior? PLAUSIBLE INFERENCE.** The matrix and earlier commits record close/reopen failures and reset-based switching, but provenance does not prove why F29 retained resets.
- **C — Does current implementation require the same reset for a clean non-OOK Sub-GHz preset transition? DIRECT CODE EVIDENCE says it is designed not to; hardware evidence is OPEN.** Normal FSK preset changes flush/reconfigure a persistent handle. A reset may still be a useful clean-state experimental control, not a proven runtime requirement.
- **D — Is the matrix switching text obsolete? YES for current implementation details.** Rows saying reset is used because `RF_close` deadlocks describe an older behavior; they should not be projected onto HEAD.
- **E — What remains insufficient?** Exact F29 binary, whether both resets succeeded, whether a no-reset F29 attempt was tried, and actual RF state in both historical and recent devices.

## 10. External RF-path ownership

There are two distinct switch domains:

1. Active CC1352 SysConfig (`firmware/cc1352/syscfg/ti_drivers_config.c`, built by CMake) carries a LaunchPad-style RF switch callback using DIO28/29/30; nonzero LO divider selects its Sub-1 path. `main_rtos.c` assumes that callback owns antenna selection. `ti_rf_config_min.c` also contains switch logic, but it is not in the active CMake source list and must not be treated as running firmware. **DIRECT CODE EVIDENCE.**
2. **HARDWARE DOCUMENTATION:** Repository PCB `hardware/CatSniffer.kicad_pcb` connects RP2040 GPIO8/9/10 to CTF1/CTF2/CTF3 on external U2 RFSW8006, while CC1352 DIO28/29/30 pads are unconnected. **HISTORICAL CODE EVIDENCE:** RP source maps `band1`/GIG to CTF 0/1/0, and `band2`/SUBGIG1 to 0/0/1.

Thus CC internal mode selection cannot, on the repository PCB, be assumed to set U2. The source/PCB combination strongly indicates U2 is RP-controlled. It does **not** prove the three installed v3 units match this revision; the checked-in schematic also contains device-value/revision ambiguity relative to the stated CC1352P7 units. Physical revision and assembly confirmation remain **OPEN / UNKNOWN**.

F29 has no external band command. CC reset does not call the RP `change_band()` path. Historical RP initialization also deserves caution: its zero-initialized band value and early-return behavior could leave initial CTF outputs dependent on prior initialization details. No historical GPIO capture exists.

Required proof ladder, with deliberately limited claims:

1. Shell accepts `band2` → proves command parsing/response only.
2. RP cached status changes → proves software state only.
3. Measure RP GPIO8/9/10 → proves RP output levels.
4. Measure CTF1/2/3 at U2 → proves levels arrive at switch pins.
5. Interpret those levels against the exact RFSW8006 truth table and board nets → identifies commanded U2 throw.
6. RF continuity/path or appropriate RF observation at the selected U2/J1 path → proves path behavior at the measurement conditions.
7. Observe intended-frequency RF energy at J1/antenna → proves energy reaches the path, not packet correctness.
8. Independent compatible receiver decodes the frame → proves an OTA waveform/frame compatible with that receiver, not Wi-SUN/Sidewalk interoperability.

## 11. Historical vs recent condition comparison

| Condition | Historical F29 (`f5863c32` procedure) | Recent committed campaign | Material comparison |
|---|---|---|---|
| Reset per preset | TX then RX before all seven | No auto-reset | Different |
| Reset success proof | None; response not read | Not attempted | Historical attempt itself unverified |
| Shell discovery | Bridge numeric suffix +2 | Explicit COM35/COM87 for band commands; no reset | Different |
| Hidden reset errors | Yes, all exceptions swallowed | N/A | F29 could continue unreset |
| RX before TX | Yes | Yes; concurrent control also kept RX reader active | Same intent |
| Boot/settle | 0.5 + 0.3 + 3.5 s per endpoint | None | Different |
| RX-start delay | 0.3 s | Helper used RX setup/negative window; concurrent control had RX ready 4.064 s before first ACK | Recent was not simply “RX too late” |
| Packet count | 10/preset | 10/run | Same |
| Marker | Reused `DEADBEEF` for all | Generally unique per run/preset | Different, recent stronger against stale hits |
| Positive read | 1.0 s wait, then 2.0 s read | Original helper reads 5.0 s after TX; concurrent run observed during and 24.690 s after ACKs | Recent wider observation |
| Acceptance | CRC-good packet containing marker; ≥10 | 10 marker hits, no exposed errors | Substantially comparable delivery criterion |
| TX power | 0 dBm default | 0 dBm requested | Same |
| Seven presets | Two at 915; five at 902.2 | Same names/current values | Same source values, binary equivalence unproven |
| PHY/config | Proprietary enum 7, channel 0, then preset | Same API/helper intent | Same at host level |
| External band | No explicit step | Explicit logical transitions; status reported Band 1 for Sub-GHz | Different; electrical state unmeasured |
| Prior PHY/state | CC reset intended before each | Campaign switched among Sub-GHz and 2.4, then returned to Sub-GHz | Different |
| Reconnect | New Radio objects after reset; disconnect each preset | Persistent campaign process/helper sessions as reported | Different |
| Retry | One full retry on any exception; none for 0 hits | Recorded runs/repetitions, no identical policy | Different |
| RF lifecycle source | Persistent non-433 fix already in source; asserted binary uncertain | Current installed binary reports custom firmware but exact Git provenance unverified | Source related; binaries unknown |
| Direction | Planning suggests fixed `/dev/ttyACM1`→`/dev/ttyACM2` | Mostly COM33→COM88; some reverse/concurrent controls | Historical identity unknown |
| Result | 70/70 documented, no raw log | Same seven 0/70; cumulative campaign 0/240 delivered | Conflict cannot establish regression without equivalence |

## 12. Hypothesis assessment

### H1 — reset/state mismatch

**Classification: plausible.**

- Supporting: F29 source resets both endpoints before each preset; recent campaign explicitly did not. Current port mappings show the arithmetic helper would miss two of three units, including COM88. CC reset destroys RF-driver/static runtime state.
- Contradicting/weakening: the persistent-handle fix predates F29; current FSK lifecycle is designed for clean transition without hardware reset. No artifact correlates reset success with a received packet. F29 can silently skip failed resets, so even the historical result does not prove reset occurred.
- Unknowns: exact historical ports, whether resets succeeded, historical binary, recent CC state, and no controlled reset/no-reset pair.
- Discriminating test: one preset, same pair/geometry, explicit Shell ports; compare verified reset against no reset while holding verified external path constant.

### H2 — CC configured but external RF path not physically selected

**Classification: plausible.**

- Supporting: repository PCB makes U2 RP-controlled and leaves CC DIO28/29/30 unconnected; F29 never selects band; recent work verified only shell/logical status, not CTF levels or RF path.
- Contradicting/weakening: recent procedure explicitly sent band transitions on both shells, so “no band command” is false for that campaign. The campaign also saw failures at 2.4 GHz, so H2 alone may not explain all zero-delivery results.
- Unknowns: installed board revision, actual RP GPIO/CTF voltages, U2 truth-table throw, RF energy at J1, and historical pre-run RP state.
- Discriminating test: observe GPIO8/9/10 and CTF1/2/3 through `band1`→`band2`, interpret with U2 truth table, then observe intended-frequency RF energy at J1 during one TX.

### H3 — host reads too late / buffering loses all packets

**Classification: weak.** The F29 and recent original helpers consume after TX, but the recent concurrent control had RX active well before and long after TX and still delivered zero. Buffering remains possible elsewhere in the CC/RP/USB path, but single-terminal delayed consumption is not a sufficient explanation. A CC-side RX counter/capture or independent receiver discriminates it.

### H4 — installed firmware differs materially from audited source

**Classification: unresolved.** Both historical and recent binary provenance are incomplete. The recent devices identify a custom FeralRF image, but no build hash proves equality to HEAD; F29 planning references inconsistent source SHAs without a binary digest. A reproducible build ID/readback is required before attributing behavior to exact source.

## 13. Golden reproduction procedure

**Proposal only; do not execute as part of this audit.** Use one preset: `wisun_915_fsk_50k` (actual 902.2 MHz), because it is in the original three-preset F29 set and the final seven.

Use current unit #2 as TX (Bridge COM33, Shell COM35) and unit #3 as RX (Bridge COM88, Shell COM87). This preserves the recent COM33→COM88 direction while replacing invalid arithmetic with explicit mappings; COM88→COM90 is known wrong. Record USB device instance/MI interface and physical serial label for each.

The reset-vs-band order is not historically proven. Treat the following as **one two-arm discriminating experiment**, not as two arbitrary campaigns:

- Arm A: explicit CC reset on both units (`boot`, wait 0.5 s, `exit`, wait 0.3 s, then at least 3.5 s), verify the intended Shell responded, then explicitly select `band2`/Sub-GHz and verify logical response plus measured CTF state.
- Arm B, only if A is 0/10 or to confirm ordering: from the same controlled starting condition, select and electrically verify Sub-GHz first, then perform verified CC resets, and re-measure CTF before TX. Because historical code predicts CC reset should preserve RP switch state, any measured difference is diagnostic.

For each arm:

1. Preserve firmware, antennas, distance, orientation, power supply, and physical roles; record CC/RP build IDs if available without changing them.
2. Open COM33 and COM88 only after reset/band preparation. Create fresh `Radio` objects at 921600.
3. Initialize both; set both to `PHY.PROPRIETARY_GFSK`, channel 0; configure both with the exact `wisun_915_fsk_50k` dictionary; set TX power 0 dBm.
4. Start RX, confirm no asynchronous RF-init error, wait 0.3 s.
5. Send ten `DE AD BE EF` markers separated by 0.1 s. Preserve timestamps and async errors. Wait 1.0 s and observe for at least the historical 2.0 s; a simultaneous reader is preferred as an additional observation without changing RF setup.
6. PASS only if RX delivers ten packets with `crc_ok=true` containing the marker. TX ACK/return, shell status, CTF levels, or detected RF energy alone is not an OTA packet PASS.
7. Stop RX, disconnect both, and retain raw console/packet transcript, device identities, measured switch state, configuration dump, and firmware hashes. Do not hide/retry the first result. If an infrastructure exception occurs, record it; one historical-style full retry may be a separately labeled attempt.

This safely reproduces the historical reset semantics without COM renumbering. It also prevents a reset attempt from being silently mistaken for a verified reset.

## 14. Minimum discriminating test plan

1. **Preflight, no RF claim:** establish exact board/firmware IDs and explicit MI_00 Bridge↔MI_04 Shell pairs. Verify shell command response, then measure CTF outputs and interpret U2. This proves control/path selection only.
2. **One-preset ordering A/B:** run the Section 13 `wisun_915_fsk_50k` pair under reset→band, and if necessary band→reset, ten packets each. This separates reset-induced CC state from external path ordering.
3. **If 10/10:** repeat once unchanged. Then reverse COM88/87 TX and COM33/35 RX. If repeatable both ways, expand only then to the remaining six F29 presets.
4. **If 0/10 with verified CTF/path selection:** observe intended-frequency energy at J1/antenna during a single controlled TX. Energy proves TX reaches that point, not framing.
5. **No energy:** probe upstream after safe test planning (CC RF output/matching/U2 input) to separate CC/TI-driver generation from external-path loss. Do not infer RX health.
6. **Energy present:** use a compatible independent receiver/analyzer to inspect carrier/frequency/modulation/frame. Decode proves TX waveform compatibility; undecodable energy narrows configuration/framing but not RX.
7. **Independent receiver decodes, peer remains 0/10:** focus on peer U2/RX path, CC RX command/queue/counters, RP bridge/UART, and host delivery. Swap roles to localize board-specific failure.

Measurement limits:

| Observation | Proves | Does not prove |
|---|---|---|
| Shell `band2` response/status | command accepted/logical state | GPIO voltage or U2 throw |
| RP GPIO and CTF levels | control levels at measured points | RF continuity or emission |
| U2 truth-table-consistent state | intended switch throw | low-loss assembled RF path |
| RF energy at J1 | energy reaches J1 at measured frequency/time | valid packet or peer RX |
| Independent decode | compatible OTA TX/frame | target peer receive path or protocol-stack interoperability |
| Peer CRC-good marker | end-to-end raw PHY delivery | Wi-SUN/Sidewalk stack interoperability |

## 15. Open questions

1. What exact CC1352 and RP2040 binary hashes, board serials, antennas, geometry, and ports produced F29 70/70?
2. Did either historical reset actually receive/execute `boot` and `exit`, or did swallowed exceptions permit continuation?
3. How was the external CatSniffer RF path placed into the Sub-GHz position during the historically reported successful 70/70 run?
4. Do the three installed v3 assemblies match the checked-in U2/CTF PCB revision and truth-table wiring?
5. What GPIO8/9/10 and CTF1/2/3 levels are physically present after `band2`, and do they survive a CC-only reset?
6. Does intended-frequency RF energy reach J1 from each current board under one known preset?
7. Does a compatible independent receiver decode the TX, or is failure before/at waveform creation?
8. Can installed firmware expose a trustworthy build SHA/config so current source can be tied to behavior?

## 16. Evidence index

| Commit/ref | Path / symbol | Why it matters |
|---|---|---|
| Current `ba27fbeb` | `python/feralrf/presets.py` / `PROP_PRESETS` | Current effective preset requests |
| Current | `python/feralrf/radio.py` / `configure_prop`, `reset_device`, `transmit`, `start_rx` | Host behavior, waits, propagation, ACK semantics |
| Current | `python/feralrf/commands.py` / `set_prop_config` | Exact 18-byte serialization |
| Current | `python/feralrf/protocol.py` | COBS/CRC/frame transport |
| Current | `firmware/cc1352/src/command_processor.c` / `CMD_SET_PROP_CONFIG`, `CMD_RX_START`, `CMD_TX_RAW` | ACK occurs before physical RX/TX result |
| Current | `firmware/cc1352/src/control_task.c` / `ControlTask_onTxRaw`, `processTxRaw`, `onRxStart` | Queued asynchronous command execution |
| Current | `firmware/cc1352/src/data_task.c` / RX-start event | Actual later RX start and async error |
| Current | `firmware/cc1352/src/radio_if.c` / `RadioIF_setPropConfig`, `switchRfMode`, `runFsAndPostRx`, `executeTxCommand`, `init` | Effective RF structures and lifecycle |
| Current | `firmware/cc1352/src/smartrf_prop_0.c` | Preamble, format, CRC, packet/RX defaults |
| Current | `firmware/cc1352/syscfg/ti_drivers_config.c` | Active DIO28/29/30 LaunchPad-style callback |
| Current | `firmware/cc1352/src/ti_rf_config_min.c` | Non-active alternate source; must not be projected into build |
| Current | `firmware/cc1352/src/main_rtos.c`, CMake source list | 433 boot handle and active config selection |
| Current | `hardware/CatSniffer.kicad_pcb`, `CatSniffer.kicad_sch` | U2 RFSW8006, RP GPIO→CTF, CC DIO disconnects |
| `685ec952` | `python/feralrf/radio.py` / `reset_device` | Origin of host reset and +2 rule |
| `68ed18f2` | `python/examples/smoke_f29_subg_915.py` | Initial F29 smoke |
| `04caba92` | same / acceptance | CRC requirement and 30/30 commit claim |
| `f5863c32` | same / complete loop | Exact seven-preset reset/retry procedure and 70/70 commit claim |
| `c45114b5`, `10fc47a7`, `2595276a` | `python/feralrf/presets.py` | F29 preset introduction/BW correction/extension |
| `7ad0a1ab`, `cd958c3f` | `docs/VALIDATION_MATRIX.md`, API docs | Later documented 70/70 and per-preset rows |
| `236e476d` | historical `radio_if.c` | Older RF close/reopen settle workaround |
| `b6814b8a` | historical `radio_if.c` | Persistent non-433 handle fix before F29 |
| `f8d7921a` | F9 harness/commit message | Claimed post-fix switch-cycle pass |
| `e910fd52` | `radio_if.c` jam/IEEE changes | Later shared-handle evolution |
| `5e0f9d5` (other branch) | `radio.py` port discovery | Attempted sibling discovery; not current and not interface-specific enough |
| `ebdfcf31^` / F29-era RP source | `firmware/rp2040/catsniffer/src/main.c` / reset and `change_band` | CC-only reset and preserved RP switch domain |
| Current branch docs | `docs/EXPERIMENT_REPORT_2026-10-12/experiments/03_*`, `04_*`, `procedure.md`, CSV | Recent explicit conditions, literal results, 0/70 and 0/240 record |

Final evidentiary boundary: the repository supports reconstructing the intended F29 procedure and current call path. It does not support reconstructing the exact historical hardware/RP band state or independently authenticating 70 received packets. The next test should create those missing artifacts rather than rely on another summarized PASS/FAIL.
