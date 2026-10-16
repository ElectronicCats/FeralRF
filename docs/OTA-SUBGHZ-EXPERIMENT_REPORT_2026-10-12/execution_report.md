# Execution report

## 1. Current environment and provenance

- Working directory: `C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF`.
- Git: branch `main`, HEAD `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`.
- Python: 3.14.7; editable `feralrf` package 0.3.0.
- TI SDK: `simplelink_cc13xx_cc26xx_sdk_8_30_01_01`, product version 8.30.01.01, submodule HEAD `5b31d0a4903351e544546e23ef3330eaa4291ceb`.
- Installed device firmware reports `feralrf_cc1352 (custom)`, firmware 1.0.0, capabilities 0x07, logical serial `464552414c524631`. No direct binary provenance links this image to the source HEAD, so no such identity is claimed.
- The worktree was clean before creating the untracked `validation_runs/` evidence directory.

## 2. Physical device identity

Windows USB PNP grouping was unambiguous:

| Physical group | MI_00 Bridge | MI_02 LoRa | MI_04 Shell | RP2040 report |
|---|---|---|---|---|
| `1E70E7AF` / board1 | COM33 | COM34 | COM35 | v3.1.0.0, Git 8eaa84c clean, built 2026-02-28 |
| `106D987F` / board2 | COM88 | COM86 | COM87 | v3.1.0.1, Git c0cd5a4 clean, built 2026-09-04, v3 RP2040 CC1352P7 |

The shared FERALRF1 logical serial was not used as board identity. No COM arithmetic was used. Both boards reported `Band: 0` initially and after the explicit `band2→band1` transition. This is firmware/control evidence, not GPIO/U2 electrical readback.

Postflight, both boards still reported the expected firmware and `Band: 0`, but board1/COM35 reported `CC1352 loss: uart_overrun=50, ring_dropped=0 bytes`; its preflight value had been zero. Board2 remained zero/zero. There was no intermediate Shell reading, so the increment cannot be assigned to a particular phase or equated directly to 50 lost OTA packets. This hardware-reported state change activated the campaign stop rule; no further RF execution or recovery/reset was attempted.

## 3. New taxonomy and matrix definition

The matrix separates technology context, validation scope, PHY, modulation, band, exact frequency/channel, preset, FeralRF TX operation, direction, control result, OTA result, evidence level, and status. A preset name does not establish a protocol stack. A command ACK does not establish RF energy or packet delivery.

Evidence model: A physical RF observation, B direct device behavior, C API/control, D static implementation, E inference, F not evaluated. Status remains independent: PASS, PARTIAL, FAIL, INCONCLUSIVE, NOT FULLY VALIDATED, BLOCKED, or DEFERRED.

## 4. Historical campaign reconstruction

- IEEE RAW: PASS-RF in both directions (historical 20/20 accidental forward, 10/10 reverse).
- FRAME: PASS-RF 10/10 in one direction for the API contract; current firmware delegates FRAME to RAW.
- BURST 5×250000: 1/5 twice, including a host-open variant; PARTIAL. Physical emitted count is unresolved.
- CONTINUOUS: interval 0 has repeated OTA evidence; positive interval has only partial evidence.
- STOP: command/ACK proven; later clean windows had no marker; immediate physical cessation remains not fully validated because host buffering invalidated host-time ordering and post-STOP RF anchors were not received.
- Proprietary: 24 runs, 240 successful `transmit()` returns, 19 unique presets, zero packets delivered, zero exposed `RxStreamError`. Receiver criterion FAIL; physical emission, receiver path, and root cause INCONCLUSIVE.
- BLE: raw PHY/control and environmental RX antecedents exist, but no marked two-CatSniffer peer OTA campaign. Deferred.

## 5. Historical command audit

See `historical_command_audit.md`. Material incomplete cases include the RX-only CONTINUOUS window, invalid RAW CLI invocation, missing literal stdout for the first proprietary run, the untimestamped first concurrent proprietary control, and current observer helpers that silently omit `RxStreamError` objects.

## 6. Tests selected for repetition and why

Only the current-session IEEE baseline and bidirectional RAW controls were repeated. They provide a current identity/path baseline, verify both receivers before marked TX, exercise both physical directions, and use a harness that exposes every `Packet` and `RxStreamError` with host and device timestamps. Historical rows were not overwritten.

FRAME was not repeated because historical 10/10 evidence is valid and the backend is the same RAW path. BURST/CONTINUOUS/STOP were not repeated because identical OTA variations would not add a new discriminating observable; their remaining questions require scheduler/lifecycle/queue instrumentation or independent RF timing. Proprietary was not repeated because 24 serial-only negative-delivery runs already exist and the next discriminating step is physical CTF/U2/J1 observation. BLE was not needed for the campaign objective.

## 7. Exact new executions

All command metadata, exact stdout bytes, exact stderr bytes, timestamps, exit codes, harness source, and the combined transcript are in this session directory. The sequence was:

1. provenance and PNP inventory;
2. `identify`, `fw_version`, `status` on COM35 and COM87;
3. `Radio.init()` and `get_stats()` on COM33 and COM88;
4. explicit 2.4-GHz `band2→band1` on both Shell ports;
5. ten-second IEEE RX baseline on each Bridge;
6. synchronized RAW board1 COM33→board2 COM88;
7. synchronized RAW board2 COM88→board1 COM33.
8. final read-only Shell identity/status, which exposed board1 `uart_overrun=50` and ended RF execution.

No reset, flashing, bootloader, Catnip, SX1262/LoRa, auto-reset, CW, PRBS, jamming, spectrum scan, 433 MHz, 169 MHz, OOK, MIOTY, BLE peer campaign, or proprietary transmission was executed.

## 8. IEEE results

Baseline RX:

- board1/COM33: 22 host-delivered packets, all 22 CRC-valid, no exposed stream errors, `RX_STOP ACK`; stats after: `rx_ok=24`, `rx_crc_err=0`.
- board2/COM88: 17 host-delivered packets, all 17 CRC-valid, no exposed stream errors, `RX_STOP ACK`; stats after: `rx_ok=20`, `rx_crc_err=0`.

Current RAW repeats:

| Direction | Marker | Negative | Requests/ACKs | CRC-valid exact-prefix len=8 hits | Received bytes | RSSI | Result |
|---|---|---:|---:|---:|---|---|---|
| COM33→COM88 | `d10c33008801` | 0 | 10/10 | 10 | `d10c33008801620c` | −29..−27 dBm | PASS |
| COM88→COM33 | `d10c88003302` | 0 | 10/10 | 10 | `d10c88003302b641` | −30..−28 dBm | PASS |

The two trailing bytes are preserved as observed FCS/final bytes. The result supports physical marked IEEE raw delivery under the recorded setup, not range, calibrated power, sensitivity, EIRP, general PER, or protocol interoperability.

The postflight board1 UART-overrun counter is reported separately as a session-level transport anomaly. It does not negate the literal 10/10 marked receptions or clean RX_STOP ACKs, but it prevents claiming that the broader bridge path remained loss-free throughout the session.

## 9. BLE status

BLE is a separate PHY area. Current source has BLE 1M, 2M, Coded S2 and Coded S8 raw paths; historical evidence includes control and environmental BLE RX, not a current marked peer-to-peer BLE OTA matrix. This campaign does not treat RAW/FRAME/BURST/CONTINUOUS as BLE-only. Marked BLE peer OTA is DEFERRED because it is unnecessary to resolve the current IEEE/proprietary campaign questions and BLE2M needs an appropriate extended-advertising observer/parser.

## 10. Proprietary results

No proprietary RF was transmitted this session. Historical receiver acceptance remains failed in all 24 runs. Repeating `gfsk_868_50k` using only the same two serial endpoints would again conflate CC RF execution, PA/front-end selection, RP2040 CTF, U2, J1/antenna path, peer RX configuration, and host delivery. The next useful test requires independent physical observation.

The MSK/4FSK/4GFSK preset names do not prove those modulations. The audited TI setup structure reserves `modType` values 4/5/6.

## 11. Protocol-vs-PHY conclusions

### A. What did we actually validate?

- IEEE 802.15.4 PHY/raw: yes, marked RAW OTA in both directions historically and again in this session; FRAME historically in one direction; environmental RX on channel 25.
- BLE PHY/raw: control/environmental RX only; marked peer OTA not validated.
- Proprietary PHY: configuration/TX control accepted historically, but no peer packet delivered; physical TX and RX paths remain inconclusive.
- Protocol stacks: none. No Zigbee, Thread, Matter-over-Thread, Wi-SUN FAN, Wireless M-Bus interoperability, or Sidewalk stack was exercised.
- FeralRF TX modes: RAW PASS-RF; FRAME PASS-RF for its API contract with shared RAW backend; BURST PARTIAL; CONTINUOUS PASS-RF only for repetition at interval 0 and PARTIAL for positive intervals; TX_STOP PASS-CONTROL but physical immediate cessation not fully validated.
- Band/frequency: physical marked OTA evidence is IEEE channel 25 / nominal 2475 MHz only. Proprietary 868/902.2/915/2440 has control evidence and negative peer delivery, not proven physical modulation/emission.

### B. Was Zigbee actually validated?

No Zigbee stack was validated. The defensible statement is: a declared Zigbee source was present while FeralRF received IEEE 802.15.4 traffic on channel 25. The record does not independently correlate every frame to that source and does not parse/validate Zigbee NWK, APS, ZCL, security, joining, or interoperability.

### C. Are RAW / FRAME / BURST / CONTINUOUS BLE operations?

No. At source commit `0178721...`, `RadioIF_transmitRaw()` in `firmware/cc1352/src/radio_if.c` dispatches according to the selected PHY: proprietary, BLE advertising raw, or IEEE raw. `ControlTask_onTxFrame()` in `firmware/cc1352/src/control_task.c` delegates to `ControlTask_onTxRaw()`. BURST and CONTINUOUS schedule repeated calls to the same PHY-dispatched RAW backend. They are FeralRF TX operations, not protocols and not BLE-only.

### D. Combinations with physical OTA evidence

| Context | Scope | PHY/modulation | Band/frequency | TX operation | Direction | Result | Evidence |
|---|---|---|---|---|---|---|---|
| None / IEEE raw | Raw frame observation | IEEE 802.15.4 O-QPSK | 2.4 GHz, ch25/2475 MHz nominal | TX_RAW | both directions | PASS historical and current | A/B/C |
| None / IEEE raw | Raw frame observation | IEEE 802.15.4 O-QPSK | 2.4 GHz, ch25 | TX_FRAME | COM33→COM88 | PASS historical | A/C/D |
| None / IEEE raw | Raw frame observation | IEEE 802.15.4 O-QPSK | 2.4 GHz, ch25 | TX_BURST | COM33→COM88 | PARTIAL, 1/5 twice | A/C/E |
| None / IEEE raw | Raw frame observation | IEEE 802.15.4 O-QPSK | 2.4 GHz, ch25 | TX_CONTINUOUS interval 0 | COM33→COM88 | PASS for repetition | A/B/C |
| Zigbee source context | Raw IEEE RX only | IEEE 802.15.4 O-QPSK | 2.4 GHz, ch25 | external source | source→CatSniffer | PASS for RX; source attribution limited | B/E |

### E. Combinations with only API/control evidence

- BLE 1M/2M/Coded selections and untested peer TX combinations.
- Proprietary presets at 868, 902.2, 915 and 2440 MHz: SET_PHY/config/TX return evidence only; peer delivery criterion failed.
- CW/PRBS and TX_TEST_STOP: historical ACK/control only, no waveform measurement.
- TX_STOP: ACK/control proven; exact physical cessation time not proven.
- Protocol-named W-MBus, Wi-SUN and Sidewalk presets: only raw PHY parameter requests/control, never protocol stack interoperability.

### F. Procedurally incomplete previous tests

See the explicit list in `historical_command_audit.md`: RX-only CONTINUOUS E2, host-time-based directed STOP v1, first proprietary run missing literal stdout, untimestamped concurrent proprietary control, invalid RAW command with unsupported arguments, missing literal commands in parts of report 01, and error-discarding observer helpers.

### G. Tests repeated this session

- IEEE receive baseline on both boards: current-session RX-path and clean-stop control.
- RAW COM33→COM88 and COM88→COM33: current-session marked peer controls in both directions, selected because they are high-information, short, previously successful, and establish that both current physical paths are functioning before interpreting historical partial/negative areas.

### H. Hardware/front-end uncertainty

Still unresolved for proprietary:

- whether the FeralRF proprietary backend reaches a successful CC1352 RF operation;
- whether the CC1352P7 emits energy with the requested frequency/rate/deviation;
- PA/output selection and matching;
- actual RP2040 CTF1/2/3 electrical levels;
- U2 switch state and truth-table-selected port;
- RF continuity/energy at J1 and the attached antenna path;
- peer proprietary RX configuration/framing and internal acceptance;
- delivery through firmware queues, UART, RP2040 USB CDC, and Python.

The Shell `Band` field is cached behavior evidence, not an electrical or RF measurement.

## 12. Hardware/front-end implications

The successful IEEE baseline and bidirectional marked RAW confirm that both devices, both bridge directions, both IEEE RX paths, and the 2.4-GHz route were functional during this session. They do not validate the proprietary 2.4-GHz backend because that backend uses different setup/framing. They also do not validate the Sub-GHz route or U2 state.

## 13. Remaining uncertainty

- Installed CC1352 binary provenance.
- Cause, timing, and impact of board1's postflight `uart_overrun=50` counter increase.
- BURST/positive-interval scheduler timebase and silent abort behavior.
- TX_STOP cancellation/completion timing.
- RX/output buffering, queue depth, and `0x90` semantics.
- Proprietary effective setup, RF execution, front-end, and RX delivery.
- Marked BLE peer OTA and protocol interoperability, both outside the necessary repeat set.

## 14. Recommended next discriminating experiment

Do not expand the preset sweep. Under appropriate RF lab authorization and equipment, issue one `gfsk_868_50k` TX request and observe in order: RP2040 CTF1/2/3 during forced `band1→band2`; U2 selected path using the RFSW8006Q truth table; RF energy at J1; then frequency, rate, deviation, sync, and marker if energy exists. This separates band-control, front-end/TX, parameter, and RX/host hypotheses. It is outside this serial-only campaign and was not executed.
