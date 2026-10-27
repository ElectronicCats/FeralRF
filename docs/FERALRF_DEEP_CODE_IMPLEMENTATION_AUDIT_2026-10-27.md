# FeralRF deep code and implementation audit

**Audit date:** 2026-10-27  
**Audited revision:** `3409955011ebacc448e96605780560cf8118b364` (`feature/hardware-validation`)  
**Comparison baseline:** `f2744e400f743df30499e20f03f156f08ad15bd8`  
**TI SDK submodule:** `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink CC13xx/CC26xx SDK 8.30.01.01  
**Python package:** `feralrf` 0.3.0  
**Method:** static, read-only code and evidence audit. No firmware build, flashing, radio transmission, reset, power cycle, or new experiment was performed.

## Executive conclusion

FeralRF has three materially different implementation classes which the public preset vocabulary currently blurs:

1. **Native radio backends:** BLE, IEEE 802.15.4 at 2.4 GHz, and TI proprietary packet mode. These have distinct RF Core command families. BLE is the most developed higher-level path; IEEE 802.15.4 is a useful raw MAC-frame backend; proprietary mode is a configurable raw packet transport.
2. **Raw profiles with misleading protocol names:** the W‑M‑Bus, Wi‑SUN, Sidewalk, and MIOTY entries only populate the generic `SET_PROP_CONFIG` structure. They do not select those protocols' packet encoders, MACs, security, channel plans, hopping, scheduling, or network stacks. Fourteen named sub-GHz profiles have historical FeralRF-to-FeralRF raw delivery evidence, but none has L3/L4 or external-conformance evidence.
3. **Profiles the current backend cannot represent correctly:** proprietary 2.4 GHz falls back to 868 MHz; `modType` values 4, 5, and 6 are reserved by the local TI command definition rather than MSK/4-FSK/4-GFSK; OOK has a repeatable terminal lifecycle failure; and MIOTY's telegram-splitting PHY is not created by setting a generic FSK radio to 396 baud.

The most important implementation defect is not a preset constant. It is the absence of an explicit contract for **what bytes the host supplies, what the RF Core generates, what the RX path returns, and when an ACK means RF work has completed**. The proprietary RX parser also reads TI-appended metadata in the wrong order. Consequently, raw payload matching is useful evidence, while current proprietary RSSI, timestamp, CRC status, `rx_ok`, and TX ACK must not be used as physical-RF proof.

The recommended direction is:

- keep generic FSK/GFSK entries, but call them **raw RF profiles**;
- implement passive Zigbee decoding on top of the existing IEEE 802.15.4 backend before considering active Zigbee participation;
- generate and integrate target-specific 2.4 GHz proprietary RF settings rather than reusing the sub-GHz setup;
- replace protocol-name presets with either native protocol backends or explicitly non-conformant `raw_candidate_*` profiles;
- correct RX metadata and command-completion semantics before further RF conclusions;
- make OOK a one-way, fail-safe state transition which requires a declared power cycle until recovery is proven.

## Scope, evidence rules, and status vocabulary

### In scope

- Python preset definitions, command serialization, transport framing, response parsing, and public radio API.
- CC1352P7 command processor, task ownership, RF backend selection, TX/RX packet construction, and generated RF settings.
- Current CMake integration and the relevant protocol components already present in the pinned TI SDK.
- Historical evidence already stored in the workspace, used only as an evidence index.
- IEEE 802.15.4 and Zigbee first; proprietary 2.4 GHz; sub-GHz at 915, 902.2, 868.95, 868.3, then 868 MHz.

### Explicitly out of detailed scope

- The 433 MHz and 169.45 MHz entries are inventoried only briefly. Their inclusion would not change the cross-cutting findings.
- BLE is informational only. It is not mixed into the preset remediation backlog.
- The RP2040 bridge source is not present in this repository. Its exact version, framing behavior, error handling, and reset implementation therefore remain unverified.
- No claim is made about legal operation, antenna match, conducted power, occupied bandwidth, frequency error, sensitivity, or spectral mask without independent instrumentation.

### Evidence labels

Every material conclusion uses one of these labels:

- **[CURRENT SOURCE CODE]** directly visible in revision `3409955`.
- **[TI SDK SOURCE OR DOCUMENTATION]** present in the pinned submodule or published by TI.
- **[PROTOCOL STANDARD]** a protocol-owner or standards-body source.
- **[HISTORICAL EXPERIMENTAL OBSERVATION]** an existing workspace record; not reproduced during this audit.
- **[STATIC ANALYSIS INFERENCE]** follows from code control/data flow but was not executed here.
- **[HYPOTHESIS]** plausible explanation requiring a targeted test.
- **[UNVERIFIED / UNKNOWN]** evidence is absent or insufficient.

Implementation status and validation level are deliberately separate:

| Implementation status | Meaning |
|---|---|
| `IMPLEMENTED` | The named behavior is represented by the active backend and code path. |
| `PARTIAL` | Some necessary behavior exists; material functionality or safety is absent. |
| `CONFIGURATION ONLY` | Constants reach a generic radio setup, but the named protocol is not implemented. |
| `DEFECTIVE` | The active code contradicts its contract or authoritative definition. |
| `BLOCKED BY CURRENT BACKEND` | The current RF command/patch/settings combination cannot express the named mode. |
| `MISSING` | No implementation path exists. |
| `UNKNOWN` | Static evidence cannot decide. |

| Validation | Meaning |
|---|---|
| `L0` | Static/source presence only. |
| `L1` | Control path accepted a command; no peer payload delivery proven. |
| `L2` | Identifiable raw payload delivered between FeralRF peers. |
| `L3` | Interoperability with an independent conformant peer. |
| `L4` | Instrumented PHY/protocol conformance evidence. |
| `NOT VALIDATED` | No suitable retained evidence. |

## End-to-end architecture and ownership

```text
Python API / examples
  presets.py -> Radio.configure_prop()/set_phy()/transmit()/read_packets()
          |
          v
USB CDC serial port exposed by RP2040 bridge
  bridge source/version unavailable in this workspace
          |
          v
CC1352 UART0, 921600 8-N-1, COBS-delimited command transport
  UART task: receive/decode queue + output queue
  RF task: command execution, RF driver calls, packet draining
          |
          +--> BLE RF Core commands + BLE LL classifier/follower
          +--> IEEE 802.15.4 RF Core commands (raw MPDU service)
          `--> CMD_PROP_* + one shared mutable proprietary setup
                   |
                   v
                RF Core / antenna switch / over the air
```

**[CURRENT SOURCE CODE]** Python opens the serial data port and derives a shell port for the RP2040 `boot`/`exit` reset cycle in `python/feralrf/radio.py:113-246`. The bridge itself is outside the source tree. CC1352 UART parameters and pins are fixed in `firmware/cc1352/include/config.h:8-14`; byte polling and writes are in `firmware/cc1352/src/host_if.c:17-45`.

**[CURRENT SOURCE CODE]** The UART task only captures a COBS frame and marks it pending. The RF task calls the command processor:

```c
/* firmware/cc1352/src/host_if_task.c:54-58 */
void HostIFTask_processPendingCommand(void) {
    if (s_pending_ready) {
        CommandProcessor_processEncodedFrame(s_pending_frame, s_pending_len);
        s_pending_ready = false;
    }
}
```

`DataTask_poll()` invokes that function before servicing stop, TX, start, RF polling, and RX delivery (`firmware/cc1352/src/data_task.c:76-137`). Both tasks run at priority 3 and depend on cooperative `Task_yield()` (`firmware/cc1352/src/main_rtos.c:54-77,110-178`). This matters: a blocking RF call in the RF task can prevent a newly received `RX_STOP` from being executed even though the UART task remains alive.

**[STATIC ANALYSIS INFERENCE]** The host-to-firmware control path is single-command pending, not a general command queue. A second command arriving before the RF task consumes the first produces an async busy error (`host_if_task.c:90-110`). Operations that are acknowledged before deferred execution can therefore leave the host and RF task with different notions of completion.

## Transport frame versus over-the-air frame

These are independent layers and must never be described as one packet format.

### USB/UART command transport

**[CURRENT SOURCE CODE]** Before COBS encoding, every host command or firmware response is:

```text
+---------+---------+--------------+-----------------+----------------+
| CMD/RSP | SEQ     | PAYLOAD_LEN  | PAYLOAD         | CRC16-CCITT    |
| 1 byte  | 1 byte  | 2 B little   | LEN bytes       | 2 B little     |
+---------+---------+--------------+-----------------+----------------+
                 COBS encode the complete row, append 0x00 delimiter
```

The Python implementation is `python/feralrf/protocol.py:11-90`; the matching firmware implementation is `firmware/cc1352/src/protocol.c:12-27,121-173`. The CRC protects the serial command frame only. It is unrelated to an IEEE FCS or proprietary RF CRC.

`SET_PROP_CONFIG` is exactly 18 bytes—`<IBIHBIH>`: frequency, modulation type, symbol-rate word, deviation, RX bandwidth, sync word, format (`python/feralrf/commands.py:100-122`). `TX_RAW` is `[payload length:1][payload][power:int8]`; `TX_FRAME` is currently the same raw path without a power field (`commands.py:57-66`).

### Firmware-to-Python RX response

**[CURRENT SOURCE CODE]** `RSP_RX_PACKET` carries:

```text
timestamp_us:u64 | channel:u8 | rssi:i8 | lqi:u8 | crc_ok:u8 |
data_len:u8 | data[data_len] | ll_kind:u8 | ll_type:u8 | ll_flags:u8
```

It is emitted in `firmware/cc1352/src/data_task.c:28-65` and decoded into the `Packet` dataclass in `python/feralrf/radio.py:18-30,636-709`. There is no `python/feralrf/models.py` in this revision.

### Generic proprietary over-the-air frame

**[CURRENT SOURCE CODE]** The shared generated commands use four preamble bytes, a 32-bit sync word, variable length, and RF-Core CRC (`firmware/cc1352/src/smartrf_prop_0.c:189-305`):

```text
Preamble (4 B) | Sync (32 b) | Length (1 B) | Host payload (N B) | CRC (2 B default)
```

The host supplies only the N payload bytes. `RadioIF_transmitPropRaw()` assigns them to `pPkt` and assigns N to `pktLen` (`radio_if.c:1757-1819`); the RF Core supplies the preamble, sync, length header, and CRC. TI's packet-format documentation confirms that `pktLen` is API state rather than a byte the application places in the TX buffer.

On RX, `bIncludeHdr=1` and `bIncludeCrc=1`, so the RF queue entry contains the length header, payload, and CRC before appended metadata (`smartrf_prop_0.c:268-305`, `radio_if.c:1619-1704`). The current firmware copies that entire PDU into `Packet.data`; it does not normalize RX data back to the TX payload contract.

### IEEE 802.15.4 over-the-air frame

**[CURRENT SOURCE CODE]** TX config sets `bIncludePhyHdr=0` and `bIncludeCrc=0`, which instructs the RF Core to generate the PHY header and FCS (`firmware/cc1352/src/smartrf_ieee_15_4_0.c:146-163`; field definitions in the pinned `rf_ieee_cmd.h:266-301`):

```text
RF Core: preamble | SFD | PHR(length)
Host:    MAC PSDU = FCF | sequence | addressing | auxiliary security? | MAC payload
RF Core: FCS
```

The TX host must therefore supply an MPDU without the PHR and without FCS. On RX, the code requests the PHY header and FCS, uses the first byte as the length, then returns the following `length` bytes (`radio_if.c:1335-1371,1497-1568`). **[STATIC ANALYSIS INFERENCE]** Subject to TI's documented semantics, `Packet.data` begins at the MAC frame-control field and includes the trailing FCS; it excludes the PHR. This asymmetry needs an explicit API flag or normalization.

## Cross-cutting defects and risks

| ID | Finding | Evidence | Consequence | Required disposition |
|---|---|---|---|---|
| X1 | Proprietary appended metadata order and status semantics are wrong. | **[CURRENT SOURCE CODE]** `radio_if.c:75-81,1706-1755` assumes status, RSSI, timestamp and treats bit `0x80` as CRC-good. TI appends RSSI, timestamp, status; the status has a two-bit `result` in bits 6–7, where result 0 means correct and 1 means CRC error (`rf_prop_cmd.h:1166-1179`). | `crc_ok`, RSSI, timestamp, `rx_ok`, and packet dropping are unreliable. | Fix offsets and decode the result field; add byte-layout unit/target tests before using metadata. |
| X2 | TX ACK means enqueue/acceptance, not RF completion. | **[CURRENT SOURCE CODE]** `command_processor.c:210-223`; `control_task.c:249-259,332-358`. | An ACK cannot prove command success or radiation. | Introduce operation IDs plus explicit success/failure completion response. |
| X3 | RX start/stop ACKs precede backend execution. | **[CURRENT SOURCE CODE]** `command_processor.c:188-208`; `data_task.c:80-85,115-124`. | Host may report active/stopped before RF state changes; deferred failure is seq 0. | Return completion after RF result; define async-event correlation. |
| X4 | Reused TX session does not refresh last-TX diagnostic fields. | **[CURRENT SOURCE CODE]** `radio_if.c:324-382`. | Readback can report stale status/event/result. | Update diagnostics on every branch and every failure path. |
| X5 | FS commands are posted without waiting for or checking completion. | **[CURRENT SOURCE CODE]** `radio_if.c:324-382,1077-1121`. | Frequency-synthesizer failure can be masked by later state. | Chain FS→RX/TX or wait and validate terminal RF events/status. |
| X6 | Python silently discards every exception while parsing asynchronous frames. | **[CURRENT SOURCE CODE]** `python/feralrf/radio.py:278-397`, especially the broad handler at line 394. | Corruption and firmware contract drift lose diagnostics. | Count, surface, and optionally retain malformed frames with bounded logging. |
| X7 | `TX_FRAME` is an alias of raw TX. | **[CURRENT SOURCE CODE]** `control_task.c:262-263`; command payload differs only by power. | The name implies framing which does not exist. | Remove/deprecate or implement a documented frame builder per backend. |
| X8 | Protocol-labelled presets all use one mutable generic proprietary setup. | **[CURRENT SOURCE CODE]** `presets.py:11-284`; `radio_if.c:2020-2170`. | Names overstate capabilities and hide missing framing/stack work. | Split raw profiles from protocol backends and report capability levels. |
| X9 | `SET_PROP_CONFIG` ACK is unconditional after a `void` setter. | **[CURRENT SOURCE CODE]** `command_processor.c:168-185`; `radio_if.h:72-83`. | Invalid/reserved/ignored configuration looks successful. | Validate all fields; return typed error or applied readback. |
| X10 | RF payload size contracts disagree. | **[CURRENT SOURCE CODE]** radio packet data is 242 bytes (`radio_if.h:15-28`), IEEE TX is capped at 125 (`radio_if.c:56-67,468-488`), and Python command length is one byte. | Truncation and backend-specific limits are implicit. | Publish per-backend limits and reject oversize at Python boundary. |

# Part I — 2.4 GHz

## 1. IEEE 802.15.4 backend

### What is implemented

**[CURRENT SOURCE CODE]** `PHY.IEEE_802_15_4 = 4` selects a native IEEE RF command set (`python/feralrf/enums.py:8-18`; `firmware/cc1352/src/phy_manager.c:16-49`). Channel handling maps 11–26 to 2405–2480 MHz in 5 MHz steps; the code also maps values 0–15 by adding 11 (`radio_if.c:428-463`). The RX command accepts all frame types, disables frame filtering and auto-ACK, and returns raw MAC bytes (`smartrf_ieee_15_4_0.c:81-144`; runtime changes at `radio_if.c:1335-1371`).

This is a credible **raw IEEE 802.15.4 MAC-frame service**, not an IEEE MAC implementation. It provides RF modulation/packetization and CRC handling, but no CSMA-CA, address filtering, ACK scheduling, retry state, association, security state, or network layer.

### Expected and observed frame responsibilities

| Responsibility | Current owner | Correct? |
|---|---|---|
| Channel frequency, O-QPSK/DSSS PHY, preamble/SFD | TI IEEE RF Core command | Yes for 2.4 GHz IEEE mode. |
| PHR length | RF Core on TX; parsed by firmware on RX | Yes, but not surfaced. |
| MAC header/body construction | Host caller | Raw capability only; undocumented. |
| FCS generation | RF Core on TX | Yes. |
| FCS validation | RF Core; firmware drops bad CRC | Functional for good-frame capture, not promiscuous diagnostics. |
| FCS presence in `Packet.data` | Firmware includes it on RX, host omits it on TX | Asymmetric and undocumented. |
| Filtering and ACK | Disabled | Correct for passive raw sniffing; insufficient for active participation. |
| MAC/Zigbee decoding | None; `LLManager` pass-through | Missing. |

**[HISTORICAL EXPERIMENTAL OBSERVATION]** Retained campaigns report raw IEEE payload delivery in both directions (10/10 and 20/20) and a `TX_FRAME` run of 10/10. Those results support `L2 raw`, not Zigbee interoperability or PHY conformance. The source index is `../Understanding-CatSniffer/06 - Experimentos/00 - Consolidación y evaluación experimental FeralRF/09 - Preset Inventory and Historical Validation Audit/02 - Historical Experimental Evidence Mapping.md:18`.

### IEEE implementation backlog

1. Define a public `Ieee802154Frame` contract: TX excludes PHR/FCS; RX explicitly reports whether FCS is retained.
2. Add independent tests for channels 11 and 26, length boundaries, FCF/addressing variants, good/bad FCS, and transmitted bytes captured by a third-party sniffer.
3. Add a capture option for CRC-failed frames instead of unconditionally dropping them in `radio_if.c:1538-1545`.
4. Stop remapping invalid channels 0–15 silently. Reject them at the API boundary unless a separate zero-based channel API is intentional.

## 2. Zigbee — priority decision

### Required layers

**[PROTOCOL STANDARD]** Zigbee is not a PHY preset. At 2.4 GHz it uses IEEE 802.15.4 PHY/MAC, then adds NWK, APS, ZDO, security, device/commissioning behavior, and application objects. A conformant active device must also maintain keys, frame counters, addressing, routes, transaction state, timing, retries, and commissioning state.

```text
IEEE MAC MPDU returned by FeralRF
  FCF | Seq | PAN/address fields | Aux security? | MAC payload | FCS
                                              |
                                              v
Zigbee NWK frame
  NWK FCF | Dst | Src | Radius | NWK Seq | optional fields | Aux security? | payload
                                                                      |
                                                                      v
APS frame -> ZDO or ZCL/application payload
```

**[CURRENT SOURCE CODE]** `LLManager_processRxPacket()` only classifies BLE; the default path passes the packet unchanged (`firmware/cc1352/src/ll_manager.c:40-159`). The CMake target links RF driver, drivers, SysBIOS, and driverlib, not Z-Stack (`firmware/cc1352/CMakeLists.txt:129-184,240-253`). There is no Zigbee Python decoder. Therefore current Zigbee status is `MISSING`, while the underlying IEEE bearer is `IMPLEMENTED` as raw capture.

### What can be preserved and decoded today

For a received CRC-good IEEE frame, FeralRF can preserve the MAC FCF, sequence number, PAN IDs, source/destination addresses, auxiliary security header, and MAC payload. A host decoder can parse plaintext Zigbee NWK/APS/ZDO/ZCL fields. Encrypted payloads can only be decrypted if the user lawfully supplies the correct key material and counters; discovery of those values is not implied by raw capture.

The recommended first deliverable is **passive host-side Zigbee decoding**:

- reuse `PHY.IEEE_802_15_4`; do **not** add a proprietary `zigbee_*` preset;
- normalize the IEEE RX frame contract and add a Python 802.15.4 MAC parser;
- parse Zigbee NWK, APS, ZDO, and common ZCL headers in Python;
- preserve undecoded bytes and security metadata rather than treating decryption failure as malformed traffic;
- emit PCAP-compatible records with channel, timestamp provenance, and verified FCS state.

This path minimizes firmware change and is sufficient for passive analysis. Active raw injection is possible only in a narrow sense: the host can construct an MPDU, but the present backend lacks standards-correct channel access, ACK timing, retries, and Zigbee state. It must be labelled an experimental injector, not a Zigbee device.

### Full-stack feasibility

**[TI SDK SOURCE OR DOCUMENTATION]** The pinned SDK contains `source/ti/zstack/` with NWK, APS, ZDO, ZCL, commissioning, and security sources; for example, startup initializes APS/ZDO in `source/ti/zstack/startup/zstackstartup.c:239-249,398-424`. The silicon family is compatible.

**[STATIC ANALYSIS INFERENCE]** Integrating Z-Stack into this firmware is not a small preset change. It would compete with FeralRF for RF-driver scheduling, RTOS tasks, clocks, storage, keys, and radio setup ownership. The safest architecture is a separate firmware target/persona, or an explicit build-time feature, with a versioned host capability handshake. Simultaneous raw BLE/IEEE/proprietary service plus Z-Stack should be treated as an independent DMM/resource project.

**Decision:** implement passive host decoding first; defer full active Zigbee until use cases, target Zigbee revision, memory budget, provisioning/security policy, and radio-concurrency requirements are approved.

## 3. Proprietary 2.4 GHz GFSK: P22 and P23

| ID / preset | Current request | Expected | Actual implementation | Status / validation |
|---|---|---|---|---|
| P22 `gfsk_2440_250k` | 2440 MHz, GFSK, 250 ksym/s, dev code 320, BW `0x59` | Target-characterized 2.4 GHz proprietary GFSK RF setup, 2.4 front end, correct overrides/power table | Generic sub-GHz setup; frequency helper rejects `>=1000 MHz` and selects 868 MHz fallback | `DEFECTIVE`; `NOT VALIDATED` |
| P23 `gfsk_2440_50k` | 2440 MHz, GFSK, 50 ksym/s, dev 100, BW `0x52` | A generated/characterized 2.4 GHz configuration for this exact PHY | Same 868 MHz fallback and sub-GHz override set | `DEFECTIVE`; `NOT VALIDATED` |

The defect is explicit:

```c
/* firmware/cc1352/src/radio_if.c:1578-1595 */
if (frequency_hz > 0u && (frequency_hz / 1000000u) < 1000u) {
    freq_mhz = (uint16_t)(frequency_hz / 1000000u);
    /* ... */
} else if (s_phy == PHY_SUBGHZ_915) {
    freq_mhz = (uint16_t)(902u + channel);
} else {
    freq_mhz = 868u;
}
```

**[TI SDK SOURCE OR DOCUMENTATION]** The CC1352P7 supports proprietary 2.4 GHz, and the local SysConfig metadata contains target-specific `RF_prop_2_4G_fsk_250kbps` and 100 kbps settings (`source/ti/prop_rf/.meta/rf_config/LP_CC1352P7_4_rf_defaults.js:112-134`; `prop_rf_config_common.js:144-185`). TI documents `CMD_PROP_RADIO_SETUP` as 2.4-only and `CMD_PROP_RADIO_DIV_SETUP` as mandatory below 1 GHz but also usable at 2.4 GHz. This means the product is capable, but the present generated setup, override, PA, antenna, and frequency code path is not.

Recommended implementation:

1. Generate a CC1352P7 2.4 GHz proprietary configuration with the pinned SysConfig/SmartRF Studio version for each intended rate.
2. Give it separate immutable setup/FS/TX/RX objects, RF mode, override arrays, and power table; do not mutate the sub-GHz objects into 2.4 GHz.
3. Route the antenna explicitly and validate the RF-driver callback behavior already noted in `main_rtos.c:104-107`.
4. Reject P22/P23 until the new backend reports applied frequency and target setup ID.
5. Validate with a spectrum analyzer/frequency counter first, then a compatible peer, then packet-error and occupied-bandwidth tests.

## 4. Other 2.4 GHz possibilities

- IEEE 802.15.4 raw already covers the radio bearer used by Zigbee, Thread, and 6LoWPAN, but none of those upper layers is implemented.
- The local SDK also contains TI 15.4-stack material. As with Z-Stack, adopting it means a separate MAC/network integration rather than a preset.
- A generic 2.4 GFSK backend is feasible after P22/P23 are corrected. Protocol names should only be added when their framing/state is implemented.
- No source evidence supports arbitrary 2.4 GHz OOK, MSK, 4-FSK, or 4-GFSK through the current command structure.

## 5. BLE informational note

**EXISTING CAPABILITY — NOT CURRENTLY SELECTED FOR DEVELOPMENT.**

BLE is selected through native BLE RF Core commands, not `PROP_PRESETS`. The firmware includes advertising reception/hopping, LL packet classification, selected scan/connect handling, data-channel following, and central/master primitives (`radio_if.c:400-966,1399-1495,2392-2840`; `ll_manager.c:40-143`). Python exposes PHY selection and advertising-hop control (`python/feralrf/radio.py:455-478,539-553`). The BLE5-Stack ICall startup is deliberately disabled until a later phase (`main_rtos.c:212-218`), so these are custom RF/LL mechanisms, not evidence of a full BLE host/controller stack. BLE shares the single RF Core, RF-driver/session machinery, antenna switching, command task, and mutable global radio state with IEEE and proprietary modes; any future concurrent Zigbee/stack design must account for that ownership. No BLE changes are recommended by this preset audit.

# Part II — sub-GHz, descending frequency

## Shared proprietary backend behavior

All entries below execute this same chain:

```text
PROP_PRESETS[name]
 -> Radio.configure_prop(**dict)
 -> CommandBuilder.set_prop_config() [18-byte payload]
 -> CMD_SET_PROP_CONFIG
 -> RadioIF_setPropConfig() [mutates shared Prop0 command objects]
 -> CMD_PROP_RADIO_DIV_SETUP_PA + CMD_PROP_TX / CMD_PROP_RX
```

**[CURRENT SOURCE CODE]** The setter computes a symbol-rate word, copies `modType`, deviation, RX bandwidth, format, and sync word into the shared generated structures (`firmware/cc1352/src/radio_if.c:2020-2170`). It does not select W‑M‑Bus, IEEE 802.15.4g, Sidewalk, Wi‑SUN, or MIOTY framing. The default format is 32-bit sync, MSB first, no FEC, and no whitening (`radio_if.c:2010-2017`; `smartrf_prop_0.c:189-226`).

The `deviation` values are RF-command codes, not hertz despite the comment in `radio_if.h:72-83`. With `deviationStepSz=0`, the current table's intended scale is 250 Hz per unit: 100 means 25 kHz, 150 means 37.5 kHz, 180 means 45 kHz, 200 means 50 kHz, and 320 means 80 kHz. This representation must be renamed or converted in the API.

## 6. 915 MHz

### P20 — `gfsk_915_50k`

| Item | Audit result |
|---|---|
| Current parameters | 915 MHz; GFSK (`modType=1`); 50 ksym/s; 25 kHz deviation; RX BW `0x52`; sync `0x930B51DE`; generic variable-length+CRC frame. |
| Expected | Generic GFSK has no external protocol expectation; its documented frame is the FeralRF/TI proprietary frame. |
| Implemented | Frequency falls within the sub-GHz path; GFSK is a valid TI value; rate/deviation/BW/sync are applied. |
| Defective/missing | Cross-cutting metadata and completion defects X1–X6; physical settings not independently measured. |
| Validation | **[HISTORICAL EXPERIMENTAL OBSERVATION]** `L2 raw` between FeralRF peers; no L3/L4. |
| Owner / action | Firmware+Python: keep as `raw_gfsk_915_50k`, document frame/units, fix cross-cutting defects. |
| Blocker | Instrumented frequency/deviation/BW/power and independent peer evidence. |

### P24/P25 — Sidewalk-labelled FSK

| Preset | Current parameters | Expected Sidewalk behavior | Gap / status | Validation |
|---|---|---|---|---|
| P24 `sidewalk_915_fsk_50k` | 915 MHz, binary FSK, 50 ksym/s, 25 kHz deviation, BW `0x52`, 32-bit `0x930B51DE`, no whitening/FEC | A selected Amazon Sidewalk FSK PHY and regional channel plan, plus Sidewalk framing, link behavior, security/provisioning, and gateway interoperability | Only generic FSK. `CONFIGURATION ONLY`; name is misleading. | `L2 raw` FeralRF peer only |
| P25 `sidewalk_915_fsk_250k` | 915 MHz, binary FSK, 250 ksym/s, 50 kHz deviation, BW `0x5A`, same raw framing | Same protocol obligations; exact rate/profile must be tied to a Sidewalk SDK configuration | Only generic FSK. `CONFIGURATION ONLY`; parameters are not derived from the pinned Sidewalk target settings. | `L2 raw` FeralRF peer only |

**[PROTOCOL STANDARD]** AWS documents Sidewalk as using BLE for short range and 900 MHz FSK or LoRa for longer range. That statement does not make an arbitrary 900 MHz FSK frame a Sidewalk frame.

**[TI SDK SOURCE OR DOCUMENTATION]** The pinned SDK contains the TI Sidewalk demo and complete third-party stack under `source/ti/ti_sidewalk/`. Its CC1352P7 FSK settings materially differ from FeralRF: the setup uses 24 sync bits and `whitenMode=7`, while advanced TX/RX use sync `0x0055904E`, a 16-bit/11-bit-length header definition, CRC, and explicit append rules (`.../ti_sidewalk/apps/ti/sid_demo/app_source/cc1352p7/sysconfig/fsk_ti_radio_config.c:188-228,283-360`). The checked-in demo is set to 920 MHz, so its regional configuration must not be copied blindly to a 915 MHz product.

Recommended options:

1. Rename P24/P25 to `raw_fsk_915_50k_candidate` and `raw_fsk_915_250k_candidate`, removing the protocol claim; or
2. build a separate Sidewalk firmware target from the pinned TI/AWS integration, with explicit supported region, qualification, provisioning, credentials, gateway/cloud setup, and licensing review.

LoRa-based Sidewalk cannot be implemented by this CC1352 proprietary FSK command. The existing comment correctly points to a Semtech-class companion radio (`python/feralrf/presets.py:279-283`).

## 7. 902.2 MHz

### P21 — `gfsk_902_50k`

| Item | Audit result |
|---|---|
| Current parameters | 902.2 MHz; GFSK; 50 ksym/s; 25 kHz deviation; BW `0x52`; `0x930B51DE`; generic FeralRF frame. |
| Expected | Generic raw GFSK only. |
| Implemented | Frequency, valid modulation, and common frame path are present. |
| Defective/missing | X1–X6; no independent RF measurement. |
| Validation | `L2 raw` historical FeralRF peer delivery. |
| Owner / action | Firmware+Python; keep and rename as a raw profile. |
| Blocker | External peer and instrumented L4 characterization. |

### P26/P28/P29/P30/P31 — Wi‑SUN-labelled FSK

Despite the names, every one of these entries requests **902.2 MHz**. None requests 915 MHz:

| Preset | Rate / deviation / BW | Current frame | Required Wi‑SUN content absent | Status / validation |
|---|---|---|---|---|
| P26 `wisun_915_fsk_50k` | 50 ksym/s / 25 kHz / `0x52` | FeralRF raw, no whitening/FEC | selected regional PHY/channel plan; IEEE 802.15.4g MR-FSK PHR/encoding; FAN MAC, frequency hopping, security, joining, IPv6 adaptation/routing | `CONFIGURATION ONLY`; `L2 raw` |
| P28 `wisun_915_fsk_100k` | 100 ksym/s / 37.5 kHz / `0x56` | same | same | `CONFIGURATION ONLY`; `L2 raw` |
| P29 `wisun_915_fsk_150k` | 150 ksym/s / 50 kHz / `0x5A` | same | same; rate/profile mapping unproven | `CONFIGURATION ONLY`; `L2 raw` |
| P30 `wisun_915_fsk_200k` | 200 ksym/s / 50 kHz / `0x5A` | same | same | `CONFIGURATION ONLY`; `L2 raw` |
| P31 `wisun_915_fsk_300k` | 300 ksym/s / 50 kHz / `0x5E` | same | same; rate/profile mapping unproven | `CONFIGURATION ONLY`; `L2 raw` |

**[PROTOCOL STANDARD]** Wi‑SUN FAN uses defined regional PHY operating modes and IEEE 802.15.4g/4-based MAC behavior, not merely an FSK rate. Full FAN adds channel plans/frequency hopping, secure network admission, key management, IPv6 adaptation, routing, and management functions.

**[TI SDK SOURCE OR DOCUMENTATION]** The pinned SDK contains `source/ti/ti_wisunfan/`, including MAC frequency-hopping code, security, network-management applications, uIP/IPv6, RPL, TLS/mbed components, and target libraries under `lib/ticlang/m4f/`. None is compiled by the current CMake target. The current project uses `arm-none-eabi` GCC; compatibility with the available TI-clang libraries is **[UNVERIFIED / UNKNOWN]** and is a concrete integration blocker.

Recommendation: rename the existing entries as raw candidates immediately. If FAN is a product requirement, choose a precise FAN revision, regional domain, operating-mode ID, channel plan, and device role, then create a dedicated TI Wi‑SUN build. Do not try to grow a FAN stack behind `SET_PROP_CONFIG`.

## 8. 868.95 MHz — W‑M‑Bus T/C-labelled profiles

| Preset | Current request | Standards-level expectation | Current gap | Status / validation |
|---|---|---|---|---|
| P14 `wireless_mbus_t_868` | 868.95 MHz, GFSK, 100 ksym/s, 50 kHz deviation, BW `0x57`, 32-bit `0x543D0000` | Mode T PHY coding/synchronization and W‑M‑Bus link frame (length/control/manufacturer/address/control-information/data with block CRC rules); mode/direction timing | Generic GFSK with FeralRF length+CRC; no 3-of-6 coding, W‑M‑Bus parser/builder, block CRC, or mode timing | `CONFIGURATION ONLY`; `L2 raw` |
| P15 `wireless_mbus_c_868` | 868.95 MHz, GFSK, 100 ksym/s, 45 kHz deviation, BW `0x57`, same sync | Mode C selected frame format (A or B), synchronization and link frame/block CRC; mode/direction timing | Same generic packet engine; no selected C-frame format or W‑M‑Bus link behavior | `CONFIGURATION ONLY`; `L2 raw` |

**[PROTOCOL STANDARD]** EN 13757-4 defines the wireless PHY/link modes; OMS adds application/profile constraints. Mode T uses 3-of-6 line coding; mode C uses uncoded NRZ with mode-specific synchronization and frame-format requirements. The exact EN 13757 edition, direction (`T1/T2`, `C1/C2`), and C frame format must be selected before code can be called conformant.

**[TI SDK SOURCE OR DOCUMENTATION]** The pinned device SDK contains `rf_patch_mce_wmbus_ctmode.c/.h` for CC13x2x7. The current `smartrf_prop_0.c` does not include or install it; it uses `rf_patch_cpe_multi_protocol` and generic proprietary MCE/RFE selection. TI explicitly directs users to W‑M‑Bus C/T settings and the W‑M‑Bus patch. This is direct evidence that P14/P15 are not native W‑M‑Bus merely because their gross radio rate is plausible.

Recommended implementation: generate exact S/T/C RF settings with the pinned target tool, create mode-specific immutable backends, then add host or firmware W‑M‑Bus frame codec. Passive RX can start with decoded link frames at the host. Active devices require timing, direction, link-layer state, and an explicit OMS/application scope.

## 9. 868.3 MHz — W‑M‑Bus S-labelled profile

### P13 — `wireless_mbus_s_868`

| Item | Audit result |
|---|---|
| Current parameters | 868.3 MHz; GFSK; 32.768 ksym/s; 18.75 kHz deviation; BW `0x52`; sync `0x543D0000`; generic frame. |
| Expected | Selected S1/S2 direction, Mode S Manchester coding/synchronization, W‑M‑Bus link fields and block CRC, timing/duty-cycle behavior. |
| Implemented | Only gross frequency/rate/modulation values through generic GFSK. |
| Missing | `rf_patch_mce_wmbus_smode`, Manchester/mode packet handling, link codec, block CRC, timing, independent peer compatibility. |
| Status / validation | `CONFIGURATION ONLY`; historical `L2 raw` only. |
| Owner / action | RF firmware for native settings/patch; Python protocol owner for passive frame decoding; product for exact standard profile. |
| Blocker | EN 13757 edition/profile choice, generated target settings, and conformant peer/capture evidence. |

The local SDK contains `rf_patch_mce_wmbus_smode.c/.h`, but FeralRF neither compiles nor loads it. A 32-bit sync value ending in zeroes is not a substitute for mode-specific synchronization and line coding.

## 10. 868 MHz

### P09/P10 — generic GFSK

| Preset | Current parameters | Implementation | Defects / validation | Recommendation |
|---|---|---|---|---|
| P09 `gfsk_868_50k` | 868 MHz, GFSK, 50 ksym/s, 25 kHz deviation, `0x52`, `0x930B51DE` | Valid generic proprietary mode and shared frame | X1–X6; `L2 raw` (including retained 10/10 control run) | Keep as `raw_gfsk_868_50k`; make the canonical recovery/control profile. |
| P10 `gfsk_868_100k` | 868 MHz, GFSK, 100 ksym/s, 37.5 kHz deviation, `0x56`, same sync | Valid generic proprietary mode | X1–X6; `L2 raw` | Keep as raw profile; instrument and independently characterize. |

### P11 — `ook_868_4k8`

Current request is 868 MHz OOK at 4.8 ksym/s, BW code 76/`0x4C`, sync `0x930B51DE`. OOK is a valid TI modulation selection and the firmware loads `rf_patch_mce_genook`/`rf_patch_rfe_genook` (`smartrf_prop_0.c:32-38,122-187`; `radio_if.c:2135-2147`). That makes configuration **present**, but lifecycle behavior is unsafe.

**[HISTORICAL EXPERIMENTAL OBSERVATION]** The controlled 2026-10-26 run recorded RX-start ACK, one TX ACK, then one timeout and eight command errors, 0/10 matching peer packets, and RX-stop timeout. Complete physical power removal restored P09 to 10/10. Source: `../Understanding-CatSniffer/06 - Experimentos/00 - Consolidación y evaluación experimental FeralRF/09 - Preset Inventory and Historical Validation Audit/validation_runs/2026-10-26_unresolved_presets_114802/summary.md:78-107,109-165`. This is `L1 / BLOCKED`, not proof of an OOK waveform failure.

Confirmed code-level hazards:

- **[CURRENT SOURCE CODE]** repeated OOK configuration can return without an error while only mutating some command fields (`radio_if.c:2030-2048`), yet the command processor always ACKs;
- `RadioIF_setPhy()` clears the software OOK flag (`radio_if.c:1912-1976`) although the comments and boot design say the loaded RF patches cannot safely be unloaded;
- stop cancels/flushes/yields but cannot prove restoration of the previous CPE/MCE/RFE state (`radio_if.c:1373-1398`);
- the permanent 433 RF handle and comments acknowledge RF Core patch/reopen constraints (`main_rtos.c:153-170`);
- ACK-before-execution and RF-task command ownership can account for some control-plane symptoms.

**[HYPOTHESIS]** The terminal state is caused by an interaction among persistent OOK patches, shared mutable setup objects, RF handle/session reuse, and/or a blocking RF call which prevents the RF task from consuming stop/recovery commands. Static inspection cannot identify which internal RF Core state the power cycle clears.

Required fail-safe state machine:

```text
NORMAL --configure OOK--> OOK_CONFIGURED --start/tx/rx--> OOK_ACTIVE
  ^                              |                           |
  |                              +---- any uncertainty ------+
  |                                                        v
  +-- only proven recovery ---------------- TERMINAL_REQUIRES_POWER_CYCLE
                                                |
                                           physical power cycle
```

Until recovery is proven, the terminal state must reject all RF commands with a typed error and the Python API must require an explicit `acknowledge_power_cycle()`/reconnect sequence. A software flag reset must never claim recovery. Capture RF-driver events/status, setup identity, handle identity, patch selection, and command correlation before and after each transition.

### P12/P18/P19 — MSK, 4-FSK, and 4-GFSK labels

```c
/* pinned SDK: .../driverlib/rf_prop_cmd.h:475-478 */
uint16_t modType:3;  // 0: FSK; 1: GFSK; 2: OOK; Others: Reserved
```

| Preset | Requested `modType` | Current behavior | Status / validation | Recommendation |
|---|---:|---|---|---|
| P12 `msk_868_50k` | 4 | Accepted by FeralRF and copied into a reserved TI field | `BLOCKED BY CURRENT BACKEND`; `NOT VALIDATED` | Remove from executable catalog or mark unsupported; research a characterized MSK/continuous-phase setup rather than inventing enum values. |
| P18 `4fsk_868_50k` | 5 | Same | `BLOCKED BY CURRENT BACKEND`; `NOT VALIDATED` | Same; require a distinct supported RF command/patch and symbol mapping. |
| P19 `4gfsk_868_50k` | 6 | Same | `BLOCKED BY CURRENT BACKEND`; `NOT VALIDATED` | Same; require native multi-level modulation evidence. |

The comments in `radio_if.h:75-78` and `presets.py:54-78,153-169` contradict the authoritative local SDK definition. The firmware must reject every `modType > 2` now. A source comment cannot extend RF Core semantics.

### P27 — `mioty_868_tsunb`

| Item | Audit result |
|---|---|
| Current parameters | 868 MHz; generic binary FSK; 396 symbols/s; deviation code 1 (250 Hz); BW `0x4A`; generic 32-bit sync and FeralRF frame. |
| Expected | ETSI TS 103 357 TS‑UNB burst construction: coded/interleaved telegram, telegram splitting into sub-packets, mode-specific pilot/header/data fields, and scheduled time/frequency placement; receiver reassembly and protocol processing. |
| Implemented | Only a low-rate generic FSK request. No split, coding/interleaving, hopping/scheduling, reassembly, or MIOTY MAC/service layer. |
| Status | `CONFIGURATION ONLY`, and likely not representable by the present generic packet command at the requested narrow settings. |
| Validation | **[HISTORICAL EXPERIMENTAL OBSERVATION]** command acceptance/TX ACK 10/10 but peer payload 0/10; P09 recovered immediately. This is a failed FeralRF raw-delivery criterion, not a MIOTY pass/fail. |
| Owner / action | Product selects ETSI revision/mode; RF specialist proves hardware waveform feasibility; firmware implements a dedicated PHY/scheduler or integrates a qualified stack; Python handles reassembly/decoding as appropriate. |
| Blocker | No native MIOTY implementation or target RF configuration in this SDK tree; exact RF Core feasibility and timing resources are unproven. |

**[PROTOCOL STANDARD]** ETSI TS 103 357 defines telegram splitting; the MIOTY Alliance physical-layer overview describes the telegram, including header and pilot, being split into radio bursts. The preset's own comment acknowledges pending native support (`python/feralrf/presets.py:229-243`).

The correct action is to remove P27 from normal smoke loops and capability reports while preserving its API ID only as `unsupported/experimental`. Do not tune `rx_bw` repeatedly as if that could create TS‑UNB framing.

## 11. Briefly excluded 433 and 169 MHz entries

- P01–P08 at 433.92 MHz and P16–P17 at 169.45 MHz remain outside this report's detailed priority scope.
- The generic/reserved modulation and protocol-label conclusions still apply: P06/P07/P08 use reserved `modType` values, P04/P05 inherit the OOK terminal-state risk, and P16/P17 are W‑M‑Bus-labelled generic GFSK without a W‑M‑Bus backend.
- The firmware has a special persistent 433 handle and target-generated 433 settings (`main_rtos.c:153-170`; `syscfg/ti_radio_config_433.c`). This architecture needs its own lifecycle audit before generalizing findings between 433 and 868/915 MHz.

# Part III — packet framing, byte ownership, and defects

## 12. Responsibility matrix

| Layer/field | Python | CC1352 application | TI RF Core | Protocol stack currently present? |
|---|---|---|---|---|
| Serial CMD/RSP, sequence, payload length | Builds/parses | Parses/builds | — | FeralRF transport only |
| Serial CRC16 + COBS delimiter | Builds/parses | Parses/builds | — | Yes |
| Proprietary preamble/sync | Supplies config values | Copies to command | Generates/detects | Generic only |
| Proprietary length | Supplies payload length out of band | Sets `pktLen` | Emits/parses header | Yes, but RX exposes it while TX does not accept it |
| Proprietary payload | Supplies bytes | Copies/queues | Modulates/demodulates | Raw only |
| Proprietary CRC | — | Enables and incorrectly interprets RX status | Generates/checks/appends | Yes; host metadata is defective |
| IEEE preamble/SFD/PHR | — | Chooses channel/options | Generates/detects | Yes |
| IEEE MPDU | Supplies complete MPDU without FCS | Copies bytes | Transmits/receives | Raw only |
| IEEE FCS | — | Configures generation/include behavior | Generates/checks | Yes; RX/TX contract asymmetric |
| Zigbee NWK/APS/ZDO/ZCL | — | — | — | No |
| W‑M‑Bus coding/link frame | — | — | Generic packet engine only | No |
| 802.15.4g/Wi‑SUN FAN | — | — | Generic packet engine only | No |
| Sidewalk framing/security | — | — | Generic packet engine only | No |
| MIOTY splitting/reassembly | — | — | Generic packet engine only | No |

## 13. Proprietary RX metadata defect

The source defines and consumes the trailer as follows:

```c
/* firmware/cc1352/src/radio_if.c:75-81,1717-1725 */
#define PROP_APPEND_STATUS_SIZE 1u
#define PROP_APPEND_RSSI_SIZE 1u
#define PROP_APPEND_TIMESTAMP_SIZE 4u
uint8_t status = entry_data[pdu_len];
int8_t rssi = (int8_t)entry_data[pdu_len + 1u];
uint32_t rat_timestamp = read_u32_le(&entry_data[pdu_len + 2u]);
```

**[TI SDK SOURCE OR DOCUMENTATION]** With `bAppendRssi`, `bAppendTimestamp`, and `bAppendStatus`, TI's RX output order is RSSI, timestamp, then status. The total trailer size is right but every offset is wrong. Since the supposed status byte is actually RSSI, the CRC bit test can randomly accept/drop frames; the supposed RSSI is the low timestamp byte; and timestamp is composed from three timestamp bytes plus status. `lqi` is then left at zero and `channel` is copied from software state.

There is a second independent defect. `PROP_STATUS_CRC_OK_BIT` is defined as `0x80`, but the local SDK defines the top two status bits as a `result` enum: 0 = correct, 1 = CRC error, 2 = correct-but-ignorable, 3 = aborted (`rf_prop_cmd.h:1166-1179`). Therefore `bool crc_ok = (status & 0x80) != 0` is not valid even after correcting the offset; it would mark result 0 as false and results 2/3 as true.

Correction must be made before any more experiments use these fields:

```text
pdu_end + 0: RSSI (int8)
pdu_end + 1..4: RAT timestamp (uint32 little-endian)
pdu_end + 5: status; decode result = (status >> 6) & 0x03
                 result 0 => received correctly
                 result 1 => CRC error
                 result 2 => correct but ignorable (policy required)
                 result 3 => aborted
```

Add a fixture containing deliberately distinct sentinel values for every byte. Test both CRC outcomes, packet retention policy, wrap/clock conversion, and the exact `Packet.data` span. Existing historical payload matches remain useful because acceptance used unique application bytes, not RSSI/LQI.

## 14. Command acknowledgement and completion

```text
Host TX command
  -> parser validates syntax
  -> ControlTask_onTxRaw copies into one pending buffer
  -> immediate ACK returned
  -> later RF task calls RadioIF_transmitRaw
  -> only failure may emit an uncorrelated async error; success emits nothing
```

The public API's `transmit()` waiting for ACK (`python/feralrf/radio.py:725-735`) therefore proves only queue acceptance. A robust contract needs:

- a 16- or 32-bit operation ID independent of the 8-bit serial sequence;
- `ACCEPTED`, `STARTED`, and one terminal `COMPLETED`/`FAILED` event with RF-driver event mask, RF command status, setup ID, frequency, and bytes scheduled;
- correlation for start/stop and config operations too;
- bounded timeouts and an explicit `UNKNOWN_OUTCOME` state if the transport is lost;
- no mutation of a queued operation's shared command structure before it finishes.

## 15. RX stop and OOK control-plane failure

`RX_STOP` is acknowledged when a task event is set, not after `RF_cancelCmd`/`RF_flushCmd` and state verification (`command_processor.c:201-208`; `control_task.c:440-448`; `data_task.c:80-85`). Because command execution occurs in the RF task, a blocking/stalled RF call can prevent the stop command from being processed at all. The historical timeout is compatible with this design, but **[HYPOTHESIS]** it does not prove this is the only cause.

Required diagnostics:

1. timestamp command receipt in the UART task and execution in the RF task;
2. record active RF handle, command handle, setup/patch ID, RF command status, and driver events;
3. make cancellation asynchronous or ensure the RF task never blocks indefinitely;
4. have a watchdog move the device to a declared fault state without pretending the radio recovered;
5. require power removal after OOK fault until a controlled recovery procedure is demonstrated.

## 16. API and naming contract

Replace protocol implication with typed capability:

```text
RadioProfile
  id: raw_gfsk_868_50k
  backend: TI_PROP_SIMPLE
  frequency_hz: 868000000
  tx_input: PAYLOAD_ONLY
  rx_output: LENGTH_PAYLOAD_CRC   # until normalized
  completion: ACCEPTED_ONLY       # until fixed
  conformance: NONE
  experimental_evidence: L2_FERALRF_PEER
```

Protocol services should be separate objects (`Ieee802154Raw`, `ZigbeePassiveDecoder`, `WmbusDecoder`, `WisunFanDevice`, `SidewalkDevice`) with explicit version, region, role, framing, key/provisioning, and conformance properties. A dictionary key must not be the only capability signal.

## 16.1 Selected code evidence for the shared execution paths

The following excerpts complement the defect-specific excerpts above. They are deliberately short and show the decisive dispatch, configuration, TX, and protocol-processing boundaries.

**Preset definition — `python/feralrf/presets.py`, `PROP_PRESETS`, lines 188–203:**

```python
"gfsk_2440_250k": dict(
    frequency_hz=2440000000,
    mod_type=1,
    symbol_rate=250000,
    deviation=320,
    rx_bw=0x59,
    sync_word=0x930B51DE,
),
"gfsk_2440_50k": dict(
    frequency_hz=2440000000,
    mod_type=1,
    symbol_rate=50000,
    deviation=100,
    rx_bw=0x52,
    sync_word=0x930B51DE,
),
```

This proves Python requests 2440 MHz; the 868 MHz result is introduced later by firmware frequency selection, not by the preset. It also shows that a preset is just a configuration dictionary—there is no protocol encoder reference.

**PHY selection — `firmware/cc1352/src/phy_manager.c`, `s_phy_profiles` and `PhyManager_select`, lines 16–24 and 42–49:**

```c
{PHY_MANAGER_PHY_IEEE_802_15_4, PHY_MANAGER_LL_DEFAULT, false, true},
{PHY_MANAGER_PHY_SUB_1GHZ_868, PHY_MANAGER_LL_DEFAULT, false, true},
{PHY_MANAGER_PHY_SUB_1GHZ_915, PHY_MANAGER_LL_DEFAULT, false, true},
{PHY_MANAGER_PHY_PROPRIETARY_GFSK, PHY_MANAGER_LL_DEFAULT, false, true},
/* ... */
bool PhyManager_select(uint8_t phy) {
    if (PhyManager_findProfile(phy) == NULL) return false;
    s_selected_phy = phy;
    return true;
}
```

IEEE and proprietary modes select the default LL profile; only BLE selects the BLE classifier. There is no Zigbee, W‑M‑Bus, FAN, Sidewalk, or MIOTY dispatch.

**PROP radio and packet configuration — `firmware/cc1352/src/smartrf_prop_0.c`, `Prop0_cmdPropRadioDivSetup`, `Prop0_cmdPropTx`, and `Prop0_cmdPropRx`, lines 200–213 and 249–294:**

```c
.modulation.modType = 0x1,
.modulation.deviation = 0x64,
.symbolRate.preScale = 0xF,
.symbolRate.rateWord = 0x8000,
.rxBw = 0x52,
.preamConf.nPreamBytes = 0x4,
.formatConf.nSwBits = 0x20,
.formatConf.fecMode = 0x0,
.formatConf.whitenMode = 0x0,
/* TX */ .pktConf.bUseCrc = 0x1, .pktConf.bVarLen = 0x1,
/* RX */ .rxConf.bIncludeHdr = 0x1, .rxConf.bIncludeCrc = 0x1,
.rxConf.bAppendRssi = 0x1,
.rxConf.bAppendTimestamp = 0x1,
.rxConf.bAppendStatus = 0x1,
```

This is the shared packet engine behind every `PROP_PRESETS` entry. It directly establishes four-byte preamble, 32-bit sync, no FEC/whitening, variable length, CRC, and the RX trailer flags. Protocol names do not alter these format fields.

**TX byte ownership — `firmware/cc1352/src/radio_if.c`, `RadioIF_transmitIeee154Raw`, lines 468–488, and `RadioIF_transmitPropRaw`, lines 1810–1819:**

```c
memcpy(s_ieee154_tx_buffer, payload, payload_len);
Ieee154_0_cmdIeeeTx.pPayload = s_ieee154_tx_buffer;
Ieee154_0_cmdIeeeTx.payloadLen = payload_len;
/* ... execute CMD_IEEE_TX ... */

Prop0_cmdPropTx.pPkt = s_prop_tx_buffer;
Prop0_cmdPropTx.pktLen = payload_len;
/* ... execute CMD_PROP_TX ... */
```

Both APIs accept host bytes and a length, but the selected RF command gives those bytes different OTA meaning: IEEE treats them as an MPDU payload while PROP wraps them in its configured proprietary packet.

**Missing protocol dispatch — `firmware/cc1352/src/ll_manager.c`, `LLManager_processRxPacket`, lines 144–159:**

```c
pkt->ll_pdu_kind = LL_PDU_KIND_UNKNOWN;
pkt->ll_pdu_type = LL_PDU_TYPE_INVALID;
pkt->ll_pdu_flags = 0u;
switch (s_selected_profile) {
case LL_MANAGER_BLE:
    return LLManager_processBlePacket(pkt);
case LL_MANAGER_DEFAULT:
default:
    return true;
}
```

Non-BLE packets are forwarded without protocol interpretation. This is positive evidence of raw preservation, and direct evidence that protocol-specific decode is absent from the active firmware path.

# Part IV — per-preset register, gap plan, and validation

## 17. Consolidated implementation register

The required register is normalized into two tables joined by `ID`; together they contain every requested column without making each Markdown row unreadably wide. `PROP` means the current simple TI proprietary packet backend. Priorities 1–6 follow the task's mandated development order, not estimated urgency inside a group.

### 17.0 Exact requested RF parameters

All proprietary rows use `packet_format=0` through the Python default, which firmware maps to 32-bit sync, MSB-first, FEC 0, whitening 0. “Deviation” shows the transmitted command code and its intended value at the current 250 Hz step; it is requested configuration, not measurement.

| ID | Exact name | Requested frequency | `modType` | Symbol rate | Deviation code (intended) | RX BW code | Sync word | Selected backend |
|---|---|---:|---:|---:|---:|---:|---:|---|
| I154 | `IEEE_802_15_4` | ch 11–26 → 2405–2480 MHz | native | 250 kbit/s (2 Mchip/s) | native | native | IEEE SFD | `CMD_IEEE_*` |
| ZB | Zigbee capability | inherits I154 | native | inherits I154 | native | native | IEEE SFD | I154 raw bearer only |
| P22 | `gfsk_2440_250k` | 2440000000 Hz | 1 GFSK | 250000 | 320 (80 kHz) | `0x59` | `0x930B51DE` | PROP, incorrectly sub-G |
| P23 | `gfsk_2440_50k` | 2440000000 Hz | 1 GFSK | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP, incorrectly sub-G |
| P20 | `gfsk_915_50k` | 915000000 Hz | 1 GFSK | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP |
| P24 | `sidewalk_915_fsk_50k` | 915000000 Hz | 0 FSK | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP |
| P25 | `sidewalk_915_fsk_250k` | 915000000 Hz | 0 FSK | 250000 | 200 (50 kHz) | `0x5A` | `0x930B51DE` | PROP |
| P21 | `gfsk_902_50k` | 902200000 Hz | 1 GFSK | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP |
| P26 | `wisun_915_fsk_50k` | **902200000 Hz** | 0 FSK | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP |
| P28 | `wisun_915_fsk_100k` | **902200000 Hz** | 0 FSK | 100000 | 150 (37.5 kHz) | `0x56` | `0x930B51DE` | PROP |
| P29 | `wisun_915_fsk_150k` | **902200000 Hz** | 0 FSK | 150000 | 200 (50 kHz) | `0x5A` | `0x930B51DE` | PROP |
| P30 | `wisun_915_fsk_200k` | **902200000 Hz** | 0 FSK | 200000 | 200 (50 kHz) | `0x5A` | `0x930B51DE` | PROP |
| P31 | `wisun_915_fsk_300k` | **902200000 Hz** | 0 FSK | 300000 | 200 (50 kHz) | `0x5E` | `0x930B51DE` | PROP |
| P14 | `wireless_mbus_t_868` | 868950000 Hz | 1 GFSK | 100000 | 200 (50 kHz) | `0x57` | `0x543D0000` | PROP |
| P15 | `wireless_mbus_c_868` | 868950000 Hz | 1 GFSK | 100000 | 180 (45 kHz) | `0x57` | `0x543D0000` | PROP |
| P13 | `wireless_mbus_s_868` | 868300000 Hz | 1 GFSK | 32768 | 75 (18.75 kHz) | `0x52` | `0x543D0000` | PROP |
| P09 | `gfsk_868_50k` | 868000000 Hz | 1 GFSK | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP |
| P10 | `gfsk_868_100k` | 868000000 Hz | 1 GFSK | 100000 | 150 (37.5 kHz) | `0x56` | `0x930B51DE` | PROP |
| P11 | `ook_868_4k8` | 868000000 Hz | 2 OOK | 4800 | 0 | `0x4C` (decimal 76) | `0x930B51DE` | PROP + genook patches |
| P12 | `msk_868_50k` | 868000000 Hz | 4 reserved | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP, invalid selection |
| P18 | `4fsk_868_50k` | 868000000 Hz | 5 reserved | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP, invalid selection |
| P19 | `4gfsk_868_50k` | 868000000 Hz | 6 reserved | 50000 | 100 (25 kHz) | `0x52` | `0x930B51DE` | PROP, invalid selection |
| P27 | `mioty_868_tsunb` | 868000000 Hz | 0 FSK | 396 | 1 (0.25 kHz) | `0x4A` | `0x930B51DE` | PROP |

### 17.1 Identity, current implementation, backend, framing, and structures

| ID | Name | Technology | Frequency | Current implementation | RF backend | Current framing | Current structures | Implementation status |
|---|---|---|---:|---|---|---|---|---|
| I154 | `IEEE_802_15_4` | IEEE 802.15.4 raw | channels 11–26 / 2405–2480 MHz | Native RF PHY; host supplies MPDU; RF Core generates PHR/FCS | `CMD_IEEE_*` | IEEE PHY + raw MAC frame | `rfc_CMD_IEEE_TX_t`, `rfc_CMD_IEEE_RX_t`, `RadioIF_RxPacket`, `Packet` | `IMPLEMENTED` raw; MAC partial |
| ZB | Zigbee capability | Zigbee | same bearer | No Zigbee parser or stack; raw IEEE bytes only | I154 bearer | IEEE MPDU only | No Zigbee structures; raw `Packet.data` | `MISSING` above PHY |
| P22 | `gfsk_2440_250k` | raw GFSK intended | 2440 MHz | Serializes config, then frequency helper falls back to 868 | sub-GHz `CMD_PROP_*` | PROP length+payload+CRC | `PropConfig`, shared `Prop0_cmd*` | `DEFECTIVE` |
| P23 | `gfsk_2440_50k` | raw GFSK intended | 2440 MHz | Same fallback | sub-GHz `CMD_PROP_*` | same | same | `DEFECTIVE` |
| P20 | `gfsk_915_50k` | raw GFSK | 915 MHz | Applies valid GFSK/rate/frequency to PROP | PROP | generic PROP | `PropConfig`, `rfc_CMD_PROP_*`, `RadioIF_RxPacket` | `IMPLEMENTED` raw; metadata defective |
| P24 | `sidewalk_915_fsk_50k` | Sidewalk-labelled raw FSK | 915 MHz | Generic FSK only | PROP | generic PROP, not Sidewalk | same shared PROP structures | `CONFIGURATION ONLY` |
| P25 | `sidewalk_915_fsk_250k` | Sidewalk-labelled raw FSK | 915 MHz | Generic FSK only | PROP | generic PROP, not Sidewalk | same | `CONFIGURATION ONLY` |
| P21 | `gfsk_902_50k` | raw GFSK | 902.2 MHz | Applies valid GFSK/rate/frequency | PROP | generic PROP | shared PROP structures | `IMPLEMENTED` raw; metadata defective |
| P26 | `wisun_915_fsk_50k` | Wi‑SUN-labelled raw FSK | **902.2 MHz** | Generic FSK only | PROP | generic PROP, not 15.4g/FAN | shared PROP structures | `CONFIGURATION ONLY` |
| P28 | `wisun_915_fsk_100k` | same | **902.2 MHz** | Generic FSK only | PROP | same | same | `CONFIGURATION ONLY` |
| P29 | `wisun_915_fsk_150k` | same | **902.2 MHz** | Generic FSK only | PROP | same | same | `CONFIGURATION ONLY` |
| P30 | `wisun_915_fsk_200k` | same | **902.2 MHz** | Generic FSK only | PROP | same | same | `CONFIGURATION ONLY` |
| P31 | `wisun_915_fsk_300k` | same | **902.2 MHz** | Generic FSK only | PROP | same | same | `CONFIGURATION ONLY` |
| P14 | `wireless_mbus_t_868` | W‑M‑Bus T-labelled GFSK | 868.95 MHz | Gross RF parameters only | PROP | generic PROP, no T coding/block CRC | shared PROP structures | `CONFIGURATION ONLY` |
| P15 | `wireless_mbus_c_868` | W‑M‑Bus C-labelled GFSK | 868.95 MHz | Gross RF parameters only | PROP | generic PROP, no C frame A/B | same | `CONFIGURATION ONLY` |
| P13 | `wireless_mbus_s_868` | W‑M‑Bus S-labelled GFSK | 868.3 MHz | Gross RF parameters only | PROP | generic PROP, no Manchester/block CRC | same | `CONFIGURATION ONLY` |
| P09 | `gfsk_868_50k` | raw GFSK | 868 MHz | Applies valid GFSK/rate/frequency | PROP | generic PROP | shared PROP structures | `IMPLEMENTED` raw; metadata defective |
| P10 | `gfsk_868_100k` | raw GFSK | 868 MHz | Applies valid GFSK/rate/frequency | PROP | generic PROP | same | `IMPLEMENTED` raw; metadata defective |
| P11 | `ook_868_4k8` | raw OOK | 868 MHz | Loads OOK patches and mutates shared setup | PROP + genook patches | generic PROP | `s_prop_ook_active`, shared `Prop0_cmd*`, RF handle/session | `PARTIAL`, lifecycle blocked |
| P12 | `msk_868_50k` | intended MSK | 868 MHz | Copies reserved `modType=4` and ACKs | PROP | undefined, not proven MSK | `PropConfig` only | `BLOCKED BY CURRENT BACKEND` |
| P18 | `4fsk_868_50k` | intended 4-FSK | 868 MHz | Copies reserved `modType=5` and ACKs | PROP | undefined | `PropConfig` only | `BLOCKED BY CURRENT BACKEND` |
| P19 | `4gfsk_868_50k` | intended 4-GFSK | 868 MHz | Copies reserved `modType=6` and ACKs | PROP | undefined | `PropConfig` only | `BLOCKED BY CURRENT BACKEND` |
| P27 | `mioty_868_tsunb` | MIOTY-labelled FSK | 868 MHz | Generic 396-symbol/s FSK request | PROP | one contiguous generic packet | shared PROP structures | `CONFIGURATION ONLY` |

### 17.2 Missing structures/functions, modifications, additions, dependencies, status, priority, and evidence

| ID | Missing structures | Missing functions | Required modifications | Required additions | Dependencies | Validation status | Priority | Source evidence |
|---|---|---|---|---|---|---|---:|---|
| I154 | explicit MPDU/FCS metadata; MAC state | filter/ACK/CSMA/retry management | `RadioIF_transmitIeee154Raw`, `RadioIF_processIeee154Packets`, Python packet/API contracts | bad-FCS capture option and fixtures | IEEE standard; independent sniffer | `L2 raw` | 2 | `radio_if.c:428-488,1335-1371,1497-1568`; `smartrf_ieee_15_4_0.c:81-163` |
| ZB | NWK/APS/ZDO/ZCL/security models | decode; for active scope: join, keys, counters, routing, state | none required for passive PHY; integrate separate stack for active scope | Python passive parser/PCAP; optional separate Z-Stack target | target Zigbee revision/role; keys; SDK Z-Stack | `NOT VALIDATED` | 1 | `ll_manager.c:40-159`; `CMakeLists.txt:129-184,240-253`; SDK `zstackstartup.c:239-249,398-424` |
| P22 | separate 2.4 RF mode/setup/FS/TX/RX/power table | backend select and 2.4 frequency apply | `RadioIF_getPropMode`, `RadioIF_applySub1ghzChannelConfig`, `RadioIF_setPropConfig`, CMake/generated settings | immutable 2.4 backend and applied-config report | SysConfig/SmartRF; antenna/hardware revision; instruments | `NOT VALIDATED` | 3 | `presets.py:188-195`; `radio_if.c:173-186,1578-1617,2020-2170` |
| P23 | same, exact 50k setup | same | same | same | exact characterized settings | `NOT VALIDATED` | 3 | `presets.py:196-203`; same firmware ranges |
| P20 | valid metadata/completion representation | reliable RX parse and TX completion | `RadioIF_processPropPackets`, TX state/API | sentinel fixtures and L4 characterization | RF instruments/external peer | `L2 raw` | 4 | `presets.py:171-178`; `radio_if.c:1706-1819` |
| P24 | Sidewalk PHY/link/security/device state | Sidewalk framing, MAC, provisioning, gateway interaction | rename preset; do not extend shared PROP setter | separate Sidewalk target if required | region/profile; TI/AWS SDK; credentials/gateway/qualification | `L2 raw`, no Sidewalk proof | 5 | `presets.py:205-212`; TI Sidewalk config `:188-228,283-360`; CMake omission |
| P25 | same | same | same | same | same | `L2 raw`, no Sidewalk proof | 5 | `presets.py:213-220`; same SDK/CMake evidence |
| P21 | valid metadata/completion representation | reliable RX parse and TX completion | shared PROP paths | fixtures and L4 characterization | instruments/external peer | `L2 raw` | 4 | `presets.py:179-186`; `radio_if.c:1706-1819` |
| P26 | 15.4g PHR, FAN MAC/FH/security/IPv6 state | encode/decode, hopping, join, routing, key management | rename preset; no protocol claim in PROP | dedicated TI Wi‑SUN firmware target | FAN version/domain/role; TI-clang library/toolchain decision | `L2 raw`, no FAN proof | 5 | `presets.py:221-228`; SDK `source/ti/ti_wisunfan/`; CMake omission |
| P28 | same, exact operating-mode binding | same | same | same | same | `L2 raw`, no FAN proof | 5 | `presets.py:247-254`; same SDK/CMake evidence |
| P29 | same | same | same | same | same | `L2 raw`, no FAN proof | 5 | `presets.py:255-262`; same evidence |
| P30 | same | same | same | same | same | `L2 raw`, no FAN proof | 5 | `presets.py:263-270`; same evidence |
| P31 | same | same | same | same | same | `L2 raw`, no FAN proof | 5 | `presets.py:271-278`; same evidence |
| P14 | T-mode coding/sync and W‑M‑Bus link/block structures | T encode/decode, CRC blocks, timing | add mode-specific backend; stop using shared PROP | codec and passive decoder | EN 13757 edition/direction; C/T patch/settings; peer | `L2 raw`, no W‑M‑Bus proof | 5 | `presets.py:121-128`; local `rf_patch_mce_wmbus_ctmode.c`; CMake omission |
| P15 | C frame A/B and link/block structures | C encode/decode, CRC blocks, timing | same | same | C1/C2 and frame A/B decision | `L2 raw`, no W‑M‑Bus proof | 5 | `presets.py:129-136`; same patch/CMake evidence |
| P13 | S-mode Manchester and W‑M‑Bus link/block structures | S encode/decode, CRC blocks, timing | add S backend | codec/passive decoder | S1/S2 choice; S patch/settings; peer | `L2 raw`, no W‑M‑Bus proof | 5 | `presets.py:113-120`; local `rf_patch_mce_wmbus_smode.c`; CMake omission |
| P09 | trustworthy metadata/completion | reliable RX parse/completion | shared PROP parser/TX contract | fixtures/L4 tests | instruments | `L2 raw`, repeated control | 4 | `presets.py:81-88`; `radio_if.c:1706-1819` |
| P10 | same | same | same | same | instruments | `L2 raw` | 4 | `presets.py:89-96`; same firmware evidence |
| P11 | durable OOK lifecycle/fault state and RF telemetry | verified configure/start/stop/recovery/completion | `RadioIF_setPhy`, `RadioIF_setPropConfig`, stop/session/TX paths; command responses | fail-safe state machine, watchdog/correlation | TI patch/driver behavior; bridge power semantics; instrumented lab | `L1/BLOCKED`, 0/10, power-cycle recovery | 4 | `presets.py:97-104`; `radio_if.c:1373-1398,1912-2170`; historical summary `:78-165` |
| P12 | supported MSK RF setup/symbol representation | MSK mod/demod | reject in `CommandProcessor`/`RadioIF_setPropConfig`; mark unsupported in Python | only after authoritative target config exists | verified TI RF command/patch or alternate radio | `NOT VALIDATED` | 6 | `presets.py:105-112`; SDK `rf_prop_cmd.h:475-478` |
| P18 | supported 4-FSK setup/mapping | 4-FSK mod/demod | same rejection | only after authoritative support | same | `NOT VALIDATED` | 6 | `presets.py:154-161`; SDK `rf_prop_cmd.h:475-478` |
| P19 | supported 4-GFSK setup/mapping | 4-GFSK mod/demod | same rejection | only after authoritative support | same | `NOT VALIDATED` | 6 | `presets.py:162-169`; SDK `rf_prop_cmd.h:475-478` |
| P27 | TS‑UNB telegram/pilot/header/fragments/reassembly state | coding, interleaving, splitting, schedule, reassembly | mark unsupported/remove from smoke path | dedicated PHY/scheduler or compatible stack/radio | ETSI mode/revision; RF feasibility; conformant peer/instruments | raw criterion `0/10`; not MIOTY verdict | 5 | `presets.py:229-243`; `radio_if.c:2020-2170`; historical summary `:31-76` |

## 18. Gap analysis by component

The dossiers below use the required A–H form. They cover each material missing capability; individual presets inherit the dossier identified in the register.

### 18.1 Passive and active Zigbee

- **A. Expected functionality:** passive scope decodes IEEE MAC and Zigbee NWK/APS/ZDO/ZCL while preserving security metadata; active scope additionally provides MAC timing/ACK/retry, joining, roles, keys/counters, routing, and application state.
- **B. Current behavior:** native IEEE RF commands return raw MPDUs; non-BLE LL processing passes bytes unchanged.
- **C. Evidence:** `radio_if.c:428-488,1335-1371,1497-1568`; `ll_manager.c:144-159`; no Z-Stack sources appear in `CMakeLists.txt:129-184,240-253`.
- **D. Missing functionality:** all Zigbee structures and processing named above; active IEEE MAC management is also absent.
- **E. Proposed implementation:** first add a Python passive decoder/PCAP path. For active operation, build a separate target around the SDK Z-Stack rather than adding a PROP preset.
- **F. Architectural placement:** untimed decode in Python; ACK/channel access/network/security state in CC1352 and the SDK stack.
- **G. Dependencies and risks:** Zigbee revision, role, key policy, resource budget, RF ownership, and lawful access to secured traffic. Stack presence alone is not build feasibility.
- **H. Verification:** fixture corpus plus independent sniffer for passive work; conformant coordinator/router/end-device joining, secured traffic, replay/counter and recovery tests for active work.

### 18.2 IEEE 802.15.4 prerequisites

- **A. Expected functionality:** a versioned raw-MPDU API with exact PHR/FCS ownership, channel validation, capture policy, limits, timestamps, and operation completion.
- **B. Current behavior:** RF Core generates PHR/FCS on TX; RX returns MAC bytes including FCS, drops CRC-bad frames, and disables filtering/auto-ACK.
- **C. Evidence:** `smartrf_ieee_15_4_0.c:81-163`; `radio_if.c:468-488,1335-1371,1497-1568`.
- **D. Missing functionality:** explicit data representation, bad-FCS capture option, strict channel API, structured frame builder/parser, and active MAC primitives.
- **E. Proposed implementation:** add backend-specific packet metadata and tests; only add MAC behavior required by the approved active Zigbee scope.
- **F. Architectural placement:** RF options/metadata in CC1352; structure parsing and passive analysis in Python; timing-critical MAC in CC1352/SDK.
- **G. Dependencies and risks:** TI command semantics, 125-byte current limit, transport latency, and ambiguity over whether callers currently append FCS.
- **H. Verification:** byte-for-byte captures at channels 11/26, min/max lengths, address modes, good/bad FCS, and independent analyzer comparison.

### 18.3 Proprietary 2.4 GHz

- **A. Expected functionality:** a CC1352P7-characterized 2.4 GHz proprietary RF setup with correct frequency synthesis, overrides, PA/power table, antenna route, and raw packet contract.
- **B. Current behavior:** P22/P23 mutate the sub-GHz setup; frequency application falls back to 868 MHz for a 2440 MHz request.
- **C. Evidence:** `presets.py:188-203`; `radio_if.c:1578-1617,2020-2170`; target defaults in `LP_CC1352P7_4_rf_defaults.js:112-134`.
- **D. Missing functionality:** separate RF mode/objects, selection logic, applied-config report, target power/override data, and hardware-route proof.
- **E. Proposed implementation:** generate immutable settings with the pinned tool and add an explicit 2.4 PROP backend; reject the presets until it is selected successfully.
- **F. Architectural placement:** generated commands and RF ownership in CC1352; validation/range checks and capability display in Python.
- **G. Dependencies and risks:** CatSniffer hardware revision/antenna path, RF driver coexistence, target tool version, regulatory limits, and unmeasured output.
- **H. Verification:** source/link-map selection proof, RF readback, center-frequency/spectrum/power capture, independent raw peer, and backend-transition tests.

### 18.4 Trustworthy metadata and command completion

- **A. Expected functionality:** correct RSSI/timestamp/status ordering, TI result-enum decoding, and a correlated terminal result for every asynchronous RF operation.
- **B. Current behavior:** trailer offsets are wrong; config/start/stop/TX ACKs precede execution or do not expose its result; reused-session diagnostics can be stale.
- **C. Evidence:** `radio_if.c:75-81,324-382,1706-1755`; `command_processor.c:168-223`; `control_task.c:249-259,332-358`.
- **D. Missing functionality:** typed applied/failed responses, operation IDs, guaranteed terminal event, valid metadata provenance, and malformed-frame telemetry.
- **E. Proposed implementation:** correct offsets; return status from setters; add an operation table/queue and structured completion; surface parse failures.
- **F. Architectural placement:** RF result capture in CC1352; timeout/state/error presentation in Python.
- **G. Dependencies and risks:** wire-protocol versioning and compatibility with older firmware; concurrency and sequence-wrap behavior.
- **H. Verification:** sentinel byte fixtures, injected RF failures, session-reuse cases, queue saturation, lost response/reconnect, and independent RSSI/time correlation.

### 18.5 W‑M‑Bus S/T/C

- **A. Expected functionality:** exact selected mode/direction, mode coding/sync, length/control/manufacturer/address/control-information fields, block organization/CRC, and timing.
- **B. Current behavior:** P13–P15 only modify generic GFSK frequency/rate/deviation/BW/sync; the common FeralRF length+CRC remains.
- **C. Evidence:** `presets.py:113-136`; `smartrf_prop_0.c:189-305`; local S and C/T patch sources exist but are absent from CMake.
- **D. Missing functionality:** native mode settings/patch selection, Manchester or 3-of-6 handling as applicable, C frame A/B selection, link codec, block CRC, and MAC timing.
- **E. Proposed implementation:** create immutable S and C/T backends using target-generated settings, then add a passive link-frame codec before active mode behavior.
- **F. Architectural placement:** line coding/timing and native patches in RF Core/CC1352; non-time-critical decode in Python; active link timing in CC1352.
- **G. Dependencies and risks:** EN 13757 edition, S1/S2/T1/T2/C1/C2 and A/B decisions, OMS profile, patch coexistence, conformant peers.
- **H. Verification:** golden encoded vectors, RF capture of line coding/sync, correct block CRC, independent meters/collectors, timing and error-case tests.

### 18.6 Wi‑SUN FAN and Sidewalk

- **A. Expected functionality:** for FAN, selected 15.4g PHY/channel plan plus hopping, secure MAC/join and IPv6/RPL; for Sidewalk, selected regional FSK framing, link/security/provisioning and gateway/cloud behavior.
- **B. Current behavior:** P24–P26/P28–P31 use generic unwhitened, no-FEC PROP frames; no protocol stack is initialized or linked.
- **C. Evidence:** `presets.py:205-228,244-278`; `smartrf_prop_0.c:189-305`; CMake link set; pinned `ti_wisunfan/` and `ti_sidewalk/` trees.
- **D. Missing functionality:** all protocol PHR/frame/state/security/network functions, exact regional profiles, and product provisioning.
- **E. Proposed implementation:** rename current profiles as raw candidates; if required, create separate builds based on the vendor SDK components.
- **F. Architectural placement:** vendor stacks and timing in CC1352; management tooling in Python/cloud; not in the generic setter.
- **G. Dependencies and risks:** stack/toolchain compatibility, region/version/role, licenses/qualification, credentials, gateways, flash/RAM, and RF concurrency.
- **H. Verification:** build/link/init proof followed by conformant secured FAN join/IPv6 traffic or provisioned Sidewalk gateway/cloud receipt; RF captures confirm exact selected PHY.

### 18.7 MIOTY TS‑UNB

- **A. Expected functionality:** coded/interleaved telegram splitting, pilot/header/data construction, fragment identity, time/frequency schedule, RX collection/reassembly, and selected upper-layer behavior.
- **B. Current behavior:** P27 sends one generic low-rate FSK packet through `CMD_PROP_TX`.
- **C. Evidence:** `presets.py:229-243`; shared frame code `smartrf_prop_0.c:189-305`; historical raw result 0/10 with immediate P09 recovery.
- **D. Missing functionality:** every TS‑UNB transformation/state listed in A; no native stack/config was found.
- **E. Proposed implementation:** first conduct an instrumented RF Core feasibility study; then implement a dedicated PHY/scheduler or select a suitable radio/stack. Keep P27 unsupported meanwhile.
- **F. Architectural placement:** deterministic burst timing in CC1352/RF Core; reassembly/analysis may be split with Python only where latency permits.
- **G. Dependencies and risks:** exact ETSI mode/revision, RF command timing/frequency agility, narrowband performance, regulatory duty-cycle, IP/stack availability.
- **H. Verification:** standard-derived golden vectors, instrumented burst sequence/time/frequency comparison, conformant base-station interoperability, loss/reassembly testing.

### 18.8 OOK and reserved modulation modes

- **A. Expected functionality:** safe OOK setup/use/stop/recovery; MSK/4-FSK/4-GFSK only when an authoritative RF backend represents them.
- **B. Current behavior:** OOK patch path can enter a terminal state; values 4/5/6 are accepted even though the TI field reserves them.
- **C. Evidence:** `radio_if.c:1373-1398,1912-2170`; SDK `rf_prop_cmd.h:475-478`; controlled OOK observation; `presets.py:97-112,154-169`.
- **D. Missing functionality:** durable OOK state/error model and verified recovery; all actual nonbinary modulation configurations and symbol handling.
- **E. Proposed implementation:** fail closed after OOK uncertainty; reject values above 2 now; only reintroduce modes backed by generated settings/patch documentation.
- **F. Architectural placement:** lifecycle and validation in CC1352, protective state in Python; modulation itself must be RF Core/target-supported.
- **G. Dependencies and risks:** unknown internal state cleared by power removal, bridge reset semantics, patch persistence, and possible silicon/backend limitations.
- **H. Verification:** instrumented state-transition campaign for OOK; for each other mode, authoritative setup plus symbol-level capture and independent demodulation.

### Python library

- Add typed profile metadata and an applied-capability response; stop presenting dictionary presence as support.
- Validate frequency, backend, modulation enum, packet size, channel, and state before transmitting.
- Split `accepted` from `completed`; expose asynchronous command errors with correlation.
- Normalize or annotate `Packet.data` by backend; include explicit `fcs_present`, `phy_header_present`, `timestamp_source`, and `metadata_valid` fields.
- Replace the broad parse-exception swallow with counters and diagnosable bounded records.
- Add passive IEEE 802.15.4 and Zigbee decoders; keep encrypted content opaque without authorized keys.
- Preserve API aliases only with deprecation and an explicit `conformance=NONE` marker.

### CC1352 firmware

- Correct proprietary append offsets and add layout tests.
- Return errors from configuration; reject reserved modulation and unsupported frequency/backend combinations.
- Separate immutable RF configurations for sub-GHz, 2.4 proprietary, IEEE, OOK, W‑M‑Bus modes, and any future native protocol backend.
- Check FS completion and all RF-driver terminal events; update diagnostic state on every branch.
- Replace one-slot deferred operations with correlated stateful operations or a bounded queue.
- Make OOK state persistent and fail-safe; only a proven hardware reset/power-cycle transition clears terminal state.
- Define how RF handles, patches, shared command structures, and antenna selection are owned across modes.

### RP2040 bridge

**[UNVERIFIED / UNKNOWN]** Source and exact deployed binary are absent. The repository can only infer its CDC data/shell roles from `Radio.connect()` and `reset_device()`. Before relying on reset or timeout evidence, record bridge source revision, firmware version, USB descriptors, port mapping, UART buffering/flow control, reset pin behavior, and whether `boot` removes CC1352 power or only resets logic. A CC1352 reset is not necessarily equivalent to physical RF-core power removal.

### Build system and TI SDK integration

The current CMake target compiles local RF settings and core FeralRF tasks only (`firmware/cc1352/CMakeLists.txt:129-184`) and links RF driver, TI drivers, SysBIOS, and driverlib (`CMakeLists.txt:240-253`). It does not compile Z-Stack, Wi‑SUN, Sidewalk, or W‑M‑Bus mode-specific settings/patches.

Recommended build topology:

```text
feralrf-raw          current BLE/IEEE raw/PROP service, hardened
feralrf-zigbee-pass  same firmware + host decoder (no stack in target)
feralrf-zstack       separate active Zigbee firmware/persona
feralrf-wisun        separate TI Wi-SUN firmware/persona
feralrf-sidewalk     separate qualified Sidewalk firmware/persona
feralrf-wmbus        native mode-specific RF backend + scoped link codec
```

Every image should expose a machine-readable build ID, SDK revision, feature list, supported regions/profiles, packet contract version, and reset/power-cycle semantics.

## 19. Prioritized implementation backlog

The ordering below intentionally follows the requested program order: Zigbee decision, IEEE prerequisites, proprietary 2.4, trust-blocking cross-cutting defects, protocol-specific sub-GHz work, then remaining modulation/backend issues. “Confidence” rates feasibility from current evidence, not implementation success.

| ID / class | Affected capability | Exact file/function | Current behavior / demonstrated gap | Proposed change | Technical dependency | Acceptance criteria | Required validation | Complexity | Confidence |
|---|---|---|---|---|---|---|---|---|---|
| ZB-01 / architecture decision | Zigbee | `ll_manager.c:144-159`; `CMakeLists.txt:129-184,240-253`; SDK `zstackstartup.c:239-249,398-424` | Raw IEEE bearer exists; no Zigbee code linked; active scope undefined | Approve passive-first scope and decide whether active Zigbee is a separate firmware persona | revision, roles, clusters, security/key policy, resource/concurrency budget | signed capability boundary states passive fields, active exclusions, and stack target | source/build review; no RF needed for decision | LOW | HIGH |
| ZB-02 / new protocol implementation | Passive Zigbee | new Python module; integrate at `radio.py:636-709` | `Packet.data` is not decoded | Add IEEE MAC and Zigbee NWK/APS/ZDO/ZCL parsing, opaque secured payload handling, PCAP output | ZB-01; normalized IEEE frame contract; licensed test corpus | known frames decode field-for-field; unknown/security-protected bytes preserved losslessly | unit/golden vectors + independent sniffer capture comparison | HIGH | HIGH |
| I154-01 / confirmed correction | IEEE raw contract | `radio_if.c:428-488,1335-1371,1497-1568`; `radio.py:18-30,636-709` | TX excludes FCS; RX appears to include it; bad CRC dropped; channel remap implicit | Version TX/RX contract, strict channels, FCS flags, bad-FCS option, backend limits | TI header semantics; backward compatibility | tests prove exact PHR/FCS/data boundaries and reject invalid channels | target capture at ch11/ch26, size boundaries, good/bad FCS | MEDIUM | HIGH |
| I154-02 / architecture decision | Active IEEE/Zigbee MAC | same IEEE paths plus future stack target | no CSMA/ACK/retry/association state | assign active MAC behavior to SDK stack/CC1352; keep host injection experimental | ZB-01; TI stack/resource analysis | selected stack initializes and owns timing; raw API cannot claim active MAC | link map, timing tests, conformant peer | VERY HIGH | MEDIUM |
| P24G-01 / confirmed correction + new backend | P22/P23 | `RadioIF_getPropMode` `radio_if.c:173-186`; `RadioIF_applySub1ghzChannelConfig` `:1578-1617`; setter `:2020-2170`; generated settings/CMake | 2440 request uses sub-G setup and falls back to 868 | generate/select immutable CC1352P7 2.4 PROP objects; add applied-config identity; reject until available | pinned SysConfig/SmartRF, hardware antenna revision, power table | software reports 2440/backend ID; no 868 fallback; correct objects in link map | spectrum/frequency/power/frame capture and independent peer | HIGH | HIGH |
| X-01 / confirmed source correction | All PROP profiles | `RadioIF_processPropPackets`, `radio_if.c:1706-1755`; `data_task.c:28-65`; SDK `rf_prop_cmd.h:1166-1179` | RSSI/timestamp/status offsets are wrong and `0x80` is misused as CRC-good instead of decoding the two-bit result | parse TI trailer as RSSI→timestamp→status; decode result enum; document LQI/channel provenance | pinned TI RX output contract | sentinel fixtures for result 0–3 yield exact fields and packet policy | unit fixture + independent timestamp/RSSI correlation | LOW | HIGH |
| X-02 / confirmed source correction | config/start/stop/TX | `command_processor.c:168-223`; `control_task.c:249-259,332-448`; `radio_if.c:324-382`; Python waiters | ACK precedes execution; success has no terminal event; reused diagnostics stale | add operation ID and exactly one terminal result; return errors/readback from config; refresh all diagnostic branches | transport protocol versioning; bounded queue/state design | injected success/failure/timeout each produces one correlated terminal status | unit/state-machine, fault injection, queue/reconnect tests | HIGH | HIGH |
| X-03 / confirmed source correction | catalog/API | `presets.py:11-284`; `commands.py:100-122`; `command_processor.c:168-185` | names imply protocols; invalid configs accepted and ACKed | typed profiles, capability/conformance fields, validation, applied readback; deprecate aliases | compatibility policy | unsupported modes cannot execute; every profile reports backend/status/evidence | host unit tests over entire catalog | MEDIUM | HIGH |
| X-04 / confirmed correction + hardware evidence | P11 OOK | `radio_if.c:1373-1398,1912-2170`; `main_rtos.c:153-170`; Python OOK warning | repeatable terminal pattern; software flag can imply false recovery | durable fail-safe OOK state, correlated telemetry, reject commands after uncertainty until proven power removal | TI patch/driver details; RP2040 reset/power truth | invalid transitions fail closed; only verified power event clears terminal state | instrumented transition/fault campaign; power-domain verification | HIGH | MEDIUM |
| WMB-01 / new protocol implementation | P13–P15 | presets `:113-136`; `smartrf_prop_0.c:17-305`; local W‑M‑Bus patches; CMake | generic PROP only; no mode coding/link/block CRC | select exact profiles; native S and C/T RF backends; passive codec; active timing only if scoped | EN 13757 edition/modes, OMS profile, generated settings, peer | golden frames/coding/block CRC match; independent peer decoded | vector/unit, spectrum/coding capture, S/T/C interoperability | VERY HIGH | MEDIUM-HIGH |
| WSF-01 / architecture + external dependency | P26/P28–P31 Wi‑SUN | SDK `source/ti/ti_wisunfan/`; current CMake omission | raw FSK only; actual freq 902.2; no FAN/15.4g | choose FAN version/domain/role and build a dedicated vendor-stack target; rename raw entries | compiler/library compatibility, licensing, network peer | stack linked/initialized; secured join and bidirectional IPv6 under selected profile | link map + conformant FAN network + PHY capture | VERY HIGH | MEDIUM |
| SW-01 / architecture + external dependency | P24/P25 Sidewalk | SDK target config `fsk_ti_radio_config.c:188-228,283-360`; current CMake omission | raw FSK differs from SDK frame and lacks stack/security | rename raw entries or create separately provisioned Sidewalk target | region/profile, credentials, gateway/cloud, licenses/qualification | official workflow recognizes device and receives secured traffic | build/init proof + gateway/cloud + RF capture | VERY HIGH | MEDIUM |
| MIO-01 / hardware/lab feasibility | P27 MIOTY | `presets.py:229-243`; generic PROP paths | one generic 396-symbol/s packet; prior raw 0/10 | freeze as unsupported; assess required burst schedule/waveform; choose dedicated implementation or alternate radio | ETSI mode, RF Core timing/narrowband capability, conformant peer | instrumented feasibility gate either reproduces required burst sequence or documents decisive incompatibility | golden vectors, time/frequency capture, MIOTY peer | VERY HIGH | LOW-MEDIUM |
| MOD-01 / confirmed source correction | P12/P18/P19 | `presets.py:105-112,154-169`; `RadioIF_setPropConfig`; SDK `rf_prop_cmd.h:475-478` | reserved values 4/5/6 accepted and ACKed | reject immediately and mark unsupported | none for rejection | all three return typed unsupported error; RF objects remain unchanged | host+firmware unit/target control tests | LOW | HIGH |
| MOD-02 / research | future MSK/4-FSK/4-GFSK | new backend only after evidence | no authoritative current command path | survey pinned/newer official TI configurations or alternate hardware; never reuse reserved enum values | vendor evidence and product need | authoritative setup plus symbol mapping and captured/demodulated waveform | RF instrumentation and independent decoder | HIGH–VERY HIGH | LOW |

## 20. Validation plan

This is a plan, not evidence that tests were executed.

### Stage 0 — static and host-only

1. Unit-test command serialization and range rejection, including every catalog entry.
2. Fixture-test COBS/CRC framing, malformed frames, sequence wrap, async errors, and parser diagnostics.
3. Fixture-test PROP and IEEE queue-entry layouts byte by byte, including appended metadata and FCS presence.
4. Assert that every catalog item reports backend, conformance level, and unsupported reason.
5. Scan the link map for intended protocol libraries/patch symbols in each dedicated firmware target.

### Stage 1 — firmware control contract without RF claims

1. Verify config/start/stop/TX operation IDs and exactly one terminal response.
2. Inject RF-driver failures and confirm error propagation, no stale status, and no false completion.
3. Exercise queue saturation, simultaneous async RX, sequence wrap, UART corruption, and bridge reconnect.
4. Verify OOK invalid transitions fail closed and the terminal state survives all software reset paths that do not truly power-cycle the RF domain.

### Stage 2 — instrumented PHY

For each native raw profile, measure center frequency, symbol rate, deviation, occupied bandwidth, power, antenna port, preamble, sync, length, payload, and CRC/FCS with independent equipment. Capture both first use and backend transitions. A FeralRF peer is not an independent instrument.

Special gates:

- P22/P23: verify 2440 MHz before sending more than a short test sequence.
- P11: instrument first configuration, first TX, stop, attempted mode change, and physical power removal; log RF Core/driver state at each point.
- P13–P15: decode line coding and W‑M‑Bus block CRC, not just application-byte coincidence.
- P27: compare burst timing/frequency structure with ETSI TS 103 357; a contiguous generic packet is an automatic non-conformance.

### Stage 3 — external interoperability

- IEEE/Zigbee passive: compare the same capture against an independent 802.15.4 sniffer and known Zigbee fixture corpus.
- W‑M‑Bus: use certified/known-good S, T, and C transmitters with exact mode/direction declared.
- Wi‑SUN: join a conformant FAN network and demonstrate secured bidirectional IPv6 traffic; raw payload delivery is insufficient.
- Sidewalk: use the official qualification/development workflow, provisioned device, compatible gateway, correct region, and cloud-side receipt.
- MIOTY: use a conformant base station/end point and verify all telegram fragments/reassembly.

### Stage 4 — robustness and regulatory preconditions

Run transition, power-cycle, long-duration RX, malformed-packet, packet-rate, and loss/reconnect tests. Regulatory domain, duty cycle, dwell/hopping, conducted/radiated power, spectral mask, and antenna configuration must be reviewed before sustained transmission. Protocol interoperability does not replace regulatory evidence.

## 21. Audit baseline and change isolation

**[CURRENT SOURCE CODE]** The diff from `f2744e4` to `3409955` contains only five documents under `docs/PRESETS_EVALUATION/`: `EXPERIMENTAL_EVIDENCE.md`, `IMPLEMENTATION_AND_PROTOCOL_GAPS.md`, `NEXT_WORK.md`, `PRESET_MATRIX.md`, and `README.md`. No Python, firmware, CMake, generated RF setting, preset, or experiment artifact changed between those revisions. Therefore all implementation findings in this audit describe both revisions; the newer revision only adds an evaluation narrative.

The working tree was clean before this report was added. This report is the only intended repository change from the audit.

## 22. Source register

### Current repository code

| Subject | Exact source |
|---|---|
| Complete preset catalog | `python/feralrf/presets.py:11-284` |
| PHY values and command/response IDs | `python/feralrf/enums.py:8-18,31-50,74-91` |
| Command serialization | `python/feralrf/commands.py:17-19,32-34,57-66,100-122` |
| COBS, serial CRC, transport frame | `python/feralrf/protocol.py:11-90` |
| Packet model, bridge connection/reset | `python/feralrf/radio.py:18-30,113-246` |
| Python command wait/response parsing | `python/feralrf/radio.py:263-397,455-537,599-750` |
| Firmware PHY dispatch | `firmware/cc1352/src/phy_manager.c:16-49`; `include/phy_manager.h:14-28` |
| Command ACK behavior | `firmware/cc1352/src/command_processor.c:61-73,128-223,245-258` |
| Deferred TX/start/stop | `firmware/cc1352/src/control_task.c:225-263,332-448`; `src/data_task.c:76-137` |
| UART/RF task ownership | `firmware/cc1352/src/host_if_task.c:20-110`; `src/main_rtos.c:54-77,110-178` |
| Host RX response layout | `firmware/cc1352/src/data_task.c:23-65`; `include/radio_if.h:15-28` |
| BLE-only LL parsing | `firmware/cc1352/src/ll_manager.c:40-159` |
| Generic PROP generated settings | `firmware/cc1352/src/smartrf_prop_0.c:17-305` |
| IEEE generated settings | `firmware/cc1352/src/smartrf_ieee_15_4_0.c:12-163` |
| RF session/TX engine | `firmware/cc1352/src/radio_if.c:324-382,986-1250` |
| IEEE channel/TX/RX processing | `firmware/cc1352/src/radio_if.c:428-488,1335-1371,1497-1568` |
| PROP frequency/TX/RX processing | `firmware/cc1352/src/radio_if.c:1578-1819` |
| PHY state and PROP setter/OOK | `firmware/cc1352/src/radio_if.c:1912-2170` |
| Build sources and link libraries | `firmware/cc1352/CMakeLists.txt:3-30,81-102,129-184,240-253` |

### Pinned TI SDK

| Subject | Exact source |
|---|---|
| Valid proprietary `modType` values | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_prop_cmd.h:475-478` |
| Proprietary RX result/status enum | same file, `:1166-1179` |
| PROP format/whitening fields | same file, `:518-527` |
| IEEE RX channel/filter/ACK fields | `.../driverlib/rf_ieee_cmd.h:103-145` |
| IEEE TX PHR/FCS generation fields | same file, `:266-301` |
| W‑M‑Bus S patch | `.../rf_patches/rf_patch_mce_wmbus_smode.c:38,534` |
| W‑M‑Bus C/T patch | `.../rf_patches/rf_patch_mce_wmbus_ctmode.c:38,561` |
| Target 2.4 GFSK defaults | `source/ti/prop_rf/.meta/rf_config/LP_CC1352P7_4_rf_defaults.js:112-134` |
| Common 2.4 proprietary config generator | `source/ti/prop_rf/.meta/rf_config/prop_rf_config_common.js:144-185` |
| Sidewalk target FSK setup/frame | `source/ti/ti_sidewalk/apps/ti/sid_demo/app_source/cc1352p7/sysconfig/fsk_ti_radio_config.c:188-228,283-360` |
| Zigbee stack startup | `source/ti/zstack/startup/zstackstartup.c:239-249,398-424` |
| Wi‑SUN stack tree | `source/ti/ti_wisunfan/` (MAC/FH, security, applications, uIP/RPL, target libraries) |

### Historical evidence retained in the workspace

| Subject | Source |
|---|---|
| Inventory and backend findings | `../Understanding-CatSniffer/06 - Experimentos/00 - Consolidación y evaluación experimental FeralRF/09 - Preset Inventory and Historical Validation Audit/01 - Existing Preset Inventory.md:23-133` |
| Experiment chronology and evidence quality | sibling file `02 - Historical Experimental Evidence Mapping.md:13-24` |
| Per-preset validation matrix | sibling file `03 - Preset Validation Coverage Matrix.md` |
| Per-preset implementation/protocol register | sibling file `07 - Per-Preset Implementation and Protocol Gap Register.md` |
| Controlled P09/P27/P11/power-cycle campaign | sibling `validation_runs/2026-10-26_unresolved_presets_114802/summary.md:11-169` |
| Phase 2 standards/feasibility assessment | `../Understanding-CatSniffer/06 - Experimentos/00 - Consolidación y evaluación experimental FeralRF/10 - Standards-Based Preset and PHY Assessment/01 - Corrected Preset Technical Taxonomy.md` through `09 - Phase 2 Source Register.md` |

Historical documents are secondary evidence: they index previous runs, some of which lack raw transcripts or independent instruments. Where the latest controlled run contradicts an older summary, this audit uses the latest controlled result and preserves the conflict rather than averaging claims.

### Standards and authoritative documentation

- IEEE 802.15.4 command semantics are grounded in the pinned TI header above; access to the normative IEEE standard is still required for a formal conformance matrix.
- Connectivity Standards Alliance, [Zigbee Specification (public 2023 copy)](https://csa-iot.org/wp-content/uploads/2023/04/05-3474-23-csg-zigbee-specification-compressed.pdf) and [current specification download page](https://csa-iot.org/developer-resource/specifications-download-request/). The exact target revision remains a product decision.
- TI, [Proprietary RF packet format](https://software-dl.ti.com/simplelink/esd/simplelink_cc13x2_26x2_sdk/3.10.00.53/exports/docs/proprietary-rf/proprietary-rf-users-guide/proprietary-rf/packet-format.html) and [PHY configuration/patch guidance](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/latest/exports/docs/proprietary-rf/proprietary-rf-users-guide/rf-core/phy-configuration.html).
- TI, [CC1352P7 product page](https://www.ti.com/product/CC1352P7), for device band/capability scope; target-generated settings are still required.
- OMS Group, [OMS Specification Vol. 1, issue 2.4.1](https://oms-group.org/wp-content/uploads/2024/05/OMS-Spec_Vol1_General_v241.pdf) and [Vol. 2, issue 5.0.1](https://oms-group.org/wp-content/uploads/2024/10/OMS-Spec_Vol2_Primary_v501_01.pdf), for W‑M‑Bus/OMS context. Normative EN 13757-4 access and exact edition/profile selection remain required.
- ETSI, [TS 103 357 V2.1.1](https://www.etsi.org/deliver/etsi_ts/103300_103399/10335702/02.01.01_60/ts_10335702v020101p.pdf), and MIOTY Alliance/Fraunhofer, [MIOTY physical-layer technology overview](https://developers.mioty-alliance.com/wp-content/uploads/2023/10/Mioty-PhysicalLayerTechnology_rev1_final.pdf).
- Wi‑SUN Alliance, [FAN technical overview](https://wi-sun.org/wp-content/uploads/Wi-SUN-FAN-Tech-Overview-r0.pdf) and [PHY PICS](https://wi-sun.org/wp-content/uploads/Case-0369_PHY-PICS.pdf). The project must select the applicable current FAN revision and regional profile.
- AWS, [How Amazon Sidewalk works](https://docs.aws.amazon.com/iot-wireless/latest/developerguide/iot-sidewalk.html), plus the pinned TI Sidewalk SDK sources for the target-specific implementation.

## A. Confirmed implementation work required

1. Correct proprietary RX metadata order and decode TI's two-bit result enum; prove both with byte-exact fixtures.
2. Replace acceptance-only ACK semantics with correlated terminal completion for configure, start, stop, and TX.
3. Reject reserved modulation values 4/5/6 and every frequency/backend combination the active RF setup cannot implement.
4. Disable P22/P23 until a target-generated proprietary 2.4 GHz backend replaces the silent 868 MHz fallback.
5. Make OOK transitions fail-safe and persistent; after uncertainty, require declared physical power removal rather than clearing a software flag.
6. Specify and test TX/RX byte ownership for proprietary and IEEE backends, including length, CRC/FCS, PHY header, maximum size, and bad-CRC policy.
7. Reclassify W‑M‑Bus, Wi‑SUN, Sidewalk, and MIOTY entries as raw/unsupported candidates unless and until their native framing and stacks are integrated.
8. Add applied-configuration/capability readback, RF setup identity, and driver-event diagnostics.
9. Preserve malformed serial/RX diagnostic evidence instead of silently discarding all parser exceptions.
10. Implement passive Zigbee decoding over the native IEEE backend before any full active-stack work.

## B. Decisions and evidence still required

1. Which Zigbee specification revision, passive/active role, device types, security/key policy, and application clusters are in scope?
2. Should active Zigbee, Wi‑SUN, and Sidewalk be separate firmware images/personas, or is concurrent stack operation a funded DMM requirement?
3. Which exact Wi‑SUN FAN revision, regional domain, operating-mode IDs, channel plan, and device role are required, and can the current GCC build consume the pinned SDK libraries?
4. Which Sidewalk region/profile is required, and are provisioning, credentials, gateway/cloud access, licensing, and qualification resources available?
5. Which EN 13757-4 edition, W‑M‑Bus directions (S1/S2, T1/T2, C1/C2), C frame format, and OMS application profile are required?
6. Is MIOTY a product requirement strong enough to justify a dedicated PHY/scheduler or different radio? Instrumented feasibility is still unknown.
7. Are MSK/4-FSK/4-GFSK required use cases, and what authoritative CC1352P7 RF setup demonstrates them? Current enum values do not.
8. What exact RP2040 bridge source/binary is deployed, and does its reset command remove power from the CC1352/RF domain or merely reset logic?
9. What are the measured center frequency, rate, deviation, bandwidth, power, antenna route, and on-air frame for every retained raw profile?
10. Can the OOK failure be localized with RF-driver/RF-Core telemetry, and is any recovery short of physical power removal reproducible and safe?



