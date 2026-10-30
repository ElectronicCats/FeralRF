# Z-Stack Partial Reuse and Zigbee Decoder Audit

Date: 2026-10-30  
Target: CC1352P7  
Scope: source-level investigation only; no build, flash, RF experiment, dependency installation, or implementation was performed.

## 1. Executive technical findings

This audit uses the following evidence labels:

- **CONFIRMED BY CURRENT SOURCE**: directly established by the checked-out FeralRF source.
- **CONFIRMED BY PINNED SDK SOURCE**: directly established by SimpleLink SDK 8.30.01.01 source or packaged artifacts.
- **TECHNICALLY PLAUSIBLE BUT UNVERIFIED**: the source contract permits the conclusion, but it has not been demonstrated against a pinned implementation or capture corpus.
- **BLOCKED BY IDENTIFIED DEPENDENCY**: a named unavailable or runtime-coupled dependency prevents the claimed use.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE**: the available source is insufficient for the claim.

The principal findings are:

1. **CONFIRMED BY CURRENT SOURCE** - The current firmware already delivers CRC-valid IEEE 802.15.4 PSDUs to Python with the one-byte PHY length removed and the two-byte FCS retained. `Packet.data` therefore contains the complete received MAC frame, including encrypted Zigbee payload bytes and the received FCS. No firmware change is needed for passive, single-channel Zigbee parsing of frames that pass CRC.

2. **CONFIRMED BY PINNED SDK SOURCE** - There is no independently reusable NWK or APS decoder in the visible Z-Stack source. The normal parsers, `NLDE_ParseMsg()` and `APSDE_ParseMsg()`, are declared but not defined in the source tree; the SDK's configured architecture links role-specific TI ARM Clang/IAR Z-Stack libraries for the NWK/APS runtime. No GCC variant is present.

3. **CONFIRMED BY PINNED SDK SOURCE** - A small subset of ZCL code is byte-oriented: `zclParseHdr()`, the static `zclBuildHdr()`, `zclGetDataTypeLength()`, `zclGetAttrDataLength()`, and `zclSerializeData()`. These functions are not a standalone decoder. Several have no input or output capacity parameter, use Z-Stack types or memory macros, and reside in a translation unit that also implements AF/APS/OSAL-driven processing. Their algorithms are candidates for extraction and hardening, not for compiling `zcl.c` unchanged.

4. **CONFIRMED BY PINNED SDK SOURCE** - The `ZDO_Parse*` routines are valuable descriptions of ZDO wire fields, but they assume that AF/APS already validated and delimited an ASDU. Most accept no buffer length; several trust count fields before allocating or iterating. `zd_object.c` itself is tied to ZDO, AF, NWK, BDB, NV, address management, and OSAL. Direct reuse as a hostile-packet parser is unsafe.

5. **BLOCKED BY IDENTIFIED DEPENDENCY** - AF, ordinary NWK/APS processing, ZMAC/TIMAC, the Z-Stack API, BDB, active security, joining, routing, and endpoint behavior require the operational Z-Stack runtime. They depend on OSAL Port tasks and messages, TI-RTOS services, global stack state, role configuration, TIMAC and Z-Stack archives, RF ownership, timers, heap, and, for persistent/security behavior, NV and crypto drivers.

6. **CONFIRMED BY CURRENT SOURCE** - A meaningful intermediate architecture exists between raw IEEE 802.15.4 and a full Zigbee stack: a host-side passive decode pipeline. It can parse MAC, NWK, APS, ZDO, ZCL, and security headers; preserve ciphertext and raw offsets; and optionally maintain address, key, fragmentation, transaction, topology, and attribute state. It does not join, route, acknowledge, commission, or behave as a Zigbee node.

7. **TECHNICALLY PLAUSIBLE BUT UNVERIFIED** - With supplied keys and sufficient nonce/address context, host code can perform Zigbee authenticated decryption without network participation. An isolated encrypted packet is not always sufficient: short-to-IEEE-address mappings, key sequence/material, security level, prior transport-key traffic, or fragmented content can be missing.

8. **CONFIRMED BY CURRENT SOURCE** - Of the compared in-process approaches, an independent Python decoder using the standard library introduces the fewest new package dependencies. An external Wireshark/PCAP workflow adds no FeralRF Python runtime dependency but is not an in-process decoder API. Scapy offers useful existing dissectors, with documented coverage gaps. KillerBee, zigpy, and WHAD do not currently provide a source-confirmed, direct, complete decoder for FeralRF's `Packet.data` contract.

These findings establish reuse and integration boundaries. They are not evidence that any proposed decoder has been implemented or validated in FeralRF.

## 2. Verified baseline

### 2.1 Repository and SDK identity

| Item | Verified value | Evidence and classification |
|---|---|---|
| FeralRF repository revision | `8a4a8d8194da7f4b2b5c5a4a221b749cc2fa882b` | Git object in the checkout, which was clean before this report was added; **CONFIRMED BY CURRENT SOURCE** |
| Revision used by the two 2026-10-27 audits | `3409955011ebacc448e96605780560cf8118b364` | The current commit adds only the two audit documents; the firmware and Python source under investigation are unchanged from that parent; **CONFIRMED BY CURRENT SOURCE** |
| SDK submodule revision | `5b31d0a4903351e544546e23ef3330eaa4291ceb` | Submodule object, tagged `lpf2-8.30.01.01`; **CONFIRMED BY CURRENT SOURCE** |
| SDK tree | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` | Full `source/ti/zstack` and SysConfig metadata are present; SDK example projects are not present in this checkout; **CONFIRMED BY CURRENT SOURCE** |
| Active processor configuration | CC1352P7, `DeviceFamily_CC13X2X7`, Cortex-M4F hard-float | `firmware/cc1352/CMakeLists.txt:3-18,32-48`; **CONFIRMED BY CURRENT SOURCE** |
| Active compiler/linker family | GNU Arm Embedded GCC and GNU linker flags | `firmware/cc1352/CMakeLists.txt:32-48`; **CONFIRMED BY CURRENT SOURCE** |
| Z-Stack/TIMAC library families in SDK | `iar` and `ticlang`, no `gcc` directory | `source/ti/zstack/lib/{iar,ticlang}` and `source/ti/ti154stack/lib/{iar,ticlang}`; **CONFIRMED BY PINNED SDK SOURCE** |

The investigation shell did not resolve `arm-none-eabi-gcc`, `tiarmclang`, `iarbuild`, or `sysconfig_cli.bat` on `PATH`. That observation is not a build result and does not alter the configured GCC target.

### 2.2 Active target composition

The active `APP_SOURCES` contain FeralRF radio, protocol, task, crypto, and support code, not Z-Stack, TIMAC, OSAL Port, NVOCMP, or Z-Stack generated configuration (`firmware/cc1352/CMakeLists.txt:129-148`). Include directories and linked libraries likewise contain TI drivers, DriverLib, RF, and TI-RTOS support but no Z-Stack or TIMAC libraries (`:81-125,240-253`). The only target definitions are device and local logging/configuration definitions, not the Z-Stack role and feature definitions (`:225-237`).

`firmware/cc1352/syscfg/ti_sysbios_config.h:36` configures a 0x1000-byte BIOS heap. This is not the Z-Stack OSAL heap. The SDK OSAL heap manager defaults separately to 3072 bytes unless overridden (`source/ti/ti154stack/common/heapmgr/rtos_heaposal.h:117-119`).

## 3. Function-level Z-Stack reuse analysis

### 3.1 Reuse matrix

`None` below means no direct dependency in the isolated algorithm. It does not mean that compiling the original containing translation unit is dependency-free.

| Component and exact interface | Actual function | Direct/important transitive dependencies | Runtime services in original use | Reuse classification | Assumption that fails in FeralRF; minimum technical adaptation | FeralRF boundary |
|---|---|---|---|---|---|---|
| `zclParseHdr()` in `stack/zcl/zcl.c:2412-2446`; `zclFrameHdr_t` in `zcl.h:825-842` | Reads ZCL frame control, optional manufacturer code, transaction sequence, and command ID | ZCL types, `zcl_memset`, byte macros; containing file includes AF, APS groups, BDB/GP, OSAL Port | No allocator/timer/NV/RF in the isolated body; whole `zcl.c` has OSAL/AF/global dependencies | **Limited adaptation; CONFIRMED BY PINNED SDK SOURCE** | It has no available-length argument. A manufacturer-specific truncated header can be over-read even though the caller checks only a three-byte minimum. Replace bitfield reliance with masks and accept `(bytes, length)` | Host ZCL decoder after APS plaintext; firmware-only use would have to be an isolated codec, not `zcl.c` |
| static `zclBuildHdr()` in `zcl.c:2459-2482` | Serializes a ZCL header | ZCL header types and byte stores | None in body | **Limited adaptation; CONFIRMED BY PINNED SDK SOURCE** | No destination capacity and not public. Use explicit flags and a bounded writer | Host encoder, or an isolated firmware encoder before a separately supplied APS/NWK/MAC envelope |
| `zclGetDataTypeLength()` in `zcl.c:3195-3283` | Returns fixed length for fixed-width ZCL data types | ZCL data-type constants | None in body | **Algorithm independently reusable; CONFIRMED BY PINNED SDK SOURCE** | Compiling `zcl.c` to obtain it imports an inappropriate translation unit. Extract the lookup with only protocol constants | Host ZCL value decoder/schema support |
| `zclGetAttrDataLength()` in `zcl.c:3296-3322` | Derives encoded attribute value length, including short and long strings | `zclGetDataTypeLength()`, ZCL string prefixes | None in body | **Requires hardening; CONFIRMED BY PINNED SDK SOURCE** | It has no remaining-length argument and trusts string prefixes. Use `(type, bytes, remaining)` and checked arithmetic | Host ZCL value decoder |
| `zclSerializeData()` in `zcl.c:2961-3076` | Serializes a typed ZCL value to wire order | ZCL constants, `zcl_memcpy`, integer/float representation assumptions | None in body | **Requires hardening; CONFIRMED BY PINNED SDK SOURCE** | It has no output capacity and trusts string lengths. Use a bounded writer and explicit byte-order/representation rules | Host ZCL encoder or isolated firmware codec |
| Foundation-command parsers such as `zclParseInReadCmd()` (`zcl.c:3609-3634`) and related `zclParseIn*` functions declared under feature macros in `zcl.h:1812-1906` | Convert ZCL foundation payloads into allocated Z-Stack result structures | Feature definitions, ZCL structs, `zcl_mem_alloc/free`, byte helpers; some processors call attribute/response machinery | OSAL allocation in the normal port; caller assumes a complete and delimited ZCL message | **Substantial replacement; CONFIRMED BY PINNED SDK SOURCE** | Build feature macros are absent; result ownership is stack-specific; validation is uneven. Supply length-bounded host result types and allocator/error semantics | Host foundation-command decoder; not a reason to add AF/OSAL to firmware |
| `zcl_ProcessMessageMSG()` in `zcl.c:2159-2310` | Dispatches AF-delivered messages, endpoint/plugin callbacks, global commands, security checks, and responses | AF incoming-message structures, endpoint registry, operational state, security, command table, plugins, allocation | OSAL task messages, AF/APS/NWK, global lists/state, heap | **Requires complete ZCL/AF runtime; BLOCKED BY IDENTIFIED DEPENDENCY** | FeralRF has no AF endpoint registration, APS indication, ZCL task, or Z-Stack state. Replacing them would amount to a new runtime | No valid direct boundary; host decoder should parse bytes instead |
| `zcl_SendCommandEx()` in `zcl.c:1039-1165` | Builds a ZCL command and sends it through AF | Endpoint lookup, AF address, APS options/security, radius, transaction state, `AF_DataRequest()`/`zcl_AF_DataRequest()` | AF/NWK/APS, allocation, globals | **Requires complete runtime; BLOCKED BY IDENTIFIED DEPENDENCY** | A ZCL command is not a complete over-the-air frame; FeralRF raw TX would still need APS, NWK, MAC, security, counters, and participation state | None for passive decoding; raw TX requires an independently constructed full MPDU |
| `zcl_general.c` cluster handlers, including Scenes paths around `:3914-4066` | Implements operational cluster behavior and response generation | ZCL core, endpoint callbacks, attribute storage; Scenes uses NV through port functions | OSAL/AF/APS, globals, allocation, and NV for applicable features | **Operational runtime; BLOCKED BY IDENTIFIED DEPENDENCY** | No registered endpoints, attributes, scene table, transaction runtime, or NV contract exists in FeralRF | Host code may reuse cluster IDs/schemas as references, not handlers |
| `ZDO_Parse*` family in `stack/zdo/zd_object.c:2914-3733` | Parses individual ZDO request/response ASDUs into Z-Stack structures | `BUILD_UINT16`, `OsalPort_buildUint32`, address copy helpers, ZDO/NWK descriptor structures, OSAL allocation for variable lists | Heap for variable-length results; containing file has AF/NWK/ZDO/BDB/NV/global dependencies | **Wire reference; substantial adaptation; CONFIRMED BY PINNED SDK SOURCE** | Inputs are assumed to be APS-delimited and most functions receive no length. Count fields can drive reads/allocations. Add an explicit remaining length, bounded counts, host-owned DTOs, and deterministic failure/cleanup | Host ZDO decoder after removal of the ZDO transaction sequence byte |
| `ZDP_IncomingData()` in `stack/zdo/zd_profile.c:2092-2134`; `zdoIncomingMsg_t` in `zd_profile.h:229-241` | Converts an AF message to ZDO dispatch form; removes the first ZDO transaction-sequence byte | AF incoming message, ZDO task/event system and dispatch tables | AF/APS, OSAL allocation/events, task state | **Runtime adapter; BLOCKED BY IDENTIFIED DEPENDENCY** | FeralRF has raw MPDUs, not an `afIncomingMSGPacket_t`. A host decoder must derive APS profile/cluster/endpoints first | Host pipeline defines the equivalent boundary: APS payload byte 0 is ZDO TSN, remainder is ASDU |
| `afRegisterExtended()` (`stack/af/af.c:171-215`), `afIncomingData()` (`:421-529`), `afBuildMSGIncoming()` (`:540-596`), `AF_DataRequest()` (`:624-810`) | Endpoint registry, APS indication conversion, OSAL delivery, and outbound APS/NWK service request | APS/NWK/ZDO/BDB, routing, endpoint globals, fragmentation/security, generated configuration | OSAL messages/heap, APS/NWK runtime, timers/security as selected | **Operational runtime; BLOCKED BY IDENTIFIED DEPENDENCY** | AF consumes already parsed APS data and is not a raw APS decoder. No corresponding runtime exists in FeralRF | No passive-decoder boundary |
| `NLDE_ParseMsg()` and `NLDE_ParseFrameControl()` declared in `stack/nwk/nwk_util.h:321,431` | Normal NWK frame parsing | Definitions absent from visible source; role-specific Z-Stack libraries, NIB/security/address/routing state | Z-Stack NWK runtime, global state, heap/security | **Unavailable as GCC source; BLOCKED BY IDENTIFIED DEPENDENCY** | No implementation is available to compile or audit, and no GCC Z-Stack archive exists | Independent host NWK parser based on header definitions |
| `APSDE_ParseMsg()` declared in `stack/nwk/aps_util.h:112` | Normal APS frame parsing | Definition absent from visible source; role-specific Z-Stack libraries and NWK/security state | APS/NWK runtime, globals, heap/security | **Unavailable as GCC source; BLOCKED BY IDENTIFIED DEPENDENCY** | Same source/archive boundary as NWK | Independent host APS parser based on definitions |
| static `StubNWK_ParseMsg()` and `StubAPS_ParseMsg()` in `stack/nwk/stub_aps.c:242-316` | Parses the special inter-PAN stub format | `StubNWK_ParseMsg()` calls unavailable `NLDE_ParseFrameControl()`; inter-PAN runtime later calls TIMAC | ZMAC/TIMAC, OSAL, globals; ordinary Zigbee NWK/APS is not implemented here | **Not a general parser; BLOCKED BY IDENTIFIED DEPENDENCY** | Static, special-purpose, incompletely length-checked, and still linked to hidden NWK logic | At most a protocol reference for a separate inter-PAN parser |
| `ZMacInit()` (`zmac/f8w/zmac.c:506-518`), `ZMacDataReqSec()` (`:1139-1182`), scan/start/association wrappers | Calls TIMAC for active IEEE 802.15.4 MLME/MCPS services | `MAP_MAC*` implementation in TIMAC archives, generated MAC configuration, callback bridge | TIMAC task, OSAL messages, RF Driver/RF Core, timers, heap, security | **Complete MAC runtime; BLOCKED BY IDENTIFIED DEPENDENCY** | Current native RF backend already owns RF clients and does not run a TIMAC task | Would require a deliberate RF/MAC ownership architecture, not a decoder insertion |
| `zmac_cb.c` callbacks around `:233-245,343,387-392` | Allocates and posts MAC confirmations/indications to stack tasks | OSAL task IDs/messages and MAC structures | OSAL heap/events/tasks | **Complete runtime; BLOCKED BY IDENTIFIED DEPENDENCY** | FeralRF does not register the expected task IDs or event loop | None for passive parsing |
| `Zstackapi_init()` (`stack/api/zstackapi.c:264-267`) and API request wrappers such as `Zstackapi_DevStartReq()` (`:514`) | Inter-task RPC to the Z-Stack task | OSAL allocation/send, stack task ID, `Task_self()`, synchronous response behavior | TI-RTOS task context, OSAL messaging/heap, complete stack task | **Not a protocol codec; BLOCKED BY IDENTIFIED DEPENDENCY** | There is no stack task to receive requests or issue callbacks | No decoder boundary |
| `SSP_ParseAuxHdr()`, `SSP_Process()`, `SSP_NwkSecurity()`, `SSP_BuildNonce()`, and `SSP_GetMicLen()` declared in `stack/sec/ssp.h:230-290` | Parses and processes operational Zigbee security, builds nonces, and applies/removes NWK security | Definitions are absent from visible source; `SSP_Info_t` contains PDU pointers, NV key identifiers, source/destination IEEE addresses, counters, key sequence, MIC length, and retry state (`ssp.h:198-214`) | Hidden Z-Stack security implementation, global counters, key/NV tables, AES/TRNG services, NWK/APS context | **Constants are reference material; implementation is BLOCKED BY IDENTIFIED DEPENDENCY** | Header availability does not provide a callable GCC implementation. A passive decoder can use the explicit auxiliary-header masks/lengths at `ssp.h:125-141`, but must implement and validate its own bounded parse/CCM* path | Host security-header parser and optional decryption service |
| `ZDSecMgrInit()`, `ZDSecMgrTransportKeyInd()`, key update/switch, and NV functions in `stack/zdo/zd_sec_mgr.c:1490,1726,2203-2283,2720,3558,3673` | Manages joining authentication, network/link keys, key transport/switch, and persistent tables | Calls hidden SSP services, APSME, ZDO/NWK address services, OSAL allocation and `osal_nv_*`; maintains global key records and counters | Complete security/ZDO/NWK runtime, NV, heap, crypto, network participation | **Operational security manager; BLOCKED BY IDENTIFIED DEPENDENCY** | It is neither a ciphertext decoder nor a stateless key store. Extracting it would still require the missing SSP/NWK/APS contracts and persistent counter semantics | No passive-parser boundary; its structures identify state a passive analyzer may model separately |
| Protocol constants and descriptor declarations in `zcl.h`, `nwk_util.h`, `nl_mede.h`, `aps_mede.h`, and ZDO headers | Names wire fields, masks, sizes, cluster/descriptor types | Headers also include generated config and runtime/pointer types | None if constants are transcribed into independent definitions | **Reference material; CONFIRMED BY PINNED SDK SOURCE** | C bitfields, native structs, pointers, and conditional layouts are not wire DTOs. Only explicit masks/widths should define a decoder | Python protocol constants and schemas |

### 3.2 Decisive call chains

#### ZCL inbound processing

```text
OSAL ZCL task message
  -> zcl_event_loop()                       zcl.c:383-420
  -> zcl_ProcessMessageMSG()                zcl.c:2159-2310
     -> zclParseHdr()                       zcl.c:2412-2446
     -> endpoint/operational/security tests
     -> plugin or foundation command table zcl.c:271-350
     -> parser/processor/callback/response
```

The reusable-looking header parser is nested inside an operational chain that begins with an AF/OSAL message, not raw bytes. `zcl_ProcessMessageMSG()` is therefore not a generic decoder.

#### ZCL outbound processing

```text
zcl_SendCommandEx()                         zcl.c:1039-1165
  -> endpoint lookup and policy
  -> allocate/build ZCL header and payload
  -> zcl_AF_DataRequest() or AF_DataRequest()
  -> APS/NWK/MAC/TIMAC/RF
```

Extracting the ZCL byte builder does not supply the lower-layer envelope or active-node state.

#### ZDO inbound processing

```text
AF/APS incoming message
  -> ZDP_IncomingData()                     zd_profile.c:2092-2134
     -> Data[0] becomes TransSeq
     -> Data[1..] becomes ASDU/asduLen
     -> ZDO dispatch
        -> ZDO_Parse*()                     zd_object.c:2914-3733
```

This explains a critical input contract: the individual TI `ZDO_Parse*` functions do not receive the first ZDO transaction-sequence byte. A host decoder must retain that byte in its ZDO header and pass only the remainder to an equivalent command-body parser.

#### Active stack startup

```text
main/startup
  -> NVOCMP load and macTaskInit()           startup/main.c:342-357
  -> stackTask_init()/zstackstartup task
  -> MAC, NWK, APS, ZDApp, Stub APS, BDB,
     Z-Stack API registration/init           zstackstartup.c:374-451
  -> TRNG/AESCCM/AESECB opens                 zstackstartup.c:628-640
  -> ZMacInit()                              zstackstartup.c:673
```

This is the initialization boundary that partial inclusion of an active AF/NWK/APS component would have to replace. Calling one public function does not bypass it.

### 3.3 ZCL and ZDO safety assumptions

The original parsers sit behind AF/APS length and message construction. Examples of assumptions that do not hold for arbitrary captured frames are:

- `zclParseHdr()` can consume five bytes for a manufacturer-specific header but has no length argument (`zcl.c:2412-2446`).
- `zclGetAttrDataLength()` reads string length prefixes with no available-length argument (`zcl.c:3296-3322`).
- `ZDO_ParseBindUnbindReq()` reads fixed fields with no length (`zd_object.c:2969-2991`).
- `ZDO_ParseNodeDescRsp()` reads a descriptor without receiving an ASDU length (`:3070-3101`).
- `ZDO_ParseEPListRsp()` allocates and copies a peer-provided count without a total-buffer check (`:3172-3189`).
- `ZDO_ParseMgmtLqiRsp()` iterates 22-byte neighbor entries according to a count without an available-buffer parameter (`:3225-3284`).
- `ZDO_ParseSimpleDescBuf()` allocates input/output cluster lists from payload counts (`:3543-3594`).
- `ZDO_ParseMgmtNwkUpdateNotify()` copies a counted energy list without an explicit remaining length (`:3680-3719`).

Consequently, using these routines unchanged as capture parsers would convert a trusted internal API into an untrusted-input API without adding the validation that boundary requires.

## 4. Compiler, linker, configuration, and runtime incompatibilities

### 4.1 Constraints by integration class

| Class | Concrete constraints | Result |
|---|---|---|
| Pure data-processing utilities | C types/macros can be isolated, but whole SDK translation units bring unrelated includes and symbols; several routines need new length/capacity contracts; C bitfields are not portable wire layouts | Source algorithms can be adapted. No TI binary library, OSAL task, NV, timer, or RF ownership is inherently required |
| ZCL/ZDO processing components | Generated/role/feature headers, AF message types, endpoint registration, OSAL Port allocation/messages, stack-owned globals, APS-delimited input, callbacks, and sometimes NV | Individual wire algorithms can inform an independent codec; processors and dispatchers cannot operate as-is |
| Active MAC/NWK/APS functionality | Missing source implementations, TI ARM Clang/IAR archives, TIMAC, OSAL task graph, TI-RTOS, role configuration, heap, timers, NV/security state, crypto instances, and RF Core ownership | The active target cannot obtain this behavior by adding a few source files |

### 4.2 GCC versus TI ARM Clang/IAR

- **CONFIRMED BY CURRENT SOURCE** - The active CMake target invokes GCC-style Cortex-M4F flags and GNU linking (`firmware/cc1352/CMakeLists.txt:32-48`).
- **CONFIRMED BY PINNED SDK SOURCE** - Z-Stack role libraries in `source/ti/zstack/lib/ticlang/m4f/` include `libZStack_nwk_all.a`, `libZStack_nwk_zed.a`, `libZStack_nwk_zr.a`, and `libZStack_gpd_sec.a`. Corresponding SDK directories are TI ARM Clang/IAR, not GCC.
- **CONFIRMED BY PINNED SDK SOURCE** - TIMAC target libraries under `source/ti/ti154stack/lib/ticlang/m4f/` are selected by device, security, RTOS, and mode. No GCC library family is packaged.
- **CONFIRMED BY PINNED SDK SOURCE** - `.meta/zstack.syscfg.js:160-225` selects the toolchain/ISA MAC library and the role-specific Z-Stack library. The SDK release notes list tested TI ARM Clang/IAR-oriented tool versions, not the current GNU CMake composition (`release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html:115-123`).

A `.a` suffix does not establish ABI compatibility. Object format, EABI attributes, runtime helper calls, section naming, library metadata, compiler-builtins, and linker-command semantics must all match. No packaged GCC archive or demonstrated GCC-to-TI-archive link exists here. Therefore normal NWK/APS/TIMAC reuse is **BLOCKED BY IDENTIFIED DEPENDENCY**, not merely untested because a compiler flag is missing.

### 4.3 Header and generated-configuration incompatibilities

Z-Stack headers derive role behavior from definitions such as `ZSTACK_DEVICE_BUILD` (`stack/sys/zglobals.h:81-105`). SDK option files such as `config/f8wrouter.opts` and `config/f8wenddev.opts` define MAC table sizes, router/end-device behavior, and OSAL NV features. The active target provides none of that role configuration.

Although an SDK `ti_zstack_config.h` template/default exists, no Z-Stack SysConfig output is generated for this target. The active SysConfig outputs cover the existing TI-RTOS, drivers, and radio configuration (`firmware/cc1352/CMakeLists.txt:168-184`). ZCL public declarations themselves are feature-conditional (`stack/zcl/zcl.h:844 onward, 1812-1906`). Compiling an arbitrary selection without the same generated feature set can change visible APIs, command tables, structure members, and required symbols.

`ZCL_STANDALONE` only suppresses selected OSAL includes (`zcl.h:67-75`). It does not turn `zcl.c` into a complete standalone parser: AF/APS types/configuration remain, and memory/NV operations must be supplied by external functions (`zcl.h:1318-1341`).

### 4.4 Missing symbols and library boundaries

Concrete unresolved boundaries include:

- `NLDE_ParseMsg()` and `NLDE_ParseFrameControl()` are declared but not defined in visible NWK source (`nwk_util.h:321,431`).
- `APSDE_ParseMsg()` is declared but not defined in visible APS source (`aps_util.h:112`).
- `zmac.c` delegates active MAC work through `MAP_MAC*` calls whose implementations are in TIMAC libraries (`zmac.c:78-89,506-518`).
- Z-Stack API wrappers require an initialized stack task and OSAL message path (`zstackapi.c:111-169,188-267`).
- `zcl_port.c` supplies memory, message, API, and NV bridges rather than eliminating them (`zcl_port.c:126,224,316,487,546-654,809,1101 onward`).

Link-time garbage collection is not a sound independence boundary for compiling `zcl.c` or `zd_object.c`: feature-controlled global dispatch tables and cross-references still determine retained sections, and the public headers/configuration are coupled even where a selected body is simple.

### 4.5 OSAL Port, task, heap, timer, and NV assumptions

OSAL Port is an active scheduler/message abstraction, not just compatibility macros. The implementation packaged under `ti154stack/common/osal_port/osal_port_posix/` registers tasks (`osal_port.c:198-215`), allocates and sends messages (`:247-268,355-381`), receives messages (`:449-475`), and provides heap calls (`:1148-1197`). Its timers maintain a linked list and use `ClockP` services (`osal_port_timers.c:73-79,156-157,218-219,353-421`). The directory name does not make these routines passive: this source still includes the TI DPL task, semaphore, clock, and heap service contracts used by the stack port.

The current firmware instead owns two long-running tasks:

- The UART task initializes HostIF, events, control/command handling, and polling (`firmware/cc1352/src/main_rtos.c:112-136`).
- The RF task initializes crypto, opens the persistent 433 MHz RF handle, initializes `DataTask`, and polls it (`main_rtos.c:141-178`).

Adding source that assumes OSAL task IDs, an OSAL event loop, or a synchronous `Task_self()` request/response path does not map onto those task owners automatically.

NV is not needed for a stateless byte parser. It is required by operational Z-Stack features including network/security state and applicable ZCL features such as Scenes. The current target has no Z-Stack NVOCMP initialization or generated NVS/NVOCMP composition. The active stack startup explicitly loads NVOCMP before the MAC/stack task graph (`startup/main.c:342-357`).

The visible NV shim is itself configured indirection rather than storage: `osal_nv_init()` and the item read/write/delete functions call function pointers in `pZStackCfg->nvFps` and translate IDs to `NVINTF_SYSID_ZSTACK` (`zstack/osal/osal_nv.c:99-146,189-254,303-430`). Supplying that C file without a configured NV interface would leave the operational persistence contract unfulfilled.

### 4.6 RF Core and crypto ownership

FeralRF holds a persistent sub-1 GHz RF handle and can hold a second persistent non-433 handle (`radio_if.c:1194-1221`). The SDK RF driver is configured for only two clients (`source/ti/drivers/rf/RFCC26X2_multiMode.c:140,327-328,4599,4686`). A TIMAC/Z-Stack client cannot be added as an independent third owner; it would require an explicit replacement or arbitration of existing ownership and command scheduling.

Z-Stack startup opens TRNG, AES-CCM, and AES-ECB instances (`zstackstartup.c:628-640`). The current generated driver configuration defines one instance of each (`firmware/cc1352/syscfg/ti_drivers_config.h:80-96`), while FeralRF opens/closes those instances on demand (`firmware/cc1352/src/crypto_engine.c:64,90,214`). Concurrent ownership and lifetime assumptions would conflict unless deliberately redesigned.

Passive host parsing has neither conflict. Host decryption still requires a correct Zigbee cryptographic implementation and keys, but not ownership of the MCU crypto peripherals.

## 5. Exact FeralRF IEEE 802.15.4 receive contract

### 5.1 Receive call chain

```text
RadioIF_startIeee154RfBackend()              radio_if.c:1335-1371
  RF Core RX queue: PHR + PSDU/FCS + RSSI + Corr/CRC + RAT timestamp

RadioIF_processIeee154Packets()              radio_if.c:1497-1568
  strips PHR and RF metadata
  drops CRC-failed frames
  copies PSDU, including FCS, into RadioRxPacket.data
  -> enqueueRxPacket()                       radio_if.c:872-880

RadioIF_poll()/RadioIF_popRxPacket()          radio_if.c:2354 onward, 2392-2400
  -> DataTask_poll()                         data_task.c:112-139
     -> LLManager_processRxPacket()          ll_manager.c:144-159
     -> DataTask_emitRxPacket()              data_task.c:23-66
        -> HostIF response

Radio.read_packets()                         python/feralrf/radio.py:636-709
  -> Packet.data
```

`LLManager_processRxPacket()` clears link-layer metadata and returns the packet unchanged for the default IEEE path. It is not a Zigbee decoder.

### 5.2 RF queue representation and transformation

`RadioIF_startIeee154RfBackend()` configures the RF command to include the PHY header and received CRC, and to append RSSI, correlation/CRC status, and a timestamp. It disables automatic CRC and ignored-frame flushing (`radio_if.c:1335-1371`; SDK field semantics in `source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:111-116`). Frame filtering and automatic ACK are disabled in the SmartRF command (`firmware/cc1352/src/smartrf_ieee_15_4_0.c:103-122`).

For each RF queue entry, `RadioIF_processIeee154Packets()` applies this transformation:

| RF queue field | Firmware treatment | Python-visible result |
|---|---|---|
| Two-byte RF data-entry length | Used only to bound queue contents | Not exposed |
| One-byte PHR length | Used to determine PSDU length | Not included in `Packet.data` |
| PSDU | Copied beginning immediately after the PHR | `Packet.data`; includes MAC header, payload/ciphertext/MIC, and received two-byte FCS |
| RSSI byte | Cast to signed 8-bit | `Packet.rssi_dbm` |
| Correlation/CRC byte | Low seven bits copied as `lqi`; bit 7 is CRC error | `Packet.lqi`; `Packet.crc_ok` is true for every emitted packet |
| Four-byte RF timestamp | Read little-endian and divided by four | Integer placed in the response's 64-bit timestamp field |

The SDK defines the appended correlation/status byte as six correlation bits, bit 6 `bIgnore`, and bit 7 `bCrcErr` (`rf_ieee_cmd.h:616-618`). FeralRF masks `0x7f`, so its `lqi` is not guaranteed to be a normalized IEEE LQI and can include the ignore flag. With current filtering disabled that flag is normally not expected to add information, but the type contract does not exclude it.

### 5.3 Host response layout

`DataTask_emitRxPacket()` constructs the response as follows (`data_task.c:23-66`):

| Offset | Width | Meaning |
|---:|---:|---|
| 0 | 8 | timestamp, little-endian |
| 8 | 1 | configured channel |
| 9 | 1 | signed RSSI byte |
| 10 | 1 | correlation/status-derived `lqi` |
| 11 | 1 | CRC-valid flag |
| 12 | 1 | data length |
| 13 | `data length` | PSDU including FCS |
| following | 3 | FeralRF link-layer metadata |

The generic protocol payload maximum is 255 bytes (`firmware/cc1352/include/protocol.h:15-28`). Thirteen bytes of RX metadata plus three bytes of link metadata leave a firmware emission cap of 239 data bytes, even though `RadioRxPacket.data` is 242 bytes (`radio_if.h:14-28`). Valid IEEE 802.15.4 PSDUs are at most 127 bytes, so valid Zigbee frames are not truncated by either cap. Oversized proprietary or malformed captures can be capped. Queue pressure instead drops whole frames: RF has three data entries and the application queue depth is eight (`radio_if.c:32-44,872-880`).

For an accepted standards-compliant frame, the MAC header, MAC payload (plaintext or ciphertext), MIC if present, and FCS are copied without byte transformation. The preservation is therefore lossless at the per-frame byte level. It does not imply lossless capture over time: CRC rejection and RF/application queue exhaustion remove whole frames before Python observes them.

### 5.4 Python `Packet.data` and parser assumptions

`Packet` exposes `data`, `timestamp`, `channel`, `rssi`, `lqi`, `crc_valid`, and link-layer fields (`python/feralrf/radio.py:18-30`). `Radio.read_packets()` decodes the response and slices the advertised data length (`radio.py:636-709`). It does not label or strip the FCS and does not parse a PHY, MAC, or Zigbee header.

The alternate response parser has the same byte contract (`python/feralrf/_responses.py:24-57`). Neither parser verifies that the response actually contains all bytes claimed by the data-length field before slicing. Firmware-generated responses are trusted; a decoder must not copy that assumption to arbitrary external bytes.

Existing host-facing assumptions a decoder must preserve are:

- `Packet.data` stays byte-for-byte unchanged; parsed objects should retain raw offsets rather than mutate or remove the FCS in place.
- The PHR is absent. A parser expecting an over-the-air PHR must not treat byte 0 as PHY length.
- The FCS is present. A parser/library expecting an FCS-less MPDU needs an explicit adaptation.
- The channel is firmware state copied at processing time, not an RF-appended per-packet channel measurement.
- The timestamp is not wall-clock time. The RF RAT is 32-bit at four ticks per microsecond (`source/ti/drivers/rf/RFCC26X2.h:455-468,1028`); firmware divides it by four and widens it without rollover extension. It wraps after about 1073.741824 seconds. The precise RF event edge represented by the appended timestamp is **NOT SUPPORTED BY AVAILABLE EVIDENCE** in the inspected code.
- `lqi` is the status/correlation mask described above, not a demonstrated standard-scaled LQI.

Although RF automatic CRC flushing is disabled, firmware discards frames whose appended status has the CRC-error bit (`radio_if.c:1538-1546`). Python therefore cannot analyze bad-FCS frames today. All emitted IEEE frames report CRC valid.

On transmit, FeralRF accepts at most 125 MPDU bytes without an FCS and configures RF Core to generate PHY/FCS (`radio_if.c:61,468-487`; `smartrf_ieee_15_4_0.c:146-158`). This RX/TX asymmetry explains why the KillerBee adapter removes two received FCS bytes before reinjection.

**Conclusion:** basic passive Zigbee decoding of CRC-valid frames requires no firmware modification. This conclusion is about byte availability, not about an existing Zigbee implementation.

## 6. Independent Zigbee decoder boundaries

### 6.1 Layer-by-layer requirements

| Layer | Required input and principal structures | Context/state and security boundary | Decodable without keys | Not reconstructable from one isolated packet | Python boundary and relevant TI references | Concrete validation required |
|---|---|---|---|---|---|---|
| IEEE 802.15.4 MAC | `Packet.data`: MPDU with FCS, no PHR. Parse frame control, frame type/version, sequence suppression, destination/source PAN and address modes, PAN compression rules, auxiliary security, header/payload IEs where applicable, payload, FCS | Header parsing is stateless. Address interpretation and duplicate/security tracking can be stateful. MAC security, if present, protects the payload and requires its own key/nonce context | All header fields, addresses, IEs, FCS result, ciphertext length, auxiliary security fields | Missed address bindings, security keys, prior fragments, and semantic meaning of opaque payload | First Python stage. TI RF definitions describe capture framing; do not use native C bitfields as wire structs | Golden frames for supported 2003/2006/2015 frame versions, every address/PAN-compression form, sequence suppression, IEs, valid/invalid FCS, truncation at every field, and maximum 127-byte PSDU |
| Zigbee NWK | Plain MAC payload after MAC security handling. Base header is 8 bytes, followed conditionally by extended destination/source, multicast control, source-route subframe, then payload or NWK security envelope | Base/optional header parse is stateless. Network-key decryption needs source IEEE address for nonce construction, key sequence/material, security level/frame counter, and often short-to-IEEE mapping. Topology and replay analysis are stateful | Frame control/version, type, route-discovery/multicast/security/source-route flags, short addresses, radius/sequence, optional IEEE addresses, source route, security header and ciphertext | Missing network key, unknown IEEE source behind a short address, missed key transport/switch, missing route/topology history | `zigbee_nwk` stage. Masks/sizes in `stack/nwk/nwk_util.h:110-182`; runtime form `NLDE_FrameFormat_t` in `nl_mede.h:247-283` is pointer-rich and not a wire DTO | Plaintext vectors for each optional-header combination, multicast/source-route and command frames; bounds/count failure vectors; independent dissector comparison; encrypted known-answer frames for each accepted key sequence/security form |
| Zigbee APS | Decrypted NWK payload. Header varies by frame type and delivery mode: data/command/ack, endpoints, group address, cluster ID, profile ID, APS counter, extended fragmentation, and optional APS auxiliary security | Header parse is stateless. Fragment reassembly, duplicate/ack tracking, binding semantics, and APS security are stateful. Link-key decryption needs the correct link key and nonce/source context | APS frame/delivery mode, endpoint/group/cluster/profile metadata, counter, fragmentation and security headers, ciphertext | Other fragments, link key, binding table, transaction history, retransmission/ack outcome | `zigbee_aps` stage. `stack/nwk/aps_mede.h:91-173,289-308` supplies masks and a runtime parsed structure; normal parser implementation is not available | Data, command, and acknowledgement frames across unicast/group/broadcast delivery; extended/fragment headers including missing/out-of-order fragments; malformed length cases; APS-security known-answer and negative-MIC vectors |
| ZDO | APS plaintext for profile 0x0000, ordinarily endpoint 0, plus APS cluster ID. Byte 0 is the ZDO transaction sequence; the remaining bytes are command ASDU | Individual descriptor/request/response parsing is mostly stateless. Request/response correlation, device tables, discovery results, and topology are stateful | All complete plaintext ZDO command fields supported by schemas | Transactions or descriptors never captured; topology outside observed messages | `zigbee_zdo` stage keyed by APS cluster ID. `zd_object.c:2914-3733` is field-order evidence; `ZDP_IncomingData()` proves the TSN split | Per-cluster request/response vectors; zero/max count descriptor and table lists; inconsistent count/length and truncation vectors; TSN/cluster dispatch checks; comparison with TI field order and an independent dissector |
| ZCL | APS plaintext for a non-ZDO profile/cluster. Header is 3 bytes normally or 5 with manufacturer code; foundation versus cluster-specific interpretation depends on frame control, direction, cluster/profile/endpoints | Header and well-defined command payloads are stateless. Attribute cache, report/transaction correlation, default-response behavior, manufacturer schemas, and cluster state are stateful | Header, foundation commands and known cluster payloads when plaintext; unknown commands remain structured raw payloads | Manufacturer-specific schema, device state not carried on wire, missed attribute/transaction history | `zigbee_zcl` stage with APS metadata as context. `zcl.h` constants and `zcl.c` helper algorithms are references | Three- and five-byte header vectors, both directions/frame types, every supported foundation data type including short/long strings, cluster command schemas, unknown/manufacturer commands, endian/round-trip checks, and malformed length prefixes |
| Zigbee security envelope | The protected layer's auxiliary security header, protected bytes, MIC, and authenticated header bytes. Key identifier, security level, frame counter, and source identity determine processing | Header parse is stateless. Authenticated decryption is contextual and stateful; replay decisions need counter history. Network and APS/link security are distinct boundaries | Security control, key-id mode/sequence where present, frame counter, source where present, MIC size, ciphertext | A key that was never observed/provided, missing IEEE source mapping, omitted fragments, authenticity without successful MIC verification | `zigbee_security` service called by MAC/NWK/APS stages; Z-Stack operational security code is not an independent decoder | Published or independently generated CCM* known-answer vectors for every supported security/key-id/MIC form; nonce/source byte-order checks; wrong-key, modified-AAD/ciphertext/MIC failures; counter/replay and missing-address-context cases |

### 6.2 Stateless, stateful passive, and active behavior

**Stateless parsing** can split a complete frame into validated bounded fields; report malformed/truncated/count-overflow conditions; expose all headers; decode plaintext ZDO/ZCL; and retain encrypted payloads. It does not need a Zigbee task, endpoint, NV store, or RF change.

**Stateful passive analysis** adds short-to-IEEE mappings, network and link keys, key-sequence history, replay/duplicate tracking, APS fragment reassembly, transaction correlation, topology, and attribute caches. It still does not transmit or join. State must distinguish observations from inferred facts and must never authenticate a payload unless MIC verification succeeds.

**Active protocol participation** includes association/joining, beacon/scan behavior, parent polling, timely MAC acknowledgements, network formation/routing, APS retries/acks, commissioning, key transport/switch/update, frame-counter persistence, endpoint discovery, and response generation. These require an operational MAC/NWK/APS/ZDO/ZCL stack or equivalent implementation; packet parsers are insufficient.

### 6.3 Firmware changes that are and are not implied

No firmware change is implied for single-channel, CRC-valid passive decoding or host-side key-assisted decryption.

Firmware behavior would have to change to expose:

- frames with invalid FCS, because `RadioIF_processIeee154Packets()` currently drops them;
- continuous timestamps beyond the 32-bit RAT wrap, wall-clock correlation, or a documented timestamp event edge;
- simultaneous/multi-channel observation, because one configured RF channel is captured at a time;
- active Zigbee MAC timing such as automatic acknowledgements, association, polling, scans, and coordinated retries;
- a TIMAC/Z-Stack runtime, because current RF client, task, heap, crypto, and NV ownership does not provide it.

Those are distinct capabilities; none is necessary merely to decode the bytes already delivered.

## 7. Existing decoder and tooling alternatives

### 7.1 Current FeralRF interfaces

`python/feralrf/integrations/killerbee.py:1-19` lazily imports KillerBee. Its device adapter reports capture/injection capabilities (`:54-62`). `pnext()` returns `Packet.data` unchanged together with validity, RSSI, and a host wall-clock timestamp (`:95-109`); it omits FeralRF's configured channel, correlation value, and RF timestamp. `inject()` removes the last two bytes for inputs of length at least five, assuming they are an FCS, before sending at most 125 bytes (`:111-120`). That is a capture/device-control adaptation, not Zigbee decoding.

The integration requires a FeralRF-specific KillerBee patch/fork. `docs/killerbee-catsniffer.patch:1-99` adds the shim/probe/dispatch; the upstream driver registry has no source-confirmed FeralRF driver. The associated dispatch behavior is tested against the patched interface in `python/tests/test_killerbee_dispatch.py:1-109`. The optional package declaration `killerbee>=3.0.0` is not a commit pin (`python/pyproject.toml:28-44`), while the inspected upstream packaging declares `3.0.0-beta.2`. Exact install-time resolution is therefore unresolved.

`python/feralrf/emulation/ieee154_device.py:1-5` explicitly models only PHY-style behavior: no network stack, association, or encryption, and its emulated payloads omit an FCS. It is not a Zigbee decoder and does not share the live RX FCS contract.

### 7.2 Candidate comparison

| Candidate | Capture/device control | IEEE 802.15.4 parsing | Zigbee decoding | Active-stack behavior | License/dependencies and exact FeralRF compatibility |
|---|---|---|---|---|---|
| Current FeralRF KillerBee adapter plus upstream KillerBee helpers | Yes, through the local patch/fork | Upstream contains legacy `dot154decode` helpers and also depends on Scapy | Upstream contains basic `zigbeedecode` NWK/APS helpers; source does not establish current-complete NWK/APS/ZDO/ZCL or modern MAC/security coverage | KillerBee performs capture/injection tooling, not a complete joining/routing Zigbee stack | Upstream LICENSE is BSD-3-Clause. Packaging brings PyUSB, PySerial, PyCrypto, rangeparser, Scapy and a libgcrypt-backed extension. `Packet.data` fits the adapter's FCS-present capture contract; decoder behavior/version is **TECHNICALLY PLAUSIBLE BUT UNVERIFIED** |
| Scapy `Dot15d4FCS` plus `zigbee.py` | Can consume packets but is not a FeralRF driver unless adapted | `Dot15d4FCS` explicitly accepts FCS-present frames and checks the Kermit CRC; DLT 195 is the FCS-present link type | Has NWK, APS, security-header, limited ZDP and ZCL dissectors. It does not demonstrate a complete ZDO/ZCL catalog or Zigbee decryption engine; source contains TODO/limited schema areas | No operational Zigbee network stack | Scapy source is GPL-2.0-only and is not currently a dependency. `Dot15d4FCS(Packet.data)` is the exact initial byte boundary, with `conf.dot15d4_protocol = "zigbee"`; newer MAC variants and corpus behavior need verification. Distribution/combination licensing requires separate review |
| zigpy | Radio integrations are adapter-specific; no FeralRF radio adapter exists | No source-confirmed raw MAC/NWK entry point was found | Rich ZDO schemas and ZCL cluster/foundation deserialization operate after lower-layer decapsulation and with cluster/application context | It is a host Zigbee stack framework with controller, device database, tasks, and radio-adapter expectations | GPL-3.0, Python >=3.11 in inspected development source, and many runtime dependencies. It is not directly compatible with raw `Packet.data`; using only upper-layer schemas remains **TECHNICALLY PLAUSIBLE BUT UNVERIFIED** |
| WHAD | Framework explicitly models devices/sniffers and active CLI roles | Uses Scapy-based packets | Documentation exposes Zigbee sniffing and optional key recovery; exact current lower/upper-layer coverage was not pinned in this repository | Provides active abstractions/CLI capabilities, but no FeralRF connector exists | MIT in inspected upstream source, with Scapy, protobuf, crypto, USB and UI dependencies. Byte adapter and version compatibility are **NOT SUPPORTED BY AVAILABLE EVIDENCE** |
| Wireshark via classic PCAP DLT 195 | Offline analysis, not device control | Mature IEEE 802.15.4 dissector; DLT 195 consumes FCS-present frames | Official display-filter references expose NWK, APS/reassembly, ZDP, ZCL, and security/decryption fields | Analyzer only, not an operational node | GPL-2.0-or-later according to the official developer guide. External process/file use adds no FeralRF Python package. `python/examples/killerbee_sniff.py:122-137` already writes `Packet.data` as DLT 195 records, but uses host timestamps and classic PCAP cannot carry the current channel/LQI metadata without a different export format |

Scapy source also binds a fixed sequence-number field in its base `Dot15d4` definition and uses simplified PAN-ID compression logic. This is concrete reason to validate 2015/enhanced-frame inputs rather than claim complete IEEE 802.15.4 coverage from class availability alone.

The nearest source-supported third-party boundaries are therefore:

- `Packet.data -> Scapy Dot15d4FCS` for in-process dissection, with known coverage and license/version checks;
- `Packet.data -> DLT 195 PCAP -> Wireshark` for external analysis;
- the local patched KillerBee device API for capture/injection interoperability, without treating its legacy helpers as a demonstrated complete decoder.

## 8. File/function-level integration boundaries

This section identifies affected files and contracts only.

### 8.1 Approach A: selective Z-Stack source reuse

Realistic scope is limited to isolated codec algorithms and protocol definitions.

| Boundary | Exact source involved | Necessary interface/change | Dependencies and result |
|---|---|---|---|
| ZCL header/value codec in firmware | Algorithms from `stack/zcl/zcl.c:2412-2482,2961-3076,3195-3322`; constants from `zcl.h` | A new isolated bounded codec would take byte span/capacity parameters and use explicit masks, not Z-Stack AF structs or bitfields. If compiled, it would have to be listed in `firmware/cc1352/CMakeLists.txt:129-148` | No Z-Stack runtime is inherently needed. Achieves only ZCL byte header/value encoding/decoding, not APS/NWK/MAC, endpoints, security, or network behavior |
| ZDO body codec in firmware | Field ordering from `stack/zdo/zd_object.c:2914-3733` | A separate bounded implementation with host/FeralRF result types; original `zd_object.c` is not a valid standalone unit | Avoids OSAL/AF/NWK only by replacing those dependencies. Achieves selected plaintext ZDO body parsing, not dispatch/discovery/network behavior |
| Optional firmware decode placement | `firmware/cc1352/src/data_task.c:132-135`, between `LLManager_processRxPacket()` and `DataTask_emitRxPacket()` | Existing protocol has no representation for decoded trees. Any MCU-derived data would need a new protocol response/metadata contract while preserving raw bytes | Adds firmware/protocol coupling with no passive byte-availability benefit. No current boundary can carry arbitrary ZDO/ZCL structures |
| Raw transmit assistance | Before current raw transmit call in `radio_if.c:468-487` | A codec could only produce bytes supplied to raw TX; all lower-layer framing/security/state must be supplied independently | ZCL/ZDO reuse alone cannot create a valid active Zigbee transmission |

Compiling `zcl.c`, `zcl_general.c`, `zd_object.c`, `af.c`, `stub_aps.c`, `zmac.c`, or `zstackapi.c` unchanged is outside this realistic selective scope for the dependencies documented above.

The TI source headers state that source and resulting object code are licensed for use only with TI devices (for example `stack/nwk/aps.h:38-46`). CC1352P7 satisfies that device condition, but extraction, notice preservation, and interaction with FeralRF's GPL-3.0 distribution require legal/license review; source presence alone does not answer it.

### 8.2 Approach B: independent Python decoders

The natural boundary is `feralrf.radio.Packet`, with `Packet.data` passed unchanged and `Packet.channel`, `Packet.timestamp_us`, `Packet.rssi_dbm`, `Packet.lqi`, and `Packet.crc_ok` retained as capture metadata. A source organization consistent with the actual layers would be new modules under `python/feralrf/decoders/` for IEEE 802.15.4, NWK, APS, ZDO, ZCL, and security. These names identify the source boundary only; no such modules currently exist.

Each layer would consume a bounded byte view and return a parsed object containing raw offsets/bytes, fields, child payload, and structured parse errors. Security processing would be an optional service invoked at the layer where the auxiliary header occurs. Passive state would be a separate context rather than hidden global state.

Existing files affected by a packaged public integration would be:

- `python/feralrf/radio.py`, only if a convenience decode entry point were exposed; the `Packet` wire contract need not change;
- `python/feralrf/__init__.py`, only for public exports;
- `python/pyproject.toml`, only if optional cryptography or parser dependencies were declared;
- tests/capture fixtures, needed to validate byte contracts and malformed inputs.

No RF firmware file, CMake file, or MCU runtime dependency is technically required for this boundary. A standard-library structural decoder adds no package dependency. Authenticated decryption can use an optional cryptographic dependency but must be validated against Zigbee CCM* rules and known vectors.

This approach achieves passive structural parsing, stateful observation, and potentially key-assisted decryption. It explicitly does not achieve joining, ACK timing, polling, routing, commissioning, APS retry behavior, or any other active stack service.

### 8.3 Approach C: third-party integration

An in-process Scapy boundary would be an optional integration module, conceptually `python/feralrf/integrations/scapy.py`, whose sole byte adaptation is `Dot15d4FCS(Packet.data)` plus Zigbee protocol selection. `python/pyproject.toml` would be affected only if Scapy were exposed as an install extra. The result is Scapy packet objects with its documented dissector limits; it is not a FeralRF active stack.

The current KillerBee boundary is already in `python/feralrf/integrations/killerbee.py`, plus the external patch represented by `docs/killerbee-catsniffer.patch`. It supplies capture/injection interoperability. Claiming decoder compatibility additionally requires pinning and validating the actual external KillerBee/Scapy versions.

The existing offline boundary is `python/examples/killerbee_sniff.py:122-137`: FCS-present `Packet.data` is written as classic PCAP DLT 195. This is directly consumable by Wireshark/compatible tools, but current host-time timestamps and loss of FeralRF radio metadata limit forensic fidelity.

zigpy would require a lower-layer adapter that produces its `ZigbeePacket`/application context rather than passing raw `Packet.data`. WHAD would require a FeralRF device connector and a pinned interface. Neither adapter exists in current source.

## 9. Concrete comparison

| Property | A. Selective Z-Stack source reuse | B. Independent Python decoder | C. Third-party decoder/tooling |
|---|---|---|---|
| Existing FeralRF files implicated | Firmware C source/CMake only if a bounded codec is placed on MCU; protocol/data-task changes if results leave MCU | New Python decoder package; optional exports/dependency declarations; `Packet` contract can remain unchanged | Existing KillerBee integration/PCAP example, or a new optional integration module and optional dependency |
| External/source functions involved | Only adapted ZCL byte helpers and ZDO field definitions are realistic; operational files are excluded | TI constants/field orders may inform independent definitions without importing runtime | Scapy `Dot15d4FCS`/Zigbee classes, patched KillerBee API, zigpy upper schemas, WHAD connector contract, or Wireshark DLT 195 |
| Input contract | A bounded ZCL/ZDO payload only; raw MPDU requires independent lower decapsulation | `Packet.data` is FCS-present MPDU with no PHR | Scapy/Wireshark DLT 195 match FCS-present bytes; other libraries require adapters |
| Compiler/runtime dependencies | GCC-safe isolated C only. Whole Z-Stack path is blocked by generated config, OSAL/TI-RTOS, and TI-only archives | Standard Python for structure; optional crypto for decryption | Library-specific packages, versions, adapters, and license conditions; Wireshark may remain external |
| RF firmware change | Not required for host decoding; required only if the codec is unnecessarily placed on MCU or active behavior is pursued | None for CRC-valid single-channel capture | None for byte dissection/offline analysis |
| Coupling/maintenance risk | High if copied from SDK internals; small but still SDK-derived for isolated algorithms | Protocol-spec/test-corpus coupling, independent of TI task/runtime changes | Upstream API/version/schema/license coupling |
| Functionality achieved | Bounded ZCL header/value and selected ZDO body codecs only | Complete-by-design passive layer parsing is possible; state/decryption can be added with context | Existing dissection according to each tool's actual coverage; Wireshark provides the broadest source-supported external analysis boundary |
| Explicitly not achieved | Ordinary NWK/APS parsing from hidden binaries; active Zigbee runtime | Active node behavior | A FeralRF-integrated active Zigbee stack; no candidate's availability proves complete modern coverage |
| Verification before compatibility claim | Bounded malformed-input tests, byte vectors, compiler/ABI test if any SDK object is used, license review | Golden captures/vectors at every layer, truncation/fuzz tests, security/MIC vectors, FCS and round-trip tests | Pin exact versions; adapter tests using FeralRF frames; coverage/security tests; license/deployment review |

Approach B has the fewest new in-process dependencies and the cleanest match to the existing raw-capture architecture. That statement is a dependency comparison, not evidence of an implementation.

## 10. Explicit technical conclusions

1. **Which Z-Stack components are realistic reuse candidates?**  
   **CONFIRMED BY PINNED SDK SOURCE:** only small byte-codec algorithms and explicit protocol constants are realistic: ZCL header parse/build logic, fixed type-length lookup, hardened attribute-length and value serialization logic, and selected ZDO field-order logic. The first group can be extracted with limited adaptation; ZDO routines require substantial replacement of length, allocation, and result ownership. No complete Z-Stack layer is independently reusable.

2. **Which apparently reusable components are runtime-coupled?**  
   **BLOCKED BY IDENTIFIED DEPENDENCY:** `zcl_ProcessMessageMSG()`, `zcl_SendCommandEx()`, general-cluster handlers, the ZDO dispatch/object translation units, AF, ordinary NWK/APS parsers, Stub APS, ZMAC callbacks/wrappers, Z-Stack API calls, BDB, and active security. Their exact dependencies are OSAL tasks/messages/heap/timers, AF/NWK/APS globals, role configuration, hidden archives, TIMAC/RF ownership, crypto services, and NV where selected.

3. **Is there a meaningful intermediate architecture?**  
   **CONFIRMED BY CURRENT SOURCE:** yes. A host passive pipeline can be stateless for structural decoding or stateful for address/key/topology/fragment/transaction tracking. It does not need an operational Zigbee stack unless it transmits or participates.

4. **What can FeralRF decode today with new Python code and no firmware changes?**  
   **CONFIRMED BY CURRENT SOURCE:** MAC and FCS; NWK/APS headers and plaintext; ZDO/ZCL plaintext messages; all visible security headers; and ciphertext preservation. **TECHNICALLY PLAUSIBLE BUT UNVERIFIED:** authenticated NWK/APS decryption and passive state reconstruction when correct keys and nonce/address context are available.

5. **Which functionality requires firmware modifications?**  
   **CONFIRMED BY CURRENT SOURCE:** bad-FCS delivery, extended/continuous or wall-clock timestamp semantics, simultaneous/multi-channel capture, and active MAC behavior. Active Z-Stack/TIMAC also requires resolving RF-client/task/crypto/NV ownership and binary/toolchain compatibility.

6. **Which capabilities require an operational stack?**  
   **CONFIRMED BY PINNED SDK SOURCE:** joining/formation, association/polling, timed ACK behavior, route discovery/maintenance, APS delivery/ack/retry/fragment services as a participant, commissioning, key establishment/transport/switch/update, persistent frame counters and network state, endpoint/attribute services, and generated responses.

7. **Which alternative introduces the fewest dependencies?**  
   **CONFIRMED BY CURRENT SOURCE:** an independent structural Python decoder can use only the existing Python runtime and `Packet` API. Optional decryption changes that result. An external Wireshark workflow adds no in-process Python dependency but is a file/tool boundary rather than a Python API.

8. **What uncertainties still prevent an implementation decision?**  
   The unresolved items in the next section prevent claims about desired protocol breadth, binary compatibility, external-library completeness, security behavior, and validation, although they do not change the raw byte contract or the major runtime-coupling conclusions.

## 11. Unresolved technical questions

- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** which IEEE 802.15.4 and Zigbee revisions, manufacturer clusters, Green Power/inter-PAN features, and malformed-frame behaviors are required by the intended decoder.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** a representative, legally usable capture corpus with plaintext, encrypted, fragmented, source-routed, manufacturer-specific, malformed, truncated, and counter/key-transition cases.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** the exact RF Core event edge represented by the appended timestamp in this command configuration.
- **BLOCKED BY IDENTIFIED DEPENDENCY:** source-level audit of the implementations of `NLDE_ParseMsg()`, `APSDE_ParseMsg()`, and TIMAC `MAP_MAC*`; those implementations are not in visible source.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** ABI/link compatibility of any TI ARM Clang archive with the current GNU target. No packaged GCC variant or demonstrated link exists.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** the exact generated Z-Stack project configuration and linker/NV layout that TI would produce for this FeralRF target; SDK examples are absent and the active SysConfig project does not select Z-Stack.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** exact installable versions and FeralRF adapters for KillerBee, Scapy, zigpy, or WHAD. The repository does not pin a resolved environment for them.
- **TECHNICALLY PLAUSIBLE BUT UNVERIFIED:** whether Scapy's current dissectors accept all frame versions and security cases present in the intended FeralRF traffic; source shows specific limitations that require capture tests.
- **TECHNICALLY PLAUSIBLE BUT UNVERIFIED:** key-assisted decryption until known-answer Zigbee CCM* vectors and captured frames verify nonce, authenticated-header, MIC-length, and key-selection rules.
- **NOT SUPPORTED BY AVAILABLE EVIDENCE:** redistribution/license conclusions for extracted TI source or combined/in-process GPL-2.0-only parser code. This audit records source licenses but is not legal analysis.

## 12. Source register

Line references are to the verified revisions in Section 2. SDK-relative paths begin at `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/` unless stated otherwise.

### 12.1 FeralRF and prior audits

| Source | Relevant symbols/lines | Evidence used |
|---|---|---|
| `docs/ZSTACK_INTEGRATION_FEASIBILITY_AUDIT_2026-10-27.md` | `:5-6,29-43,58-60,79-102,279-301` | Prior revision, toolchain, SDK and architecture baseline |
| `docs/FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md` | `:4-6` | Prior revision baseline |
| `firmware/cc1352/CMakeLists.txt` | `:3-18,27-48,81-125,129-148,168-184,225-253` | Device, GCC flags, sources, SysConfig, definitions, libraries |
| `firmware/cc1352/src/main_rtos.c` | `:112-178,201-210,220-235` | Current UART/RF task and semaphore ownership |
| `firmware/cc1352/src/radio_if.c` | `:32-44,61,468-487,872-880,1194-1221,1335-1371,1497-1568,1822-1835,2354 onward,2392-2400` | RF clients, RX/TX limits, IEEE queue configuration, parsing, dropping and polling |
| `firmware/cc1352/include/radio_if.h` | `:14-28` | `RadioRxPacket`, 242-byte data capacity |
| `firmware/cc1352/src/smartrf_ieee_15_4_0.c` | `:103-122,146-158` | Frame filtering, auto-ACK, RX/TX PHY/FCS configuration |
| `firmware/cc1352/src/ll_manager.c` | `:144-159` | Default no-op RX packet processing |
| `firmware/cc1352/src/data_task.c` | `:23-66,112-139` | Response layout and poll/emit chain |
| `firmware/cc1352/include/protocol.h` | `:15-28` | 255-byte protocol payload maximum |
| `firmware/cc1352/src/crypto_engine.c` | `:64,90,214` | Current crypto driver opens/closes |
| `firmware/cc1352/syscfg/ti_drivers_config.h` | `:80-96` | Single TRNG/AES-ECB/AES-CCM instances |
| `firmware/cc1352/syscfg/ti_sysbios_config.h` | `:36` | 0x1000 BIOS heap |
| `python/feralrf/radio.py` | `:18-30,636-709` | `Packet` and host RX parsing |
| `python/feralrf/_responses.py` | `:24-57` | Alternate response parser |
| `python/feralrf/integrations/killerbee.py` | `:1-19,54-62,95-120` | Optional import, capabilities, capture/injection mapping |
| `python/feralrf/emulation/ieee154_device.py` | `:1-5` | PHY-only emulator boundary |
| `python/pyproject.toml` | `:28-44` | Current and optional Python dependencies |
| `python/examples/killerbee_sniff.py` | `:25,40-66,122-137` | DLT 195 classic-PCAP export |
| `docs/killerbee-catsniffer.patch` | `:1-99` | Required external KillerBee dispatch/driver patch |
| `python/tests/test_killerbee_dispatch.py` | `:1-109` | Patched dispatch expectations |

### 12.2 Pinned SimpleLink SDK

| Source | Relevant symbols/lines | Evidence used |
|---|---|---|
| `zstack/stack/zcl/zcl.c` | `:43-62,160-196,271-420,1039-1165,2159-2482,2961-3076,3195-3322,3609-3634` | Includes/globals/task loop; ZCL processing/sending; byte helpers; sample parser |
| `zstack/stack/zcl/zcl.h` | `:67-75,825-842,844 onward,1063-1086,1181-1186,1318-1341,1812-1994` | Standalone mode, frame types, AF-coupled structures, memory macros, feature APIs |
| `zstack/stack/zcl/zcl_port.c` | `:126,224,316,487,546-654,809,1101 onward` | NV registration, initialization, allocation/message/API bridges, Scenes NV |
| `zstack/stack/zcl/zcl_general.c` | `:3914-4066` | Operational Scenes/NV example |
| `zstack/stack/zdo/zd_profile.h` | `:229-241` | `zdoIncomingMsg_t` and ASDU contract |
| `zstack/stack/zdo/zd_profile.c` | `:2092-2134` | `ZDP_IncomingData()`, transaction-sequence removal |
| `zstack/stack/zdo/zd_object.c` | `:69-96,2914-3733` | Runtime includes and complete inspected `ZDO_Parse*` range |
| `zstack/stack/af/af.c` | `:70-90,171-231,421-810` | Runtime includes, endpoint registry, inbound AF and data request |
| `zstack/stack/nwk/nwk_util.h` | `:110-182,321,431` | NWK wire masks/sizes and parser declarations |
| `zstack/stack/nwk/nl_mede.h` | `:247-283` | `NLDE_FrameFormat_t` runtime parsed structure |
| `zstack/stack/nwk/nwk_globals.c` | `:355,381 onward` | NWK/NIB initialization and persistent/random state |
| `zstack/stack/nwk/aps_mede.h` | `:91-173,289-308` | APS masks and `aps_FrameFormat_t` |
| `zstack/stack/nwk/aps_util.h` | `:112` | `APSDE_ParseMsg()` declaration |
| `zstack/stack/nwk/aps.h` | `:38-46` | TI-device restriction on source and resulting object code |
| `zstack/stack/nwk/stub_aps.c` | `:242-316,871-920` | Inter-PAN-only parsers and TIMAC indication path |
| `zstack/zmac/f8w/zmac.c` | `:78-89,319,506-518,865-889,986-1061,1139-1201` | TIMAC/config coupling and active MLME/MCPS wrappers |
| `zstack/zmac/f8w/zmac_cb.c` | `:233-245,343,387-392` | OSAL allocation/delivery callbacks |
| `zstack/stack/api/zstackapi.c` | `:111-169,188-267,514` | OSAL/TI-RTOS RPC behavior and stack-task initialization |
| `zstack/stack/api/zstacktask.c` | `:666-715,743 onward` | Callback registration and stack event loop |
| `zstack/stack/sec/ssp.h` | `:125-141,198-240,266-290` | Security auxiliary-header constants, `SSP_Info_t`, global counters and declaration-only security interfaces |
| `zstack/stack/zdo/zd_sec_mgr.c` | `:1490,1529,1726,2203-2283,2720,3126,3259,3558,3673-3771` | Operational security initialization, hidden SSP calls, key transport/update/load and NV-backed key tables |
| `zstack/stack/zdo/zd_sec_mgr.h` | `:126,137-180,383-526` | Security state/API and explicit NV/key-table interfaces |
| `zstack/osal/osal_nv.c` | `:99-146,189-254,303-430` | `pZStackCfg->nvFps`-backed Z-Stack NV shim |
| `zstack/startup/zstackstartup.c` | `:289-308,374-451,478 onward,628-673` | Task construction, subsystem initialization, crypto and MAC startup |
| `zstack/startup/main.c` | `:342-357` | NVOCMP, MAC and stack task initialization |
| `zstack/stack/sys/zglobals.h` | `:81-105` | Device-build/role configuration |
| `zstack/config/f8wrouter.opts` | `:1-11` | Router MAC sizing, `RTR_NWK`, extended OSAL NV and table persistence definitions |
| `zstack/config/f8wenddev.opts` | `:1-8` | End-device MAC sizing, extended OSAL NV and table persistence definitions |
| `zstack/sysconfig/ti_zstack_config.h` | `:60-130` and remaining defaults | SDK-supplied network/PAN/join configuration header, not generated for FeralRF |
| `ti154stack/common/heapmgr/rtos_heaposal.h` | `:117-119` | Default OSAL heap size |
| `ti154stack/common/osal_port/osal_port_posix/osal_port.c` | `:198-215,247-268,355-381,449-475,1148-1197` | Task/message/heap services |
| `ti154stack/common/osal_port/osal_port_posix/osal_port_timers.c` | `:73-79,156-157,218-219,353-421` | Timer list and ClockP use |
| `drivers/rf/RFCC26X2_multiMode.c` | `:140,327-328,4599,4686` | Two-client RF driver limit |
| `drivers/rf/RFCC26X2.h` | `:455-468,1028` | RAT rate, width, and clock behavior |
| `devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h` | `:111-116,616-618` | IEEE command and appended correlation/CRC semantics |
| `zstack/.meta/zstack.syscfg.js` | `:160-225` | Toolchain/ISA, TIMAC, and role-library selection |
| `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html` (repository-relative) | `:115-123` | Tested tool versions |

### 12.3 External sources inspected

External projects are not pinned by FeralRF. Line references describe the upstream source inspected on 2026-10-30 and must not be treated as a FeralRF lockfile.

| Source | Relevant interfaces/lines | Evidence used |
|---|---|---|
| `https://raw.githubusercontent.com/secdev/scapy/master/scapy/layers/dot15d4.py` | `:17,93-169,207-243,376-381,450-452` | DLT values, `Dot15d4`/`Dot15d4FCS`, FCS, encrypted payload boundary, dispatch and PAN compression behavior |
| `https://raw.githubusercontent.com/secdev/scapy/master/scapy/layers/zigbee.py` | `:1-10,284 onward,558-684,906-943,946-1015,1273-1330` | License tag, NWK/security/APS, limited ZDP, data types and ZCL |
| `https://raw.githubusercontent.com/secdev/scapy/master/LICENSE` | Entire file | GPL-2.0 license text |
| `https://github.com/riverloopsec/killerbee` and raw `killerbee/__init__.py` | Driver dispatch/imports, approximately `:18-20,61-170` | Upstream decoder imports and absence of a vanilla FeralRF driver |
| `https://raw.githubusercontent.com/riverloopsec/killerbee/master/setup.py` | `:56-75` | Version, dependencies, libgcrypt extension |
| `https://raw.githubusercontent.com/riverloopsec/killerbee/master/zigbeedecode.py` and `dot154decode.py` | `ZigBeeNWKPacketParser`, `ZigBeeAPSPacketParser`, legacy MAC/security helpers | Basic helper coverage; insufficient evidence for modern/full decoding |
| `https://raw.githubusercontent.com/riverloopsec/killerbee/master/LICENSE.txt` | Entire file | BSD-3-Clause license |
| `https://raw.githubusercontent.com/zigpy/zigpy/dev/pyproject.toml` | `:3-25` | Python version, GPL-3.0 declaration and dependencies |
| `https://raw.githubusercontent.com/zigpy/zigpy/dev/zigpy/application.py` | `:65 onward` | Controller/application/task/database model |
| `https://raw.githubusercontent.com/zigpy/zigpy/dev/zigpy/zcl/__init__.py` | `:668-705` | Cluster-level ZCL deserialization boundary |
| `https://raw.githubusercontent.com/zigpy/zigpy/dev/zigpy/zdo/types.py` | `:38-55,433-753` | ZDO descriptors, schemas and header deserialization |
| `https://whad.readthedocs.io/en/latest/zigbee/started.html` | Zigbee sniffer documentation | WHAD device/Sniffer/Scapy and key-recovery boundary |
| `https://raw.githubusercontent.com/whad-team/whad-client/main/pyproject.toml` | `:10-48` | Version, MIT declaration, POSIX and dependencies |
| `https://www.wireshark.org/docs/dfref/z/zbee_nwk.html` | Current display-filter reference | NWK/security dissection fields |
| `https://www.wireshark.org/docs/dfref/z/zbee_aps.html` | Current display-filter reference | APS and reassembly fields |
| `https://www.wireshark.org/docs/dfref/z/zbee_zdp.html` | Current display-filter reference | ZDP fields |
| `https://www.wireshark.org/docs/dfref/z/zbee_zcl.html` | Current display-filter reference | ZCL fields |
| `https://www.wireshark.org/docs/wsdg_html/` | Licensing section | GPL-2.0-or-later project license |

## 13. Final boundary statement

The available evidence supports selective reuse of small, hardened ZCL codec algorithms and use of ZDO source as a wire-format reference. It does not support compiling any complete ZCL, ZDO, AF, NWK, APS, ZMAC, or Z-Stack API component into the current FeralRF target as an independent decoder. The exact existing RX contract is already sufficient for a host-side passive Zigbee decoder or an FCS-aware third-party dissector. Operational Zigbee behavior remains a different system boundary because it requires MAC timing, persistent protocol state, security services, task/runtime integration, and RF ownership rather than packet parsing alone.
