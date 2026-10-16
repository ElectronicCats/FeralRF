# FeralRF Sub-GHz Firmware and Software Provenance Audit

Audit date: 2026-10-15 (America/Mexico_City)

Scope: read-only reconstruction of the software, firmware, helper, and hardware provenance associated with the historical F29 70/70 claim, the later 0/70 and accumulated 0/240 campaigns, and recovery Checkpoints 3A through 4A. No serial port was opened, no hardware command was sent, no RF test was run, and no firmware was built or flashed during this audit.

Primary evidence:

- `SUBGHZ_RECOVERY_AUDIT.md`
- `SUBGHZ_RECOVERY_RUN.md`
- `experimental_artifacts/checkpoint_3a` through `checkpoint_4a`
- `docs/EXPERIMENT_REPORT_2026-10-12/`
- FeralRF Git history and current source tree
- local CatSniffer-Firmware Git history, inspected read-only for RP2040 provenance

## 1. Executive provenance conclusion

The current successful environment can be mapped exactly to the **host source tree and experimental helper files**, but it cannot be mapped exactly end-to-end to the installed RP2040 and CC1352 binaries.

The strongest current mapping is:

- **VERIFIED host repository state:** branch `feature/hardware-validation`, HEAD `ba27fbeb8c5683a31cca4339982cadeaaa406444`; tracked tree clean at recovery preflight and at this audit. The audit/run records and `experimental_artifacts/` are untracked additions.
- **VERIFIED host code selection:** the recovery helpers prepend this checkout's `python/` directory to `sys.path`, so Checkpoints 3A through 4A used repository Python source directly rather than resolving the separately installed editable package by normal import precedence.
- **VERIFIED helper identity:** the recovery helpers exist and have recorded SHA256 hashes below.
- **SUPPORTED current interpreter identity:** CPython 3.14 was used (`checkpoint_3a_single_run.cpython-314.pyc` exists); the interpreter currently resolves as Python 3.14.7. The exact patch version at every checkpoint was not printed in the raw logs and is therefore not independently VERIFIED for execution time.
- **VERIFIED current source package identity:** FeralRF Python `0.3.0`; current editable installation also points to this repository's `python/` directory. Current dependency inspection found `pyserial 3.5`, `pyserial-asyncio 0.6`, and `cobs 1.2.2`, but no checkpoint-local dependency freeze proves those exact installations were unchanged for every run.
- **SUPPORTED RP runtime identity only:** both recovery units reported `FW: v3.1.0.1`. Recovery did not capture `fw_version` output containing Git SHA, dirty flag, build time, or compiler, and no installed UF2 readback/hash exists. Association with CatSniffer-Firmware tag `v3.1.0.1` / commit `c0cd5a45...` is plausible but **INFERRED, not proven**.
- **SUPPORTED CC runtime identity only:** both endpoints returned `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')`; Shell status displayed `feralrf_cc1352 (custom)`. These values are generic and contain no build SHA. No matching installed ELF/BIN/HEX digest or readback exists.
- **VERIFIED source build baseline, not installed-binary identity:** the current CC source defaults to CC1352P7, TI-RTOS7, GCC, TI SimpleLink SDK 8.30.01.01 at submodule commit `5b31d0a4903351e544546e23ef3330eaa4291ceb`, generated Radio Config module 1.20.0, and SmartRF Studio data 2.32.0. There is no evidence that the installed images were produced from this exact configuration.

The historical claimed F29 70/70 environment cannot be mapped exactly. Its closing helper and source tree are recoverable, but its Python interpreter/dependencies, RP image, CC image, build configuration, flashed artifact hashes, exact physical identities, and raw execution transcript are not.

The 0/70 and 0/240 environments have stronger host and RP self-identification than F29: FeralRF `0178721c...`, Python 3.14.7, editable package 0.3.0, SDK source identity, and RP runtime Git/build strings were recorded. They still lack exact CC binary identity and binary hashes for either MCU. Consequently they also cannot be mapped exactly end-to-end.

No specific software regression commit is supported as the cause of the PASS/FAIL contradiction. The strongest remaining provenance gap is the absent CC1352 binary/build identity, followed by the missing exact current RP2040 binary identity and missing F29 installed-image identities.

## 2. Evidence and confidence rules

| Term | Meaning in this audit |
|---|---|
| **VERIFIED** | Direct repository, file, hash, runtime transcript, or Git-object evidence establishes the exact claim. |
| **SUPPORTED** | Multiple direct observations support the claim, but at least one identity link is absent. |
| **INFERRED** | The claim is a reasonable interpretation, not directly demonstrated. |
| **UNKNOWN** | Available evidence does not establish the field. |
| **CONTRADICTORY** | Available sources assert incompatible identities or configurations. |

The following identities are kept separate:

```text
source commit
    -> build configuration/toolchain
    -> ELF/BIN/HEX/UF2 artifact
    -> artifact digest
    -> flashed device
    -> runtime identity
```

A matching version string, tag, source checkout, or generic `DeviceInfo` value does not bridge missing arrows in this chain.

## 3. Campaign provenance matrix

### 3.1 Campaign, host, and procedure

| Campaign/checkpoint | Date | Physical units and direction | Host source | Python/helper identity | Reset / band / preceding state | Result | Confidence and main unresolved fields |
|---|---|---|---|---|---|---|---|
| Historical F29 claimed 70/70 | 2026-05-03 | TX `/dev/ttyACM1` -> RX `/dev/ttyACM2`; exact boards UNKNOWN | Closing source commit `f5863c3288439a04732488b30c0d4ca32528d6a1`; branch at execution UNKNOWN; package `0.2.0` | `python/examples/smoke_f29_subg_915.py`; Git blob `275488a4e9a69ba5d913a60931da18466545a7f1` | Intended reset of both before each preset using Bridge suffix `+2`; exceptions swallowed; no band command; fresh objects per preset; one whole-preset retry after any exception | Documented 70/70; raw execution unavailable | Source/procedure **VERIFIED**; result is a documented claim; interpreter, dependencies, actual reset success, physical units, RP/CC binaries UNKNOWN |
| Later named-preset 0/70 | 2026-10-08 | report labels board1 COM33 -> board2 COM88; exact equivalence to recovery unit numbering is SUPPORTED by PnP group continuity but naming differs | `main`, `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`; package `0.3.0`; Python 3.14.7; editable checkout | Embedded `Invoke-FeralPresetOta` in committed procedure/report; no standalone helper file/hash. Procedure document SHA256 `4743AD0D...F90DB0`; extended report SHA256 `B7E7C32A...5A135` | No CC reset; explicit logical Sub-GHz transition before group; fresh objects per invocation; RX start -> 3 s negative read -> 10 TX -> 5 s positive read; no automatic retry | 0/70 delivered | Host/procedure **SUPPORTED to VERIFIED**; embedded helper's separate artifact identity UNKNOWN; CC binary and physical RF state UNKNOWN |
| Accumulated proprietary 0/240 | 2026-10-08 record | COM33/COM88 in multiple directions/runs; 19 presets including Sub-GHz and proprietary 2.4 GHz | Same `0178721c...`, Python/package baseline as 0/70 | Same embedded helper plus separately documented concurrent controls; no single standalone campaign executable/hash | No auto-reset; logical band commands by frequency group; mixed preceding preset/PHY history; 3 s/5 s helper for most runs | 240 TX returns, 0 delivered packets across 24 runs | Direct report artifact **SUPPORTED**; full per-process dependency freeze, CC binary, physical switch state UNKNOWN |
| 3A | 2026-10-15 | recovery CatSniffer #2 COM33 TX -> #3 COM88 RX | `feature/hardware-validation`, `ba27fbeb...`; package source 0.3.0 loaded from checkout | Base helper SHA256 `485E3989...74C842`; reset/band transcript separate | Fresh verified-control-path reset both; `band2` both; fresh objects; current 0.3 s/1 s/2 s procedure | 10/10 | Host/helper/result **VERIFIED**; installed RP/CC binaries UNKNOWN |
| 3A-R | 2026-10-15 | same | same | wrapper SHA256 `FEF8AA47...A9C07`, loading base helper above; shell helper SHA256 `5B6A33A3...B42F` | Same as 3A | 10/10 | Same boundary as 3A |
| 3C | 2026-10-15 | same | same | wrapper SHA256 `8998D687...A9C19`, loading base helper; band helper SHA256 `9EAE8903...923C8` | No reset; `band2` both; inherited successful warm state | 10/10 | Host/helper/result **VERIFIED**; installed binaries UNKNOWN |
| 3D | 2026-10-15 | same | same | wrapper SHA256 `D569F93F...EA22`; band-control helper SHA256 `F8C6CB1E...68CE2`; loads base helper | No reset; explicit `band1`, both report Band 0 | 10/10 | Host/helper/result **VERIFIED**; electrical U2 state UNKNOWN |
| 3E | 2026-10-15 | same | same | legacy helper SHA256 `AF11BAAB...1AF1C`; prestatus helper SHA256 `9B3817A4...F38ED` | No reset or band command; inherited Band 0; fresh objects; 3 s negative/5 s positive reads | 10/10 | Host/helper/result **VERIFIED**; installed binaries UNKNOWN |
| 4A | 2026-10-15 | same; explicit USB identities `E661A89753565932` and `E661A89753656A35` | same | wrapper SHA256 `242E2FCD...AA6D`; initial-status helper SHA256 `7495551E...D52`; loads base helper | Both units unpowered about 15 s; no reset/band after reconnect; untouched Band 0 | 10/10 | Power boundary, host/helper/result **VERIFIED**; installed binaries still UNKNOWN |

The supplementary post-4A manual 1/1 packet is supporting confirmation only and is not included in the formal 60/60 aggregate.

### 3.2 Firmware/build identity by campaign

| Campaign | RP runtime evidence | RP source/binary identity | CC runtime evidence | CC source/binary identity | SDK / generated RF identity | Preset identity |
|---|---|---|---|---|---|---|
| F29 70/70 | UNKNOWN | Planning references are inconsistent; no RP artifact/hash | Planning mentions firmware at `04caba9` while another specification says `a0e3e07` or newer; no independent runtime transcript | UNKNOWN binary/hash; F29 tree's proprietary implementation is recoverable but not tied to devices | F29 source tree pins SDK submodule `5b31d0a...` (8.30.01.01); installed build use UNKNOWN | Seven dictionaries at F29 source are recoverable; no runtime config dump |
| 0/70 | COM33: v3.1.0.0, Git `8eaa84c`, clean, built 2026-02-28. COM88: v3.1.0.1, Git `c0cd5a4`, clean, built 2026-09-04 | Runtime self-identification **SUPPORTED** against local tags; installed UF2/hash absent | `1.0.0`, caps 7, logical serial `FERALRF1`; generic custom label | Exact CC source/binary/hash UNKNOWN | Host source pins SDK 8.30.01.01 / `5b31d0a...`; installed build relation UNKNOWN | Current source dictionaries, including exact 0/70 seven; runtime application ACKed but effective RF not independently read back |
| 0/240 | Same mixed RP builds | Same boundary | Same generic DeviceInfo | Same boundary | Same boundary | Nineteen source presets documented; exact on-device RF realization UNKNOWN |
| 3A–4A | both devices report v3.1.0.1 only | Tag `v3.1.0.1` points to `c0cd5a4`, but recovery did not capture embedded Git/build fields; source-to-installed relation **INFERRED**, binary hash UNKNOWN | `1.0.0`, caps 7, logical serial `FERALRF1`; generic custom label | Exact source commit and binary hash UNKNOWN | Current source pins SDK 8.30.01.01 / `5b31d0a...`; generated source identities below; installed relation UNKNOWN | Exact host preset dictionary VERIFIED; peer delivery proves compatible endpoint behavior, not exact installed source provenance |

Known source constants common to the later failure and recovery host trees are `wisun_915_fsk_50k = {902200000, mod_type 0, 50000, 100, 0x52, 0x930B51DE}`. F29 and current source contain materially the same seven F29 dictionaries. This does not prove that any installed CC image executed those source files.

## 4. Host/Python provenance

### 4.1 Current successful environment

- Repository: `https://github.com/ElectronicCats/FeralRF.git`
- Branch/HEAD: `feature/hardware-validation` / `ba27fbeb8c5683a31cca4339982cadeaaa406444`
- Tracked state: clean. Untracked: the two recovery Markdown records and `experimental_artifacts/`.
- Package version in `python/feralrf/__init__.py` and `python/pyproject.toml`: `0.3.0`.
- Installed distribution: editable project at this checkout's `python/` directory.
- Current interpreter: Python 3.14.7.
- Current relevant dependencies: pyserial 3.5, pyserial-asyncio 0.6, cobs 1.2.2. This is a present-time inventory, not a checkpoint-local lockfile.
- Recovery import method: every OTA helper explicitly inserts `<repo>/python` at index zero of `sys.path`. Therefore the checked-out Python files, not an unrelated wheel, are the directly supported source.

Current key source SHA256 values:

| File | SHA256 |
|---|---|
| `python/feralrf/radio.py` | `D6E92A96571C3CC98F998B5761F0652A10753B5AD520370B23FD29E6B8C7219A` |
| `python/feralrf/commands.py` | `DE56EB5CF465BD0C8D4B3709487518E835B268737F3980FC2DB724764C566E67` |
| `python/feralrf/protocol.py` | `42C248DDC0F31286D418A8F1DC67DCD24ED3F8507DBF74508DC5A7C0EFC0FCD5` |
| `python/feralrf/presets.py` | `9B4C9A0D478594DEC4C4F37D0FE214639CFAD0870F8BD186855AB5477A3A943C` |
| `python/feralrf/__init__.py` | `6DF433453814D814C1378BEC30EF48450C0714447ACC765ED4834A9CA6831B2A` |

### 4.2 API implementation ownership

All listed high-level operations are methods of `Radio` in `python/feralrf/radio.py`:

| Operation | Current implementation |
|---|---|
| connect/disconnect | `Radio.connect()` line 158; `Radio.disconnect()` line 248 |
| init / DeviceInfo | `Radio.init()` line 399 |
| set PHY | `Radio.set_phy()` line 455; payload in `CommandBuilder.set_phy()` |
| proprietary config | `Radio.configure_prop()` line 490; serialization in `CommandBuilder.set_prop_config()` |
| power | `Radio.set_power()` line 480; payload in `CommandBuilder.set_power()` |
| RX start/stop | `Radio.start_rx()` line 599; `Radio.stop_rx()` line 609 |
| packet stream | `Radio.read_packets()` line 636 |
| TX | `Radio.transmit()` line 725; payload in `CommandBuilder.tx_raw()` |

### 4.3 Historical comparison

- F29 source package reports `0.2.0`; the later failure and current recovery source report `0.3.0`.
- F29's helper blob is identical at current HEAD. The helper procedure itself did not drift after `f5863c32`.
- Between F29 and `0178721c`, `radio.py` changed substantially in file size because BLE connection/GATT APIs were removed and `read_one_packet()` was added. Static diff does not show a material change to the proprietary `set_phy` / `configure_prop` / `start_rx` / `transmit` contract capable by itself of explaining 70/70 versus 0/70.
- Exact Python dependencies and interpreter for F29 are UNKNOWN.
- The 0/70/0/240 report directly records Python 3.14.7 and an editable 0.3.0 checkout at `0178721c`.

## 5. Experiment-helper provenance

### Historical F29

`smoke_f29_subg_915.py` at `f5863c32` is exactly identified by Git blob `275488a4e9a69ba5d913a60931da18466545a7f1`. It remains the same blob at current HEAD.

Per preset it:

1. derived Shell as the Bridge numeric suffix plus two;
2. sent `boot`, waited 0.5 s, sent `exit`, waited 0.3 s, then waited 3.5 s;
3. read no reset response and swallowed all reset exceptions;
4. created fresh TX/RX `Radio` objects;
5. initialized, selected proprietary PHY, applied the preset, set 0 dBm, and started RX;
6. waited 0.3 s, sent ten markers at 0.1 s spacing, waited 1 s, and read for 2 s;
7. counted CRC-good marker packets;
8. retried the entire preset once after any exception;
9. issued no `band1`/`band2` command.

The exact script is VERIFIED. Its exact execution, reset success, binary identities, and claimed 70 received packets are not independently recoverable.

### Later 0/70 and 0/240

The principal helper was an inline PowerShell/Python function preserved in the committed procedure and reports. It used explicit Bridge ports, fresh `Radio` objects, no CC reset, a 3 s negative window, ten TX calls at 0.1 s spacing, and a 5 s positive window. Logical band changes were performed separately. Cleanup exceptions were swallowed.

The helper text is recoverable inside the documents, but no standalone timestamped helper artifact was preserved. Therefore its document container hashes are exact; a separate executed-helper SHA256 is UNKNOWN. Concurrent RX/TX controls were also documented and returned zero, so delayed host reading was not the only negative procedure used.

### Recovery 3A–4A

The current procedure is preserved as a one-run helper with explicit COM33/COM88, one preset, exactly ten TX calls, no retry, asynchronous error collection, statistics, and explicit cleanup. 3A-R, 3C, 3D, and 4A are thin wrappers that load the exact 3A file; 3E is an independent faithful legacy-window implementation. Hashes are recorded in Section 3. No production Python or firmware source was modified.

## 6. RP2040 / Cat-Bridge provenance

### Recoverable source history

The local CatSniffer-Firmware repository is on clean branch `v3.x` at `c0cd5a45e019dbd14ed11d039aacb13f300e5731`.

- Tag `v3.1.0.0` points to `8eaa84c0b7db1f51fd3c39d39b040cbec58c88a7`.
- Tag `v3.1.0.1` points to `c0cd5a45e019dbd14ed11d039aacb13f300e5731`.
- Between these commits, the RP2040 subtree changes only `RP2040/catsniffer/src/shell_commands.c`: `fw_version` adds a `Board: v3 RP2040 CC1352P7` output line. Band switching, reset, UART bridge, USB interface layout, and loss-counter behavior do not change in this tag-to-tag diff.
- The source embeds Git SHA, dirty flag, UTC build time, compiler ID, and compiler version in `fw_version` when built through its CMake path.

### Runtime mapping

The failed campaign captured stronger RP provenance:

- COM33 self-reported v3.1.0.0, Git `8eaa84c`, clean, built 2026-02-28.
- COM88 self-reported v3.1.0.1, Git `c0cd5a4`, clean, built 2026-09-04.

This is **SUPPORTED installed-runtime-to-source identification**, but no exact UF2/hash or flash/readback attestation exists.

Recovery captured only `FW: v3.1.0.1` on both units through `status`. Because it did not capture the embedded Git/build fields, mapping both installed recovery images to `c0cd5a4` is **INFERRED**. No local RP2040 UF2 artifact matching either installed image was found.

The demonstrable RP version difference between failed and successful campaigns is therefore real for COM33, but the only source difference between the tagged RP versions is an informational status line. It is not a credible code-level RF or UART regression candidate on present evidence.

## 7. CC1352 / FeralRF firmware provenance

### Current source build definition

- Default device: `CC1352P7` / `DeviceFamily_CC13X2X7`.
- RTOS: TI-RTOS7 enabled by default.
- Toolchain requested: `arm-none-eabi-gcc`; flags include Cortex-M4F hard-float and `-O0`.
- Exact compiler version on the machine that built the installed firmware: UNKNOWN.
- SDK: SimpleLink Low Power F2 SDK 8.30.01.01, Git submodule `5b31d0a4903351e544546e23ef3330eaa4291ceb` in F29, failure, and current source trees.
- Generated driver target: `LP_CC1352P7_4`.
- Generated radio target: `CC1352P7RGZ`.
- Radio Config module: 1.20.0.
- SmartRF Studio data: 2.32.0.
- Standard generated RF design identifies `LP_CC1352P7-1`; the separate 433 generated configuration identifies `LP_CC1352P7-4`.
- No `.elf`, FeralRF `.bin`, FeralRF `.hex`, `.map`, build manifest, or matching release artifact was found in the FeralRF working tree.

Current critical source SHA256 values:

| File | SHA256 |
|---|---|
| `firmware/cc1352/src/radio_if.c` | `036B8D6EFC305C703A08F28B64573A6106A20B74A9A62DE8E19155DDBE414AE2` |
| `firmware/cc1352/src/control_task.c` | `F95113B4BEF4204792F72B001C2ED2408A2332F44F38D88A617D1FCA3E0D225D` |
| `firmware/cc1352/src/command_processor.c` | `806F20FEEFB667B18E5590B1BC5F4AD919C102C1B2C76FFEE229FBFD714F4AEF` |
| `firmware/cc1352/syscfg/ti_drivers_config.c` | `7C3DF7DB84D92022C1C6E47FC9A00918FC440DE2DBDFE1DB0811B41BD4B2B264` |
| `firmware/cc1352/syscfg/ti_radio_config_433.c` | `A805FA9BAD9F1FAC06AE2595A1EB164313DFBD2885E51D46008E708ED49BE391` |
| `firmware/cc1352/CMakeLists.txt` | `53537858102A0FF7216CC1BA46C911AB2EA71E472826EA6B66317A71FB442DBD` |

### Relevant implementation history

The persistent non-433 RF-handle change `b6814b8a` predates F29. Git blame and diff show the current proprietary path is principally inherited from pre-F29 commits:

- `RadioIF_isSub1ghzPhySelected()` and frequency selection: `b27f6b38` / `a9419952`.
- `RadioIF_setPropConfig()`, modulation/rate/deviation/BW/sync updates, and band overrides: `a9419952`, `bd0cca5b`, `2cec6ed6`, and related early-April fixes.
- TX setup/FS/TX handling: early-April commits, with the persistent non-433 mode switch from `b6814b8a`.
- RX FS/post path: early-April implementation.
- Generated `ti_drivers_config.c`, `smartrf_prop_0.c`, and `ti_radio_config_433.c` have no relevant differences between F29 closing source and current HEAD.

Post-F29 changes to `radio_if.c` are primarily BLE legacy advertising and `e910fd52`'s jam-session reuse of the shared handle. Commit `785a0937` removes BLE stack code. Static diff does not alter `RadioIF_setPropConfig()`, `RadioIF_isSub1ghzPhySelected()`, the standard proprietary frequency/LO-divider calculation, the proprietary TX/RX commands, or the generated proprietary RF structures.

The CMake path changed at `e02d1277` from a machine-specific full-SDK path to a configurable/default SDK path, and `785a0937` removed BLE application sources. Those can change a produced binary's layout and linked composition, but no installed artifact or symptom ties either build change to proprietary delivery. They are provenance differences, not demonstrated regressions.

### Installed runtime

All later failure and recovery devices reported the same generic CC values: firmware 1.0.0, capabilities 7, logical serial `FERALRF1`. Source defines those generic fields and does not embed a Git SHA. The values cannot distinguish commits or binaries. Exact installed CC source commit, compiler, CMake cache, linked library digests, generated-file set, ELF, binary hash, and flash history are UNKNOWN.

## 8. Hardware and assembly provenance

| Item | Status | Evidence and limitation |
|---|---|---|
| Recovery USB identity and Bridge/Shell pairing | **VERIFIED** | #2 `E661A89753565932`: COM33 MI_00 / COM35 MI_04. #3 `E661A89753656A35`: COM88 MI_00 / COM87 MI_04. |
| Failed-campaign PnP grouping | **SUPPORTED** | COM33/35 grouped by `6&1E70E7AF`; COM88/87 by `6&106D987F`. These fragments recur in recovery, but campaign board numbering changed. |
| Physical units are CatSniffer V3 with RP2040 and CC1352P7 | **SUPPORTED** | Experiment records and RP firmware identification say V3/CC1352P7; no component-marking inspection is preserved here. |
| #2 and #3 are identical assemblies/revisions | **UNKNOWN** | Matching interfaces and runtime strings do not establish identical BOM, PCB revision, matching network, or rework. |
| Repository PCB identity | **VERIFIED as source only** | `hardware/CatSniffer.kicad_pcb` title block says Cat-Stick revision v3.2. |
| Repository schematic identity | **VERIFIED as source only** | `hardware/CatSniffer.kicad_sch` title block says revision 2.0. |
| Schematic device versus observed/device target | **CONTRADICTORY** | Schematic symbol/value names CC1352P1F3RGZT; experiments and CC build target identify CC1352P7. |
| Installed boards equal repository PCB/schematic | **UNKNOWN** | No assembly serial/BOM/revision correlation exists. |
| External U2 control topology | **SUPPORTED for repository design** | PCB assigns RP GPIO8/9/10 to CTF1/2/3 and leaves CC DIO28/29/30 unconnected. Installed electrical equivalence is unverified. |
| Physical switch state during campaigns | **UNKNOWN** | No GPIO/CTF/U2 measurement. Successful peer delivery proves a usable path existed during recovery, not which U2 throw or loss. |

## 9. PASS-versus-FAIL provenance differences

### Identical or materially equivalent fields

- Same nominal physical Bridge direction COM33 -> COM88 for the 0/70 subset and recovery.
- Same host-requested seven F29 preset dictionaries; the exact `wisun_915_fsk_50k` dictionary is unchanged.
- Same generic CC runtime values: 1.0.0, capabilities 7, `FERALRF1`.
- Same repository SDK submodule identity in F29, failure, and current source trees.
- Same material proprietary CC source path between F29 and current audited source.
- The legacy 3 s negative / 5 s positive host ordering used during failures succeeded 10/10 in 3E.

### Demonstrably different fields

- F29 host source/package: `f5863c32`, package 0.2.0. Failure: `0178721c`, package 0.3.0. Recovery: `ba27fbeb`, package 0.3.0.
- F29 helper reset both endpoints before every preset, derived Shell with `+2`, swallowed errors, and allowed one automatic retry; failure helper did not reset; recovery controlled resets explicitly and later proved success without reset.
- Failure COM33 RP reported v3.1.0.0 while recovery COM33 reported v3.1.0.1. The tagged RP source behavioral diff is only extra identity output.
- Failure campaign used a mixed multi-preset/multi-direction sequence; recovery used isolated one-preset checkpoints.
- Recovery preserved exact helper files and detailed raw logs. F29 did not.

### Materially unknown fields

- Exact CC binary for every campaign.
- Exact RP binary/hash for F29 and recovery; exact artifact hashes for failure.
- Whether the same physical assemblies and antennas were used historically.
- Exact F29 Python interpreter/dependencies.
- Historical/current flash history and build configurations.
- Physical U2 state and RF path loss.

### Procedural differences already controlled by 3A–4A

- Fresh reset is not required for every current success: 3C–4A pass without it.
- Reported Band 1 is not required for the current close-range link: 3D–4A pass at Band 0.
- Legacy delayed reading alone does not cause failure: 3E passes.
- Accumulated volatile warm state alone does not cause success: 4A passes after a full power cycle.

## 10. Candidate regression analysis

### Candidate 1: post-F29 CC proprietary implementation regression

- **Candidate commits:** none established.
- **Paths/functions checked:** `radio_if.c` proprietary lifecycle, `RadioIF_setPropConfig()`, `RadioIF_isSub1ghzPhySelected()`, frequency/LO divider, TX/RX/FS operations, PA selection, `control_task.c`, `command_processor.c`, SysConfig and SmartRF generated sources.
- **Finding:** the material proprietary implementation predates F29 and is unchanged in the later failure source. Post-F29 `radio_if.c` changes concern BLE advertising and jam handle reuse; generated proprietary sources are unchanged.
- **For:** installed binaries could still have been built differently or from another source.
- **Against:** no source-level proprietary change brackets the claimed pass and later fail.
- **Confidence:** **HIGH** that Git history does not identify a specific proprietary regression; **UNKNOWN** for untracked installed-image drift.

### Candidate 2: RP v3.1.0.0 versus v3.1.0.1

- **Commits:** `8eaa84c0` -> `c0cd5a45`.
- **Path/function:** `RP2040/catsniffer/src/shell_commands.c`, `cmd_fw_version()`.
- **Exact change:** adds a Board identification line to `fw_version`; no band, reset, UART bridge, USB, or GPIO behavior changes in the RP subtree.
- **For:** failed and successful campaigns report a different COM33 runtime version.
- **Against:** the source diff cannot affect OTA delivery; COM88 already used v3.1.0.1 during failure; 0/240 included both directions.
- **Confidence:** **VERY LOW** as a behavioral regression candidate.

### Candidate 3: host package 0.2.0 versus 0.3.0

- **Paths:** `python/feralrf/radio.py`, command/protocol modules.
- **Change:** substantial BLE API removal and addition of `read_one_packet`; no identified change in the relevant proprietary configuration/TX/RX contract.
- **For:** version and commit differ.
- **Against:** both current and legacy read procedures pass under 0.3.0; exact preset serialization is stable.
- **Confidence:** **LOW** without a more specific code change.

### Candidate 4: CC build/link composition

- **Commits:** `e02d1277` CMake SDK-path portability and `785a0937` BLE-stack removal.
- **Possible effect:** different linked libraries or memory layout can change a binary in principle.
- **For:** installed binaries are unidentified; build configuration was not archived.
- **Against:** no direct proprietary behavioral change or artifact correlation.
- **Confidence:** **LOW as a named regression**, but **HIGH as an unresolved provenance class**.

Conclusion: provenance currently supports **no specific regression hypothesis**. The scientifically defensible hypothesis is broader: one or more installed binary/build, sequence-state, physical-path, or environmental conditions differed, but which one is not established.

## 11. Current installed-binary traceability

### RP2040 recovery images

```text
CatSniffer-Firmware source/tag c0cd5a4 / v3.1.0.1     SUPPORTED candidate
    -> exact CMake/Zephyr/toolchain configuration       UNKNOWN
    -> exact UF2                                        MISSING
    -> SHA256                                           MISSING
    -> flash record per board                           MISSING
    -> runtime status v3.1.0.1                          VERIFIED
```

The link from source/tag to installed binary is not established.

### CC1352 recovery images

```text
FeralRF source ba27fbeb                                 VERIFIED checkout
    -> exact CMake cache/compiler/prebuilt-lib inputs   UNKNOWN
    -> exact ELF/BIN/HEX/MAP                            MISSING
    -> SHA256                                           MISSING
    -> flash record per board                           MISSING
    -> DeviceInfo 1.0.0 / caps 7 / FERALRF1             VERIFIED runtime response
```

Only the checkout and generic runtime response are established; there is no evidence joining them.

## 12. Remaining UNKNOWN fields

1. Exact CC1352 binary and source commit installed on every unit in every campaign.
2. Exact RP2040 binary/hash installed during F29 and recovery.
3. F29 RP version, compiler, build date, and flashed image.
4. F29 Python interpreter and dependency versions.
5. Exact CMake cache, GCC version, and precompiled TI library digests for installed CC builds.
6. Exact SysConfig/generated-source identity inside installed CC binaries.
7. Complete flash history and whether both boards carried identical images.
8. Exact physical PCB/BOM/rework revision of each board.
9. Physical external-switch state and RF path loss.
10. Historical antennas, geometry, supply, interference, and preceding device state.

## 13. Recommended provenance improvements

Minimum future mechanism:

1. Generate a build manifest containing repository URL, full Git SHA, dirty flag, UTC build time, SDK version and commit, compiler ID/version, target device/package, board/RF design, RTOS option, CMake options, and SHA256 of every linked prebuilt library and generated configuration file.
2. Embed at least full Git SHA, dirty flag, build timestamp, SDK version, target, and a truncated binary SHA/build ID in both RP and CC images.
3. Expose the RP values through `fw_version` and the CC values through an extended `DeviceInfo` command.
4. Archive ELF, BIN/HEX/UF2, MAP, manifest, and SHA256 sidecar together as one immutable release bundle.
5. Record the exact artifact digest flashed to each physical USB serial and timestamp the flash operation.
6. At experiment start, capture host commit/status, interpreter, `pip freeze`, helper SHA256, runtime build IDs, USB identities, and configuration dump into the raw artifact directory.

Do not infer binary identity from a semantic version after these mechanisms exist; compare build ID or digest.

## 14. Future Sub-GHz preset validation matrix

Classification:

- **A:** eligible for OTA validation.
- **B:** eligible with limitations.
- **C:** not physically supported by the current CatSniffer/CC target as presently evidenced.
- **D:** implementation incomplete or not meaningful to validate.
- **E:** requires prior code audit.
- **F:** compatibility unknown.

Common raw implementation path `P0`: `PHY.PROPRIETARY_GFSK` -> host `configure_prop()` -> `CMD_SET_PROP_CONFIG` -> `RadioIF_setPropConfig()` -> proprietary FS/RX/TX. `P433` additionally selects the separate 433 SysConfig structure. `POOK` loads genook MCE/RFE patches and has an explicit non-recoverable-without-reset/power-cycle lifecycle warning. Protocol-named presets implement only parameter dictionaries, not protocol stacks.

All frequencies and power requests require appropriate regional authorization, antenna/load, separation/attenuation, and operator approval.

| Class | Preset | Frequency | Requested modulation | Rate | Dev. | RX BW | Sync | Path / support assessment | Protocol-name boundary | Priority / lifecycle decision |
|---|---|---:|---|---:|---:|---:|---|---|---|---|
| E | `gfsk_433_50k` | 433.920 MHz | GFSK (1) | 50k | 100 | 0x52 | 0x930B51DE | `P433`; CC source has a 433 configuration, but current external-front-end/antenna compatibility is not established | none | Do not include until 433 hardware path and generated config are reviewed |
| E | `gfsk_433_10k` | 433.920 MHz | GFSK (1) | 10k | 50 | 0x52 | 0x930B51DE | `P433`; same unresolved hardware/config boundary | none | Hold |
| E | `fsk_433_50k` | 433.920 MHz | FSK (0) | 50k | 100 | 0x52 | 0x930B51DE | `P433`; silicon mode requested, effective waveform/path unverified | none | Hold |
| E | `ook_433_4k8` | 433.920 MHz | OOK (2) | 4.8k | 0 | 0x4C | 0x930B51DE | `POOK`; 433 path plus OOK patch/lifecycle risk | none | Must not execute yet |
| E | `ook_433_2k4` | 433.920 MHz | OOK (2) | 2.4k | 0 | 0x4C | 0x930B51DE | `POOK`; same | none | Must not execute yet |
| E | `msk_433_50k` | 433.920 MHz | MSK value 4 | 50k | 100 | 0x52 | 0x930B51DE | `P433`; code passes raw modType 4, but effective TI modulation mapping needs audit | none | Hold |
| E | `4fsk_433_50k` | 433.920 MHz | 4-FSK value 5 | 50k | 100 | 0x52 | 0x930B51DE | `P433`; raw modType mapping/format and hardware path require audit | none | Hold |
| E | `4gfsk_433_50k` | 433.920 MHz | 4-GFSK value 6 | 50k | 100 | 0x52 | 0x930B51DE | `P433`; same | none | Hold |
| A | `gfsk_868_50k` | 868.000 MHz | GFSK (1) | 50k | 100 | 0x52 | 0x930B51DE | `P0`; native Sub-GHz range and standard source path; current installed path at 868 remains unverified | none | High, after 902.2 controls |
| A | `gfsk_868_100k` | 868.000 MHz | GFSK (1) | 100k | 150 | 0x56 | 0x930B51DE | `P0`; same, higher rate | none | High after 50k |
| E | `ook_868_4k8` | 868.000 MHz | OOK (2) | 4.8k | 0 | 0x4C | 0x930B51DE | `POOK`; source explicitly warns that loaded patches lock the RF core until reset/power cycle | none | Must not execute until lifecycle/recovery review |
| E | `msk_868_50k` | 868.000 MHz | MSK value 4 | 50k | 100 | 0x52 | 0x930B51DE | `P0`; silicon family may support MSK-like modes, but raw TI modType interpretation/effective waveform is not independently established | none | Hold for code/RF-command audit |
| B | `wireless_mbus_s_868` | 868.300 MHz | GFSK (1) | 32.768k | 75 | 0x52 | 0x543D0000 | `P0`; meaningful raw PHY parameter test | W-MBus framing/coding/interoperability not implemented by the name | Medium |
| B | `wireless_mbus_t_868` | 868.950 MHz | GFSK (1) | 100k | 200 | 0x57 | 0x543D0000 | `P0`; meaningful raw packet test | Not W-MBus protocol validation | Medium |
| B | `wireless_mbus_c_868` | 868.950 MHz | GFSK (1) | 100k | 180 | 0x57 | 0x543D0000 | `P0`; meaningful raw packet test | Not W-MBus protocol validation | Medium |
| C | `wireless_mbus_n_169_2k4` | 169.450 MHz | GFSK (1) | 2.4k | 154 | 0x44 | 0x543D0000 | Source contains an override but current audit/report evidence places 169 outside the supported CC/U4 path; LO/config realization is not credible without redesign/audit | Not W-MBus protocol validation | Do not execute on current hardware |
| C | `wireless_mbus_n_169_4k8` | 169.450 MHz | GFSK (1) | 4.8k | 154 | 0x44 | 0x543D0000 | Same | Not W-MBus protocol validation | Do not execute on current hardware |
| E | `4fsk_868_50k` | 868.000 MHz | 4-FSK value 5 | 50k | 100 | 0x52 | 0x930B51DE | `P0`; raw modType/format mapping requires source-to-TI-command audit before waveform claim | none | Hold |
| E | `4gfsk_868_50k` | 868.000 MHz | 4-GFSK value 6 | 50k | 100 | 0x52 | 0x930B51DE | `P0`; same | none | Hold |
| A | `gfsk_915_50k` | 915.000 MHz | GFSK (1) | 50k | 100 | 0x52 | 0x930B51DE | `P0`; native Sub-GHz source path; external path supported by design but not electrically verified | none | High |
| A | `gfsk_902_50k` | 902.200 MHz | GFSK (1) | 50k | 100 | 0x52 | 0x930B51DE | `P0`; same frequency/rate as known-good control, changing requested modulation filtering | none | First new preset after control |
| B | `sidewalk_915_fsk_50k` | 915.000 MHz | FSK (0) | 50k | 100 | 0x52 | 0x930B51DE | `P0`; meaningful raw FSK test | Sidewalk stack/authentication/interoperability absent; LR/LoRa not supported here | High after generic 915 |
| B | `sidewalk_915_fsk_250k` | 915.000 MHz | FSK (0) | 250k | 200 | 0x5A | 0x930B51DE | `P0`; higher-rate raw FSK, bandwidth chosen historically | Same boundary | Medium |
| B | `wisun_915_fsk_50k` | 902.200 MHz | FSK (0) | 50k | 100 | 0x52 | 0x930B51DE | `P0`; **formally validated 60/60** in current recovery | No Wi-SUN FAN/RPL/security/interoperability | Positive control |
| D | `mioty_868_tsunb` | 868.000 MHz | FSK (0) | 396 | 1 | 0x4A | 0x930B51DE | Source explicitly marks native support pending and suggests a custom CPE patch; prior 0/10 claim | MIOTY TS-UNB stack/interleaving absent | Must not execute as a validation preset |
| B | `wisun_915_fsk_100k` | 902.200 MHz | FSK (0) | 100k | 150 | 0x56 | 0x930B51DE | `P0`; raw higher-rate FSK | No Wi-SUN interoperability | High, ascending-rate sequence |
| B | `wisun_915_fsk_150k` | 902.200 MHz | FSK (0) | 150k | 200 | 0x5A | 0x930B51DE | `P0`; raw higher-rate FSK, historically heuristic BW | Same | Medium |
| B | `wisun_915_fsk_200k` | 902.200 MHz | FSK (0) | 200k | 200 | 0x5A | 0x930B51DE | `P0`; same limitations | Same | Medium |
| B | `wisun_915_fsk_300k` | 902.200 MHz | FSK (0) | 300k | 200 | 0x5E | 0x930B51DE | `P0`; highest rate, source comment calls BW choice probable/empirical | Same | Lower, after lower rates |

Current source contains no separate generic `fsk_868_*` preset. It contains two proprietary 2.4 GHz presets, but those are not Sub-GHz and are excluded from this future Sub-GHz matrix.

## 15. Recommended manual OTA execution order

No test in this section was executed.

### Pre-campaign provenance gate

Before RF work, create and archive the build manifests/artifacts described in Section 13, record exact device build IDs, host environment, USB identities, helper hash, antenna/load, geometry, and regulatory authorization. If installed CC/RP build identity remains unavailable, explicitly label the campaign as validating unknown installed binaries.

### Group 1 — known-good sentinel

1. `wisun_915_fsk_50k` at its actual 902.2 MHz.

Purpose: verify the 3A–4A reference condition before introducing another preset. This is a raw FSK positive control, not Wi-SUN interoperability.

### Group 2 — same-frequency discriminants, ascending complexity

2. `gfsk_902_50k`
3. `wisun_915_fsk_100k`
4. `wisun_915_fsk_150k`
5. `wisun_915_fsk_200k`
6. `wisun_915_fsk_300k`

Purpose: hold 902.2 MHz fixed, first change FSK/GFSK request at 50k, then increase rate/BW monotonically. A planned control may be inserted between groups, but it must be declared in advance and must not hide a first failure.

### Group 3 — 915 MHz

7. `gfsk_915_50k`
8. `sidewalk_915_fsk_50k`
9. `sidewalk_915_fsk_250k`

Purpose: establish generic 915 GFSK first, then raw Sidewalk-associated FSK parameters from low to high rate.

### Group 4 — 868 MHz standard GFSK and protocol-associated raw parameters

10. `gfsk_868_50k`
11. `gfsk_868_100k`
12. `wireless_mbus_s_868`
13. `wireless_mbus_t_868`
14. `wireless_mbus_c_868`

Purpose: establish the generic 868 path before W-MBus-named raw dictionaries. Success validates only raw delivery under those settings.

### Explicitly excluded pending review

- All OOK presets: RF-core lifecycle and safe recovery must be reviewed first; place them last in a separate campaign with a predeclared power-cycle boundary.
- MIOTY: implementation is explicitly incomplete.
- Both 169 MHz W-MBus N presets: current silicon/front-end support is not established and existing evidence classifies the path as unsupported.
- All 433 MHz presets: require generated-config, matching/front-end, antenna, and regulatory review first.
- MSK, 4FSK, and 4GFSK at either band: audit the TI `modType`, `formatConf`, patch, and effective command structures before claiming these waveforms.

### Common future one-run method

For each authorized preset:

- explicitly record physical USB identities and Bridge/Shell MI mappings; never use COM arithmetic;
- use a fresh one-run helper and fresh `Radio` objects;
- record an exact preset dump and requested power;
- start RX before TX;
- use a unique marker encoding preset and run identity;
- optionally use a fixed negative window, consistently across the matrix;
- send a fixed ten packets with fixed spacing;
- preserve every TX return, `Packet`, `RxStreamError`, full payload, CRC, metadata, host/device timestamps, stats, loss counters, and cleanup outcome;
- do not retry automatically;
- define PASS as 10/10 CRC-good peer marker matches, PARTIAL as 1–9/10, FAIL-DELIVERY as 0/10 after valid setup, and BLOCKED as invalid setup/control;
- test COM33 -> COM88 first; reverse direction only after review or where localization is justified;
- stop after each preset for the required human checkpoint;
- never convert raw delivery into a protocol-interoperability claim.

To minimize state contamination, exclude OOK and unaudited modulation values from this standard campaign. Do not combine all presets in one process or add an automatic sweep/retry loop.

## 16. Open questions

### Required for reproducible validation

1. What exact RP and CC artifact hashes are installed on recovery CatSniffer #2 and #3?
2. Can CC `DeviceInfo` and RP `fw_version` expose immutable build IDs tied to archived artifacts?
3. What exact compiler, CMake options, and prebuilt TI library digests produced each CC image?
4. Which exact PCB/BOM revision and RF design correspond to each physical board?

### Required for historical root-cause work

1. Can any F29 ELF/BIN/HEX/UF2, build log, flash log, or raw execution transcript be recovered?
2. Were F29's `/dev/ttyACM1` and `/dev/ttyACM2` the same physical assemblies as the later COM33/COM88 units?
3. Did an unidentified installed CC build, rather than repository source evolution, differ between 70/70 and 0/70?
4. Did the 0/240 multi-preset sequence enter a state that an isolated current test does not reproduce?

### Required before expanded modulation/hardware claims

1. What are the exact TI RF command semantics for source values 4, 5, and 6 and the required `formatConf`/patches?
2. Is the installed 433/169 RF path physically compatible with the requested bands?
3. What actual frequency, modulation, power, and path loss reach J1 for each eligible group?
4. Why do successful packets report channel 37, mixed-sign RSSI, and LQI 0?

## 17. Final decision

- **Can the current successful environment be mapped exactly to source?** **No, not end-to-end.** Host source and helpers are exact; installed RP and CC binary-to-source links are missing.
- **Can the historical F29 70/70 environment be mapped exactly?** **No.** The closing source/helper is exact, but installed binaries, physical identities, execution environment, and raw run are not.
- **Can the 0/70 environment be mapped exactly?** **No.** Host and RP self-identification are strong, but CC binary identity and all binary artifact hashes are absent.
- **Can the 0/240 environment be mapped exactly?** **No.** It shares the 0/70 provenance strengths and gaps and includes additional sequence-state uncertainty.
- **Strongest remaining gap:** exact installed CC1352 binary/build provenance for all campaigns.
- **Specific regression supported?** **No.** No relevant proprietary source change is demonstrated between F29 and the later failure source; RP tag differences do not change RF/bridge behavior.
- **Next non-RF action:** implement, in a separately approved development task, reproducible build manifests and runtime build IDs, rebuild/archive signed or hashed RP/CC artifacts, and record per-board flash provenance before expanding OTA testing.
- **Later OTA scope:** the known-good 902.2 MHz control; generic GFSK at 902.2/915/868; ascending-rate Wi-SUN-named raw FSK; Sidewalk-named raw FSK; and W-MBus S/T/C raw settings.
- **Do not yet execute:** OOK, MIOTY, 169 MHz, any 433 MHz preset, or MSK/4FSK/4GFSK until the identified lifecycle, silicon/front-end, and command-encoding audits are complete.

This audit performed no hardware action and did not start the preset-validation campaign.
