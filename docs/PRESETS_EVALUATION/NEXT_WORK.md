# Recommended next work

Updated: 2026-10-26.

The remaining work has shifted from broad preset-delivery testing to correctness, backend implementation, physical characterization, and independent interoperability.

**No additional broad raw preset sweep is currently justified.**

## 1. Immediate correctness fixes

| Priority | Work | Why it comes first | Completion evidence |
|---:|---|---|---|
| 1 | Correct proprietary RX metadata parsing | Current status/RSSI/timestamp offsets make `crc_ok`, `rx_ok`, RSSI, and timestamp defective | Unit vectors plus RF packets with independently known status/RSSI/timing |
| 2 | Expose real TX completion and errors | TX ACK currently means enqueue, not RF completion | Operation ID and explicit attempted/completed/failed event per TX |
| 3 | Redesign OOK lifecycle | P11 repeatably leaves ambiguous state and cleanup timeout | Durable `requires_power_cycle`/terminal state, prevented unsafe mode changes, completion-aware stop, deterministic recovery |
| 4 | Reject or implement reserved modulation values | P12/P18/P19 pass 4/5/6 into a reserved TI field | Explicit rejection now, or supported TI backend plus later L3 |
| 5 | Implement a valid proprietary 2.4 GHz backend | P22/P23 request 2440 MHz but current proprietary path applies 868 fallback | Correct setup selection/readback followed by measured 2.4 GHz operation |

## 2. Protocol design decisions

| Priority | Family | Decision required before implementation |
|---:|---|---|
| 6 | Wireless M-Bus | Exact S/T/C/N modes, roles, frame variants, coding, block/CRC behavior, and security scope |
| 7 | Wi-SUN | FAN version/profile, SUN-FSK mode IDs, PHR, FEC/whitening requirements, channel plan, hopping, MAC, and security scope |
| 8 | Sidewalk | Exact FSK profile, regional role, framing/MAC/security requirements, and peer/gateway integration strategy |
| 9 | MIOTY | TS-UNB role/profile, splitting/coding/interleaving, burst timing/frequency rules, reassembly, and CC1352P7 feasibility |

Preset names should not be expanded into protocol claims until these decisions are explicit.

## 3. Protocol implementation

| Priority | Work | Scope |
|---:|---|---|
| 10 | Implement selected PHY framing | W-MBus EN 13757 framing/coding; SUN-FSK/PHR; Sidewalk framing; MIOTY TS-UNB telegram/burst processing |
| 11 | Implement only required stack layers | MAC, network, security, and application/profile behavior selected by product requirements |
| 12 | Add offline vectors and parsers before OTA | Known frames, negative vectors, round-trip encoder/decoder tests, and state-machine tests |

For raw FSK/GFSK profiles, no protocol stack is missing if their intended role remains explicitly raw.

## 4. Physical validation

After correctness/backend gates pass, independently measure:

- carrier frequency and error;
- modulation type;
- symbol rate and deviation;
- occupied bandwidth;
- conducted/radiated power and harmonics;
- actual frontend/switch/antenna route;
- OOK waveform and TX completion by attempt;
- proprietary 2.4 GHz frequency after backend correction.

This is L3 work. Requested configuration and peer payload delivery are not substitutes.

## 5. Interoperability

Use an independent implementation only after the selected framing/stack exists:

| Family | Required independent peer |
|---|---|
| Wireless M-Bus | Standards-conformant meter/gateway or verified decoder |
| Wi-SUN | Independent FAN device/test system |
| Sidewalk | Independent compatible peer/gateway |
| MIOTY | Independent base station/terminal |
| Zigbee, if selected | Independent Zigbee coordinator/device |

This is L4 work. Two CatSniffers running the same raw proprietary framing cannot establish interoperability.

## 6. Reproducible identity

Establish source→binary→device traceability:

- FeralRF commit and dirty-state marker;
- TI SDK revision;
- CC1352 target and build configuration;
- firmware image hash/build ID;
- RP2040 image hash/build ID;
- unique physical device identity distinct from logical `FERALRF1`;
- PCB/BOM/front-end revision;
- USB serial + MI interface mapping;
- test manifest, raw logs, exit status, and cleanup result.

## 7. Validation strategy after implementation

1. Unit and serialization tests.
2. Offline framing/parser vectors.
3. Backend selection and explicit rejection tests.
4. Instrumented L3 measurement.
5. Symmetric FeralRF regression where useful.
6. Independent L4 interoperability.

Do not use proprietary metadata as an acceptance oracle until its parser is corrected and verified. Do not use TX ACK as proof of RF completion.

## Source anchors for implementation work

- [Host radio API](../../python/feralrf/radio.py)
- [Command builder](../../python/feralrf/commands.py)
- [CC command processor](../../firmware/cc1352/src/command_processor.c)
- [TX control lifecycle](../../firmware/cc1352/src/control_task.c)
- [RF configuration, RX/TX parsing, OOK, and frequency selection](../../firmware/cc1352/src/radio_if.c)
- [RX response emission](../../firmware/cc1352/src/data_task.c)
- [Firmware source selection](../../firmware/cc1352/CMakeLists.txt)
