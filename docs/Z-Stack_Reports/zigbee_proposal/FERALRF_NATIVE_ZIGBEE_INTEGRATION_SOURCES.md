# Registro de fuentes — propuesta nativa Zigbee para FeralRF, revisión 1.2

**Fecha de actualización:** 2026-11-04
**Propuesta asociada:** [FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md](FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md)
**Revisión crítica incorporada:** [FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md](FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md)
**Revisión normativa/licencias:** [FERALRF_ZIGBEE_NORMATIVE_AND_LICENSE_REVIEW_2026-11-04.md](zigbee_proposal/FERALRF_ZIGBEE_NORMATIVE_AND_LICENSE_REVIEW_2026-11-04.md)

Las rutas FeralRF son relativas a la raíz del repositorio. `SDKROOT/` equivale a `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/`. Los rangos de línea identifican evidencia fuente consultada; no implican que el código haya sido ejecutado o que la funcionalidad Zigbee exista en FeralRF.

## 1. Estados de evidencia

- **CURRENT SOURCE:** implementación visible en el checkout verificado.
- **PINNED SDK SOURCE:** fuente/definición visible en el submódulo exacto; no prueba compatibilidad ni integración.
- **OFFICIAL DOCUMENTATION:** documentación primaria TI, CSA o IEEE con versión/fecha declarada.
- **HISTORICAL EXPERIMENTAL EVIDENCE:** resultado anterior limitado a su setup y artefacto.
- **ARCHITECTURAL INFERENCE:** conclusión derivada de fuente confirmada.
- **CONDITIONALLY FEASIBLE:** requiere norma, decisión, implementación o validación pendiente.
- **UNRESOLVED/UNSUPPORTED:** disponible evidencia insuficiente; no promover a claim.

## 2. Baseline actual

| ID | Verificación | Resultado | Implicación |
|---|---|---|---|
| BASE-01 | `git rev-parse HEAD` | `f0a64ef9a00438757d474fd037c0d1847e6596e4` | HEAD al inicio de revisión normativa, branch below. |
| BASE-02 | `git branch --show-current` | `feature/hardware-validation` | Rama examinada. |
| BASE-03 | `git status --short --branch`, previo a editar | Rama `feature/hardware-validation`; complementary research dir and prior architecture review were untracked | Files pre-existing were preserved, not overwritten. |
| BASE-04 | `git submodule status -- firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` | `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`lpf2-8.30.01.01`) | Único SDK usado. |
| BASE-05 | `SDKROOT/.metadata/product.json` | `8.30.01.01` | Versión del paquete SDK. |
| BASE-06 | `git diff 59816be3460884481f2827eacb630d54cfff6ae8..HEAD -- firmware python` | Sin cambios | FeralRF implementation baseline remains source-equivalent in those paths; no broad claim about other folders. |
| BASE-07 | `firmware/cc1352/CMakeLists.txt:12-18,35-43` | Target `CC1352P7`, toolchain configurado GNU Arm (`arm-none-eabi-gcc/g++`) | No se compiló ni verificó toolchain instalado. |
| BASE-08 | Paths solicitados | Repository, firmware, Python y `SDKROOT/source/ti/zstack` existen | No se usó otra copia de SDK/repositorio. |

El estado final previsto modifica sólo propuesta y source register y crea el informe de revisión permitido; documentos complementarios previos untracked se preservan.

## 3. Documentos previos consultados

| ID | Ruta | Uso y cautela |
|---|---|---|
| DOC-01 | `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md` | Versión 1.0 revisada y sustituida documentalmente por 1.1. |
| DOC-02 | `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md` | Registro versión 1.0 actualizado en esta tarea. |
| DOC-03 | `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md` | Revisión crítica; su archivo no se modifica. |
| DOC-04 | `docs/FERALRF_NATIVE_ZIGBEE_CAPABILITY_ASSESSMENT.md:68-252` | Baseline de capacidades, RF/MAC, reuso y discrepancia NWK. |
| DOC-05 | `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_PROPOSAL.md:1-535` | Alternativas de convergencia; persona separada no seleccionada. |
| DOC-06 | `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md:1-186` | Evidencia histórica; referencia errónea a `nwk.c` anotada como discrepancia. |
| DOC-07 | `docs/Z-Stack_Reports/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md:1-603` | Reuso/decoders/licencias/correcciones A–F; no constituye validación de herramienta. |
| DOC-08 | `docs/Z-Stack_Reports/ZSTACK_INTEGRATION_FEASIBILITY_AUDIT_2026-10-27.md:1-938` | Fronteras runtime, toolchain y RF; no prueba ABI/link. |
| DOC-09 | `docs/FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md:1-1084` | Contratos, metadata, ACK/completion y riesgos multi-PHY. |
| DOC-10 | `docs/Z-Stack_Reports/zigbee_proposal/FERALRF_ZIGBEE_PENDING_DECISIONS_RESEARCH_2026-11-04.md` | Hipótesis y decisiones pendientes; verificación directa registrada aparte. |
| DOC-11 | `docs/Z-Stack_Reports/zigbee_proposal/FERALRF_ZIGBEE_DECISION_RESEARCH_SOURCES_2026-11-04.md` | URL index, licencias y normas; evidencia secundaria a la verificación de este informe. |

## 4. Código FeralRF actual

### 4.1 Python/host

| ID | Ruta, símbolo y líneas | Evidencia y límite |
|---|---|---|
| FW-H01 | `python/feralrf/radio.py:18-30` (`Packet`) | timestamp, channel, RSSI, `lqi`, `crc_ok`, data y metadata LL opcional; no contiene PHY ni FCS-policy field. |
| FW-H02 | `python/feralrf/radio.py:113-120,455-478` (`Radio`, `set_phy`, `set_channel`) | PHY/canal se conservan en estado `Radio`; no se exponen dentro de cada `Packet`. |
| FW-H03 | `python/feralrf/radio.py:599-709` (`start_rx`, `stop_rx`, `read_packets`) | RX control y parseo response→Packet; el parser no comprueba explícitamente que `pkt_len` esté totalmente presente antes del slice. |
| FW-H04 | `python/feralrf/radio.py:725-750` (`transmit`, `transmit_frame`) | Host espera ACK/ERROR; no hay evento terminal correlacionado de completion. |
| FW-H05 | `python/feralrf/protocol.py:11-90` | COBS/CRC16 para comandos y respuestas, no IEEE FCS ni Zigbee. |
| FW-H06 | `python/feralrf/enums.py:8-18,21-89` | `PHY.IEEE_802_15_4=4`, comandos/respuestas actuales. |
| FW-H07 | `python/pyproject.toml:5-44` | GPL-3.0; runtime pyserial, pyserial-asyncio, COBS; cryptography/dev y KillerBee opcionales. |
| FW-H08 | `python/feralrf/integrations/killerbee.py:1-19,95-119` | KillerBee opcional/lazy; consume `pkt.data`, no es decoder Zigbee. |

### 4.2 Firmware/RF

| ID | Ruta, símbolo y líneas | Evidencia y límite |
|---|---|---|
| FW-F01 | `firmware/cc1352/src/protocol.c:115-180` | Frame serial `CMD|SEQ|LEN|PAYLOAD|CRC16`; separado de protocolos RF. |
| FW-F02 | `firmware/cc1352/src/command_processor.c:188-258` | RX/TX ACK enviado después de validar/encolar. |
| FW-F03 | `firmware/cc1352/src/control_task.c:332-357,440-454` | TX se ejecuta después; error posterior es evento async seq 0; RX control diferido. |
| FW-F04 | `firmware/cc1352/src/data_task.c:28-65,76-139` | Serializa Packet metadata; hasta ocho packet drains por poll; RX path. |
| FW-F05 | `firmware/cc1352/src/ll_manager.c:40-159` | BLE classification; default/IEEE pass-through sin MAC/Zigbee parse. |
| FW-F06 | `firmware/cc1352/include/radio_if.h:15-28` | `RadioIF_RxPacket`: timestamp/channel/RSSI/lqi/crc/data. |
| FW-F07 | `firmware/cc1352/src/radio_if.c:324-381` | RF TX `RF_runCmd` y status interno; no host-correlated completion. |
| FW-F08 | `firmware/cc1352/src/radio_if.c:457-488` | IEEE TX acepta 1–125 bytes, copia MPDU, `TRIG_NOW`. |
| FW-F09 | `firmware/cc1352/src/radio_if.c:1335-1371` | IEEE RX config incluye PHR/FCS/RSSI/corr/timestamp. |
| FW-F10 | `firmware/cc1352/src/radio_if.c:1497-1568` | PHR length; elimina PHR; CRC-bad descartado; MPDU+FCS y metadata quedan en queue. |
| FW-F11 | `firmware/cc1352/src/smartrf_ieee_15_4_0.c:81-163` | Filtering/auto-ACK/CCA apagados; TX no aporta PHR/FCS. |
| FW-F12 | `firmware/cc1352/CMakeLists.txt:12-18,129-184,225-253` | CC1352P7 GNU target; no incluye Z-Stack/TIMAC runtime del rol. |
| FW-F13 | `firmware/cc1352/src/data_task.c:28-65` | Payload RX lleva timestamp/channel/RSSI/lqi/crc/len/data y metadata LL. |

### 4.3 Evidencia experimental histórica

| ID | Ruta | Observación | Límite |
|---|---|---|---|
| EXP-01 | `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/SUMMARY.md:16-37` | Raw IEEE/FRAME observed en canal 25 bajo setup registrado. | No Zigbee, conformance, PER/range general ni identidad actual de firmware. |
| EXP-02 | `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/01_ieee_raw_frame_burst.md:13,358-443,891-906` | RAW 20/20, 10/10; FRAME 10/10 histórico. | ACK de enqueue no prueba RF por sí solo. |
| EXP-03 | `docs/HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md:47-59` | Índice de evidencia previa. | No se promovió a validación Zigbee. |
| EXP-04 | `docs/HARDWARE_VALIDATION_RESULTS/VALIDATION_MATRIX_CURRENT.md:57-59,67,71-79` | Estado histórico de RX/TX/completion. | Claims siguen limitados por firmware/build y telemetry. |
| EXP-05 | `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/execution_report.md:77-107` | Repetición bilateral 10/10; informa expresamente que no validó Zigbee stack. | Canal 25/setup y frames marcados. |

## 5. TI SimpleLink SDK fijado

### 5.1 ZCL

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-Z01 | `source/ti/zstack/stack/zcl/zcl.c:9-37`; local SDK manifest TI Z-Stack Licenses | Copyright TI 2019; BSD-3-Clause file notice. Manifest separately lists BSD source and TI Text limited source/binary for stack paths; file header decides exact source classification. |
| SDK-Z02 | `source/ti/zstack/stack/zcl/zcl.c:40-63` | Unidad depende de config, AF, BDB, Green Power/ports; no codec autónomo completo. |
| SDK-Z03 | `source/ti/zstack/stack/zcl/zcl.c:2412-2446`; `zcl.h:822-842` | `zclParseHdr()`, header 3/5 bytes, sin length arg; BSD file but bounded port still not recommended. |
| SDK-Z04 | `source/ti/zstack/stack/zcl/zcl.c:2459-2482` | `zclBuildHdr()` static, sin capacity; fuera baseline. |
| SDK-Z05 | `source/ti/zstack/stack/zcl/zcl.c:2961-3076` | `zclSerializeData()`, layout C y sin capacity. |
| SDK-Z06 | `source/ti/zstack/stack/zcl/zcl.c:3195-3322` | Helpers longitud; zero ambiguo; attr length lee prefix sin available bytes. |
| SDK-Z07 | `source/ti/zstack/stack/zcl/zcl.h:490-557,597-652,822-1016` | Frame flags, command IDs, datatype IDs/structures. No sustituye ZCL7 normativa. |

### 5.2 ZDO, NWK y APS

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-Z08 | `source/ti/zstack/stack/zdo/zd_profile.h:86-87,154-186,229-241` | ZDO endpoint/profile 0; selected cluster IDs; `zdoIncomingMsg_t` separates TSN/asdu. Header is TI Limited License; reference only absent clearance. |
| SDK-Z09 | `source/ti/zstack/stack/zdo/zd_profile.c:2104-2106` | Extrae `TransSeq=pData->cmd.Data[0]`, `asdu=pData->cmd.Data+1`, longitud menos TSN. |
| SDK-Z10 | `source/ti/zstack/stack/zdo/zd_object.c:3005-3194,3543-3618`; `zd_object.h:109-145,200-205` | Address/node/simple/active EP/device announce field order; heap/bounds risks. File header is TI Limited License; no direct port planned. |
| SDK-Z11 | `source/ti/zstack/stack/nwk/nwk_util.h:111-155,319-321`; `nwk.h`, `nwk_bufs.h`, `nwk_globals.c/.h` | NWK masks/layout hints; `NLDE_ParseMsg()` declaration only. `stack/nwk/nwk.c` absent. `nwk_util.h` carries TI Limited License. |
| SDK-Z12 | `source/ti/zstack/stack/nwk/aps_mede.h:91-124,288-308`; `aps_util.h:110-113` | `aps_mede.h` exists; APS controls/struct; `APSDE_ParseMsg()` declaration only. Both files inspected; TI Limited License. |
| SDK-Z13 | `source/ti/zstack/stack/nwk/aps.h:9-63`; `nwk_util.h:9-63` | TI Limited License clause visible by-file: source and resulting derivatives for TI Devices; interpretation of host Python scope remains legal-review issue. |
| SDK-Z14 | `source/ti/zstack/stack/zdo/zd_profile.h/.c`, `zd_object.c/.h` first 65 lines | Each file header inspected: copyright TI 2004–2015/14, Limited License; ZDO source not BSD merely because manifest has a general BSD source row. |
| SDK-Z15 | `source/ti/zstack/stack/nwk/aps_mede.h`, `aps_util.h`, `aps.h` first 65 lines | Each header inspected; copyright TI, Limited License. `aps_mede.h` verified present. |

### 5.3 ZMAC/TIMAC/runtime

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-M01 | `source/ti/zstack/zmac/f8w/zmac.c:506-529` + header lines 1-63 | `ZMacInit()` delega `MAP_MAC_*` y estado TIMAC; file carries TI Limited License. |
| SDK-M02 | `source/ti/zstack/zmac/f8w/zmac.c:865-917,1140-1203` | Association y MCPS wrapper delegan a MAC API; allocations/callbacks runtime. |
| SDK-M03 | `source/ti/zstack/zmac/f8w/zmac_cb.c:164-244` + header lines 1-63 | Callback ownership, OSAL queues/allocations hacia NWK; file carries TI Limited License. |
| SDK-M04 | `source/ti/zstack/.meta/zstack.syscfg.js:150-226` | Librerías binarias se seleccionan por dispositivo/rol/ISA/toolchain. |
| SDK-M05 | `source/ti/zstack/lib/iar/`, `source/ti/zstack/lib/ticlang/` | No se acredita target de rol GNU GCC. |
| SDK-M06 | `source/ti/zstack/stack/sec/`, `stack/zdo/zd_sec_mgr.c`, `stack/sec/ssp.h` | Keys/counters/security services; no se integran en decoder. |
| SDK-M07 | `source/ti/zstack/stack/af/af.c:463-486,697-718` | AF compara ProfileID de endpoint; no define allowlist de decoder ni hace que cualquier payload sea ZCL. |

## 6. Documentación oficial primaria

Fecha de acceso de fuentes investigadas en esta revisión: 2026-11-04. URL/index extraction verifies the following identified documents/sections; permissions for local retention and redistribution remain governed by the standards owner terms.

| ID | Documento oficial y URL | Versión/fecha registrada | Soporte y límite |
|---|---|---|---|
| TI-D01 | [Z-Stack 8.30 User’s Guide](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/zigbee-users-guide.html) | SDK 8.30.01.01 | Arquitectura/configuración del runtime; no prueba fit FeralRF. |
| TI-D02 | [Hardware Architecture SDK 8.30](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/cc13xx_cc26xx/hardware-architecture.html) | SDK 8.30, © 2005–2024 registrado | CM0 PHY, system core superiores/RF Driver; descripción TI. |
| TI-D03 | [Zigbee Configuration SDK 8.30](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/syscfg/zigbee.html) | SDK 8.30 | Roles, channels, BDB, keys, endpoints/config generated. |
| TI-D04 | [Z-Stack 8.30.00 Release Notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/release_notes_zigbee_8_30_00.html) | 8.30.00 | Declara Zigbee PRO 2017/R22 y ZCL7; soporta selección de profile, no compatibilidad/decoder FeralRF. |
| TI-D05 | [SimpleLink SDK 8.30.01.01 Release Notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | 8.30.01.01 | Package/tools/components. |
| TI-D06 | [SDK manifest](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html), local copy `SDKROOT/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html` | manifest SRAS00012194, package 8.30.01.01 | Lists TI Z-Stack 8.30.00.x under TI Text File License binary/limited source and BSD-3 Clause source. Legend says path wildcard and exception convention; file header must be inspected. License clauses in official manifest and local `license_simplelink...txt`. |
| TI-D07 | [CC1352P7 product](https://www.ti.com/product/CC1352P7), [datasheet](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf) | Product page/datasheet Rev. A; prior access 2026-11-03 | Device capability; no integration proof. |
| IEEE-D01 | [IEEE Std 802.15.4-2015](https://standards.ieee.org/ieee/802.15.4/5788/) | Published 2016-04-22; record says superseded by 2020 and lists errata/amendments; accessed 2026-11-04 | Exact edition now verified in R22 Annex D §D.1.1–D.1.2; get licensed text/corrigendum before implementation. |
| CSA-D01 | [Zigbee Specification 05-3474-22](https://zigbeealliance.org/wp-content/uploads/2021/10/docs-05-3474-22-0csg-zigbee-specification-1.pdf) | Revision 22 1.0, 2017-04-19; indexed/extracted 2026-11-04 | R22 document ID, date, ZDO schema sections and explicit 802.15.4-2015 reference. CSA copyright/terms apply; not redistribution permission. |
| CSA-D02 | [Zigbee Cluster Library 07-5123-07](https://www.zigbeealliance.org/wp-content/uploads/2021/10/07-5123-07-ZigbeeClusterLibrary_Revision_7-1.pdf) | Revision 7, copyright 2007–2018; indexed/extracted 2026-11-04 | Table 2-3 command IDs; §2.5.5.1.2 successful Write Attributes response; §§2.6.2.11–.12 short strings; copyright owner’s conditions apply. |
| CSA-D03 | [CSA Terms](https://csa-iot.org/terms-conditions/), [specification downloads](https://csa-iot.org/developer-resource/specifications-download-request/) | Access 2026-11-04 | Download/use subject to terms/notices; site content copying/republication restricted without written consent; underlying other IP is not licensed by general terms. Legal interpretation reserved. |
| CSA-D04 | [Zigbee Specification 05-3474-21](https://zigbeealliance.org/wp-content/uploads/2019/12/docs-05-3474-21-0csg-zigbee-specification.pdf) | R21, 2015; Table 2.44, accessed 2026-11-04 | Names Profile ID `0x0104` Common (HA); historical profile identifier context, not an R22 declaration. |
| CSA-D05 | [Lighting & Occupancy Device Specification 15-0014-05](https://www.zigbeealliance.org/wp-content/uploads/2019/11/docs-15-0014-05-0plo-Lighting-OccupancyDevice-Specification-V1.0.pdf) | 2016-02-24 | Complementary official application-profile source; official link currently redirects via CSA page in inspection; do not claim full clause-level check until authorized copy retained. |
| CSA-D06 | [CSA Trademark and Logo Usage Guidelines](https://csa-iot.org/wp-content/uploads/2021/12/CSA-Trademark-and-Logo-Usage-Guidelines-05.06.2021.pdf) | 2021-05-06 document, accessed 2026-11-04 | No “Zigbee Certified” / logo claim without certification/authorization. |
| CSA-D07 | [CSA specification download page](https://csa-iot.org/developer-resource/specifications-download-request/) | Access 2026-11-04 | R22/R22.1 and R23 are distinct labels; no substitution without approval. |

### 6.1 APS application profile evidence

The pinned SDK defines `ZDO_PROFILE_ID=0` and endpoint 0 (`SDK-Z08`) and routes profile IDs configured by application endpoints (`SDK-M07`); it does not establish this decoder's allowlist. R21 Table 2.44 names `0x0104` Common (HA), and the complementary 2016 application profile/TI Academy references support it historically. It is not declared in the extracted Core R22 text and is not SDK-confirmed. Proposal 1.2 recommends it as first profile-context for generic foundation ZCL only, conditional on profile source and supervisor approval. No cluster schemas or manufacturer-specific ZCL become enabled by that profile identifier.

## 7. Externos de comparación y licencias

| ID | Fuente | Uso admisible | Licencia/limitación |
|---|---|---|---|
| EXT-01 | [Scapy v2.8.0, commit `e2e35c07`](https://github.com/secdev/scapy/tree/e2e35c07bce2f40e7513e1b29a6d300d5329dbc3); upstream [`pyproject.toml`](https://github.com/secdev/scapy/blob/master/pyproject.toml) | Comparator MAC/NWK/APS/ZDP/ZCL | GPL-2.0-only; not run in this review; don't embed/import by default into GPL-3 FeralRF. |
| EXT-02 | [zigpy 2.4.1, commit `10b68377`](https://github.com/zigpy/zigpy/tree/10b68377bcbd9a8b15cbbfb506cebdc450704614) | Schema cross-check ZDO/ZCL | GPL-3.0 registrado; controller context, no raw-MPDU oracle. |
| EXT-03 | [WHAD v1.2.18, commit `9d480163`](https://github.com/whad-team/whad-client/tree/9d48016375cd2fdae58517fe6f2fb97022c3bb61) | Comparar abstracción sniffer | Upstream repository declares MIT; exact snapshot not run, no FeralRF connector shown. |
| EXT-04 | [KillerBee upstream](https://github.com/riverloopsec/killerbee) | Referencia del adapter existente | Upstream DEVELOPMENT says BSD and references LICENSE; installed extra version not pinned here. Adapter remains separate. |
| EXT-05 | [Wireshark Zigbee NWK fields](https://www.wireshark.org/docs/dfref/z/zbee_nwk.html); [license README](https://github.com/wireshark/wireshark/blob/master/README.md) | Comparator desde PCAP | Application GPLv2; most components GPL-2-or-later, some file-level exceptions; exact release/config must be fixed. |

Los comparadores son secundarios; no copian resultados a runtime y no resuelven divergencias frente a la norma.

### 7.1 Política de provenance y licencia

La preferencia es implementación independiente desde documentos normativos autorizados, conservando versión/citas en schemas, y usando TI como contraste. Esto evita portar ownership `OsalPort_malloc/free`, structs dependientes de ABI, globals/tasks y supuestos de runtime/compiler. Las licencias visibles difieren: `zcl.c` tiene aviso BSD-style (`SDK-Z01`), mientras `zd_object.c`, `nwk_util.h`, `aps.h` expresan restricciones TI Devices en sus encabezados; no debe afirmarse que todo Z-Stack sea libremente reutilizable. Protocol spec, headers, código fuente y binarios son categorías distintas. Una futura adaptación requiere revisión file-by-file y preservación de notices; este registro no emite dictamen legal.

## 8. Discrepancias reconciliadas

| ID | Discrepancia anterior | Evidencia actual | Efecto |
|---|---|---|---|
| DISC-01 | Referencia histórica a `source/ti/zstack/stack/nwk/nwk.c` | Ruta ausente del SDK; inventario real aparece en SDK-Z11 | No citar parser/source NWK reusable. |
| DISC-02 | `Packet` tratado como si describiera contrato completo | `Packet` no tiene PHY/FCS policy (FW-H01); PHY vive en `Radio` | `CaptureContext` requerido; faltante produce stop. |
| DISC-03 | Classifier antes de parser NWK consultaba campos NWK | Proposal 1.1 §7.2–8 duplicates offsets | Single eligibility gate → single NWK bounded parser → classifier over result; parse status separate. |
| DISC-03a | Semejanza estructural puede sugerir “confirmed Zigbee” | FeralRF transporta IEEE raw, sin protocolo Zigbee metadata (FW-F05, FW-H01) | Rename to `verified_by_provenance`; declared context isn't independent verification. |
| DISC-04 | Seguridad dibujada principalmente en NWK/APS | MAC aux security también cambia payload visible; revisión normativa requerida | Security gate en los tres límites, no decryption. |
| DISC-05 | ZCL dispatch implícito por cluster/profile | R21 Table 2.44 names `0x0104` Common (HA); R22 text doesn't give that literal; SDK AF uses configured profile IDs | Recommend conditional `0x0104` for generic foundation only; no cluster-specific or manufacturer's semantics. |
| DISC-06 | ACK TX podía interpretarse como completion | ACK en `command_processor`; ejecución diferida `control_task`; API espera ACK (FW-F02/03, FW-H04) | V-RAW-01 separa accepted/completed/observed/ACK/app response. |
| DISC-07 | ZDO ASDU podía contarse incluyendo TSN | `zd_profile.c:2104-2106` separa el byte TSN | Schemas ZDO comienzan después de TSN. |
| DISC-08 | Reuso TI descrito como general | Diferentes licencias y parser source ausente (SDK-Z01/13, SDK-Z11/12) | Prioridad a implementación independiente y revisión por archivo si cambia. |

## 9. Mapa de evidencia a requisitos

| Requisito | IDs de fuente | Evidencia que justifican | Límite abierto |
|---|---|---|---|
| `R-BASE-01` | FW-H01/02/03, FW-F04/06/09/10 | Bytes y metadata actuales llegan a Python | Contexto PHY/FCS en wrapper; contract integrity requiere fixtures. |
| `R-MAC-01` | FW-F09/10/11, IEEE-D01, CSA-D01 | MPDU+FCS disponible; R22 Annex D references IEEE 802.15.4-2015 | MAC subset/errata crosswalk and corpus. |
| `R-ID-01` | FW-H01, FW-F05, CSA-D01, CSA-D02 | Raw IEEE has no Zigbee label; one NWK structural parse can provide candidate status | Independent provenance/corpus still needed; one parser not implemented. |
| `R-NWK-01` | SDK-Z11, TI-D04, CSA-D01 | Masks visible, TI parse symbol declaration-only; Core R22 describes fields | Parser/corpus not implemented. |
| `R-APS-01` | SDK-Z12/13, TI-D04 | Campos APS visibles y límites de licencia | Layout exacto/fragment/security y Core R22. |
| `R-ZDO-01` | SDK-Z08/09/10, TI-D04, CSA-D01 | IDs/field order in R22 §2.4; TI TSN split | Full status/count validation corpus and profile context remain. |
| `R-ZCL-01` | SDK-Z03/07, TI-D04, CSA-D02 | Header helper and ZCL7 frame definitions | Need approved APS profile; helper itself lacks bounds. |
| `R-ZCL-02` | SDK-Z05/06/07, TI-D04, CSA-D02 | IDs, data types; ZCL7 confirms success Write response and `0xFF` string invalid sentinel | Full type/direction crosswalk/corpus still pending. |
| `R-SEC-01` | SDK-Z12, SDK-M06, IEEE-D01, TI-D04 | Indicios/campos de seguridad | CCM*, MIC level, AAD/key context quedan fuera baseline. |
| `R-RAW-01` | FW-H01, FW-F06, FW-F10 | Buffer y packet fields existentes | Ownership/lifetime del wrapper futuro. |
| `R-ERR-01` | SDK-Z03–10, FW-H03 | Helpers de entrada hostil no todos bounded | API futura de issues. |
| `R-VAL-01` | EXT-01/05, CSA-D01, IEEE-D01 | Opciones de comparator y autoridad normativa | Releases/corpus no están fijados/ejecutados. |

## 10. Vacíos de evidencia pendientes

1. Maintain licensed copies/hash and errata/corrigendum for Core R22/ZCL7/IEEE-2015; complete all status/flags/type/MIC subset crosswalk.
2. Confirm applicable 0x0104 profile document and source rights; only generic foundation scope proposed, supervisor approval pending.
3. Corpus legal y versionado con provenances para Zigbee known captures, no-Zigbee, secured, malformed, manufacturer-specific y fragmented.
4. Confirmación byte-exacta FCS/PHR/length y metadata para firmware/artifact identificado; timestamp wrap/continuidad y pérdida bajo carga.
5. Versión y config fijada de dissector independiente y comparación ejecutada.
6. Revisión legal file-by-file si se reutiliza código TI; esta propuesta sólo recomienda implementación independiente.
7. Throughput/buffer backlog host bajo captura sostenida antes de estado pasivo.
8. Completion RF correlacionado no se expone; cualquier futuro claim terminal requiere interfaz o instrumento independiente claramente definido.

Estos vacíos no cambian el sentido arquitectónico host-side. Sí bloquean detalles semánticos, assertions de validación y cualquier claim de Zigbee interoperability.

## 11. Inventario legal TI por archivo y símbolo (revisión 2026-11-04)

El manifest local se encuentra en `SDKROOT/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html` (submódulo exacto BASE-04) y su licencia local en `SDKROOT/license_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.txt`. El manifest remoto muestra la tabla TI Z-Stack en el bloque HTML dedicado (líneas web 226–233), con dos filas generales: TI Text File License para binary/limited source y BSD-3-Clause para source, sobre la misma ruta de instalación. Su leyenda dice que un wildcard de ruta marca todos los ficheros como la licencia indicada y que las excepciones se enumeran aparte. No es clasificación precisa de cada fichero de este conjunto; por ello inspeccioné la cabecera real de cada archivo. El path registrado por manifest no sustituye el aviso de copyright/license en la fuente.

| Source file | Symbol / kind | Copyright holder | File license / notice | Manifest | Intended use in proposal | Proposed implementation | Compatibility concern | Treatment / required action | Evidence & confidence |
|---|---|---|---|---|---|---|---|---|---|
| `SDKROOT/source/ti/zstack/stack/zcl/zcl.c:9-37` | `zclParseHdr()` 2412–2446; `zclBuildHdr()` 2459–2482; `zclSerializeData()` 2961–3076; `zclGetDataTypeLength()` 3195–3283; `zclGetAttrDataLength()` 3296–3322 | Texas Instruments Inc. (2019) | BSD-style 3-clause file notice | TI Z-Stack source appears in BSD-3 row; do not generalize | Technical reference; algorithm comparison | Independent bounded Python ZCL7 parser; builder not baseline | Parse helper has no input length; attr-length reads prefix without available bytes; static builder has no capacity; C unit includes runtime services | `REFERENCE_ONLY`; direct ZCL parser is `DIRECT_REUSE_POTENTIALLY_PERMITTED` by notice subject to notices/disclaimer/no-endorsement, but `NOT_RECOMMENDED_FOR_DIRECT_REUSE` technically. Obtain legal review before copy/derivative in GPL package. | Local file header lines 9–37/function lines above; manifest section; official manifest URL below. Confirmed source. |
| `SDKROOT/source/ti/zstack/stack/zcl/zcl.h:9-37,490-557,597-652,822-1016` | FCF definitions, command/data-type constants and structs | Texas Instruments Inc. (2019) | BSD-style 3-clause | Same general BSD row, but header notice still controls | Cross-check names and field/type distinctions | Tables from ZCL7, project-owned representation | C struct layout/enum does not itself provide all wire semantics and ABI layouts are not Python schemas | `REFERENCE_ONLY`; `INDEPENDENT_IMPLEMENTATION_FROM_SPEC` preferred. If copying selected declarations/text, preserve notices and obtain legal review. | Local header cited lines; manifest. Confirmed source. |
| `SDKROOT/source/ti/zstack/stack/zdo/zd_profile.h:1-65` | ZDO profile/cluster constants and structures | Texas Instruments (2004–2015) | `All rights not granted`, `Limited License`, TI Devices condition | Same Z-Stack general bucket; manifest contains separate TI Text and BSD rows | Field/context cross-check only | Core R22-based independent ZDO schemas | Distribution/source derivative only licensed for TI Devices per clause; patent language combination-limited; generated code/runtime context | `REFERENCE_ONLY`; any copy/translation `REQUIRES_LICENSE_REVIEW`; do not reproduce definitions without clearance. | Local header 1–65; SDK manifest/installed license. Confirmed notice; legal scope interpretation remains unresolved. |
| `SDKROOT/source/ti/zstack/stack/zdo/zd_profile.c:2104-2106` | `fillAndSend` path, TSN/ASDU split; ZDP field processing | Texas Instruments (2004–2015) | TI `Limited License` as file header | Same general rows | Validate TSN separation/order | Parser independently from Core R22 | Runtime state, globals, message dispatch; not just wire reader | `REFERENCE_ONLY`; do not copy/translate without legal review. | Local header and lines 2104–2106; confirmed. |
| `SDKROOT/source/ti/zstack/stack/zdo/zd_object.c:9-65,3005-3194,3543-3618` | `ZDO_ParseNodeDescRsp`, `ZDO_ParseSimpleDescRsp`, `ZDO_ParseEPListRsp`, related parse functions | Texas Instruments (2004–2015) | TI `Limited License`; TI Devices clauses | Same general rows | Compare field order and current behavior | Independent parser from R22 with spans/errors | Heap allocation, `OsalPort_*`, input structs, ownership and insufficient bounds; C functions not self-contained | `NOT_RECOMMENDED_FOR_DIRECT_REUSE`; `REFERENCE_ONLY`; copying/translation/distribution needs legal review. | Local function/header refs and official TI manifest. Confirmed source. |
| `SDKROOT/source/ti/zstack/stack/zdo/zd_object.h:1-65,449-474` | `ZDO_Parse*` declarations and structs | Texas Instruments (2004–2014) | TI `Limited License` | General Z-Stack rows | API/declaration cross-check | Project-specific data models from R22, no TI ABI types | Header is not independently licensed because it is a header; declarations do not grant implementation | `REFERENCE_ONLY`; don't vendor/include this header. Legal review for copied declaration. | Local header lines/functions; confirmed. |
| `SDKROOT/source/ti/zstack/stack/nwk/nwk_util.h:1-63,111-155,319-321` | NWK masks/fields; declaration `NLDE_ParseMsg()` | TI (2004–2015) | TI `Limited License` | Same general rows | Wire-field cross-check | Single independent Core R22 NWK parser | `NLDE_ParseMsg` has declaration only; `stack/nwk/nwk.c` absent | `REFERENCE_ONLY`; no copying header or claim of reusable implementation. | Local header; `rg` definition search; confirmed. |
| `SDKROOT/source/ti/zstack/stack/nwk/aps_mede.h:1-65,91-124,288-308` | APS frame controls/structures | TI (2004–2015) | TI `Limited License`; file exists | Same general rows | Frame layout cross-check | Independent APS parser from Core R22 | Contains primitives/runtime-facing types; not parser implementation | `REFERENCE_ONLY`; no direct reuse absent review. | Local header; verified existence/notice. |
| `SDKROOT/source/ti/zstack/stack/nwk/aps_util.h:1-65,110-113` | declaration `APSDE_ParseMsg()` | TI (2004–2007) | TI `Limited License` | Same general rows | Demonstrate declared symbol and dependencies | Independent APS parser | No source definition accessible; declaration does not prove reuse | `REFERENCE_ONLY`, `UNRESOLVED` implementation availability; no vendor/call. | Local header and SDK search; confirmed declaration only. |
| `SDKROOT/source/ti/zstack/stack/nwk/aps.h:1-65` | APS task API/structures | TI (2004–2015) | TI `Limited License` | Same general rows | Runtime boundary evidence | No header import | OSAL/task/runtime and TI-device distribution condition | `REFERENCE_ONLY`; legal review for any copying. | Local header; confirmed. |
| `SDKROOT/source/ti/zstack/zmac/f8w/zmac.c:1-63,506-529,865-917,1140-1203` | `ZMacInit`, wrappers and MAC mapping | TI (2005–2017) | TI `Limited License` | Binary/limited-source plus source rows for Z-Stack | Exclusion/runtime boundary evidence | No FeralRF reuse | `MAP_MAC_*`, TIMAC binary libraries, OSAL/runtime; no verified GNU linkage | `REFERENCE_ONLY`; not recommended direct reuse; licensing and ABI constraints. | Local source header and code refs; confirmed. |
| `SDKROOT/source/ti/zstack/zmac/f8w/zmac_cb.c:1-63,164-244` | MAC/NWK callback and ownership | TI (2005–2017) | TI `Limited License` | Same general rows | Exclusion/runtime boundary evidence | No FeralRF reuse | OSAL ownership/queues and stack lifecycle | `REFERENCE_ONLY`; no copy or linkage. | Local source header/code; confirmed. |

### 11.1 License clause reading and FeralRF compatibility

The TI Text File License attached to the manifest says the license for redistributed/modified source and resulting derivative works is limited to TI Devices; it also requires preservation/reproduction of notices, restricts redistribution/use for binary to TI Devices, bars reverse engineering of supplied binary and includes no-endorsement, patent-scope, warranty and liability language. The SDK manifest offers a distinct BSD-3-Clause source category; `zcl.c`/`zcl.h` have corresponding BSD notices, while the listed ZDO/NWK/APS/ZMAC headers have TI Limited License. Do not conclude all source tree BSD from the manifest or all source TI-only from it either.

The license does not clearly settle whether redistributed host Python that only decodes frames associated with a TI device is considered use “only with TI Devices”, or whether translating code into Python is a derivative in the relevant legal sense. It also does not make a product's CC1352P7 automatically satisfy every condition for distributing host code. These are counsel questions, not this report's legal conclusion.

`LICENSE` at repository root is GNU GPL v3-or-later for the project; `python/pyproject.toml` declares GPL-3.0 for package. BSD-3 source may be combinable with GPL-3 when notices/conditions are preserved, but each file/derivative needs review. TI restricted source should not be copied, modified, translated or redistributed into the host decoder without explicit legal review/permission. Translation is not presumed clean-room or independent. Protocol facts, identifiers and field values are not automatically protected as source expression; nevertheless source text, code, comment arrangement, header layouts and selected expressive structures require separate assessment. Implement from legally accessed normative requirements with independent prose, representation, tests and provenance; do not copy long specification excerpts. Patent rights, trademark/certification and CSA/IEEE contract terms are not resolved by implementing protocol requirements.

### 11.2 Requested activity classifications

| Candidate/activity | Classification | Conditions |
|---|---|---|
| Inspect/cite TI source without code excerpts | `REFERENCE_ONLY` | Identify file/line, keep excerpts minimal, do not claim compatibility. |
| Implement bounded ZCL/NWK/APS/ZDO parser independently from authorised specification | `INDEPENDENT_IMPLEMENTATION_FROM_SPEC` | Independent code/data organization and provenance; normative document use/rights, patents, names and attribution reviewed. |
| Copy/adapt `zclParseHdr()` from BSD-noticed `zcl.c` | `DIRECT_REUSE_POTENTIALLY_PERMITTED` and technically `NOT_RECOMMENDED_FOR_DIRECT_REUSE` | Preserve license/copyright/disclaimer/no-endorsement; add bounds/hardening; legal/package review before distributing. |
| Copy/adapt ZDO parser or NWK/APS TI-restricted headers/source | `REQUIRES_LICENSE_REVIEW` / `NOT_RECOMMENDED_FOR_DIRECT_REUSE` | Explicit permission/qualified legal opinion required for host Python and distribution; do not treat hardware presence as automatic satisfaction. |
| Translate a TI-restricted C implementation into Python | `REQUIRES_LICENSE_REVIEW` | Translation/derivative question cannot be resolved by labeling implementation “independent”. |
| Reproduce protocol fields/constants based on normative requirements | `INDEPENDENT_IMPLEMENTATION_FROM_SPEC` conditional | Avoid copying source comments/struct layouts; document standards edition/section and inspect for source expression. |
| Link TI Z-Stack/TIMAC binary libraries | `NOT_RECOMMENDED_FOR_DIRECT_REUSE` | Outside architecture; toolchain/runtime and TI-only binary restrictions unresolved. |

## 12. Licencias de comparadores / dependencias externas

| Dependency | Exact record / upstream license evidence | Intended relation to FeralRF | Position |
|---|---|---|---|
| Scapy | EXT-01 tag/commit; upstream `pyproject.toml` says GPL-2.0-only | Candidate independent comparator only | Do not import, bundle or add runtime dependency by default to GPL-3.0 package; external-process scenario is separate and not automatic safe harbor. |
| zigpy | EXT-02 registered version/commit; upstream repo declares GPL-3.0 | ZCL/ZDO schema/controller cross-check; not raw MPDU oracle | External dev tool only; review its dependency/fixtures if redistributed. |
| Wireshark | EXT-05 live reference; upstream README: app GPLv2, most components GPL-2-or-later and exceptions exist | PCAP comparator | Fix exact version/dissector settings before validation; don't link/embed; file-level notices matter. |
| WHAD | EXT-03 registered version/commit; upstream declares MIT | Concept/alternative sniffer abstraction comparison | No integration dependency or proven FeralRF transport. |
| KillerBee | EXT-04 upstream DEVELOPMENT says BSD; FeralRF package optional extra `killerbee>=3.0.0` | Existing separate integration | Not part of Zigbee decoder; exact resolved package version not audited here. |

Use of another executable and its output is not automatically the same as incorporating its code, but relationship/distribution/result rights must be assessed by facts. Comparator output does not replace normative oracle. Keep comparators outside decoder runtime and record version, dissector prefs, FCS/link-layer policy and command line in future validation.

## 13. Normative profile/provenance reconciliation

| Conclusion | Evidence | State/limit |
|---|---|---|
| Core `05-3474-22`, R22 1.0, dated 2017-04-19 | Official CSA PDF metadata; TI Z-Stack 8.30 release notes support R22 | Verified source; use exact R22 not R22.1/R23 unless a new SchemaProfile. |
| IEEE 802.15.4-2015 is R22 MAC/PHY base | R22 Annex D §§D.1.1–D.1.2 and IEEE record | Confirmed; IEEE text/errata copy remains to be acquired under license. |
| ZCL `07-5123-07` R7 | CSA PDF metadata and TI release notes | Confirmed revision; selected field-level subset not all independently reviewed yet. |
| ZCL Write Attributes Response success | ZCL7 §2.5.5.1.2 / figure 2-12: one success status record; omitted Attribute ID | Confirmed; main proposal corrected. |
| short string sentinel | ZCL7 §§2.6.2.11–2.6.2.12: 0x00 empty, 0xFF invalid, no data octets for either | Confirmed; `0xFF` datatype “unknown” is a separate field value. |
| APS profile `0x0104` | R21 Table 2.44 names Common (HA); 2016 app profile and vendor sources corroborate historical use; literal absent in extracted R22; no SDK definition | Sufficient to recommend conditionally as generic foundation context, not enough to claim current complete application profile or activate every cluster. Supervisor approval/document-rights matter. |
| `R-ID-01` disposition | FeralRF raw IEEE; one NWK parser provides structural success; provenance is separate evidence | Rename `confirmed_by_context` to `verified_by_provenance`; classifier cannot reparse NWK. Caller declaration/fixture identity does not prove OTA source. |
| Scope A security | R22 security describes layer-specific protected payload and levels | Headers visible per format; NWK data may hide APS/ZDO/ZCL if encrypted. No decrypt/auth claim; Scope B separate. |

## 14. Evidencia/limitaciones de derechos

1. TI manifest is versioned and local; it lists multiple license families for same component tree. File-by-file headers were checked for every source used as algorithm/wire reference in the proposal.
2. CSA Core/ZCL public URLs were reached by indexed official PDF extraction; downloads/web pages redirect in some cases. This validates document identifiers and the specific extracted provisions cited but does not authorize redistribution or establish local archival rights.
3. IEEE official record verifies edition/status but the normative PDF requires licensed access; no PDF copied into repository.
4. The profile document `15-0014-05` official URL redirected during inspection; its exact clause is not asserted. `0x0104` identity follows R21 Table 2.44, with profile-specific scope still conditional.
5. No lawyer reviewed the TI clauses against FeralRF combined distribution, the host/device boundary, foreign law, GPL obligations or patents. No legal clearance is claimed.
6. No code, compile, source translation, import of comparator, execution, test, firmware flash or RF measurement was performed. All Zigbee capabilities remain proposed/unvalidated.
