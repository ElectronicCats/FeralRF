# Z-Stack Integration Feasibility Audit — FeralRF / CatSniffer

**Audit date:** 2026-10-27  
**Audit type:** static architecture and code-level feasibility investigation  
**Repository revision:** `3409955011ebacc448e96605780560cf8118b364` (`feature/hardware-validation`)  
**Pinned SDK submodule:** `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` at `5b31d0a4903351e544546e23ef3330eaa4291ceb`, tag/description `lpf2-8.30.01.01`  
**Target:** CC1352P7 / `DeviceFamily_CC13X2X7`  
**Prior report used as an evidence index:** `docs/FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md`

## Audit boundary and confidence language

This audit rechecked the relevant current source and pinned SDK. It did not compile, link, run, flash, or exercise hardware. It did not modify firmware, Python, SDK, CMake, or existing documentation. The installed SDK checkout has source, libraries, SysConfig templates, linker files, release notes, and manifests, but no `examples/` directory or importable TI example project files. Consequently, this report distinguishes four different claims:

- **Source available:** a source or header file is present in this checkout.
- **Build available:** the sources, binary libraries, generated configuration, linker layout, and supported compiler inputs needed for a target are available.
- **Target supported:** the SDK contains CC13x2x7/CC26x2x7-specific libraries, linker layout, or SysConfig handling.
- **Integrated and verified:** FeralRF actually builds, links, starts, and interoperates with that component. No Z-Stack item reaches this last state.

The requested classifications mean:

- **FEASIBLE WITH CURRENT EVIDENCE:** the existing interfaces are sufficient and no identified architectural blocker must first be removed.
- **FEASIBLE WITH CONDITIONS:** a credible supported path exists, but named build, board, resource, or validation conditions remain.
- **NOT CURRENTLY DEMONSTRATED:** the installed evidence does not establish a working result.
- **BLOCKED BY IDENTIFIED CONSTRAINT:** the present configuration has a concrete incompatibility, not merely an unspecified difficulty.
- **NOT RECOMMENDED:** another architecture achieves the objective with materially lower coupling or risk.

## 1. Executive findings

1. **The previous audit's central findings remain correct at the current commit.** FeralRF provides a raw IEEE 802.15.4 2.4-GHz bearer. It selects native IEEE RF Core commands, receives CRC-good MAC frames, forwards bytes and radio metadata to Python, and can transmit a host-supplied MPDU. It does not implement Zigbee NWK, APS, ZDO, ZCL, commissioning, routing, key state, joining, or an active IEEE MAC service.

2. **The pinned SDK contains a substantial Z-Stack implementation, but not as a wholly reusable source tree.** ZDO, BDB, ZCL, AF, API, startup, NPI/MT, and considerable glue are visible as source. The actual TIMAC implementation and core NWK/APS functionality are reached through precompiled archives. Z-Stack source presence therefore does not establish independent module reuse.

3. **The current FeralRF GCC/CMake target cannot consume the Z-Stack build as presently configured.** FeralRF hard-codes `arm-none-eabi-gcc`, a GNU linker script, local generated driver files, and a small set of GCC archives. Z-Stack's role archives exist only under `lib/ticlang` and `lib/iar`; Z-Stack SysConfig constructs a toolchain-specific path and has no Z-Stack GCC archive in the checkout. The supplied CC13x2x7 Z-Stack linker file is TI-clang syntax. The SDK release notes identify CCS 12.8.0, TI ARM Clang 3.2.2.LTS, and SysConfig 1.21.1 as recommended dependencies. None of `arm-none-eabi-gcc`, CMake, TI ARM Clang, SysConfig CLI, Make, or Ninja was available on the audit shell `PATH`, so no tool version or link compatibility was inferred from an executable.

4. **The current RF ownership models conflict.** FeralRF directly opens RF Driver handles, mutates global RF command structures, posts/runs RF commands, switches modes, and deliberately keeps a 433-MHz handle open for the life of the image. Z-Stack's MAC library owns its radio schedule and timing behind ZMAC/TIMAC. They cannot safely be allowed to issue unrelated RF Driver operations concurrently. TI's DMM is evidence that supported arbitration is possible in designed multiprotocol applications, but using it would require all RF clients to register with DMM and route RF operations through the DMM scheduler. FeralRF is not a DMM client.

5. **Passive Zigbee analysis does not require Z-Stack and is the preferred first implementation.** The current RX path preserves channel, RSSI, LQI/correlation, CRC status, a RAT-derived timestamp, and the received IEEE MPDU including its two-byte FCS. Python can parse MAC, NWK, APS, ZDO, ZCL, and security headers. Encrypted payloads can still be identified and preserved as ciphertext plus MIC; decryption must be conditional on authorized key and nonce context. Basic decoding needs no firmware change, though an explicit frame/FCS metadata contract is recommended.

6. **Selective embedded reuse is poor for active Zigbee.** Headers, constants, data definitions, and some pure utility logic may be adaptable, subject to license review. ZMAC, NWK, APS, ZDO, BDB, security, and ZCL application behavior are coupled by OSAL Port queues, service IDs, timers, NV, compile-time role selection, generated configuration, and binary libraries. Pulling a few visible `.c` files into FeralRF would not create a functioning partial stack.

7. **If active Zigbee is required, a separate firmware target is the defensible architecture.** Start from the matching SDK's supported TI-Clang/SysConfig Z-Stack project rather than from the current FeralRF CMake image. For the smallest active proof, select a Zigbee End Device (ZED), a fixed endpoint, and a minimal cluster set; this avoids coordinator formation and router child/routing responsibilities, though it requires an independent coordinator. For a host-controlled general-purpose Zigbee adapter, the vendor ZNP/NPI/MT persona is a stronger semantic fit than inventing a thin wrapper around raw FeralRF commands.

8. **No active Zigbee feasibility claim is complete yet.** The matching full SDK example project set, TI toolchain, generated CatSniffer board configuration, link map, boot proof, NV behavior, RF/antenna validation, and independent-device interoperability are still required.

### Decision summary

| Strategy | Classification | Decision |
|---|---|---|
| A — passive decoder over current IEEE backend | **FEASIBLE WITH CURRENT EVIDENCE** | Recommended first. No Z-Stack runtime required. |
| B — selected Z-Stack components without full runtime | **NOT RECOMMENDED**; active subset **BLOCKED BY IDENTIFIED CONSTRAINT** | Reuse definitions or isolated pure helpers only; do not assemble an active partial stack. |
| C — complete Z-Stack in the current FeralRF image | **BLOCKED BY IDENTIFIED CONSTRAINT** and **NOT RECOMMENDED** | Current compiler/linker, RF ownership, NV layout, task/config, and resource model must all change. |
| D — separate Zigbee firmware target | **FEASIBLE WITH CONDITIONS** | Recommended for active Zigbee; preserve a separate FeralRF raw-radio image. |

## 2. Verified SDK, Z-Stack, toolchain, and build baseline

### 2.1 Repository and SDK identity

The current repository HEAD is exactly the previously audited revision, `3409955011ebacc448e96605780560cf8118b364`. The SDK submodule is exactly `5b31d0a4903351e544546e23ef3330eaa4291ceb`, described as `lpf2-8.30.01.01`. The only pre-existing worktree item at audit start was the untracked prior audit report; it was treated as user-owned evidence and not edited.

The SDK release notes describe the package as including a Zigbee 3.0 protocol suite and DMM. `source/ti/zstack/stack/api/zsversion.h:67-69` defines a Z-Stack server/API version `2.6.3`; that number must not be mistaken for the Zigbee specification revision.

### 2.2 What is actually present

Verified Z-Stack areas include:

- `source/ti/zstack/startup/` — main and stack task startup;
- `source/ti/zstack/zmac/` and `zmac/f8w/` — Zigbee-to-TIMAC adaptation and callbacks;
- `source/ti/zstack/stack/` — API, AF, ZDO, ZCL, BDB, visible NWK glue/globals, Green Power, system state, and utilities;
- `source/ti/zstack/apps/` — application examples such as GenericApp;
- `source/ti/zstack/npi/` and `mt/` — network processor transport and monitor/test command interface;
- `source/ti/zstack/sysconfig/` and `.meta/` — role-specific configuration templates and library selection logic;
- `source/ti/zstack/boards/cc13x2x7_cc26x2x7/` — TI-clang and IAR memory layouts;
- `source/ti/zstack/lib/{ticlang,iar}/...` — role-specific NWK archives;
- `source/ti/ti154stack/lib/{ticlang,iar}/m4f/` — CC13x2x7/CC26x2x7 MAC and MAC-OSAL archives;
- `source/ti/dmm/` — scheduler, policy, RF API remapping, and Zigbee DMM application material.

The checkout does **not** contain:

- Z-Stack GCC role archives;
- TI 15.4 MAC GCC archives;
- a GNU Z-Stack linker script for CC13x2x7;
- the SDK `examples/` tree or CCS project specifications;
- a FeralRF Z-Stack target;
- a generated CatSniffer Z-Stack board/radio/pin configuration;
- a Z-Stack link map or memory report for this board.

### 2.3 Current FeralRF build configuration

`firmware/cc1352/CMakeLists.txt` currently establishes:

- device `CC1352P7`, subfamily `cc13x2x7_cc26x2x7`, and local GNU script `linker/cc1352p7.ld`;
- TI-RTOS7 enabled by default;
- `arm-none-eabi-gcc/g++`, Cortex-M4F hard-float, C11, `-O0`, and GNU `ld` flags;
- local SysBIOS and TI Drivers generated C files;
- FeralRF application sources and generated BLE/IEEE/proprietary RF settings;
- only the GCC RF Driver, Drivers, SysBIOS, and DriverLib archives.

It does not add Z-Stack, TIMAC, OSAL Port, NVOCMP, NVS, ZCL, BDB, NPI, MT, DMM, or generated `ti_zstack_config` sources.

### 2.4 Vendor build expectation

The SDK release notes at `release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html:118-123` name CCS 12.8.0, TI ARM Clang 3.2.2.LTS, and SysConfig 1.21.1. Z-Stack's library generator at `source/ti/zstack/.meta/zstack.syscfg.js:149-226` selects both a MAC archive and a role-specific `libZStack_*` archive according to toolchain, ISA, RTOS, device, and role. The installed role archives are:

- `libZStack_nwk_all.a` for ZNP;
- `libZStack_nwk_zr.a` for coordinator or router;
- `libZStack_nwk_zed.a` for an end device;
- `libZStack_gpd_sec.a` for Green Power device security.

They exist only under IAR and TI-clang directories. This is a concrete current-build constraint, not an inference based on complexity.

## 3. Current FeralRF IEEE 802.15.4 architecture

### 3.1 End-to-end execution path

```text
Python Radio.set_phy(PHY.IEEE_802_15_4) / set_channel / start_rx / transmit_frame
  -> CommandBuilder binary payloads
  -> protocol framing over Cat-Bridge serial transport
  -> CommandProcessor_processCommand()
  -> ControlTask deferred request/event
  -> RF task: DataTask_poll()
  -> PhyManager selection + RadioIF operation
  -> mutable Ieee154_0_cmdRadioSetup / CMD_FS / CMD_IEEE_RX or CMD_IEEE_TX
  -> TI RF Driver RF_open/RF_postCmd/RF_runCmd scheduling
  -> RF Core

RX return:
RF Core data queue
  -> RadioIF_processIeee154Packets()
  -> RadioIF_RxPacket queue
  -> LLManager_processRxPacket() pass-through
  -> DataTask_emitRxPacket()
  -> RSP_RX_PACKET serialization
  -> Python Radio.read_packets()
  -> Packet(timestamp, channel, RSSI, LQI, CRC, raw bytes)
```

### 3.2 Responsibility map

| Responsibility | Exact current implementation | Consequence |
|---|---|---|
| Python API | `python/feralrf/radio.py:455-478,599-709,725-750` | Selects PHY/channel, controls RX, returns raw `Packet`, sends raw/frame TX. |
| Command serialization | `python/feralrf/commands.py:17-34,42-66` | Compact byte payloads only; no Zigbee semantic commands. |
| Firmware command dispatch | `firmware/cc1352/src/command_processor.c:128-148,188-258` | Validates sizes and defers RX/TX actions; no Zigbee state machine. |
| PHY selection | `firmware/cc1352/src/phy_manager.c:9-49` | IEEE selects `LL_DEFAULT`, not a Zigbee/MAC layer. |
| RF-task execution | `main_rtos.c:54-77,141-178`; `data_task.c:76-137` | Two equal-priority cooperative tasks; RF operations execute from the RF task. |
| IEEE channel | `radio_if.c:428-463` | Maps 11–26 and sets both RX channel and `CMD_FS` frequency. |
| IEEE RX start | `radio_if.c:1335-1371` | Creates an RF data queue, enables PHR/FCS and metadata, switches mode, posts RX. |
| IEEE RX extraction | `radio_if.c:1497-1568` | Validates queue length, rejects CRC-bad frames, copies the PHR-reported MPDU bytes. |
| IEEE TX | `radio_if.c:468-488`; `smartrf_ieee_15_4_0.c:146-163` | Host supplies up to 125 bytes; RF Core creates PHY header and FCS. |
| RF scheduling/ownership | `radio_if.c` calls `RF_open`, `RF_postCmd`, `RF_runCmd`, cancellation/flush functions | FeralRF owns handles and command lifetimes directly. |
| Frame buffer | `RadioIF_RxPacket` in `include/radio_if.h:17-28`; static RF queue and packet queue in `radio_if.c` | A generic byte buffer, not an IEEE or Zigbee structure. |
| Link-layer processing | `ll_manager.c:144-159` | BLE alone is classified; IEEE is returned unchanged. |
| Host forwarding | `data_task.c:28-65`; `radio.py:673-707` | Preserves common metadata and bytes but has no explicit `fcs_present` or frame-format field. |

### 3.3 Frame and FCS contract

At RX start, FeralRF overrides `bIncludePhyHdr=1` and `bIncludeCrc=1`. The RF Core queue therefore contains the one-byte PHY length followed by the received MPDU including its FCS, then appended RSSI, correlation/CRC, and timestamp. `RadioIF_processIeee154Packets()` uses the PHY length as `pkt.data_len` and copies from the byte after the PHY header. Thus `Packet.data` contains MAC frame bytes **including the received two-byte FCS**, while the PHY length byte is removed.

At TX, `bIncludePhyHdr=0` and `bIncludeCrc=0`; TI's command definition says the RF Core determines the PHY header and appends the calculated CRC. The current KillerBee adapter accordingly strips two FCS bytes before calling `transmit_frame()`.

The RX code drops CRC-bad frames before Python receives them. This is suitable for normal passive decoding but prevents analysis of damaged frames. The Python `Packet` carries `crc_ok`, but for IEEE traffic it will normally only be true because failures were already discarded.

### 3.4 What exists and what is absent

**Implemented:** 2.4-GHz O-QPSK IEEE PHY selection; channels 11–26; native IEEE RX/TX commands; promiscuous acceptance with frame filtering and auto-ACK disabled; good-frame delivery; FCS inclusion on RX and generation on TX; RSSI/LQI/channel/timestamp metadata; host raw transmission.

**Absent:** parsed MAC FCF/addressing/security/IE representation; MAC association; active address filtering; CSMA-CA service; automatic ACK/retry policy; indirect data/poll state; MAC security tables; Zigbee NWK/APS/ZDO/ZCL; joining/formation; route and neighbor tables; BDB commissioning; persistent keys/counters/network state; Zigbee application endpoints; Zigbee-specific host commands.

Raw injection must therefore not be described as active Zigbee device behavior. A host can construct bytes, but it cannot meet stack timing and state obligations through the present raw API.

## 4. Z-Stack component inventory and realistic reuse

| Component | Exact source/library path and key entry points | Purpose and dependencies | Independent? | Realistic FeralRF reuse |
|---|---|---|---|---|
| TIMAC / IEEE MAC | Glue: `source/ti/zstack/zmac/f8w/zmac.c` (`ZMacInit`, `ZMacStartReq`, `ZMacDataReqSec`), `zmac_cb.c`; implementation: `source/ti/ti154stack/lib/{ticlang,iar}/m4f/maclib_*cc13x2x7_2_4g_tirtos7.a` plus `maclib_osal_tirtos7_cc13x2x7_26x2x7.a` | Active IEEE MAC: PIB, scan, association, data requests, ACK/CSMA/timing, callbacks. Requires MAC user config, OSAL Port, RF Driver, crypto, role build, task/service registration, generated radio/driver config. | **No.** ZMAC calls `MAP_MAC_*`; those functions come from the binary MAC runtime. | Only as part of the supported runtime. It cannot be replaced by FeralRF's raw `CMD_IEEE_*` path. |
| NWK | Headers/globals/glue under `source/ti/zstack/stack/nwk/`; core role archives `source/ti/zstack/lib/{ticlang,iar}/m4f/libZStack_nwk_{zr,zed,all}.a`; startup calls `nwk_init` | Addressing, neighbor/route state, discovery, forwarding, network security integration. Requires MAC, NV, OSAL events/timers, global `_NIB`, role macros. | **No.** `nwk_init` and core behavior are not provided as a standalone source module. | Active use requires the role archive and complete runtime. Headers can inform host parsing but are not a clean parser API. |
| APS | Interface headers in `stack/nwk/aps*.h`; AF source in `stack/af/af.c`; startup calls `APS_Init`; core behavior is tied to NWK archive/runtime | Application support, groups/binding, delivery, fragmentation, APS security and counters. | **No.** Service ID, NWK, timers, buffers, NV, and security state are assumed. | Not suitable as a passive decoder or isolated active layer. |
| ZDO | `stack/zdo/zd_app.c` (`ZDApp_Init`), `zd_object.c`, `zd_profile.c`, `zd_nwk_mgr.c` | Device/network management, discovery requests, state transitions, leave/join handling, network manager behavior. Depends on NWK/APS, OSAL, BDB, NV, callbacks. | **No** for operational behavior. | Data definitions may be adapted; operational code requires full stack. |
| ZCL | `stack/zcl/zcl.c`, `zcl_port.c`, cluster libraries such as `zcl_general.c`; app registration in `apps/genericapp/zcl_genericapp.c` | Endpoint/attribute/cluster command handling above AF/APS. Depends on ZCL port, endpoints, AF, application task, OSAL allocation/messages, often NV. | Some encoding/attribute helpers may be separable, but the delivered application model is not. | Do not embed merely for passive parsing. A purpose-built Python parser is smaller and safer. Reuse in a vendor active target is appropriate. |
| BDB commissioning | `stack/bdb/bdb.c` (`bdb_Init`), finding/binding, reporting, touchlink sources; invoked through `Zstackapi_bdbStartCommissioningReq` | Formation, steering, finding/binding, network recovery and commissioning callbacks. Depends on ZDO/NWK/APS/security/NV/timers. | **No.** | Full-runtime reuse only. |
| Security | `stack/zdo/zd_sec_mgr.c`, NWK/APS security APIs and role archive; crypto initialization in `startup/zstackstartup.c:574-669` | Trust center policy, link/network keys, install-code flows, counters, CCM/ECB/TRNG. Requires AESCCM, AESECB, TRNG instances, NWK/APS state, NV, authorized provisioning. | **No** as an active subsystem. | Passive header parsing can be independently implemented. Active security belongs to full Z-Stack. |
| Join/formation | `zd_app.c`, `bdb.c`, ZStack API requests, NWK archive | Scan, select, associate, authenticate, restore/rejoin, form network by role. | **No.** | Full-runtime reuse only. |
| Routing | NWK role archive plus `zd_nwk_mgr.c` and route-management APIs | Route discovery/maintenance, broadcast forwarding, concentrator/source route behavior. | **No.** | Full-runtime reuse only; ZED minimizes but does not eliminate NWK dependency. |
| Device roles | SysConfig role templates; `.meta/zstack.syscfg.js:44-67,207-226`; role archives | Coordinator and router use `nwk_zr`; end device uses `nwk_zed`; ZNP uses `nwk_all`; compile-time macros gate behavior. | **No runtime role swapping for ordinary apps.** ZNP selects role through its own NVS/host model. | Select one role per separate application target, or use the vendor ZNP architecture. |
| Application API | `stack/api/zstackapi.c`, `zstacktask.c`, `zstackmsg.h`; GenericApp source | Synchronous request wrappers over OSAL message queues and asynchronous indications to an application service task. | **No direct-call library.** Caller must be an OSAL-registered task and process messages. | Reusable in a vendor target. A FeralRF UART task cannot safely call it without application-task integration. |
| Network processor host API | `npi/`, `mt/`, `sysconfig/znp.syscfg`; `ZNP_NPI` startup path | Vendor serial/SPI network processor protocol and command surface. | It is a complete firmware persona, not a library for the raw image. | Strong candidate for a separate active-Zigbee image and new Python transport adapter. |
| NV | `source/ti/common/nv/` NVOCMP, `zstack/osal/osal_nv.*`, generated NVS config | Persists network, key, counter, binding, and commissioning state. | **No** for reliable active operation. | Must be adopted and allocated in linker/SysConfig for active target. |
| DMM | `source/ti/dmm/`, `dmm_rfmap.h`, scheduler/policy, DMM Zigbee app material | Arbitrates RF commands from registered stack clients according to policies. | Not a transparent library added after the fact. | Possible foundation for a redesigned multiprotocol image, not proof that current direct RF code can coexist. |

### Source availability is not independent reuse

The decisive coupling can be seen in startup: one TI-RTOS task contains an OSAL-style scheduler whose service functions are registered in strict order for MAC, NWK, APS, ZDO, BDB, and the ZStack API. ZMAC callbacks enqueue messages directly to `NWK_TaskID`. `Zstackapi_*` calls allocate messages, send to the stack service task, block the caller task, and wait for a matching response. These are runtime contracts, not optional convenience wrappers.

## 5. Z-Stack initialization and execution paths

### 5.1 Startup sequence

The vendor path in `source/ti/zstack/startup/main.c` is:

1. Initialize board/cache and determine the IEEE extended address.
2. Install NVOCMP function pointers and initialize NV.
3. Construct the dedicated Z-Stack task with `stackTask_init()`.
4. Optionally create NPI for a network-processor build.
5. For non-ZNP builds, construct a 3000-byte application task at priority 2.
6. Start SYS/BIOS.

The stack task has a 3072-byte stack at priority 5. `stackInit()` installs MAC user configuration, initializes TRNG/AESCCM/AESECB, initializes ZMAC, extended address, core NV globals, AF, all service functions, MAC buffers, and low-level MAC. `stackServiceFxnsInit()` allocates an event table and registers MAC, NWK, APS, ZDO, optional network manager/touchlink/Green Power, BDB, and ZStack API services. The task then runs a priority-ordered OSAL event scheduler.

### 5.2 Application path

GenericApp demonstrates the intended application contract:

1. Create an application semaphore and register the TI-RTOS task with OSAL Port.
2. Register endpoint, simple descriptor, cluster callbacks, attribute list, ZDO/stack callbacks, and BDB parameters.
3. Issue commissioning mode `0` at startup to restore prior network configuration.
4. Wait on the application semaphore.
5. Receive ZStack indications and process BDB notifications, AF incoming messages/confirms, and state changes.
6. Issue formation or steering commissioning requests when application policy requires it.

This means an active CatSniffer application needs a real Zigbee application task or ZNP host interface. Mapping a UART opcode directly to `Zstackapi_AfDataReq()` from the existing polling task without OSAL registration and indication processing would violate the intended execution model.

### 5.3 Role implications

- **Coordinator:** uses the router-capable NWK archive plus coordinator compile-time behavior; forms a network, is normally trust center, manages permit-join/security policy, and must persist network/key/counter state.
- **Router:** uses the router-capable archive; joins an existing network, remains RX-on, routes traffic, may accept children, and maintains neighbor/route/child state.
- **End Device:** uses `libZStack_nwk_zed.a`; joins through a parent and may poll/sleep. It avoids network formation, trust-center, child, and routing responsibilities. This is the smallest active proof role, although an independent coordinator is required.
- **ZNP:** uses `libZStack_nwk_all.a`, NPI/MT, and no ordinary application task. It is the natural vendor host-controlled device architecture but implies a different host protocol.

## 6. RF Core and RTOS ownership comparison

### 6.1 FeralRF ownership

FeralRF's `RadioIF` is the single application radio abstraction, but internally it owns multiple global handles, RF objects, mutable generated command structures, queues, callbacks, active command IDs, session state, and mode transitions. `main_rtos.c:153-170` opens a dedicated 433-MHz handle at boot and intentionally never closes it. The RF task also initializes a separate crypto engine. IEEE, BLE, proprietary, test, and jam operations all flow through the same stateful module.

### 6.2 Z-Stack ownership

Z-Stack delegates radio operation to TIMAC. The application and higher layers do not obtain the RF handle or schedule `CMD_IEEE_*` operations themselves. MAC timing, ACK, CSMA, scans, association, polling, and data confirms are internal to the MAC library and fed back through ZMAC callbacks into NWK. Z-Stack additionally assumes its generated radio/MAC user configuration, crypto driver instances, NV, timers, OSAL heap, and task priorities.

| Resource | FeralRF | Z-Stack | Coexistence consequence |
|---|---|---|---|
| RF handles/commands | `RadioIF_switchRfMode()`, direct `RF_open/post/run/cancel/flush`, global active command/session state | TIMAC hides RF scheduling behind `MAP_MAC_*` and MAC callbacks | A shared image needs one owner or DMM interception; two independent owners are unsafe. |
| Interrupts/callbacks | RF Driver callback `RadioIF_rfCallback()` signals/updates Feral state; global driver callback performs antenna switching | MAC low-level runtime, OSAL events, semaphores, and crypto driver interrupts/callbacks | Generated interrupt priorities and callbacks must be merged and measured, not duplicated. |
| Timers/timing | RF Core triggers/RAT timestamps plus polling-loop timeout/state machines | MAC deadlines, scan/association/poll/retry timing and OSAL Port timers | Pausing the application loop does not safely pause Z-Stack; explicit in-process persona switching lacks a lifecycle contract. |
| Allocation | Static RF/data/packet queues and a 16-KiB HeapMem | OSAL message allocation, default 6-KiB heap, stack/app tasks, tables and fragmentation buffers | A combined RAM budget requires a link map and runtime high-water measurements. |
| Persistent storage | No Zigbee network/key/counter state; current linker does not reserve the P7 Zigbee NV range | NVOCMP/NVS stores configuration, keys, counters, network/commissioning state | Active Zigbee requires exclusive flash-region ownership and update/factory-reset policy. |
| Antenna/front end | Generated `rfDriverCallbackAntennaSwitching()` reacts to RF setup commands; application warns not to force DIO28/29/30 | SysConfig RF design and MAC radio setup must drive the same hardware correctly | A CatSniffer-specific generated RF design must replace LaunchPad assumptions and remain the sole authority. |
| BLE/proprietary modes | Custom BLE RF Core operations, proprietary/sub-GHz commands, test/jam modes; BLE5-Stack ICall startup is disabled at `main_rtos.c:212-218` | Z-Stack assumes ongoing MAC availability; DMM has explicit supported-client policies | Current modes switch through Feral global state and are not concurrent protocol stacks. Continuous/jam/test modes conflict directly with joined Zigbee service. |

### 6.3 Coexistence alternatives

#### A. Exclusive Z-Stack radio ownership

**Classification: FEASIBLE WITH CONDITIONS.** A single image could retain selected non-radio FeralRF host utilities while Z-Stack/TIMAC exclusively owns RF. Required changes:

- remove or compile out FeralRF `RadioIF` RF handles and all raw/BLE/proprietary RF commands while Zigbee is active;
- make the vendor startup, SysConfig, linker layout, crypto, NV, and task model authoritative;
- route host actions into a registered Zigbee application task or use NPI/MT;
- adapt CatSniffer UART, pins, RF design, antenna switching, and reset behavior.

Risk: this is no longer the current multipurpose FeralRF firmware. It is functionally a Zigbee target sharing some transport code.

#### B. Explicit switching between FeralRF raw mode and Z-Stack

**Classification: NOT CURRENTLY DEMONSTRATED and NOT RECOMMENDED.** Static source shows startup and ongoing event/timer/NV state, but no supported “destroy Z-Stack, release TIMAC, then let an unrelated RF owner take over” lifecycle. FeralRF's intentional never-close 433 handle further contradicts clean transfer. Implementing ad hoc `RF_close`/reopen would not suspend MAC timers, pending callbacks, commissioning, security counters, or OSAL messages safely.

A full reboot into another firmware persona or image slot is defensible; that is Alternative D, not in-process switching.

#### C. Simultaneous operation through supported multiprotocol architecture

**Classification: FEASIBLE WITH CONDITIONS at SDK level; NOT CURRENTLY DEMONSTRATED for FeralRF.** DMM remaps `RF_open`, `RF_postCmd`, `RF_runCmd`, scheduling, cancel, and flush calls to `DMMSch_*`. Each stack task registers a handle and a declared role, and policy controls conflicts. The SDK includes DMM Zigbee coordinator/router/end-device application material.

To use DMM, FeralRF would need to become an explicit DMM client: all RF entry points must be intercepted, persistent handles assessed, custom stack role/policy states defined, deadlines assigned, multi-mode RF patches/config generated, and every BLE/proprietary/IEEE/test/jam operation audited for scheduler compatibility. Continuous RX, jamming, and timing-sensitive Zigbee traffic inherently contend. Existing DMM examples support designed pairs of protocol applications; they do not establish support for FeralRF's arbitrary raw-radio service plus Z-Stack.

#### D. Separate firmware targets

**Classification: FEASIBLE WITH CONDITIONS and recommended for active Zigbee.** Keep current FeralRF for raw analysis and build a separate TI-supported ZED, coordinator/router app, or ZNP image. This gives one RF owner, one generated configuration, one linker/NV layout, and a testable capability boundary. Operational switching can be by explicit reflashing or a separately designed bootloader/multi-image scheme; neither is part of this audit.

## 7. Build and toolchain feasibility

### 7.1 Concrete incompatibilities in the current target

| Area | Current FeralRF | Z-Stack requirement/evidence | Consequence |
|---|---|---|---|
| Compiler | `arm-none-eabi-gcc`, hard-float M4F | Role and MAC archives installed for TI-clang and IAR; release recommends TI ARM Clang 3.2.2.LTS | Present CMake has no supported Z-Stack archive set. |
| Library selection | Manually names four GCC SDK archives | SysConfig chooses MAC + role archive from device/role/toolchain | Must use/generated equivalent complete library set. |
| Linker | GNU `.ld`; app flash extends to `0xAFFA8` | TI-clang `.cmd`; app flash ends `0xAA000`, NV occupies `0xAA000–0xADFFF`, last page begins `0xAE000` | Current layout can place code where Zigbee NV must live. |
| Generated configuration | Local SysBIOS/Drivers/radio files | `ti_zstack_config`, MAC user config, NVS, crypto, RF design, driver and RTOS configuration | Cannot use stock FeralRF generated files without regeneration/merge. |
| RTOS/task model | Two 4096-byte priority-3 polling tasks | 3072-byte priority-5 stack task, 3000-byte priority-2 app task, optional NPI task, OSAL services | Scheduler and task/heap budgets must be redesigned. |
| Persistent state | No reserved Zigbee internal NV region | NVOCMP/NVS and role/network/security records | Linker, flash erase ownership, factory reset, and migration policy required. |
| RF ownership | Direct FeralRF RF Driver calls, persistent 433 handle | TIMAC ownership or DMM interception | Cannot run independently and concurrently. |
| Crypto | FeralRF `crypto_engine_init()` | Z-Stack opens generated TRNG/AESCCM/AESECB handles and aborts on failure | Instance and peripheral ownership must be unified. |
| Project material | Standalone CMake | Pinned checkout lacks SDK example projects; only templates/source/libs | A matching full installer/example project must be imported or reconstructed. |

The TI 15.4 Stack metadata contains a GCC-to-TI-clang archive path fallback for its own generic stack generator (`ti154stack.syscfg.js:420-426`), but Z-Stack's generator does not contain that fallback and the Z-Stack `lib/gcc` directory is absent. It would be unsafe to infer ABI support for the role archives from the generic stack's behavior. A supported project and successful link are required.

### 7.2 Minimum safe verification experiments — specified, not executed

1. **Vendor baseline build:** obtain the complete matching 8.30.01.01 SDK example set and supported TI ARM Clang/SysConfig tools. Import an unmodified CC1352P7 ZED GenericApp project, generate configuration, compile, and link. Archive command lines, generated files, libraries, warnings, map, flash/RAM usage, and symbols. Do not add FeralRF yet.
2. **Board-only build:** clone that target and change only CatSniffer board/RF design, UART pins, antenna switching, CCFG, and internal NV placement. Link without flashing. This separates target support from board adaptation.
3. **GCC question:** in an isolated throwaway project, ask the matching SysConfig to generate a GCC Z-Stack role build and record its requested paths. If it requests nonexistent `zstack/lib/gcc` artifacts, treat GCC as unsupported. Do not force-link TI-clang archives unless TI documentation for this exact release explicitly supports it.

These are the smallest static/build experiments that resolve the main uncertainties without touching the working firmware or hardware.

## 8. Memory and resource considerations

The CC1352P7 linker layouts expose 704 KiB total flash (`0xB0000` address space), 144 KiB SRAM (`0x24000`), and 8 KiB GPRAM (`0x2000`). The current FeralRF GNU script permits application content through `0xAFFA8`, followed by CCFG. The Z-Stack TI-clang layout instead limits application flash to `0xAA000`, reserves 16 KiB at `0xAA000` for internal NV, and reserves the last 8 KiB page beginning at `0xAE000`.

Known static/runtime allocations include:

- FeralRF UART task stack: 4096 bytes;
- FeralRF RF task stack: 4096 bytes;
- FeralRF primary HeapMem: 16 KiB;
- Z-Stack stack task stack: 3072 bytes;
- non-ZNP application task stack: 3000 bytes;
- Z-Stack OSAL heap default: `0x1800` (6144 bytes) in `rtos/app_release.cfg`;
- ZNP omits the ordinary application task but adds NPI transport resources;
- radio queues, packet queues, route/neighbor/child/binding/key tables, timers, generated driver objects, and MAC/NWK library state add unquantified RAM;
- code, constants, ZCL clusters, NPI/MT, DMM, logging/UI, and crypto alter flash materially.

These values are not additive proof that an integrated image fits: heaps may be configured differently, and link-time removal varies by role/features. No current Z-Stack map exists. A link map plus task stack high-water and heap/NV stress measurements are mandatory before a fit claim. The separate ZED target is likely the lowest resource active persona because it uses the ZED NWK archive and can disable unused clusters, UI, Green Power proxy, touchlink, OTA, and external-flash features. “Likely” is not a measured result.

## 9. Passive Zigbee architecture without Z-Stack

### 9.1 What the current packet preserves

For each accepted IEEE packet, Python receives:

- RF RAT-derived timestamp converted to microseconds;
- channel;
- signed RSSI;
- seven-bit correlation/LQI value;
- CRC-good flag;
- MAC bytes including the two-byte FCS.

Because the MAC bytes are preserved, the host can parse the MAC frame control field, sequence-number suppression, destination/source address modes, PAN-ID compression rules, frame version, auxiliary security header, information elements where applicable, and MAC payload. The decoder must not assume every captured frame is a Zigbee data frame; it must classify beacon, data, acknowledgment, MAC command, multipurpose/reserved forms, and unsupported versions without losing bytes.

### 9.2 Parser boundaries (proposed names, not existing APIs)

The following are **proposed** Python modules and structures:

```text
feralrf/ieee802154.py
  parse_ieee802154(data, *, fcs_present) -> Ieee802154Frame
  verify_fcs(data) -> FcsResult
  Ieee802154Frame / MacAddress / AuxiliarySecurityHeader / HeaderIE

feralrf/zigbee/nwk.py
  parse_nwk(mac_payload, context) -> ZigbeeNwkFrame
  ZigbeeNwkHeader / SourceRouteSubframe / NwkSecurityHeader

feralrf/zigbee/aps.py
  parse_aps(nwk_payload, context) -> ZigbeeApsFrame
  ApsHeader / ApsSecurityHeader / fragmentation metadata

feralrf/zigbee/zdo.py
  parse_zdo(cluster_id, payload) -> typed-or-opaque ZDO record

feralrf/zigbee/zcl.py
  parse_zcl(profile_id, cluster_id, payload) -> ZclFrame
  foundation command parser + registry for selected clusters

feralrf/zigbee/security.py
  parse security headers; resolve nonce/key context; optional AES-CCM verify/decrypt
  return explicit ENCRYPTED_NO_KEY / AUTH_FAILED / DECRYPTED status

feralrf/capture.py
  emit PCAP IEEE-802.15.4-with-FCS or metadata-rich PCAPNG/TAP records
```

Every parser should return consumed bytes, remaining payload, warnings, and the original raw slice. Unknown versions, commands, clusters, and encrypted payloads must remain lossless. Layer selection should be evidence-based: IEEE frame type and payload, Zigbee NWK protocol version/frame control, APS frame control, endpoint/profile/cluster context—not a preset name.

### 9.3 Security behavior

The host can always parse the unencrypted outer MAC fields and, depending on which layer is secured, the visible NWK/APS security header: security control, frame counter, source extended address when present, key sequence/identifier, and MIC length. Without an authorized key and complete nonce context, the correct output is metadata plus opaque ciphertext/MIC. It must not guess plaintext or label authentication as successful.

Optional decryption can be entirely Python. It needs the correct network/link key, key identifier/sequence, sender extended address, security control, frame counter, and authenticated header bytes. Address-to-EUI-64 context may have to be learned from earlier device announcements or supplied by the user. The existing optional `cryptography` development dependency can provide primitives if its API is suitable; no Zigbee key acquisition mechanism is implied.

### 9.4 Existing repository and installed dependencies

- `python/feralrf/integrations/killerbee.py` is an adapter for channel control, sniffing, and raw injection. It returns `pkt.data`; it does not decode MAC or Zigbee.
- `python/feralrf/emulation/ieee154_device.py` is not a Zigbee decoder or stack.
- The core package depends only on `pyserial`, `pyserial-asyncio`, and `cobs`.
- `killerbee>=3.0.0` is declared as an optional extra, but no installed `killerbee` distribution was found; retained project evidence also reports that dependency resolution previously blocked validation.
- No installed `scapy`, `zigpy`, or `whad` distribution was found in the audit environment.
- No repository-native Zigbee parser was found.

An external parser could reduce work, but it must be separately selected, licensed, pinned, and tested against the exact with-FCS byte contract. The current evidence does not justify naming any uninstalled package as the implementation choice.

### 9.5 Firmware changes

**Required for a basic passive decoder:** none. Current CRC-good frame bytes and metadata are enough.

**Recommended contract improvements before calling the capture interface stable:** add explicit PHY/frame format and `fcs_present` metadata; define timestamp epoch/wrap behavior; document LQI derivation; add an opt-in mode to forward CRC-bad frames if forensic analysis requires them; expose dropped/overflow metrics with captures. These are future tasks, not changes made by this audit.

## 10. Active Zigbee architecture

### 10.1 Minimum selected role: Zigbee End Device

For the smallest vendor-stack proof, select **ZED** with one fixed endpoint and only the clusters needed for a test message. This choice minimizes active scope:

- uses `libZStack_nwk_zed.a`;
- joins an existing network rather than forming one;
- does not route or accept child devices;
- avoids coordinator trust-center and network-formation responsibilities;
- can initially be configured RX-on rather than adding low-power policy complexity.

It still requires the complete MAC/NWK/APS/ZDO/BDB/security runtime, an application endpoint, NV, keys/counters, joining/rejoin behavior, and a conformant coordinator.

### 10.2 Minimum subsystems

1. Vendor startup, TI-RTOS7 task, OSAL Port services, timers, and heap.
2. Generated MAC user configuration, RF design, antenna controls, and TIMAC archives.
3. ZED NWK archive, APS/AF, ZDO, BDB network steering, and security manager.
4. NVOCMP/NVS with reserved internal flash and factory-reset semantics.
5. TRNG, AESCCM, AESECB and any configured install-code/Trust Center policy.
6. One application task, endpoint/simple descriptor, cluster/attribute registrations, and indication loop; or ZNP/NPI/MT instead of a local endpoint app.
7. A host boundary for commissioning status, network state, messaging, errors, and reset.

### 10.3 Coordinator and router expansion

A coordinator adds network formation, channel/PAN selection, Trust Center policy, permit join, network/link key provisioning, and management of joined devices. A router adds always-on receiver behavior, route/neighbor/child state, forwarding, and child aging. Neither should be enabled merely because the `nwk_zr` archive exists; they are product roles with security and lifecycle obligations.

### 10.4 Host protocol decision

Existing FeralRF framing, COBS, CRC, sequence IDs, output queue concepts, and Cat-Bridge serial transport could be reused. Existing command semantics cannot: `SET_PHY`, `RX_START`, and `TX_FRAME` describe radio operations, while Z-Stack requires stateful requests and asynchronous indications.

Two valid separate-target choices are:

- **Vendor ZNP:** retain TI NPI/MT semantics and implement a new Python adapter. This maximizes vendor-path reuse and gives a broad host-controlled Zigbee surface, but replaces the FeralRF command protocol for this image.
- **Feral transport over a Z-Stack app:** keep Feral framing but introduce new Zigbee commands/events. The UART receiver queues requests to a registered Zigbee application task; that task invokes `Zstackapi_*` and emits asynchronous status/data events. It must not expose raw radio control while the stack owns RF.

Proposed semantic operations include capability/version query, network state, start/stop commissioning, factory reset, network information, send application message/ZCL command, permit join where role allows, and structured AF/ZDO/ZCL/commissioning indications. Key material APIs require a separately approved security policy and should not default to exporting secrets.

## 11. Detailed comparison of integration alternatives

| Criterion | A — passive decoder | B — selected components | C — full stack in current image | D — separate Zigbee target |
|---|---|---|---|---|
| Primary capability | Observe/decode traffic | Intended partial active/passive reuse | Active Zigbee plus existing raw modes | Active Zigbee persona |
| Feasibility | **FEASIBLE WITH CURRENT EVIDENCE** | **NOT RECOMMENDED**; active subset blocked | **BLOCKED** in current build; **NOT RECOMMENDED** | **FEASIBLE WITH CONDITIONS** |
| Reuse | Current IEEE firmware, Python transport, optional definitions | Headers/constants; few pure helpers | Many Z-Stack sources but extensive adaptation | Vendor startup, runtime, libs, role app/ZNP |
| New code | Host parsers, models, capture export, tests | Large glue/runtime reconstruction | Host bridge, full generated config, DMM or exclusive mode, linker/build port | Board adaptation, host bridge/adapter, selected app surface |
| Build dependency | Existing Python environment; optional crypto/parser dependency | TI binary libraries still needed for active path | TI-clang/IAR archives conflict with GCC target | Supported TI ARM Clang/SysConfig project |
| RF ownership | Existing FeralRF only | Unresolved for active use | Conflicting unless raw RF removed or DMM redesign | Z-Stack/TIMAC only |
| RTOS | No change | OSAL/TI-RTOS coupling appears as soon as operational code is used | Major task/scheduler/heap merge | Vendor task model authoritative |
| Memory | Host-heavy; negligible target change | Uncertain, little benefit | Highest and unmeasured | Role/features can be minimized and measured |
| Host API | Add decoder above `Packet` | Awkward split APIs | Must partition raw vs stack states | New Zigbee API or vendor MT/NPI |
| Maintenance | Low/moderate; protocol parser corpus | High; unsupported internal boundary | Very high; every SDK update reopens merge | Moderate; separate target follows vendor structure |
| Principal risk | Parser correctness and byte-contract ambiguity | Hidden binary/runtime dependency | compiler/linker/RF/NV/resource failure | board port, tool availability, interoperability |
| Validation | Golden frames + independent capture | Unit/link/runtime proof for every seam | Full build, resource, recovery, multiprotocol, interop | Vendor baseline, board build, boot/join/message/interop |

## 12. Mandatory source-code excerpts and interpretation

Each excerpt is short and is followed by its technical consequence.

### 12.1 FeralRF IEEE backend selection

**Path:** `firmware/cc1352/src/phy_manager.c:16-24`  
**Structure:** `s_phy_profiles`

```c
{PHY_MANAGER_PHY_BLE_1M, PHY_MANAGER_LL_BLE, true, true},
...
{PHY_MANAGER_PHY_IEEE_802_15_4, PHY_MANAGER_LL_DEFAULT, false, true},
{PHY_MANAGER_PHY_SUB_1GHZ_868, PHY_MANAGER_LL_DEFAULT, false, true},
```

**Interpretation:** IEEE selects the generic pass-through link-layer profile.  
**Integration consequence:** no existing dispatch point invokes a Zigbee MAC or decoder; adding a preset is insufficient.

### 12.2 FeralRF IEEE RF Core RX setup

**Path:** `firmware/cc1352/src/radio_if.c:1347-1363`  
**Function:** `RadioIF_startIeee154RfBackend`

```c
Ieee154_0_cmdIeeeRx.pRxQ = &s_rf_data_queue;
Ieee154_0_cmdIeeeRx.rxConfig.bIncludePhyHdr = 1u;
Ieee154_0_cmdIeeeRx.rxConfig.bIncludeCrc = 1u;
Ieee154_0_cmdIeeeRx.rxConfig.bAppendRssi = 1u;
Ieee154_0_cmdIeeeRx.rxConfig.bAppendCorrCrc = 1u;
Ieee154_0_cmdIeeeRx.rxConfig.bAppendTimestamp = 1u;
...
RadioIF_switchRfMode(&Ieee154_0_mode,
                     (RF_RadioSetup *)&Ieee154_0_cmdRadioSetup);
```

**Interpretation:** FeralRF uses native IEEE RF commands and asks RF Core to preserve PHR/FCS and metadata.  
**Integration consequence:** the existing backend is sufficient as a passive capture bearer; it is not TIMAC.

### 12.3 FeralRF RX frame extraction and FCS preservation

**Path:** `firmware/cc1352/src/radio_if.c:1521-1559`  
**Function:** `RadioIF_processIeee154Packets`

```c
frame_with_hdr_len = (uint16_t)(entry_len - IEEE_15_4_APPENDED_TOTAL_LEN);
expected_with_hdr_len = (uint16_t)(1u + entry_data[0]);
...
pkt.crc_ok = (corrcrc & IEEE_15_4_CORRCRC_CRC_ERR_BIT) == 0u;
if (!pkt.crc_ok) { ... continue; }
pkt.data_len = entry_data[0];
memcpy(pkt.data, &entry_data[1], pkt.data_len);
```

**Interpretation:** the PHY length byte is removed; that many following bytes—including FCS because RX configured it—are forwarded. CRC failures are discarded.  
**Integration consequence:** Python must parse `Packet.data` as with-FCS IEEE data and must not expect bad frames.

### 12.4 FeralRF TX FCS handling

**Path:** `firmware/cc1352/src/smartrf_ieee_15_4_0.c:146-161`  
**Structure:** `Ieee154_0_cmdIeeeTx`

```c
.txOpt.bIncludePhyHdr = 0x0,
.txOpt.bIncludeCrc = 0x0,
.payloadLen = 0x00,
.pPayload = 0,
```

**Interpretation:** TI's command contract makes RF Core generate PHR and calculated FCS.  
**Integration consequence:** host TX accepts MPDU without FCS; this still lacks active MAC scheduling, ACK, retry, and Zigbee state.

### 12.5 FeralRF task and persistent RF ownership

**Path:** `firmware/cc1352/src/main_rtos.c:141-175`  
**Function:** `RfTask_taskFxn`

```c
if (!crypto_engine_init()) { ... }
...
RF_Handle h = RF_open(RadioIF_get433Object(), &Prop0_mode433,
                      (RF_RadioSetup *)&Prop0_cmdPropRadioDivSetup433, &rfParams);
if (h != NULL) {
    RF_postCmd(h, (RF_Op *)&Prop0_cmdFs433, RF_PriorityNormal, NULL, 0);
    RadioIF_set433Handle(h);
}
DataTask_init();
while (1) { DataTask_poll(); Task_yield(); }
```

**Interpretation:** the RF task directly initializes crypto and opens a long-lived RF handle outside a stack arbiter.  
**Integration consequence:** TIMAC cannot be added as an unrelated concurrent RF owner.

### 12.6 FeralRF packet forwarded unchanged above RF

**Path:** `firmware/cc1352/src/ll_manager.c:144-159`  
**Function:** `LLManager_processRxPacket`

```c
switch (s_selected_profile) {
case LL_MANAGER_BLE:
    return LLManager_processBlePacket(pkt);
case LL_MANAGER_DEFAULT:
default:
    return true;
}
```

**Interpretation:** non-BLE frames receive no link/network processing.  
**Integration consequence:** all Zigbee layers are absent, while host parsing can be added cleanly above the existing packet.

### 12.7 Z-Stack main initialization

**Path:** `source/ti/zstack/startup/main.c:341-357,371-387`  
**Function:** `main`

```c
NVOCMP_loadApiPtrs(&zstack_user0Cfg.nvFps);
if(zstack_user0Cfg.nvFps.initNV) {
    zstack_user0Cfg.nvFps.initNV(NULL);
}
stackTask_init(&zstack_user0Cfg);
...
taskParams.stack = myTaskStack;
taskParams.stackSize = APP_TASK_STACK_SIZE;
taskParams.priority = 2;
Task_construct(&myTask, taskFxn, &taskParams, NULL);
```

**Interpretation:** NV, a dedicated stack task, and normally an application task are startup requirements.  
**Integration consequence:** Z-Stack is not initialized by adding a library and calling a single radio function.

### 12.8 Z-Stack service graph

**Path:** `source/ti/zstack/startup/zstackstartup.c:380-451`  
**Function:** `stackServiceFxnsInit`

```c
tmpServiceId = OsalPort_registerTask(stackTaskHndl, stackSemHandle, &macServiceEvents);
MAP_macMainSetTaskId(tmpServiceId);
...
nwk_init(tmpServiceId);
...
APS_Init(tmpServiceId);
...
ZDApp_Init(tmpServiceId);
...
bdb_Init(tmpServiceId);
...
ZStackTaskInit(stackServiceId);
```

**Interpretation:** MAC through BDB/API are services in one ordered OSAL runtime.  
**Integration consequence:** operational layers are not independently pluggable into the FeralRF polling loop.

### 12.9 Z-Stack MAC interface reaches binary TIMAC

**Path:** `source/ti/zstack/zmac/f8w/zmac.c:506-519`  
**Function:** `ZMacInit`

```c
MAP_MAC_Init();
if (ZG_BUILD_RTR_TYPE) {
    MAP_MAC_InitCoord();
}
if (ZG_BUILD_JOINING_TYPE) {
    MAP_MAC_InitDevice();
}
```

**Interpretation:** visible ZMAC source is an adapter into `MAP_MAC_*` implementation supplied by MAC libraries.  
**Integration consequence:** ZMAC source alone cannot replace FeralRF raw commands with an active MAC.

### 12.10 MAC-to-NWK coupling

**Path:** `source/ti/zstack/zmac/f8w/zmac_cb.c:383-393`  
**Function:** MAC event callback path

```c
if(gpDataCnf == true) {
    OsalPort_msgSend(gp_TaskID, (uint8_t *)msgPtr);
} else {
    OsalPort_msgSend(NWK_TaskID, (uint8_t *)msgPtr);
}
```

**Interpretation:** MAC confirmations/indications are queued directly to registered higher-layer service IDs.  
**Integration consequence:** a standalone MAC shim would still need the OSAL service graph and ownership rules.

### 12.11 Z-Stack application API is message/task based

**Path:** `source/ti/zstack/stack/api/zstackapi.c:125-168`  
**Function:** `sendReqDefaultRsp`

```c
pMsg = (zstackmsg_genericReq_t *)OsalPort_msgAllocate(msgSize);
...
OsalPort_msgSend(stackServiceTaskId, (uint8_t *)pMsg);
...
OsalPort_blockOnEvent(Task_self());
pCmdStatus = (zstackmsg_genericReq_t *)
    OsalPort_msgFindDequeue(appServiceTaskId, pMsg->hdr.event);
```

**Interpretation:** public API wrappers require an OSAL-registered caller task and block for a stack response.  
**Integration consequence:** FeralRF host commands must be marshalled into a Zigbee application task, not invoked arbitrarily from UART polling context.

### 12.12 Generic application initialization and commissioning

**Path:** `source/ti/zstack/apps/genericapp/zcl_genericapp.c:363-431`  
**Function:** `zclGenericApp_Init`

```c
zclport_registerEndpoint(appServiceTaskId, &zclGenericAppEpDesc);
zclGeneral_RegisterCmdCallbacks(GENERICAPP_ENDPOINT,
                                &zclGenericApp_CmdCallbacks);
zcl_registerAttrList(GENERICAPP_ENDPOINT,
                     zclGenericApp_NumAttributes, zclGenericApp_Attrs);
SetupZStackCallbacks();
zstack_bdbStartCommissioningReq.commissioning_mode = 0;
Zstackapi_bdbStartCommissioningReq(appServiceTaskId,
                                   &zstack_bdbStartCommissioningReq);
```

**Interpretation:** endpoint/cluster registration, callbacks, and BDB restore/start are application responsibilities.  
**Integration consequence:** active Zigbee needs a defined application profile and lifecycle, not only a radio role.

### 12.13 Role-specific binary dependency

**Path:** `source/ti/zstack/.meta/zstack.syscfg.js:207-226`  
**Function:** `getLibs`

```javascript
case "zc":  devType = "nwk_zr"; break;
case "znp": devType = "nwk_all"; break;
...
devType = "nwk_" + devType;
...
zlib += toolchain + "/" + isa + "/";
zlib += "libZStack_" + devType + ".a";
results.libs.push(zlib);
```

**Interpretation:** role and compiler select a precompiled core archive.  
**Integration consequence:** visible sources are not a compiler-independent full stack; current GCC has no matching installed Z-Stack path.

### 12.14 Zigbee NV linker reservation

**Path:** `source/ti/zstack/boards/cc13x2x7_cc26x2x7/cc13x2x7_cc26x2x7_tirtos7_ticlang.cmd:69-96`  
**Structure:** memory map

```c
#define FLASH_SIZE      0xAA000
#define FLASH_NV_BASE   0xAA000
#define FLASH_NV_SIZE   0x4000
#define FLASH_LAST_BASE 0xAE000
#define FLASH_LAST_SIZE 0x2000
...
FLASH_NV (RWX) : origin = FLASH_NV_BASE, length = FLASH_NV_SIZE
```

**Interpretation:** active Z-Stack reserves a fixed 16-KiB internal NV region on P7.  
**Integration consequence:** FeralRF's current flash-through-`0xAFFA8` GNU layout is incompatible until repartitioned.

### 12.15 DMM is explicit RF interception, not automatic coexistence

**Path:** `source/ti/dmm/dmm_rfmap.h:64-77`  
**Structure:** RF API remapping

```c
#define RF_open           DMMSch_rfOpen
#define RF_close          DMMSch_rfClose
#define RF_postCmd        DMMSch_rfPostCmd
#define RF_runCmd         DMMSch_rfRunCmd
#define RF_scheduleCmd    DMMSch_rfScheduleCmd
#define RF_cancelCmd      DMMSch_rfCancelCmd
#define RF_flushCmd       DMMSch_rfFlushCmd
```

**Interpretation:** supported multiprotocol operation intercepts every relevant RF call and associates commands with registered clients/policy.  
**Integration consequence:** current direct FeralRF RF usage is not simultaneously safe merely because DMM source exists.

## 13. Exact integration points

### Passive path

- Attach decoding immediately above `Packet.data` returned by `Radio.read_packets()` (`radio.py:636-709`), not inside the serial protocol.
- Preserve `Packet` as the raw record; create decoded wrapper/result objects so unknown or malformed data remains recoverable.
- Treat `Packet.data[-2:]` as FCS only when the capture metadata/session declares IEEE-with-FCS.
- Reuse `KillerBeeFeralRF` only as a raw capture adapter or PCAP integration boundary; do not treat it as the decoder.
- No change is needed in `PhyManager`, `LLManager`, or `RadioIF` for first-pass good-frame parsing.

### Active separate target

- Base startup on `source/ti/zstack/startup/main.c` and `zstackstartup.c`, with role-specific SysConfig and libraries.
- For ZED app: adapt `apps/genericapp/zcl_genericapp.c` patterns, reduce endpoint/cluster surface, and queue host commands to its OSAL-registered application task.
- For ZNP: use `startup`'s `ZNP_NPI` branch plus `npi/`, `mt/`, `sysconfig/znp.syscfg`, and implement a Python MT/NPI client.
- Adapt CatSniffer board/RF configuration rather than importing LaunchPad display/button/external-flash instances blindly.
- Reserve P7 internal NV at `0xAA000/0x4000`; establish factory reset, image upgrade, and state migration policy.
- Make the TI RF design/antenna-switch callback authoritative and validate CatSniffer front-end pins for 2.4 GHz.
- Reuse Feral COBS/CRC framing only if choosing a custom application host protocol; do not layer it on NPI without a clearly defined encapsulation.

## 14. Required development tasks by viable alternative

### Alternative A — passive decoder

**Reusable:** current IEEE RF firmware; `Radio`, `Packet`, command transport; KillerBee raw adapter concepts; optional `cryptography` dependency.

**Modify later:** `python/feralrf/radio.py` only if adding an ergonomic decode API; packaging for any selected parser/PCAP/crypto dependency; firmware protocol only for explicit capture-format metadata or bad-frame mode.

**New proposed modules:** IEEE MAC parser/FCS verifier; NWK, APS, ZDO, ZCL, and security parsers; parse-context/address map; PCAP/PCAPNG exporter; golden-vector corpus; malformed/fuzz tests.

**Structures:** raw slice + parsed layer objects, explicit security status, address/PAN types, warnings/errors that retain undecoded bytes.

**Hardware prerequisites:** none for unit parsing; later, one CatSniffer plus an independent sniffer/source for capture equivalence.

**Validation:** known with-FCS IEEE vectors; Zigbee plaintext and secured fixtures; Wireshark/independent decoder comparison; truncated/unknown frame behavior; optional authorized decryption vectors.

### Alternative D1 — separate minimal ZED application

**Reusable:** SDK startup, ZMAC, role archive, ZDO/BDB/ZCL/AF/API sources, OSAL Port, NVOCMP, TI Drivers/RTOS, GenericApp patterns; selected Feral serial framing/transport code after task-safe adaptation.

**Modify/adapt:** role SysConfig; board/RF/antenna/UART/CCFG; endpoint/simple descriptor/clusters; application task; linker/NV layout; production security defaults; factory reset.

**New modules:** Zigbee-host command queue/dispatcher; structured event encoder; Python `ZigbeeRadio` or equivalent client; capability/version negotiation; recovery/watchdog reporting.

**Build:** separate TI ARM Clang/SysConfig project and artifact name; no inclusion in current FeralRF executable until the vendor baseline is proven.

**Hardware prerequisites:** CatSniffer RF front end correctly configured; independent coordinator; later RF capture and second Zigbee device.

**Validation:** vendor baseline link; board-port link; boot/NV; join/rejoin; authorized key/counter persistence; application message both directions; reset/power-cycle recovery; independent sniffer trace.

### Alternative D2 — separate ZNP image

**Reusable:** SDK ZNP SysConfig, `NPI`, `MT`, ZStack API/task, `nwk_all` archive, startup branch without ordinary app task.

**Modify/adapt:** NPI UART/SPI binding to Cat-Bridge, board/RF/antenna/CCFG/NV, transport reset/flow control, security defaults.

**New modules:** Python NPI frame codec and MT command/event client, device discovery/persona detection, configuration and error mapping.

**Build/validation:** same vendor-toolchain and board steps as D1, plus serial conformance, asynchronous event load, reset synchronization, and host command coverage.

**Tradeoff:** broad host control and vendor semantics at the cost of a new Python protocol and a firmware persona distinct from FeralRF.

### Conditional future DMM target

This is not presently a viable first implementation. Before coding, define exactly which two radio clients must coexist and forbid unsupported operations. Then generate a vendor DMM baseline, register every client/task/role, map all RF APIs, define policy states/deadlines, remove or redesign the persistent 433 handle, budget memory, and prove recovery under collisions. Jam/test/continuous RX modes should default to mutually exclusive with active Zigbee.

## 15. Technical risks

1. **False completeness:** visible ZDO/ZCL sources can obscure that core MAC/NWK/APS behavior is binary and role/compiler coupled.
2. **Unsupported ABI:** forcing TI-clang archives into GCC could link partly yet fail through ABI, runtime, section, or configuration assumptions.
3. **Flash corruption:** current application placement overlaps the Z-Stack internal NV address range.
4. **RF double ownership:** independent RF handles and commands can break MAC timing, callbacks, patches, RAT synchronization, or recovery.
5. **Task deadlock:** `Zstackapi_*` blocks an OSAL-registered task awaiting a response; use from the wrong context can stall the host or stack.
6. **Crypto collision:** duplicate TRNG/AES handle assumptions or interrupt configuration can abort Z-Stack or corrupt security behavior.
7. **Security lifecycle:** key storage, frame-counter monotonicity, factory reset, rejoin, image update, and secret export require explicit policy.
8. **Resource exhaustion:** route/neighbor/binding tables, OSAL messages, capture queues, UART events, and ZCL payloads compete for RAM; no integrated map exists.
9. **Board mismatch:** LaunchPad SysConfig templates reference LEDs, buttons, display UART, external flash, and RF design components not automatically valid for CatSniffer.
10. **Maintenance divergence:** a custom partial stack or monolithic port creates a high-cost fork from TI's generated project and update model.
11. **Passive parser ambiguity:** PAN compression, frame-version differences, security boundaries, fragmentation, and unknown clusters must be parsed defensively.
12. **Capability confusion:** raw TX, decoded capture, ZED participation, coordinator behavior, and ZNP control must be reported as distinct capabilities.

## 16. Validation strategy — not executed

### A. Correct passive Zigbee packet identification

- **Static evidence:** verify with-FCS contract and layer-dispatch rules.
- **Unit:** CRC-known IEEE frames covering data/beacon/command/ACK and non-Zigbee payloads; require no false “Zigbee” label from PHY alone.
- **Capture:** replay a licensed fixture corpus and compare packet boundaries/channel/FCS with an independent decoder.

### B. Correct Zigbee header decoding

- **Unit/golden:** NWK data/command, source routing, APS data/command/ack, fragmentation, ZDO clusters, ZCL global and cluster-specific headers.
- **Negative:** truncation at every byte, reserved bits, unsupported versions, malformed lengths, unknown clusters; raw bytes must remain available.
- **Independent comparison:** field-for-field comparison with Wireshark or another qualified implementation.

### C. Correct encrypted-frame handling without assuming keys

- Parse visible security metadata and report ciphertext/MIC length.
- With no key, return `encrypted/no key`, never guessed plaintext.
- With known authorized test keys, use published/golden nonce, CCM authentication, and wrong-key/wrong-counter negative vectors.
- Test cases where source EUI-64 context is missing; require an explicit unresolved result.

### D. Successful Z-Stack initialization

- **Build:** supported vendor project, generated files, correct role/MAC archives, clean link, map/NV assertions.
- **Runtime later:** boot logs/status, stack/app task alive, crypto handles valid, NV initialized, no assert, network state indication.
- **Recovery:** repeated reset/power cycle and factory reset with state transitions recorded.

### E. Successful joining or formation

- For selected ZED, join a controlled independent coordinator using approved credentials, then power-cycle and verify restore/rejoin and counter continuity.
- If coordinator is later selected, verify channel selection, formation, Trust Center policy, permit-join window, key transport/install-code policy, and persistence.

### F. Reliable independent-device communication

- Send and receive a minimal chosen cluster command with a non-TI independent Zigbee peer.
- Correlate host request, stack confirm, over-the-air frame, peer response, and application indication.
- Include retries, loss, duplicate suppression, maximum chosen payload, and long-running message tests.

### G. Safe RF ownership and recovery

- Exclusive target: assert no FeralRF RF handle opens; induce RX/TX failures and verify stack recovery.
- DMM target if ever attempted: trace client registration, policy state, scheduled commands, denials/preemption, and recovery. Stress conflicting deadlines and forbid jam/test modes while joined unless explicitly designed.
- After any RF error, verify continued join state, monotonic security counters, and successful independent-peer messaging.

Static evidence and unit tests can prove parser and build properties. Only a link proves library/configuration compatibility. Only hardware boot proves initialization on CatSniffer. Only an independent Zigbee device and RF capture prove interoperability. None of these later stages was run in this audit.

## 17. Unresolved questions

1. What product capability is actually required: passive decoding, active endpoint, coordinator/lab controller, router, or general host-controlled ZNP?
2. Which Zigbee specification/profile and exact clusters must be supported, and is certification in scope?
3. Is a separate reflashed firmware acceptable, or is field-selectable multi-image boot required?
4. Can the matching full SDK example set, TI ARM Clang 3.2.2.LTS (or TI-supported compatible version), CCS/project metadata, and SysConfig 1.21.1 be made reproducible in CI?
5. Does TI explicitly support these Z-Stack TI-clang archives from GNU GCC in SDK 8.30.01.01? Current installed metadata does not demonstrate it.
6. What are flash/RAM/stack high-water values for minimal ZED, ZNP, and any DMM candidate on CC1352P7?
7. Which CatSniffer hardware revision, antenna switch pins, PA path, RF design, UART pins, and CCFG MAC provisioning must the target use?
8. What is the NV layout and factory-reset/update policy, including protection of keys and monotonic counters?
9. Should the host protocol follow vendor MT/NPI or preserve Feral framing with a narrower custom Zigbee API?
10. Are key provisioning/decryption/export features allowed, and under what authorization and secret-handling policy?
11. Does passive capture need CRC-bad frames, channel hopping, hardware timestamps mapped to host time, or PCAPNG/TAP metadata?
12. Which independent coordinator/device and golden capture corpus will be the interoperability reference?

## 18. Source register

### FeralRF current source

| Evidence | Path and lines |
|---|---|
| Python packet model/API | `python/feralrf/radio.py:18-30,455-478,599-750` |
| Python command payloads | `python/feralrf/commands.py:17-34,42-66` |
| Optional KillerBee raw adapter | `python/feralrf/integrations/killerbee.py:1-128` |
| PHY/LL selection | `firmware/cc1352/src/phy_manager.c:9-58`; `ll_manager.c:136-159` |
| Command dispatch | `firmware/cc1352/src/command_processor.c:128-258` |
| Task ownership | `firmware/cc1352/src/main_rtos.c:54-77,110-178,212-235` |
| RF task and packet forwarding | `firmware/cc1352/src/data_task.c:28-137` |
| IEEE channel/TX/RX | `firmware/cc1352/src/radio_if.c:428-488,1194-1250,1335-1371,1497-1568` |
| IEEE RF commands | `firmware/cc1352/src/smartrf_ieee_15_4_0.c:60-163` |
| Packet structure | `firmware/cc1352/include/radio_if.h:14-35` |
| Current source/link set | `firmware/cc1352/CMakeLists.txt:3-48,75-125,129-184,215-253` |
| Current memory layout | `firmware/cc1352/linker/cc1352p7.ld:33-47,247-271` |
| Python dependencies | `python/pyproject.toml:31-45` |

### Pinned TI SDK

| Evidence | Path and lines |
|---|---|
| SDK capabilities/recommended tools | `release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html:55,118-123` |
| Z-Stack API version | `source/ti/zstack/stack/api/zsversion.h:67-69` |
| Main/NV/task startup | `source/ti/zstack/startup/main.c:125-155,290-398` |
| Stack task and service graph | `source/ti/zstack/startup/zstackstartup.c:149-203,289-315,365-453,478-525` |
| Crypto/MAC initialization | `source/ti/zstack/startup/zstackstartup.c:574-749` |
| ZMAC adapter | `source/ti/zstack/zmac/f8w/zmac.c:498-565,1014-1065,1129-1205` |
| MAC callbacks into NWK | `source/ti/zstack/zmac/f8w/zmac_cb.c:330-404` |
| ZStack app API coupling | `source/ti/zstack/stack/api/zstackapi.c:100-170,780-835,1349-1360` |
| ZStack task callbacks/events | `source/ti/zstack/stack/api/zstacktask.c:666-722,743-862` |
| Generic app task/endpoint/BDB | `source/ti/zstack/apps/genericapp/zcl_genericapp.c:258-300,355-432,541-668` |
| ZDO | `source/ti/zstack/stack/zdo/zd_app.c:295-333`; related `zd_*` sources |
| BDB | `source/ti/zstack/stack/bdb/bdb.c:290` and adjacent BDB sources |
| Security | `source/ti/zstack/stack/zdo/zd_sec_mgr.c`; startup crypto lines above |
| Role/library selection | `source/ti/zstack/.meta/zstack.syscfg.js:44-67,149-226` |
| ZED role and P7 NV | `source/ti/zstack/sysconfig/zed_genericapp.syscfg:162-173` |
| ZNP role/config | `source/ti/zstack/sysconfig/znp.syscfg:37-70,122-165,176-180` |
| P7 Z-Stack linker layout | `source/ti/zstack/boards/cc13x2x7_cc26x2x7/cc13x2x7_cc26x2x7_tirtos7_ticlang.cmd:55-120` |
| OSAL heap default | `source/ti/zstack/rtos/app_release.cfg:680-758` |
| RF Core FCS semantics | `source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:108-115,289-293` |
| DMM RF interception | `source/ti/dmm/dmm_rfmap.h:64-77`; `dmm_scheduler.h:88-117` |
| DMM Zigbee registration | `source/ti/dmm/apps/common/zstack/source/main.c:560-599` |
| Binary inventories | `source/ti/zstack/lib/{ticlang,iar}/`; `source/ti/ti154stack/lib/{ticlang,iar}/m4f/` |

## Technical decision

**Can FeralRF realistically leverage the installed Z-Stack implementation?**  
Yes, but not by treating `source/ti/zstack/` as a drop-in library for the present image. It can leverage the installed implementation as the basis of a **separate vendor-structured active-Zigbee firmware target**. It can also reuse protocol definitions selectively, but passive analysis does not need the embedded stack.

**For which Zigbee capabilities?**  
The current FeralRF image can support passive IEEE/MAC and Zigbee NWK/APS/ZDO/ZCL/security-header decoding in Python. A separate Z-Stack target can support joining, commissioning, security, addressing, messaging, and—if deliberately selected—ZED, router, coordinator, or ZNP behavior.

**Under which architecture?**  
Use the current raw IEEE firmware plus host parsers for passive analysis. Use a separate TI ARM Clang/SysConfig ZED or ZNP image for active Zigbee. Do not use independent simultaneous RF ownership. Consider DMM only as a later, separately designed multiprotocol product after the exclusive target is proven.

**What must be modified?**  
Passive work needs new Python parsers/models/export and, preferably, an explicit capture/FCS metadata contract. Active work needs a new build target, supported toolchain and role archives, generated board/RF/RTOS/crypto/NVS configuration, reserved NV linker layout, CatSniffer antenna/UART/CCFG adaptation, a Zigbee application or ZNP host boundary, security lifecycle, and a new semantic Python API or MT/NPI client.

**What evidence is still required before implementation?**  
A reproducible unmodified vendor CC1352P7 build; a CatSniffer board-only link and map; exact flash/RAM/task/heap budgets; confirmed toolchain/library support; boot/NV/crypto initialization; antenna/RF ownership proof; ZED join or network formation according to the selected role; bidirectional communication with an independent Zigbee device; and reset/power-loss/rejoin/security-counter recovery. Until those exist, full active integration is **feasible with conditions**, not demonstrated.
