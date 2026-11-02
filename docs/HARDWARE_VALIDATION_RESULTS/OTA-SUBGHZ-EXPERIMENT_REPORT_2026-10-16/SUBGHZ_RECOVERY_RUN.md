# Sub-GHz Recovery Experimental Record

This file records the checkpointed recovery campaign. It does not replace `SUBGHZ_RECOVERY_AUDIT.md`. Evidence levels are kept separate: command acceptance, software state, GPIO/CTF state, RF energy, decoded packets, and peer reception are not treated as equivalent.

## Checkpoint 0 — Preflight / device identity

- Date/time: 2026-10-15 09:35:00–09:35:13 America/Mexico_City (`15:35:00–15:35:13Z`)
- Objective: establish repository state, explicit USB-interface mappings, Shell addressability, visible process conflicts, and non-destructive identity without resetting, selecting a band, opening Bridge data ports, configuring RF, or transmitting.
- Result: **PASS**

### Repository baseline

- Branch: `feature/hardware-validation`
- HEAD: `ba27fbeb8c5683a31cca4339982cadeaaa406444`
- Origin: `https://github.com/ElectronicCats/FeralRF.git`
- `SUBGHZ_RECOVERY_AUDIT.md`: present and readable; 40,729 bytes at preflight
- Initial Checkpoint 0 `git status --short`: `?? SUBGHZ_RECOVERY_AUDIT.md`

Evidence classification: **DIRECT TEST ARTIFACT** for the captured commands; **DIRECT CODE EVIDENCE** for repository identity.

### Devices and explicit mapping

| Role | Physical unit | Bridge | Shell | USB serial | Bridge interface | Shell interface |
|---|---|---|---|---|---|---|
| TX candidate | CatSniffer #2 | COM33 | COM35 | `E661A89753565932` | MI_00 / location `1-1:x.0` | MI_04 / location `1-1:x.4` |
| RX candidate | CatSniffer #3 | COM88 | COM87 | `E661A89753656A35` | MI_00 / location `1-3:x.0` | MI_04 / location `1-3:x.4` |

Windows PnP IDs independently grouped COM33 with COM35 under device instance fragment `6&1E70E7AF`, and COM88 with COM87 under `6&106D987F`. No COM arithmetic was used.

Evidence classification: **DIRECT TEST ARTIFACT** from `Win32_SerialPort` and `serial.tools.list_ports` enumeration.

### Process/interference check

The visible process inventory contained no PuTTY, `tio`, `minicom`, `picocom`, or Python serial client. Arduino Create's `serial-discovery.exe` (PID 4436 at observation time) was running. Its presence proves enumeration activity only; whether it ever opens a port transiently is **UNKNOWN**. Both Shell ports were subsequently opened, queried, and closed successfully, so neither was exclusively held during those queries. No process was terminated.

### Minimal non-destructive Shell identity queries

Only `status\r\n` was sent, once per explicit Shell at 115200 baud. No `boot`, `exit`, `band1`, or `band2` command was sent.

#### TX candidate — CatSniffer #2 / COM35

- Open attempt: `2026-10-15T15:35:00.596063Z`
- Status sent: `2026-10-15T15:35:00.599654Z`
- Serial write: 8 bytes completed
- Closed: `2026-10-15T15:35:01.602125Z`
- Literal response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### RX candidate — CatSniffer #3 / COM87

- Open attempt: `2026-10-15T15:35:01.602183Z`
- Status sent: `2026-10-15T15:35:01.608009Z`
- Serial write: 8 bytes completed
- Closed: `2026-10-15T15:35:02.610804Z`
- Literal response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Evidence classification: **CONTROL COMMAND ACCEPTED** and **SOFTWARE STATE REPORTED**. The response supplies an RP firmware version string and a generic CC firmware identity string. Exact CC/RP build SHA and binary equivalence to repository HEAD remain **UNVERIFIED**.

### Reset evidence

No reset was attempted in Checkpoint 0. Reset state and successful CC restart are **NOT ESTABLISHED**.

### RF-switch evidence level

Software status reported `Band: 0` on both units before any recovery band command. This is a software-state observation only. No GPIO8/9/10 or CTF1/CTF2/CTF3 level was measured, U2 throw was not established, and no RF path or energy was observed.

Classification: **SOFTWARE STATE CHANGED: NOT TESTED**; **GPIO STATE OBSERVED: NO**; **RF SWITCH CONTROL STATE OBSERVED: NO**; **RF ENERGY OBSERVED: NO**; **PACKET DECODED: NO**; **PEER PACKET RECEIVED: NO**.

### Preset / effective frequency

No preset was configured and no Bridge data port was opened. The planned later preset remains `wisun_915_fsk_50k`, expected approximately 902.2 MHz; this was not exercised here.

### Conclusion

Checkpoint 0 passed its limited objective. The repository baseline is intact; the audit is readable; COM33↔COM35 and COM88↔COM87 are directly supported by shared USB serial identity plus MI_00/MI_04; both explicit Shells answered a non-destructive status query; and no arithmetic discovery was used.

This checkpoint does **not** prove that either CC1352 was reset, that `band2` works, that RP GPIO or CTF lines have any particular voltage, that U2 selected Sub-GHz, that RF energy exists near 902.2 MHz, or that either board can transmit or receive a packet.

### Remaining unknowns

- Exact CC1352 and RP2040 binary hashes/build provenance.
- Whether `serial-discovery.exe` ever accesses these ports outside the observed query window.
- Verified CC reset/restart behavior on COM35 and COM87.
- Physical GPIO/CTF state and U2 switch throw.
- RF path, RF energy, decoded waveform, and peer delivery.

### Next proposed checkpoint

Checkpoint 1 only: reset CatSniffer #2 CC1352 through explicit Shell COM35, then independently reset CatSniffer #3 CC1352 through explicit Shell COM87, using `boot`, 0.5 s, `exit`, 0.3 s, and at least 3.5 s final wait. Preserve responses and stop without selecting a band, opening COM33/COM88, configuring RF, or transmitting.

## Checkpoint 1 — Reset verification

- Date/time: 2026-10-15 09:43:07–09:43:34 America/Mexico_City (`15:43:07–15:43:34Z`)
- Objective: independently perform and verify the historical CC1352 `boot`→`exit` reset sequence through the two explicit Shell interfaces.
- Overall result: **PASS — both reset procedures VERIFIED at the RP2040 command/control-path level**
- Prohibited actions check: no COM arithmetic; no `band1`/`band2`; no Bridge port opened; no PHY/preset configured; no RF transmission.

### Devices and explicit mapping

| Role | Physical unit | Bridge (not opened) | Explicit Shell used |
|---|---|---|---|
| TX candidate | CatSniffer #2 | COM33 | COM35 |
| RX candidate | CatSniffer #3 | COM88 | COM87 |

### Reset 1 — CatSniffer #2

- Shell: COM35
- Open attempt: `2026-10-15T15:43:07.424239Z`
- Opened: `2026-10-15T15:43:07.428520Z`
- `boot` sent: `2026-10-15T15:43:07.428550Z`; 6 bytes written
- 0.5 s requested wait/read window completed: `2026-10-15T15:43:07.988831Z`
- Bytes returned in that first window: none
- `exit` sent: `2026-10-15T15:43:07.988884Z`; 6 bytes written
- 0.3 s requested wait/read window completed: `2026-10-15T15:43:08.456254Z`
- Final settling interval: `2026-10-15T15:43:08.456315Z` to `2026-10-15T15:43:11.965532Z` (3.509 s)
- Post-settle `status` sent: `2026-10-15T15:43:11.965580Z`
- Status response complete: `2026-10-15T15:43:13.025881Z`
- Closed: `2026-10-15T15:43:13.026712Z`

Literal command response captured after `exit` (the delayed `boot` response and the `exit` response arrived together):

```text
boot
BOOT

exit
PASSTHROUGH
```

Literal post-settle response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Classification: **VERIFIED at the RP2040 command/control-path level.** This is more than a completed write: COM35 returned both command-specific completion strings, and `PASSTHROUGH` is emitted after `change_mode(PASSTHROUGH)` returns. The audited RP source implements that mode change by releasing the CC boot pin, calling `reset_cc1352()`, and restoring the UART to 921600 before replying. The Shell remained responsive after the full settle interval.

Evidence boundary: the CC reset-pin voltage was **NOT MEASURED**, and no live CC Bridge handshake or boot banner was queried. The `CC1352 FW` string in `status` comes from RP firmware metadata and is not independent proof that the CC rebooted into that exact binary.

### Reset 2 — CatSniffer #3

- Shell: COM87
- Open attempt: `2026-10-15T15:43:28.883940Z`
- Opened: `2026-10-15T15:43:28.888355Z`
- `boot` sent: `2026-10-15T15:43:28.888372Z`; 6 bytes written
- 0.5 s requested wait/read window completed: `2026-10-15T15:43:29.390104Z`
- Bytes returned in that first window: none
- `exit` sent: `2026-10-15T15:43:29.390122Z`; 6 bytes written
- 0.3 s requested wait/read window completed: `2026-10-15T15:43:29.918358Z`
- Final settling interval: `2026-10-15T15:43:29.918380Z` to `2026-10-15T15:43:33.423500Z` (3.505 s)
- Post-settle `status` sent: `2026-10-15T15:43:33.423560Z`
- Status response complete: `2026-10-15T15:43:34.485667Z`
- Closed: `2026-10-15T15:43:34.487273Z`

Literal command response captured after `exit`:

```text
boot
BOOT

exit
PASSTHROUGH
```

Literal post-settle response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Classification: **VERIFIED at the RP2040 command/control-path level**, with the same evidence boundary as COM35. COM87 returned both completion strings after executing the corresponding mode-change functions and remained responsive after the full settle interval. The reset-pin waveform and a live post-reset CC Bridge response were **NOT MEASURED**.

### Evidence classification and conclusion

- **CONTROL COMMAND ACCEPTED:** established independently on COM35 and COM87 by `BOOT` and `PASSTHROUGH` responses.
- **SOFTWARE STATE CHANGED:** the RP command path entered BOOT and returned to PASSTHROUGH according to command-specific responses; final status reported `Mode: 0` on each.
- **CC RESET CONTROL PATH:** verified from the matching response behavior plus audited source ordering in `change_mode()`. Physical reset-pin assertion remains unmeasured.
- **GPIO STATE OBSERVED:** no.
- **RF SWITCH CONTROL STATE OBSERVED:** no. Final `Band: 0` is software status only; no band command was sent.
- **RF ENERGY OBSERVED:** no.
- **PACKET DECODED:** no.
- **PEER PACKET RECEIVED:** no.

Both required explicit-port reset procedures completed without suppressed exceptions and with the required waits. Checkpoint 1 therefore passes at its defined command/control-path verification level. It does not establish external RF-switch state, RF configuration, RF emission, or OTA reception.

### Remaining unknowns

- Electrical CC reset-pin waveform and direct proof of CC reboot execution.
- Exact installed CC1352 binary/build SHA; the displayed identifier is RP metadata.
- GPIO8/9/10, CTF1/2/3, U2 throw, and RF path state.
- 902.2 MHz RF energy, waveform validity, and peer delivery.

### Next proposed checkpoint

Checkpoint 2 only, after explicit human approval: starting from this reset-controlled campaign state, send `band2` independently through COM35 and COM87 and preserve Shell responses. Treat those responses as control/software evidence only. Before OTA, stop for operator measurement instructions for GPIO8/9/10 and/or CTF1/CTF2/CTF3 if electrical verification is available.

## Checkpoint 2 — RF switch / band control

- Date/time: 2026-10-15 09:51:17–09:51:30 America/Mexico_City (`15:51:17–15:51:30Z`)
- Objective: explicitly request the RP2040 Sub-GHz state on each reset-controlled unit and determine the highest evidence level reached before OTA.
- Result: **CONTROL ONLY**
- Prohibited actions check: no COM arithmetic; no Bridge port opened; no PHY/preset configured; no RF transmission; no OTA test.

### CatSniffer #2 — explicit Shell COM35

- Open attempt: `2026-10-15T15:51:17.553610Z`
- Opened: `2026-10-15T15:51:17.557370Z`
- `band2` sent: `2026-10-15T15:51:17.557382Z`; 7 bytes written
- Band response complete: `2026-10-15T15:51:18.373949Z`
- `status` sent: `2026-10-15T15:51:18.373997Z`
- Status response complete: `2026-10-15T15:51:19.430470Z`
- Closed: `2026-10-15T15:51:19.431297Z`

Literal band response:

```text
band2
SUB-GHz Band
```

Literal status response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

### CatSniffer #3 — explicit Shell COM87

- Open attempt: `2026-10-15T15:51:28.294472Z`
- Opened: `2026-10-15T15:51:28.299649Z`
- `band2` sent: `2026-10-15T15:51:28.299663Z`; 7 bytes written
- Band response complete: `2026-10-15T15:51:29.108803Z`
- `status` sent: `2026-10-15T15:51:29.108823Z`
- Status response complete: `2026-10-15T15:51:30.165740Z`
- Closed: `2026-10-15T15:51:30.167329Z`

Literal band response:

```text
band2
SUB-GHz Band
```

Literal status response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

### Evidence classification

- **LEVEL 1 — CONTROL COMMAND ACCEPTED:** reached on both devices by the literal `SUB-GHz Band` responses.
- **LEVEL 2 — SOFTWARE STATE CHANGED:** reached on both devices. Each reported `Band: 1`, compared with `Band: 0` at Checkpoints 0 and 1.
- **LEVEL 3 — RP2040 GPIO STATE OBSERVED:** not reached.
- **LEVEL 4 — CTF STATE OBSERVED AT U2:** not reached.
- **LEVEL 5 — U2 THROW ESTABLISHED:** not reached.
- **LEVEL 6 — RF PATH/ENERGY AT J1:** not reached.
- **LEVEL 7 — INDEPENDENT DECODE:** not reached.
- **LEVEL 8 — PEER PACKET RECEIVED:** not reached.

Highest evidence level: **LEVEL 2 — SOFTWARE STATE CHANGED**. The Shell response and cached `Band: 1` do not electrically validate GPIO, CTF lines, U2 selection, RF continuity, or RF energy.

### Required electrical measurement procedure

Perform this independently on CatSniffer #2 and CatSniffer #3 while each remains powered in the current post-`band2` state. Do not transmit. Use a high-impedance DMM in DC-voltage mode or a 10× oscilloscope probe. Do not use continuity/ohms mode on a powered board. Secure the ground lead to board GND or USB shield before touching a signal; use a fine insulated microprobe and avoid bridging adjacent U2/RP2040 pads.

Repository net mapping:

| Signal | RP2040 source | U2 control input | Accessible PCB-net alternatives |
|---|---|---|---|
| CTF1 | U3 pad 11 / GPIO8 | U2 pad 3 / CRF1 | vias at PCB coordinates approximately `(112.17,112.64)` near U3 or `(157.98,108.14)` near U2 |
| CTF2 | U3 pad 12 / GPIO9 | U2 pad 7 / CRF2 | vias at approximately `(112.54,115.0195)` near U3 or `(154.225,110.85)` near U2 |
| CTF3 | U3 pad 13 / GPIO10 | U2 pad 6 / CRF3 | vias at approximately `(112.95,112.71)` near U3 or `(153.65,109.83)` near U2 |

Use KiCad to highlight `/CTF1`, `/CTF2`, and `/CTF3` before probing; the vias are safer targets than the 2 × 2 mm U2 package pins. First measure the near-U3 points (LEVEL 3), then the corresponding near-U2 points (LEVEL 4). Record actual volts, not only high/low.

Expected state after `band2`, from the audited RP `change_band(SUBGIG_1)` implementation:

| Line | Expected logic | Datasheet voltage interpretation |
|---|---:|---|
| CTF1 / CRF1 | 0 | LOW: 0–0.20 V |
| CTF2 / CRF2 | 0 | LOW: 0–0.20 V |
| CTF3 / CRF3 | 1 | HIGH: 1.2–4.8 V; approximately 3.3 V is expected for a 3.3 V GPIO but must be measured |

The RFSW8006 control table defines `CRF1,CRF2,CRF3 = 0,0,1` as RF3 connected to ANT. The repository PCB connects the U2 RF3 net through the path labeled `UBP_SUB-1_GHZ`; therefore matching levels at U2 establish the *intended control-selected throw*, not RF continuity or emission.

Interpretation for each board:

- U3 `0,0,1` and U2 `0,0,1`: LEVEL 4 is electrically observed; using the truth table and repository net mapping, LEVEL 5 is supported for the intended RF3/Sub-GHz throw. RF continuity and energy remain unproved.
- U3 differs from `0,0,1`: RP software state did not produce the expected GPIO outputs; classify band control **FAILED electrically** and do not proceed to OTA.
- U3 is `0,0,1` but U2 differs: GPIO generation is correct but the CTF net/interconnect to U2 is faulty or the measurement/board mapping is wrong; classify **FAILED/BLOCKED** and do not proceed.
- Both sites read all low, more than one high, or an intermediate voltage between 0.20 V and 1.2 V: unexpected/invalid control state; stop and report literal voltages.
- CTF1 alone high (`1,0,0`): datasheet selects RF1, not the intended Sub-GHz RF3 route.
- CTF2 alone high (`0,1,0`): datasheet selects RF2; repository PCB maps that route to `UBP_2_4_GHZ`, not the intended Sub-GHz route.

Sources: repository `hardware/CatSniffer.kicad_pcb`; historical RP2040 `change_band()` at `ebdfcf31^`; Qorvo RFSW8006Q product data sheet/control table.

### Conclusion and stop condition

Checkpoint 2 is **CONTROL ONLY**, with LEVEL 2 as the highest evidence. No OTA work is authorized until the operator supplies the six near-U3 voltages and six near-U2 voltages (CTF1/2/3 for each of the two boards), or explicitly reports that electrical measurement is unavailable and requests a reviewed next action.

---

## Checkpoint 3A — RESET -> BAND -> CONFIGURE -> RX -> TX

### Date/time and authorization boundary

- Date: `2026-10-15`
- Fresh reset/band-control interval: `2026-10-15T16:49:48.177180Z` through `2026-10-15T16:50:26.702392Z`
- OTA execution interval: `2026-10-15T16:52:38.169041Z` through `2026-10-15T16:52:46.420827Z`
- Post-run Shell status interval: `2026-10-15T16:52:57.198315Z` through `2026-10-15T16:52:59.229807Z`
- Authorization: Checkpoint 3A only. No repeat, Checkpoint 3B, reversed direction, or additional preset was executed.
- Operator decision carried into this checkpoint: physical GPIO8/9/10 and CTF1/CTF2/CTF3 probing was intentionally omitted because the accessible points were considered too small to probe without unacceptable board-damage risk.

### Test objective

Test whether the historical `wisun_915_fsk_50k` raw/proprietary FSK parameters deliver ten CRC-good `DE AD BE EF` markers from CatSniffer #2 to CatSniffer #3 after a fresh, explicitly controlled reset and logical Sub-GHz selection. This test does not test Wi-SUN protocol interoperability.

### Devices and explicit mapping

| Role | Physical unit | Bridge | Shell |
|---|---|---|---|
| TX | CatSniffer #2 | `COM33` | `COM35` |
| RX | CatSniffer #3 | `COM88` | `COM87` |

All four ports were specified literally. No Shell port was derived from a Bridge port. `Radio.reset_device()` and the historical arithmetic reset helper were not used.

### Fresh reset evidence

#### TX — CatSniffer #2, explicit Shell COM35

- Open attempt: `2026-10-15T16:49:48.177180Z`
- Opened: `2026-10-15T16:49:48.181850Z`
- `boot` sent: `2026-10-15T16:49:48.181862Z`; 6 bytes written
- Boot-only observation window response: empty
- 0.5 s boot wait complete: `2026-10-15T16:49:48.743794Z`
- `exit` sent: `2026-10-15T16:49:48.743809Z`; 6 bytes written
- Combined literal response after `exit`:

```text
boot
BOOT

exit
PASSTHROUGH
```

- 0.3 s exit wait complete: `2026-10-15T16:49:49.212096Z`
- Final settling period: `2026-10-15T16:49:49.212115Z` through `2026-10-15T16:49:52.767905Z` (approximately 3.556 s)
- `status` sent: `2026-10-15T16:49:52.767928Z`
- Literal status response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

- Closed: `2026-10-15T16:49:53.825401Z`
- Classification: **VERIFIED at the RP2040 command/control-path level**. This does not directly observe the CC1352 reset pin or independently prove the CC1352 internal reset state.

#### RX — CatSniffer #3, explicit Shell COM87

- Open attempt: `2026-10-15T16:50:02.829769Z`
- Opened: `2026-10-15T16:50:02.833810Z`
- `boot` sent: `2026-10-15T16:50:02.833832Z`; 6 bytes written
- Boot-only observation window response: empty
- 0.5 s boot wait complete: `2026-10-15T16:50:03.387785Z`
- `exit` sent: `2026-10-15T16:50:03.387807Z`; 6 bytes written
- Combined literal response after `exit`:

```text
boot
BOOT

exit
PASSTHROUGH
```

- 0.3 s exit wait complete: `2026-10-15T16:50:03.853631Z`
- Final settling period: `2026-10-15T16:50:03.853658Z` through `2026-10-15T16:50:07.409818Z` (approximately 3.556 s)
- `status` sent: `2026-10-15T16:50:07.409853Z`
- Literal status response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

- Closed: `2026-10-15T16:50:08.471592Z`
- Classification: **VERIFIED at the RP2040 command/control-path level**. This does not directly observe the CC1352 reset pin or independently prove the CC1352 internal reset state.

No reset exception occurred or was suppressed. Both reset procedures met the same command/control-path evidence standard used in Checkpoint 1, so the checkpoint proceeded.

### RF-switch evidence

#### TX — CatSniffer #2, explicit Shell COM35

- Opened: `2026-10-15T16:50:16.511521Z`
- `band2` sent: `2026-10-15T16:50:16.511538Z`; 7 bytes written
- Literal band response:

```text
band2
SUB-GHz Band
```

- Band response complete: `2026-10-15T16:50:17.325084Z`
- Literal status response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

- Closed: `2026-10-15T16:50:18.386794Z`

#### RX — CatSniffer #3, explicit Shell COM87

- Opened: `2026-10-15T16:50:24.838131Z`
- `band2` sent: `2026-10-15T16:50:24.838164Z`; 7 bytes written
- Literal band response:

```text
band2
SUB-GHz Band
```

- Band response complete: `2026-10-15T16:50:25.642534Z`
- Literal status response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

- Closed: `2026-10-15T16:50:26.702392Z`

Band-control classification: **LEVEL 2 / SOFTWARE STATE ONLY**. No GPIO, CTF, U2 throw, or RF-path electrical state was directly measured. The successful OTA result below raises the overall experiment to peer-delivery evidence but does not retrospectively turn `band2` or `Band: 1` into electrical switch validation.

### Preset and exact OTA procedure

- Preset: `PROP_PRESETS["wisun_915_fsk_50k"]`
- Effective configured frequency: `902200000 Hz` (approximately 902.2 MHz)
- Literal preset values used: `mod_type=0`, `symbol_rate=50000`, `deviation=100`, `rx_bw=82 (0x52)`, `sync_word=0x930B51DE`
- TX power: `0 dBm`
- Marker: `DE AD BE EF`
- Direction: CatSniffer #2 `COM33` -> CatSniffer #3 `COM88`

Procedure executed once:

1. Created fresh `Radio` objects for `COM33` and `COM88` and initialized each endpoint.
2. Both returned `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`.
3. Applied `set_phy(PHY.PROPRIETARY_GFSK, channel=0)` to TX and RX; both commands returned ACK.
4. Applied the unmodified preset to TX and RX; both commands returned ACK.
5. Applied TX power 0 dBm; command returned ACK.
6. Obtained pre-test stats from both devices: all reported counters were zero.
7. Started RX first; RX start returned ACK at `2026-10-15T16:52:40.889749Z`.
8. Waited 0.3022745 s before the first TX attempt.
9. Issued exactly ten `transmit(DE AD BE EF, power_dbm=0)` calls. Every call returned normally after an ACK; no call was retried.
10. Used approximately 0.1001–0.1005 s host sleep after each transmit call.
11. Waited 1.0006789 s after the tenth spacing interval.
12. Observed the RX stream for 2.0044892 s.
13. Observed the TX stream for a further 0.5133730 s for pending asynchronous errors.
14. Stopped RX with one stop attempt; it returned ACK.
15. Obtained post-test stats, then disconnected TX and RX cleanly.

Raw artifacts:

- `experimental_artifacts/checkpoint_3a/checkpoint_3a_20261015_raw.log`
- `experimental_artifacts/checkpoint_3a/checkpoint_3a_20261015_post_shell_status.log`
- Experimental one-run helper: `experimental_artifacts/checkpoint_3a/checkpoint_3a_single_run.py`

The helper has fixed explicit ports, one fixed preset, ten TX attempts, no reset helper, no COM arithmetic, no seven-preset loop, and no automatic test retry. Production library and firmware files were not modified.

### Raw/literal OTA result

- TX command/ACK accepted: **10/10**
- Total peer packet records: **10**
- Peer packets with `crc_ok == true` and `DE AD BE EF` present: **10/10**
- RX asynchronous errors: **0 observed**
- TX asynchronous errors: **0 observed**
- Fatal exceptions: **none**

All ten RX packet records were reported literally as follows. The library-reported `channel=37`, `lqi=0`, and RSSI values are retained without reinterpretation; no independent calibration of those metadata fields was performed.

| # | Host observation time (UTC) | Device timestamp (us) | Channel | RSSI (dBm, reported) | LQI | CRC | Payload | Marker |
|---:|---|---:|---:|---:|---:|---|---|---|
| 1 | `16:52:43.651188Z` | 615461 | 37 | 36 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 2 | `16:52:43.759275Z` | 615976 | 37 | -109 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 3 | `16:52:43.874140Z` | 616504 | 37 | 47 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 4 | `16:52:43.991513Z` | 617026 | 37 | -46 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 5 | `16:52:44.103953Z` | 617558 | 37 | -75 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 6 | `16:52:44.214494Z` | 618083 | 37 | -108 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 7 | `16:52:44.320117Z` | 618644 | 37 | 16 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 8 | `16:52:44.436176Z` | 619158 | 37 | -49 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 9 | `16:52:44.555838Z` | 619681 | 37 | -54 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 10 | `16:52:44.667607Z` | 620192 | 37 | -107 | 0 | true | `04 DE AD BE EF 33 C8` | present |

Pre/post device statistics:

| Endpoint | Before | After |
|---|---|---|
| TX `COM33` | `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` | `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` |
| RX `COM88` | `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` | `rx_ok=10, rx_crc_err=0, rx_drop=0, rx_overflow=0` |

Post-run Shell status on both `COM35` and `COM87` reported:

```text
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

These counters establish that those reported RP2040/CC1352 transport-loss conditions were not observed. They do not establish that every conceivable internal error mode was absent.

### Evidence classification and result

**CHECKPOINT 3A RESULT: PASS-RF**

The acceptance criterion was met literally: exactly ten peer packet records had `crc_ok == true` and contained `DE AD BE EF`.

- Band-control evidence: **LEVEL 2 / SOFTWARE STATE ONLY**
- Overall OTA evidence: **LEVEL 8 / PEER PACKET RECEIVED** for this raw proprietary-FSK experiment

### Conclusion

This run demonstrates end-to-end raw PHY delivery from CatSniffer #2 to CatSniffer #3 under the controlled Checkpoint 3A condition: fresh explicit-port reset procedures, logical `band2` state, identical unmodified 902.2 MHz preset configuration, RX started first, and 0 dBm TX. It supports restoration of the historical raw Sub-GHz OTA behavior for this one preset and direction.

### What this result does not prove

- It does not prove that the reset was the necessary or sufficient recovery action; there was no no-reset control in this checkpoint.
- It does not prove that `band2` physically changed GPIO8/9/10, CTF1/2/3, or U2. Those electrical states remain unmeasured.
- It does not prove which external-switch throw was selected by the control command. The functional OTA success only shows that the complete path was usable during this run.
- It does not prove RF power, spectral compliance, center-frequency accuracy, modulation quality, or RF energy at J1; none was independently measured.
- TX command ACKs alone do not prove RF emission. The separate peer CRC-good marker observations provide the end-to-end delivery evidence here.
- It does not validate Wi-SUN protocol interoperability. The preset name identifies a historical proprietary/raw FSK parameter set only.
- It does not establish repeatability, the reverse direction, another preset, or the cause of the earlier failure.
- The unusual mix of positive and negative library-reported RSSI values and reported channel 37 was not independently validated; their meaning is **UNVERIFIED** in this checkpoint and does not alter the CRC-good marker count.

### Hypothesis impact

- **H1 — reset / CC1352 state:** The historical behavior was restored in a run that began with fresh resets of both endpoints, so H1 remains compatible with the evidence. Because Checkpoint 3A changed/controlled reset and band state together and lacks a no-reset control, reset causality is **NOT ESTABLISHED**.
- **H2 — external RF switch/path:** A path capable of end-to-end delivery existed during this run, so an external-path mismatch was not functionally blocking these ten packets. However, GPIO/CTF/U2 electrical state and whether `band2` caused physical switch movement remain **UNVERIFIED**. H2 as an explanation for earlier failures remains **OPEN / NOT ISOLATED**.

### Recommended next discriminating checkpoint

After human review and only with explicit approval, run **Checkpoint 3B — ordering control** once: logical band selection first, reset both CC1352 devices through explicit Shell ports, re-check the available Level-2 software status, and then execute the identical one-preset OTA procedure. Because physical probing has been declined, this control can test functional sensitivity to ordering but still cannot independently establish GPIO/CTF/U2 state. Do not execute it automatically.

---

## Checkpoint 3A-R — Exact Repeatability Control

### Date/time and objective

- Date: `2026-10-15`
- Reset and logical band-control interval: `2026-10-15T17:00:29.753312Z` through `2026-10-15T17:00:44.594026Z`
- OTA execution interval: `2026-10-15T17:01:00.831019Z` through `2026-10-15T17:01:09.071269Z`
- Post-run Shell status interval: `2026-10-15T17:01:18.338107Z` through `2026-10-15T17:01:20.374490Z`
- Objective: determine whether the successful Checkpoint 3A condition is immediately reproducible. This was not a new-hypothesis test.
- Authorization boundary: one exact repeat only. Checkpoint 3B, reverse direction, other presets, and automatic retry were not executed.

### Devices and explicit mapping

| Role | Physical unit | Bridge | Shell |
|---|---|---|---|
| TX | CatSniffer #2 | `COM33` | `COM35` |
| RX | CatSniffer #3 | `COM88` | `COM87` |

All ports were specified literally. No Shell port was derived arithmetically.

### Fresh reset evidence

#### TX — CatSniffer #2, Shell COM35

- Opened: `2026-10-15T17:00:29.757621Z`
- `boot` sent: `2026-10-15T17:00:29.757772Z`; 6 bytes written
- 0.5 s boot observation completed: `2026-10-15T17:00:30.257906Z`; response at that boundary was empty
- `exit` sent: `2026-10-15T17:00:30.459277Z`; 6 bytes written
- Literal accumulated response:

```text
boot
BOOT

exit
PASSTHROUGH
```

- Exit observation completed: `2026-10-15T17:00:30.788670Z`
- Final settling period: approximately 3.500870 s, completing `2026-10-15T17:00:34.289728Z`
- Literal post-reset status:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

- Classification: **VERIFIED at the RP2040 command/control-path level**

#### RX — CatSniffer #3, Shell COM87

- Opened: `2026-10-15T17:00:35.297932Z`
- `boot` sent: `2026-10-15T17:00:35.298125Z`; 6 bytes written
- 0.5 s boot observation completed: `2026-10-15T17:00:35.806853Z`; response at that boundary was empty
- `exit` sent: `2026-10-15T17:00:35.999808Z`; 6 bytes written
- Literal accumulated response:

```text
boot
BOOT

exit
PASSTHROUGH
```

- Exit observation completed: `2026-10-15T17:00:36.336421Z`
- Final settling period: approximately 3.500307 s, completing `2026-10-15T17:00:39.836901Z`
- Literal post-reset status:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

- Classification: **VERIFIED at the RP2040 command/control-path level**

No reset exception occurred or was suppressed. This evidence does not directly observe either CC1352 reset pin or independently prove internal reset state.

### Logical band-selection evidence

#### TX — Shell COM35

- `band2` sent: `2026-10-15T17:00:40.866184Z`; 7 bytes written
- Literal response:

```text
band2
SUB-GHz Band
```

- Literal status:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### RX — Shell COM87

- `band2` sent: `2026-10-15T17:00:42.756268Z`; 7 bytes written
- Literal response:

```text
band2
SUB-GHz Band
```

- Literal status:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Classification: **LEVEL 2 / SOFTWARE STATE ONLY**. GPIO8/9/10, CTF1/2/3, and U2 were not electrically measured.

### OTA configuration and procedure

- Direction: CatSniffer #2 `COM33` -> CatSniffer #3 `COM88`
- Preset: unmodified `PROP_PRESETS["wisun_915_fsk_50k"]`
- Preset values: `frequency_hz=902200000`, `mod_type=0`, `symbol_rate=50000`, `deviation=100`, `rx_bw=82 (0x52)`, `sync_word=0x930B51DE`
- Effective expected frequency: approximately 902.2 MHz
- PHY on both: `PHY.PROPRIETARY_GFSK`, channel argument `0`
- TX power: 0 dBm
- Marker: `DE AD BE EF`
- Count: exactly 10 TX calls

The same one-run helper procedure as Checkpoint 3A was invoked through a label-only wrapper. Fresh `Radio` objects were initialized; both reported `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`. PHY and preset commands returned ACK on both endpoints. TX power returned ACK. RX was started first and returned ACK. The pre-TX wait was approximately 0.300276 s. All ten TX calls were issued once with approximately 0.1001–0.1007 s host sleep after each. The post-TX wait was approximately 1.000864 s and the RX observation window was approximately 2.000509 s. RX was stopped with one successful stop attempt, and both Bridge connections were closed cleanly.

### Literal result

- TX accepted: **10/10**
- Total peer packets: **10**
- Peer `crc_ok == true` packets containing `DE AD BE EF`: **10/10**
- RX asynchronous errors: **0 observed**
- TX asynchronous errors: **0 observed**
- Fatal exceptions: **none**

| # | Host observation time (UTC) | Device timestamp (us) | Channel | RSSI (dBm, reported) | LQI | CRC | Payload | Marker |
|---:|---|---:|---:|---:|---:|---|---|---|
| 1 | `17:01:06.331837Z` | 108433 | 37 | -62 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 2 | `17:01:06.443312Z` | 108980 | 37 | -122 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 3 | `17:01:06.556823Z` | 109502 | 37 | 65 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 4 | `17:01:06.672299Z` | 110052 | 37 | 16 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 5 | `17:01:06.785550Z` | 110569 | 37 | -61 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 6 | `17:01:06.895277Z` | 111108 | 37 | -102 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 7 | `17:01:07.011228Z` | 111615 | 37 | -101 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 8 | `17:01:07.120277Z` | 112131 | 37 | 42 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 9 | `17:01:07.232634Z` | 112647 | 37 | 93 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 10 | `17:01:07.340889Z` | 113199 | 37 | -55 | 0 | true | `04 DE AD BE EF 33 C8` | present |

RX statistics changed from `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` before RX to `rx_ok=10, rx_crc_err=0, rx_drop=0, rx_overflow=0` after RX. TX statistics remained zero.

Post-run Shell status on both devices reported:

```text
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Raw artifacts:

- `experimental_artifacts/checkpoint_3a_r/checkpoint_3a_r_20261015_reset_band.log`
- `experimental_artifacts/checkpoint_3a_r/checkpoint_3a_r_20261015_raw.log`
- `experimental_artifacts/checkpoint_3a_r/checkpoint_3a_r_20261015_post_shell_status.log`
- `experimental_artifacts/checkpoint_3a_r/checkpoint_3a_r_shell_control.py`
- `experimental_artifacts/checkpoint_3a_r/checkpoint_3a_r_single_run.py`

Production firmware and Python library code were not modified.

### Checkpoint result and comparison with 3A

**CHECKPOINT 3A-R RESULT: PASS-RF**

| Execution | TX accepted | Peer CRC-good marker |
|---|---:|---:|
| Original 3A | 10/10 | 10/10 |
| Repeat 3A-R | 10/10 | 10/10 |

Both executions observed zero asynchronous errors, RX `rx_ok=10`, zero reported CRC/drop/overflow counters, and zero post-run RP2040/CC1352 UART-overrun/ring-drop counters.

### Repeatability assessment

The successful Checkpoint 3A condition was **immediately reproduced once**: two consecutive controlled executions each produced 10/10 accepted TX commands and 10/10 CRC-good peer marker matches. This is evidence of short-term repeatability for this physical pair, direction, preset, power, ordering, and laboratory condition. The sample remains only two executions.

### RX metadata anomaly reproduction

- `channel=37` reproduced on all 10 packets in 3A-R, matching all 10 packets in 3A, despite the host API having been called with channel argument 0.
- Mixed positive and negative reported RSSI values reproduced. In 3A-R, values were `-62, -122, 65, 16, -61, -102, -101, 42, 93, -55` dBm as reported by the library.
- LQI remained 0 on every packet in both runs.

These metadata observations are flagged only. Their correctness, encoding, and cause remain **UNVERIFIED**. No code investigation or modification was performed.

### What the pair proves

- Under the same controlled sequence—fresh explicit-port reset, logical `band2`, identical raw/proprietary FSK configuration, RX first, and 0 dBm TX—the same CatSniffer pair delivered 10/10 CRC-good markers in two consecutive executions.
- A usable end-to-end RF path existed during each execution, reaching peer-packet evidence in each run.
- The prior 3A success was not an isolated one-off within this immediate two-run comparison.

### What the pair does not prove

- It does not establish long-term reliability or a success probability beyond these two executions.
- It does not attribute causality to reset; both runs used reset and there is no contrasting no-reset arm.
- It does not attribute causality to `band2`; both runs used the same logical command and there is no contrasting ordering/state arm.
- It does not electrically validate GPIO8/9/10, CTF1/2/3, or U2. Peer reception is functional OTA evidence, not direct electrical switch evidence.
- It does not establish RF output power, spectrum, frequency accuracy, modulation quality, or J1 energy.
- It does not validate Wi-SUN interoperability, reverse direction, or any other preset.
- It does not explain the reproduced channel/RSSI metadata anomalies.

### Hypothesis impact

- **H1 — reset / CC1352 state:** Two successful runs are compatible with the controlled-reset condition and strengthen confidence that this condition is reproducible. They do **not** show that reset caused recovery or was necessary, because reset was not varied.
- **H2 — external RF switch/path:** A usable functional RF path existed during both executions, so an external-path mismatch did not block either run. The actual GPIO/CTF/U2 electrical state and whether `band2` physically moved U2 remain **UNVERIFIED**; H2 as an explanation for earlier failures remains **OPEN / NOT ISOLATED**.

### Stop condition

Checkpoint 3A-R is complete. No further checkpoint is authorized. Await explicit human approval before Checkpoint 3B or any other hardware experiment.

---

## Checkpoint 3C — No-Reset Control

### Date/time and experimental variable

- Date: `2026-10-15`
- Logical band-control interval: `2026-10-15T17:13:54.543165Z` through `2026-10-15T17:13:58.252501Z`
- OTA execution interval: `2026-10-15T17:14:07.132312Z` through `2026-10-15T17:14:15.475394Z`
- Post-run Shell status interval: `2026-10-15T17:14:23.550036Z` through `2026-10-15T17:14:25.583771Z`
- Experimental variable: the fresh CC1352 reset used by 3A and 3A-R was intentionally omitted.
- Fresh CC1352 reset performed: **NO**.
- No `boot`, `exit`, `Radio.reset_device()`, `--auto-reset`, or equivalent reboot operation was issued before or after this OTA execution.
- Authorization boundary: one no-reset control only. No retry, restoration run, Checkpoint 3B, reverse direction, or other preset was executed.

### Objective

Determine whether fresh CC1352 reset is necessary for the recovered raw Sub-GHz OTA behavior in the immediate state following successful Checkpoints 3A and 3A-R. All practical RF-test variables other than reset omission were held at the 3A procedure.

### Devices and explicit mapping

| Role | Physical unit | Bridge | Shell |
|---|---|---|---|
| TX | CatSniffer #2 | `COM33` | `COM35` |
| RX | CatSniffer #3 | `COM88` | `COM87` |

All ports were specified literally. No Shell-port arithmetic was used.

### Logical band-selection evidence

#### TX — CatSniffer #2, Shell COM35

- Opened: `2026-10-15T17:13:54.547597Z`
- `band2` sent: `2026-10-15T17:13:54.547805Z`; 7 bytes written
- Literal response:

```text
band2
SUB-GHz Band
```

- Literal status:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### RX — CatSniffer #3, Shell COM87

- Opened: `2026-10-15T17:13:56.372024Z`
- `band2` sent: `2026-10-15T17:13:56.372580Z`; 7 bytes written
- Literal response:

```text
band2
SUB-GHz Band
```

- Literal status:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Band-control classification: **LEVEL 2 / SOFTWARE STATE ONLY**. GPIO8/9/10, CTF1/2/3, and U2 were not electrically measured.

### OTA configuration and exact procedure

- Direction: CatSniffer #2 `COM33` -> CatSniffer #3 `COM88`
- Fresh `Radio` objects: yes
- Preset: unmodified `PROP_PRESETS["wisun_915_fsk_50k"]`
- Preset values: `frequency_hz=902200000`, `mod_type=0`, `symbol_rate=50000`, `deviation=100`, `rx_bw=82 (0x52)`, `sync_word=0x930B51DE`
- Effective expected frequency: approximately 902.2 MHz
- PHY on both: `PHY.PROPRIETARY_GFSK`, channel argument `0`
- TX power: 0 dBm
- Marker: `DE AD BE EF`
- Count: exactly 10 TX calls

The exact one-run OTA procedure used by 3A and 3A-R was invoked through a 3C label wrapper. Both endpoints initialized and reported `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`. PHY, preset, and power operations returned ACK. RX was started first and returned ACK. The pre-TX wait was approximately 0.300687 s. Each of ten TX calls was made once, with approximately 0.1001–0.1007 s host sleep after each. The post-TX wait was approximately 1.000575 s. The RX observation window was approximately 2.014200 s. RX stopped on the first stop attempt, and both Bridge ports disconnected cleanly.

### Literal result

- TX accepted: **10/10**
- Total peer packets: **10**
- Peer `crc_ok == true` packets containing `DE AD BE EF`: **10/10**
- RX asynchronous errors: **0 observed**
- TX asynchronous errors: **0 observed**
- Fatal exceptions: **none**

| # | Host observation time (UTC) | Device timestamp (us) | Channel | RSSI (dBm, reported) | LQI | CRC | Payload | Marker |
|---:|---|---:|---:|---:|---:|---|---|---|
| 1 | `17:14:12.714611Z` | 3180019 | 37 | -63 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 2 | `17:14:12.823515Z` | 3180574 | 37 | 32 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 3 | `17:14:12.935605Z` | 3181087 | 37 | -87 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 4 | `17:14:13.046131Z` | 3181647 | 37 | 48 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 5 | `17:14:13.161779Z` | 3182206 | 37 | 59 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 6 | `17:14:13.272803Z` | 3182775 | 37 | 38 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 7 | `17:14:13.386212Z` | 3183336 | 37 | -50 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 8 | `17:14:13.496472Z` | 3183898 | 37 | -127 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 9 | `17:14:13.603257Z` | 3184422 | 37 | 116 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 10 | `17:14:13.715793Z` | 3184941 | 37 | -81 | 0 | true | `04 DE AD BE EF 33 C8` | present |

RX statistics changed from `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` before RX to `rx_ok=10, rx_crc_err=0, rx_drop=0, rx_overflow=0` after RX. TX statistics remained zero.

Post-run Shell status on both devices reported:

```text
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

This reports no UART overrun or ring-drop condition at the available Shell counter level; it does not exclude every possible internal error.

Raw artifacts:

- `experimental_artifacts/checkpoint_3c/checkpoint_3c_20261015_band.log`
- `experimental_artifacts/checkpoint_3c/checkpoint_3c_20261015_raw.log`
- `experimental_artifacts/checkpoint_3c/checkpoint_3c_20261015_post_shell_status.log`
- `experimental_artifacts/checkpoint_3c/checkpoint_3c_band_control.py`
- `experimental_artifacts/checkpoint_3c/checkpoint_3c_single_run.py`

Production firmware and Python library code were not modified.

### Checkpoint result and controlled comparison

**CHECKPOINT 3C RESULT: PASS-RF**

| Execution | Fresh reset | TX accepted | Peer CRC-good marker |
|---|---|---:|---:|
| 3A | yes | 10/10 | 10/10 |
| 3A-R | yes | 10/10 | 10/10 |
| 3C | no | 10/10 | 10/10 |

All three runs observed zero asynchronous errors and an RX counter transition from `rx_ok=0` to `rx_ok=10`, with zero reported CRC/drop/overflow counters.

### Metadata comparison

- Reported channel: every packet in 3A, 3A-R, and 3C reported `channel=37`, despite the host call using channel argument 0.
- Reported 3C RSSI values: `-63, 32, -87, 48, 59, 38, -50, -127, 116, -81` dBm. Mixed positive/negative values therefore reproduced for a third execution.
- LQI: every packet in all three runs reported `0`.

The channel, RSSI, and LQI fields are retained literally. Their encoding, correctness, and cause remain **UNVERIFIED**. They were not investigated and no code was changed.

### Effect on H1 — reset / CC1352 state

Checkpoint 3C weakens the proposition that every successful execution of this preset requires a fresh CC1352 reset: in the immediate state following two successful reset-controlled runs, the same OTA test remained functional without a fresh reset. It does **not** establish that reset is universally unnecessary, that reset played no role in establishing the current state, or that the older failed campaign state would recover without reset.

### Effect on H2 — external RF switch/path

A usable functional RF path again existed during 3C, so a path mismatch did not block this execution. The `band2` response and cached `Band: 1` remain Level-2 evidence only. GPIO/CTF/U2 electrical state and whether the command physically moved U2 remain **UNVERIFIED**. H2 as an explanation for the older failures remains **OPEN / NOT ISOLATED**.

### What this experiment proves

- In the immediate hardware/software state after successful 3A and 3A-R runs, the identical raw-FSK OTA procedure remained functional without a fresh CC1352 reset.
- This one no-reset execution produced 10/10 accepted TX commands and 10/10 CRC-good peer markers.
- A usable end-to-end RF path existed during this execution, reaching peer-packet evidence.

### What this experiment does not prove

- It does not prove that reset is universally unnecessary.
- It does not recreate or explain the older failed campaign state.
- It does not prove whether the earlier resets established a persistent state that carried into 3C.
- It does not isolate reset from all prior state history; this was an immediate sequential control after reset-based successful runs.
- It does not electrically validate GPIO8/9/10, CTF1/2/3, U2 selection, RF power, or spectrum.
- It does not validate Wi-SUN interoperability, reverse direction, other presets, or long-term reliability.
- It does not explain the repeated channel/RSSI/LQI metadata anomalies.

### Recommended next discriminating experiment

After human review and only with explicit authorization, the planned **Checkpoint 3B ordering control** remains the smallest defined experiment for testing whether `band2` state and CC1352 reset ordering interact: logical band selection, reset both endpoints through explicit Shell ports, re-check the available logical band status, then run the same single OTA procedure. With physical probing omitted, such a result would be a functional ordering control only and would not directly establish U2 electrical state.

### Stop condition

Checkpoint 3C is complete. No restoration run, repeat, Checkpoint 3B, reverse direction, or additional preset was executed. Await explicit human approval before any further hardware experiment.

---

## Checkpoint 3D — Functional Band-State Contrast

### Date/time and experimental variable

- Date: `2026-10-15`
- Pre-status and logical `band1` control interval: `2026-10-15T18:33:59.129323Z` through `2026-10-15T18:34:04.887593Z`
- OTA execution interval: `2026-10-15T18:34:10.538500Z` through `2026-10-15T18:34:18.836159Z`
- Post-run status interval: `2026-10-15T18:34:28.143610Z` through `2026-10-15T18:34:30.179066Z`
- Experimental variable relative to 3C: RP2040 logical band state changed from `band2` / `Band: 1` / Sub-GHz to `band1` / `Band: 0` / 2.4 GHz.
- Fresh CC1352 reset performed: **NO**.
- No `boot`, `exit`, `Radio.reset_device()`, `--auto-reset`, or equivalent CC1352 reboot was used.
- No `band2` restoration was performed after entering the 3D condition.

### Devices and explicit mapping

| Role | Physical unit | Bridge | Shell |
|---|---|---|---|
| TX | CatSniffer #2 | `COM33` | `COM35` |
| RX | CatSniffer #3 | `COM88` | `COM87` |

All ports were specified literally. No Shell-port arithmetic was used.

### Pre-test logical band state

Both pre-test statuses were queried and closed before either `band1` command was issued.

#### CatSniffer #2 — COM35

- Status sent: `2026-10-15T18:33:59.134005Z`
- Literal response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### CatSniffer #3 — COM87

- Status sent: `2026-10-15T18:34:00.199993Z`
- Literal response:

```text
status
Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Both devices therefore matched the expected inherited logical `Band: 1` state before the controlled change.

### Intentional logical `band1` control

#### CatSniffer #2 — COM35

- `band1` sent: `2026-10-15T18:34:01.212022Z`; 7 bytes written
- Literal response:

```text
band1
2.4GHz Band
```

- Resulting literal status:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### CatSniffer #3 — COM87

- `band1` sent: `2026-10-15T18:34:03.051772Z`; 7 bytes written
- Literal response:

```text
band1
2.4GHz Band
```

- Resulting literal status:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Band-control classification: **LEVEL 2 / SOFTWARE STATE ONLY**. The commands were accepted and both cached states changed from `Band: 1` to `Band: 0`. GPIO8/9/10, CTF1/2/3, U2 control inputs, and U2 throw were **NOT MEASURED**.

### Unchanged RF configuration and procedure

- Direction: CatSniffer #2 `COM33` -> CatSniffer #3 `COM88`
- Fresh `Radio` objects: yes
- Preset: unmodified `PROP_PRESETS["wisun_915_fsk_50k"]`
- Values: `frequency_hz=902200000`, `mod_type=0`, `symbol_rate=50000`, `deviation=100`, `rx_bw=82 (0x52)`, `sync_word=0x930B51DE`
- Effective expected frequency: approximately 902.2 MHz
- PHY on both: `PHY.PROPRIETARY_GFSK`, channel argument `0`
- TX power: 0 dBm
- Marker: `DE AD BE EF`
- Count: exactly 10 TX calls

Both endpoints initialized and reported `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`. Both `set_phy` calls returned ACK. Both `configure_prop` calls returned ACK. TX power returned ACK. RX start returned ACK. The pre-TX wait was approximately 0.300609 s. Each of ten TX calls was made once with approximately 0.1002–0.1006 s host sleep after each. The post-TX wait was approximately 1.000524 s. RX observation lasted approximately 2.016022 s, followed by approximately 0.516336 s of TX-side asynchronous observation. RX stopped on the first stop attempt, and both Bridges disconnected cleanly.

### Literal result

- TX accepted: **10/10**
- Total peer packets: **10**
- Peer `crc_ok == true` packets containing `DE AD BE EF`: **10/10**
- RX asynchronous errors: **0 observed**
- TX asynchronous errors: **0 observed**
- Fatal exceptions: **none**

| # | Host observation time (UTC) | Device timestamp (us) | Channel | RSSI (dBm, reported) | LQI | CRC | Payload | Marker |
|---:|---|---:|---:|---:|---:|---|---|---|
| 1 | `18:34:16.055962Z` | 971900 | 37 | -36 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 2 | `18:34:16.165519Z` | 972412 | 37 | 116 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 3 | `18:34:16.274099Z` | 972932 | 37 | 2 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 4 | `18:34:16.389661Z` | 973464 | 37 | 105 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 5 | `18:34:16.502294Z` | 973979 | 37 | 84 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 6 | `18:34:16.610457Z` | 974492 | 37 | 15 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 7 | `18:34:16.718272Z` | 975011 | 37 | 50 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 8 | `18:34:16.830452Z` | 975526 | 37 | 21 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 9 | `18:34:16.939230Z` | 976050 | 37 | 126 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 10 | `18:34:17.050031Z` | 976591 | 37 | -51 | 0 | true | `04 DE AD BE EF 33 C8` | present |

RX statistics changed from `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` before RX to `rx_ok=10, rx_crc_err=0, rx_drop=0, rx_overflow=0` after RX. TX statistics remained zero.

### Post-run Shell status and loss counters

Both status queries continued to report `Band: 0` after the OTA run.

CatSniffer #2 / COM35:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

CatSniffer #3 / COM87:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

The available counters report no UART overrun or ring-drop condition; they do not exclude every possible internal error.

### Raw artifacts

- `experimental_artifacts/checkpoint_3d/checkpoint_3d_20261015_prestatus_band1.log`
- `experimental_artifacts/checkpoint_3d/checkpoint_3d_20261015_raw.log`
- `experimental_artifacts/checkpoint_3d/checkpoint_3d_20261015_post_shell_status.log`
- `experimental_artifacts/checkpoint_3d/checkpoint_3d_band_control.py`
- `experimental_artifacts/checkpoint_3d/checkpoint_3d_single_run.py`

The helpers use explicit COM ports, one preset, exactly ten TX calls, no reset operation, no COM arithmetic, no preset loop, and no test retry. Production firmware and library code were not modified.

### Checkpoint result and comparison

**CHECKPOINT 3D RESULT: PASS-RF**

| Execution | Fresh reset | RP logical band | Peer CRC-good marker |
|---|---|---|---:|
| 3A | yes | Sub-GHz / `Band: 1` | 10/10 |
| 3A-R | yes | Sub-GHz / `Band: 1` | 10/10 |
| 3C | no | Sub-GHz / `Band: 1` | 10/10 |
| 3D | no | 2.4 GHz / `Band: 0` | 10/10 |

### Metadata comparison

- Channel: all ten 3D packets reported `channel=37`, reproducing 3A, 3A-R, and 3C despite the host channel argument being 0.
- RSSI: `-36, 116, 2, 105, 84, 15, 50, 21, 126, -51` dBm as reported. Mixed positive/negative values reproduced again.
- LQI: all ten packets reported 0, again matching the prior runs.

The metadata values are preserved literally. Their encoding, correctness, and cause remain **UNVERIFIED**.

### What this experiment proves

- End-to-end 902.2 MHz configured raw-FSK delivery remained functional in this execution after both RP2040 Shells accepted `band1`, changed their cached state from `Band: 1` to `Band: 0`, and continued to report `Band: 0` after the OTA run.
- Under the observed software states and otherwise matched procedure, changing the RP2040 logical band state did not prevent 10/10 CRC-good peer marker delivery.
- A usable end-to-end RF path existed during the 3D execution.

### What this experiment does not prove

- It does not prove that U2 remained in the Sub-GHz position.
- It does not prove that U2 moved to the intended 2.4 GHz position.
- It does not electrically validate GPIO8/9/10, CTF1/2/3, U2 control inputs, or U2 throw.
- It does not determine whether the unexpected result is due to actual switch movement behavior, installed hardware routing/revision, logical-state interpretation, RF isolation/leakage/path behavior, or another unmeasured mechanism. Which explanation is correct is **NOT ESTABLISHED**.
- It does not establish RF output power, path loss, center-frequency accuracy, modulation quality, or spectral compliance.
- It does not validate Wi-SUN interoperability, reverse direction, another preset, or long-term reliability.
- It does not explain the channel/RSSI/LQI metadata anomalies.

### Impact on H1 — reset / CC1352 state

Checkpoint 3D is another successful no-reset execution in the immediate recovered state. Together with 3C, it further weakens the proposition that each successful run requires a fresh reset. It still does not reproduce the older failed state, determine whether earlier resets established persistent state, or establish that reset was irrelevant to the historical failure.

### Impact on H2 / external-band-control hypothesis

The intended functional contrast did not produce loss or degradation in the literal packet count: `Band: 1` and `Band: 0` executions both delivered 10/10. Therefore the proposition that the reported RP2040 logical Sub-GHz state is necessary for current close-range end-to-end delivery is **not supported by this contrast**.

This materially challenges at least one unverified assumption concerning actual external-switch movement, installed hardware routing/revision, interpretation of logical state, RF isolation/leakage/path behavior, or another unmeasured mechanism. The experiment cannot select among those explanations. Direct U2 electrical state remains **UNVERIFIED**, and H2 as an explanation for the older campaign failure is **NOT ESTABLISHED** rather than disproved.

### Recommended next discriminating checkpoint

Do not perform an automatic `band2` restoration test. First review this unexpected result. If the operator later authorizes another hardware checkpoint and suitable RF instrumentation/connection is available, the most discriminating next test is a **non-invasive J1 902.2 MHz RF-energy/path-loss contrast** between the retained logical `Band: 0` state and a separately authorized logical `Band: 1` state, using the identical TX configuration and controlled attenuation. This could determine whether the logical state changes measurable 902.2 MHz energy at J1 without probing the small GPIO/CTF/U2 pads. It would still not directly measure U2 control voltages.

### Stop condition

Checkpoint 3D is complete. The devices were left reporting logical `Band: 0`; Sub-GHz state was not restored. No repeat, restoration run, Checkpoint 3B, reverse direction, additional preset, physical probing, commit, or push was performed. Await explicit human approval before any further action.

---

## Checkpoint 3E — Legacy Helper Procedure Reproduction

### Date/time and controlled variable

- Date: `2026-10-15`
- Pre-test status-only interval: `2026-10-15T18:39:28.478768Z` through `2026-10-15T18:39:30.525886Z`
- Legacy OTA procedure interval: `2026-10-15T18:40:09.814283Z` through `2026-10-15T18:40:22.017019Z`
- Post-run status-only interval: `2026-10-15T18:40:31.748721Z` through `2026-10-15T18:40:33.892183Z`
- Controlled variable relative to 3D: host RX/TX procedure changed from the current 0.3 s pre-TX plus 1 s/2 s observation procedure to the legacy 3 s negative window, ten TX calls, and 5 s positive window.
- Fresh CC1352 reset: **NO**.
- Band command issued: **NO**.
- No `boot`, `exit`, `band1`, `band2`, `Radio.reset_device()`, `--auto-reset`, or equivalent reset/band operation was used.

### Evidence reviewed before execution

The current audit/run record, raw artifacts for 3A, 3A-R, 3C, and 3D, `docs/EXPERIMENT_REPORT_2026-10-12/procedure.md`, `experiments/03_proprietary_gfsk_868_915.md`, and the repository OTA helper sources were reviewed. The preserved legacy wrapper defines the sequence exercised here: connect, initialize/configure, start RX, consume `read_packets(timeout=3.0)`, issue ten TX calls with 0.1 s spacing, then consume `read_packets(timeout=5.0)`. Its historical cleanup suppression was not reproduced; cleanup outcomes were recorded explicitly.

### Devices and explicit mapping

| Role | Physical unit | Bridge | Shell |
|---|---|---|---|
| TX | CatSniffer #2 | `COM33` | `COM35` |
| RX | CatSniffer #3 | `COM88` | `COM87` |

No Shell port was derived arithmetically.

### Inherited logical state — status only

#### CatSniffer #2 — COM35

Literal pre-test response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### CatSniffer #3 — COM87

Literal pre-test response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Both devices retained the required logical `Band: 0` state. No state-changing Shell command was issued.

### Preset and procedure

- Direction: CatSniffer #2 `COM33` -> CatSniffer #3 `COM88`
- Preset: `PROP_PRESETS["wisun_915_fsk_50k"]`
- Literal preset dictionary: `frequency_hz=902200000`, `mod_type=0`, `symbol_rate=50000`, `deviation=100`, `rx_bw=82`, `sync_word=0x930B51DE`
- PHY on both: `PHY.PROPRIETARY_GFSK`, channel argument 0
- TX power: 0 dBm
- Marker: `DE AD BE EF`
- TX count: exactly 10 calls

Fresh `Radio` objects were created. TX `COM33` and RX `COM88` were explicitly connected. Both initialized successfully and returned `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`. Both `set_phy` operations returned ACK. Both `configure_prop` operations returned ACK. TX power returned ACK. RX start returned ACK.

### Legacy negative window

- Requested duration: 3.0 s
- Observed host duration: approximately 3.009648 s
- Objects returned: 0
- Packets: 0
- CRC-good packets: 0
- CRC-good marker hits: 0
- `RxStreamError` objects: 0
- Other objects: 0

There were no negative-window objects to enumerate. No corrective action was taken.

### TX attempts

Exactly ten `transmit(DE AD BE EF, power_dbm=0, timeout=2.0)` calls were issued. Every call returned normally after ACK, and none was retried.

| Attempt | ACK host time (UTC) | Return | Following host sleep (s) |
|---:|---|---|---:|
| 1 | `18:40:15.274703Z` | `None` | 0.1002365 |
| 2 | `18:40:15.408934Z` | `None` | 0.1001720 |
| 3 | `18:40:15.545827Z` | `None` | 0.1002275 |
| 4 | `18:40:15.680682Z` | `None` | 0.1003014 |
| 5 | `18:40:15.824428Z` | `None` | 0.1002271 |
| 6 | `18:40:15.968736Z` | `None` | 0.1006751 |
| 7 | `18:40:16.113191Z` | `None` | 0.1002095 |
| 8 | `18:40:16.249465Z` | `None` | 0.1003645 |
| 9 | `18:40:16.388607Z` | `None` | 0.1003858 |
| 10 | `18:40:16.531775Z` | `None` | 0.1003908 |

TX accepted: **10/10**.

### Legacy positive window

- Requested duration: 5.0 s
- Observed host duration: approximately 5.018232 s
- Objects returned: 10
- Packets: 10
- CRC-good packets: 10
- CRC-good marker matches: **10/10**
- `RxStreamError` objects: 0
- Other objects: 0

Because the legacy procedure materializes the entire generator with `list(...)` before printing it, the nearly identical host logging times below are times of post-window enumeration, not independently captured packet-arrival times. Device timestamps are preserved separately.

| # | Host logging time (UTC) | Device timestamp (us) | Channel | RSSI (dBm, reported) | LQI | CRC | Payload | Marker |
|---:|---|---:|---:|---:|---:|---|---|---|
| 1 | `18:40:21.650776Z` | 2384606 | 37 | 94 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 2 | `18:40:21.650908Z` | 2385117 | 37 | 53 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 3 | `18:40:21.650964Z` | 2385641 | 37 | -12 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 4 | `18:40:21.651020Z` | 2386176 | 37 | -96 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 5 | `18:40:21.651062Z` | 2386704 | 37 | -109 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 6 | `18:40:21.651110Z` | 2387265 | 37 | -82 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 7 | `18:40:21.651148Z` | 2387830 | 37 | -83 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 8 | `18:40:21.651185Z` | 2388392 | 37 | -36 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 9 | `18:40:21.651233Z` | 2388926 | 37 | -4 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 10 | `18:40:21.651272Z` | 2389469 | 37 | -9 | 0 | true | `04 DE AD BE EF 33 C8` | present |

### Statistics and cleanup

- Pre-run device statistics: **NOT MEASURED**, because inserting stats commands into the defining legacy sequence would have changed the procedure under test.
- Post-run TX statistics: `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0`
- Post-run RX statistics: `rx_ok=10, rx_crc_err=0, rx_drop=0, rx_overflow=0`
- RX stop: ACK received on the first explicit stop attempt
- TX disconnect: completed without exception
- RX disconnect: completed without exception
- Fatal exception: none

Only asynchronous errors surfaced by the negative/positive reads or synchronous TX calls were observable under the reproduced legacy procedure. None was observed. No additional TX-side read window was added because that would not reproduce the legacy host sequence; any otherwise pending TX-side asynchronous event is **UNKNOWN**.

### Post-run logical state and loss counters

Both status-only queries continued to report `Band: 0`.

CatSniffer #2 / COM35:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

CatSniffer #3 / COM87:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

The available counters report no UART overrun or ring-drop condition. They do not establish absence of every possible internal error.

### Raw artifacts

- `experimental_artifacts/checkpoint_3e/checkpoint_3e_20261015_prestatus.log`
- `experimental_artifacts/checkpoint_3e/checkpoint_3e_20261015_raw.log`
- `experimental_artifacts/checkpoint_3e/checkpoint_3e_20261015_post_shell_status.log`
- `experimental_artifacts/checkpoint_3e/checkpoint_3e_prestatus.py`
- `experimental_artifacts/checkpoint_3e/checkpoint_3e_legacy_single_run.py`

The helper uses explicit COM33/COM88, one fixed preset, one run, exactly ten TX calls, no retries, no reset, no band commands, no COM arithmetic, and no preset loop. Production firmware and library code were not modified.

### Checkpoint result and comparison

**CHECKPOINT 3E RESULT: PASS-RF**

| Execution | Host procedure | Reset | Logical band | CRC-good marker |
|---|---|---|---|---:|
| 3D | current one-run | no | `Band: 0` | 10/10 |
| 3E | legacy negative/positive windows | no | `Band: 0` | 10/10 |

### What this proves

- The legacy helper procedure is currently capable of successful 10/10 delivery under this immediate hardware/software state.
- With the retained `Band: 0` state, unchanged preset, direction, and power, changing from the current observation procedure to the legacy 3 s negative/5 s positive sequence did not prevent delivery in this execution.
- The legacy helper procedure alone is not sufficient to explain the earlier recorded 0/70 campaign result.

### What this does not prove

- It does not identify the historical hidden state or condition that caused the earlier failures.
- It does not prove that every historical helper invocation was equivalent to 3E in firmware, device state, hardware state, timing, environment, or binary versions.
- It does not prove that reset was irrelevant to the older failed state.
- It does not validate physical U2 state, GPIO/CTF levels, RF output power, spectrum, or path loss.
- It does not validate Wi-SUN interoperability, reverse direction, another preset, or long-term reliability.
- It does not establish correctness of the repeated channel/RSSI/LQI metadata fields.

### Impact on H1 — reset / CC1352 state

Checkpoint 3E is a third successful no-reset execution in the immediate recovered state. This further weakens the proposition that each current successful execution requires a fresh reset. It does not reconstruct the older failure state or establish whether an earlier reset created persistent state that remains present.

### Impact on H2 — external RF switch/path

Successful delivery again occurred with both Shells reporting logical `Band: 0`. This further shows that reported `Band: 1` is not necessary for the current close-range link. Direct GPIO/CTF/U2 electrical state remains **UNVERIFIED**, and the mechanism supporting delivery remains **NOT ESTABLISHED**.

### Impact on host/procedure hypothesis

The hypothesis that the legacy negative-window/positive-window read sequence by itself causes the observed delivery failure is **not supported under the current state**. The legacy procedure remains a possible contributor only in combination with another historical condition; such an interaction is **UNKNOWN**.

### Recommended next discriminating checkpoint

Do not automatically run an A/B/A restoration because 3E did not fail. After review and only with explicit operator authorization, a high-information next checkpoint would establish a controlled fresh-start boundary: fully power-cycle both physical CatSniffers, record their untouched initial Shell states before any reset or band command, and run one explicitly selected procedure/preset. That would test whether an unobserved persistent warm-state condition is carrying the current success. It would change device power/initialization state and therefore requires a separately specified checkpoint and human coordination; it was not performed here.

### Stop condition

Checkpoint 3E is complete. Both devices were left reporting logical `Band: 0`. No reset, band command, repeat, restoration, Checkpoint 3B, reverse direction, additional preset, physical probing, commit, or push was performed. Await explicit human approval before any further action.

---

## Checkpoint 4A — Cold Power-Cycle Baseline

### Physical power-cycle boundary

- Date: `2026-10-15`
- Operator confirmation: both CatSniffer #2 and #3 were completely disconnected from USB/power, remained fully unpowered for approximately 15 seconds, were reconnected, and completed Windows re-enumeration.
- Operator reported no serial terminal or command use between reconnection and authorization to continue.
- Physical power-cycle: **CONFIRMED by operator**.
- Power-off duration: approximately **15 seconds**, operator reported; not independently timed by Codex.

Before the operator disconnected power, all prior checkpoint helpers had exited, their logs showed clean disconnects, and no common serial-terminal process was found. A corrected exclusive zero-byte availability check succeeded and immediately closed COM33, COM35, COM88, and COM87; no bytes were sent. An earlier attempted availability-check command had an unsigned-mask conversion error and is not evidence.

### Post-power-cycle Windows enumeration

Windows PnP enumeration was performed without opening a COM port. The COM numbers were not assumed and no Bridge/Shell arithmetic was used.

| Physical unit | USB parent identity | Interface | Reported function | COM |
|---|---|---|---|---|
| CatSniffer #2 | `USB\\VID_1209&PID_BABB\\E661A89753565932` | `MI_00` | Cat-Bridge | `COM33` |
| CatSniffer #2 | same parent | `MI_02` | Cat-LoRa | `COM34` |
| CatSniffer #2 | same parent | `MI_04` | Cat-Shell | `COM35` |
| CatSniffer #3 | `USB\\VID_1209&PID_BABB\\E661A89753656A35` | `MI_00` | Cat-Bridge | `COM88` |
| CatSniffer #3 | same parent | `MI_02` | Cat-LoRa | `COM86` |
| CatSniffer #3 | same parent | `MI_04` | Cat-Shell | `COM87` |

Windows preserved the prior explicit mapping:

- CatSniffer #2: Bridge `COM33`, Shell `COM35`
- CatSniffer #3: Bridge `COM88`, Shell `COM87`

### Untouched first post-boot Shell observations

Before any Bridge initialization, PHY configuration, RF transmission, reset command, or band command, exactly one `status` query was issued independently through each explicit Shell.

#### CatSniffer #2 — COM35

- Status sent: `2026-10-15T18:57:03.591920Z`
- Literal response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

#### CatSniffer #3 — COM87

- Status sent: `2026-10-15T18:57:04.651603Z`
- Literal response:

```text
status
Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

Untouched initial software state on both devices:

- Mode: 0
- Band: 0
- Radio: LoRa
- LoRa: initialized
- LoRa Mode: Stream
- RP firmware: `v3.1.0.1`
- Displayed CC firmware: `feralrf_cc1352 (custom)`
- `uart_overrun=0`
- `ring_dropped=0 bytes`

This is software-state evidence only. GPIO8/9/10, CTF1/2/3, U2 control inputs, and U2 throw were **NOT MEASURED**.

### Prohibited-command confirmation

- Reset command after power-on: **NO**
- Band command after power-on: **NO**
- `boot`: not sent
- `exit`: not sent
- `band1`: not sent
- `band2`: not sent
- `Radio.reset_device()`: not called
- `--auto-reset`: not used

### Preset and current one-run procedure

- Direction: CatSniffer #2 `COM33` -> CatSniffer #3 `COM88`
- Preset: `PROP_PRESETS["wisun_915_fsk_50k"]`
- Literal configuration: `frequency_hz=902200000`, `mod_type=0`, `symbol_rate=50000`, `deviation=100`, `rx_bw=82`, `sync_word=0x930B51DE`
- Effective expected configured frequency: approximately 902.2 MHz
- PHY on both: `PHY.PROPRIETARY_GFSK`, channel argument 0
- TX power: 0 dBm
- Marker: `DE AD BE EF`
- TX count: exactly 10 calls

Fresh `Radio` objects were created. TX initialized at `2026-10-15T18:57:21.590230Z` and RX at `2026-10-15T18:57:22.698307Z`; both returned `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`. Both `set_phy` and `configure_prop` operations returned ACK. TX power returned ACK. RX start returned ACK at `2026-10-15T18:57:23.213705Z`.

The pre-TX wait was approximately 0.300987 s. Exactly ten TX calls were issued once, with approximately 0.1001–0.1005 s host sleep after each. The post-TX wait was approximately 1.000173 s. RX observation lasted approximately 2.006345 s, followed by approximately 0.510032 s of TX-side asynchronous observation. RX stopped on its first explicit stop attempt, and both Bridges disconnected cleanly.

### Literal result

- TX accepted: **10/10**
- Total peer packets: **10**
- Peer `crc_ok == true` packets containing `DE AD BE EF`: **10/10**
- RX asynchronous errors: **0 observed**
- TX asynchronous errors: **0 observed**
- Fatal exceptions: none

| # | Host observation time (UTC) | Device timestamp (us) | Channel | RSSI (dBm, reported) | LQI | CRC | Payload | Marker |
|---:|---|---:|---:|---:|---:|---|---|---|
| 1 | `18:57:25.960522Z` | 1104537 | 37 | 108 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 2 | `18:57:26.072430Z` | 1105068 | 37 | 51 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 3 | `18:57:26.190606Z` | 1105584 | 37 | 90 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 4 | `18:57:26.316626Z` | 1106100 | 37 | 125 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 5 | `18:57:26.426420Z` | 1106609 | 37 | 24 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 6 | `18:57:26.552037Z` | 1107128 | 37 | -121 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 7 | `18:57:26.667181Z` | 1107649 | 37 | -1 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 8 | `18:57:26.792091Z` | 1108170 | 37 | 94 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 9 | `18:57:26.907890Z` | 1108686 | 37 | 65 | 0 | true | `04 DE AD BE EF 33 C8` | present |
| 10 | `18:57:27.027520Z` | 1109223 | 37 | 52 | 0 | true | `04 DE AD BE EF 33 C8` | present |

Reported metadata again showed channel 37, LQI 0, and mixed-sign RSSI. Its encoding/correctness remains **UNVERIFIED**.

### Statistics, cleanup, and post-run Shell state

| Endpoint | Before | After |
|---|---|---|
| TX `COM33` | `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` | `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` |
| RX `COM88` | `rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0` | `rx_ok=10, rx_crc_err=0, rx_drop=0, rx_overflow=0` |

- RX stop: ACK received
- TX disconnect: completed without exception
- RX disconnect: completed without exception

Post-run status-only queries on both Shells continued to report `Band: 0` and:

```text
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
```

These counters do not establish absence of every possible internal error.

### Raw artifacts

- `experimental_artifacts/checkpoint_4a/checkpoint_4a_20261015_post_power_enumeration.log`
- `experimental_artifacts/checkpoint_4a/checkpoint_4a_20261015_initial_status.log`
- `experimental_artifacts/checkpoint_4a/checkpoint_4a_20261015_raw.log`
- `experimental_artifacts/checkpoint_4a/checkpoint_4a_20261015_post_shell_status.log`
- `experimental_artifacts/checkpoint_4a/checkpoint_4a_initial_status.py`
- `experimental_artifacts/checkpoint_4a/checkpoint_4a_single_run.py`

Production firmware and library code were not modified.

### Checkpoint result and comparison

**CHECKPOINT 4A RESULT: PASS-RF**

| Execution | Electrical state boundary | Reset after boundary | Band command after boundary | Initial/inherited software band | CRC-good marker |
|---|---|---|---|---|---:|
| 3E | warm sequential | no | no | `Band: 0` | 10/10 |
| 4A | complete power-cycle | no | no | untouched `Band: 0` | 10/10 |

### What this proves

- The recovered 902.2 MHz-configured end-to-end behavior survived one complete electrical power-cycle of both CatSniffers.
- Under this test condition, successful delivery did not require an explicit CC reset or band command after that startup.
- Both devices cold-started reporting logical `Band: 0`, and the single authorized run delivered 10/10 CRC-good marker packets.

### What this does not prove

- It does not identify the root cause of the older 0/70 campaign or show that result was invalid.
- It does not establish that every cold start will succeed; this is one power-cycle observation.
- It does not establish actual GPIO/CTF/U2 electrical state or RF-switch position.
- It does not establish RF output power, path loss, spectral quality, or independent center-frequency measurement.
- It does not validate Wi-SUN interoperability, reverse direction, another physical pair, another preset, or long-term reliability.
- It does not explain the repeated channel/RSSI/LQI metadata anomalies.

### Impact on persistent/warm-state hypothesis

This result substantially weakens the hypothesis that current success depends only on warm/runtime state accumulated during Checkpoints 3A through 3E. It does not exclude a persistent state that survives power loss elsewhere in the system, nor does one cold-start pass establish universal cold-start behavior. Which historical state differed remains **UNKNOWN**.

### Impact on H1 — reset / CC1352 state

Checkpoint 4A demonstrates one successful cold electrical startup followed by 10/10 delivery without an explicit CC reset. This further weakens the proposition that explicit reset is necessary for current success. It does not establish that reset was irrelevant to the older failure state.

### Impact on H2 — external RF switch/path

Both untouched cold-start Shell states reported `Band: 0`, yet delivery succeeded. This further weakens the proposition that reported `Band: 1` is necessary for the current close-range link. Physical GPIO/CTF/U2 state remains **UNVERIFIED**, and the path mechanism remains **NOT ESTABLISHED**.

### Recommended next discriminating checkpoint

Do not automatically change band or repeat 4A. After review and only with explicit authorization, the most discriminating next hardware checkpoint remains a non-invasive 902.2 MHz RF-energy/path-loss comparison at J1 or under controlled attenuation between logical band states, one approved arm at a time. This could test whether `Band: 0` delivery is associated with materially different RF-path loss despite identical close-range packet counts, without probing the small GPIO/CTF/U2 nodes.

### Stop condition

Checkpoint 4A is complete. Both devices remain reporting logical `Band: 0`. No reset, band command, recovery action, retry, reverse direction, additional preset, Checkpoint 3B, firmware modification, physical probing, commit, or push was performed. Await explicit human approval before any further action.
