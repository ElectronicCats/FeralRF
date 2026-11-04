# Registro de fuentes — propuesta nativa Zigbee para FeralRF, revisión 1.1

**Fecha de actualización:** 2026-11-04
**Propuesta asociada:** [FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md](FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md)
**Revisión crítica incorporada:** [FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md](FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md)

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
| BASE-01 | `git rev-parse HEAD` | `59816be3460884481f2827eacb630d54cfff6ae8` | HEAD al actualizar esta revisión. |
| BASE-02 | `git branch --show-current` | `feature/hardware-validation` | Rama examinada. |
| BASE-03 | `git status --short --branch`, previo a editar | Rama sincronizada con origen; revisión arquitectónica previa `??` | La revisión previa se preservó. |
| BASE-04 | `git submodule status -- firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` | `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`lpf2-8.30.01.01`) | Único SDK usado. |
| BASE-05 | `SDKROOT/.metadata/product.json` | `8.30.01.01` | Versión del paquete SDK. |
| BASE-06 | `git diff fc0de99..HEAD -- firmware python hardware` | Sin cambios | Los commits posteriores al baseline auditado fueron documentación. |
| BASE-07 | `firmware/cc1352/CMakeLists.txt:12-18,35-43` | Target `CC1352P7`, toolchain configurado GNU Arm (`arm-none-eabi-gcc/g++`) | No se compiló ni verificó toolchain instalado. |
| BASE-08 | Paths solicitados | Repository, firmware, Python y `SDKROOT/source/ti/zstack` existen | No se usó otra copia de SDK/repositorio. |

El estado final tras esta tarea debe contener cambios sólo en la propuesta y este registro; la revisión crítica previa seguirá untracked y no se modifica.

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
| SDK-Z01 | `source/ti/zstack/stack/zcl/zcl.c:9-37` | Aviso BSD-style de ese translation unit; no aplica automáticamente a todo Z-Stack. |
| SDK-Z02 | `source/ti/zstack/stack/zcl/zcl.c:40-63` | Unidad depende de config, AF, BDB, Green Power/ports; no codec autónomo completo. |
| SDK-Z03 | `source/ti/zstack/stack/zcl/zcl.c:2412-2446`; `zcl.h:822-842` | `zclParseHdr()`, header 3/5 bytes, sin length arg. |
| SDK-Z04 | `source/ti/zstack/stack/zcl/zcl.c:2459-2482` | `zclBuildHdr()` static, sin capacity; fuera baseline. |
| SDK-Z05 | `source/ti/zstack/stack/zcl/zcl.c:2961-3076` | `zclSerializeData()`, layout C y sin capacity. |
| SDK-Z06 | `source/ti/zstack/stack/zcl/zcl.c:3195-3322` | Helpers longitud; zero ambiguo; attr length lee prefix sin available bytes. |
| SDK-Z07 | `source/ti/zstack/stack/zcl/zcl.h:490-557,597-652,822-1016` | Frame flags, command IDs, datatype IDs/structures. No sustituye ZCL7 normativa. |

### 5.2 ZDO, NWK y APS

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-Z08 | `source/ti/zstack/stack/zdo/zd_profile.h:86-87,154-186,229-241` | ZDO endpoint/profile 0; selected cluster IDs; `zdoIncomingMsg_t` separa TSN/asdu. |
| SDK-Z09 | `source/ti/zstack/stack/zdo/zd_profile.c:2104-2106` | Extrae `TransSeq=pData->cmd.Data[0]`, `asdu=pData->cmd.Data+1`, longitud menos TSN. |
| SDK-Z10 | `source/ti/zstack/stack/zdo/zd_object.c:3005-3194,3543-3618`; `zd_object.h:109-145,200-205` | Address/node/simple/active EP/device announce field order; heap/bounds risks. |
| SDK-Z11 | `source/ti/zstack/stack/nwk/nwk_util.h:111-155,319-321`; `nwk.h`, `nwk_bufs.h`, `nwk_globals.c/.h` | NWK masks/layout hints; `NLDE_ParseMsg()` declaration only. `stack/nwk/nwk.c` absent. |
| SDK-Z12 | `source/ti/zstack/stack/nwk/aps_mede.h:91-124,288-308`; `aps_util.h:110-113` | APS controls/struct; `APSDE_ParseMsg()` declaration only. |
| SDK-Z13 | `source/ti/zstack/stack/nwk/aps.h:9-63`; `nwk_util.h:9-63` | TI Devices license terms visible in these files; not a general permission statement. |

### 5.3 ZMAC/TIMAC/runtime

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-M01 | `source/ti/zstack/zmac/f8w/zmac.c:506-529` | `ZMacInit()` delega `MAP_MAC_*` y estado TIMAC. |
| SDK-M02 | `source/ti/zstack/zmac/f8w/zmac.c:865-917,1140-1203` | Association y MCPS wrapper delegan a MAC API; allocations/callbacks runtime. |
| SDK-M03 | `source/ti/zstack/zmac/f8w/zmac_cb.c:164-244` | Callback ownership, OSAL queues/allocations hacia NWK. |
| SDK-M04 | `source/ti/zstack/.meta/zstack.syscfg.js:150-226` | Librerías binarias se seleccionan por dispositivo/rol/ISA/toolchain. |
| SDK-M05 | `source/ti/zstack/lib/iar/`, `source/ti/zstack/lib/ticlang/` | No se acredita target de rol GNU GCC. |
| SDK-M06 | `source/ti/zstack/stack/sec/`, `stack/zdo/zd_sec_mgr.c`, `stack/sec/ssp.h` | Keys/counters/security services; no se integran en decoder. |
| SDK-M07 | `source/ti/zstack/stack/af/af.c:463-486,697-718` | AF compara ProfileID de endpoint; no define allowlist de decoder ni hace que cualquier payload sea ZCL. |

## 6. Documentación oficial primaria

Fecha de acceso registrada en auditorías previas: 2026-11-03. Estas URLs son fuentes para consultar y aprobar textos exactos; la página catálogo no sustituye el documento normativo completo.

| ID | Documento oficial y URL | Versión/fecha registrada | Soporte y límite |
|---|---|---|---|
| TI-D01 | [Z-Stack 8.30 User’s Guide](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/zigbee-users-guide.html) | SDK 8.30.01.01 | Arquitectura/configuración del runtime; no prueba fit FeralRF. |
| TI-D02 | [Hardware Architecture SDK 8.30](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/cc13xx_cc26xx/hardware-architecture.html) | SDK 8.30, © 2005–2024 registrado | CM0 PHY, system core superiores/RF Driver; descripción TI. |
| TI-D03 | [Zigbee Configuration SDK 8.30](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/syscfg/zigbee.html) | SDK 8.30 | Roles, channels, BDB, keys, endpoints/config generated. |
| TI-D04 | [Z-Stack 8.30.00 Release Notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/release_notes_zigbee_8_30_00.html) | 8.30.00 | Declara Zigbee PRO 2017/R22 y ZCL7; alineación provisional. |
| TI-D05 | [SimpleLink SDK 8.30.01.01 Release Notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | 8.30.01.01 | Package/tools/components. |
| TI-D06 | [SDK manifest](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | 8.30.01.01 | Component license inventory; no file-level legal conclusion. |
| TI-D07 | [CC1352P7 product](https://www.ti.com/product/CC1352P7), [datasheet](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf) | Product page/datasheet Rev. A; prior access 2026-11-03 | Device capability; no integration proof. |
| IEEE-D01 | [IEEE Std 802.15.4-2020](https://standards.ieee.org/ieee/1159/7029/) | Published 2020-07-23; page identifies supersession by 2024 | PHY/MAC revision source; exact R22-mapped edition remains unresolved. |
| CSA-D01 | [CSA specification downloads](https://csa-iot.org/developer-resource/specifications-download-request/) | Access 2026-11-03 | Catalog includes Zigbee Core R22.1/R23.x, ZCL R7/R8 and BDB revisions; full exact text must be obtained. |
| CSA-D02 | [Zigbee Specification document 05-3474-23](https://csa-iot.org/wp-content/uploads/2023/04/05-3474-23-csg-zigbee-specification-compressed.pdf) | R23 public draft/document | Evidence another revision exists; not target baseline. |
| CSA-D03 | [ZCL Specification 075123 rev. 6](https://csa-iot.org/wp-content/uploads/2019/12/07-5123-06-zigbee-cluster-library-specification.pdf) | Revision 6, 2016 | Historical public reference only; does not replace ZCL7. |

### 6.1 APS application profile evidence

The pinned SDK directly defines `ZDO_PROFILE_ID=0` and endpoint 0 (`SDK-Z08`) and routes application endpoint profile IDs as configured (`SDK-M07`). It does not establish an application profile allowlist appropriate for this decoder. `0x0104` is documented in proposal revision 1.1 only as a **candidate requiring official-source verification and supervisor approval**, not as enabled or SDK-confirmed. No application profile is activated in the initial schema profile.

## 7. Externos de comparación y licencias

| ID | Fuente | Uso admisible | Licencia/limitación |
|---|---|---|---|
| EXT-01 | [Scapy v2.8.0, commit `e2e35c07`](https://github.com/secdev/scapy/tree/e2e35c07bce2f40e7513e1b29a6d300d5329dbc3) | Comparator MAC/NWK/APS/ZDP/ZCL | GPL-2.0-only según registro previo; versión no ejecutada aquí. |
| EXT-02 | [zigpy 2.4.1, commit `10b68377`](https://github.com/zigpy/zigpy/tree/10b68377bcbd9a8b15cbbfb506cebdc450704614) | Schema cross-check ZDO/ZCL | GPL-3.0 registrado; controller context, no raw-MPDU oracle. |
| EXT-03 | [WHAD v1.2.18, commit `9d480163`](https://github.com/whad-team/whad-client/tree/9d48016375cd2fdae58517fe6f2fb97022c3bb61) | Comparar abstracción sniffer | MIT; sin connector FeralRF demostrado. |
| EXT-04 | [KillerBee upstream](https://github.com/riverloopsec/killerbee) | Referencia del adapter existente | Snapshot upstream no fijado. |
| EXT-05 | [Wireshark Zigbee NWK fields](https://www.wireshark.org/docs/dfref/z/zbee_nwk.html) | Comparator desde PCAP | Página live; release debe fijarse antes de validación. |

Los comparadores son secundarios; no copian resultados a runtime y no resuelven divergencias frente a la norma.

### 7.1 Política de provenance y licencia

La preferencia es implementación independiente desde documentos normativos autorizados, conservando versión/citas en schemas, y usando TI como contraste. Esto evita portar ownership `OsalPort_malloc/free`, structs dependientes de ABI, globals/tasks y supuestos de runtime/compiler. Las licencias visibles difieren: `zcl.c` tiene aviso BSD-style (`SDK-Z01`), mientras `zd_object.c`, `nwk_util.h`, `aps.h` expresan restricciones TI Devices en sus encabezados; no debe afirmarse que todo Z-Stack sea libremente reutilizable. Protocol spec, headers, código fuente y binarios son categorías distintas. Una futura adaptación requiere revisión file-by-file y preservación de notices; este registro no emite dictamen legal.

## 8. Discrepancias reconciliadas

| ID | Discrepancia anterior | Evidencia actual | Efecto |
|---|---|---|---|
| DISC-01 | Referencia histórica a `source/ti/zstack/stack/nwk/nwk.c` | Ruta ausente del SDK; inventario real aparece en SDK-Z11 | No citar parser/source NWK reusable. |
| DISC-02 | `Packet` tratado como si describiera contrato completo | `Packet` no tiene PHY/FCS policy (FW-H01); PHY vive en `Radio` | `CaptureContext` requerido; faltante produce stop. |
| DISC-03 | Semejanza estructural puede sugerir “confirmed Zigbee” | FeralRF transporta IEEE raw, sin protocolo Zigbee metadata (FW-F05, FW-H01) | `R-ID-01` distingue provenance de consistencia estructural. |
| DISC-04 | Seguridad dibujada principalmente en NWK/APS | MAC aux security también cambia payload visible; revisión normativa requerida | Security gate en los tres límites, no decryption. |
| DISC-05 | ZCL dispatch implícito por cluster/profile | SDK AF usa profile IDs de endpoint; no decoder allowlist (SDK-M07) | App allowlist inicialmente vacía; `0x0104` candidate no habilitado. |
| DISC-06 | ACK TX podía interpretarse como completion | ACK en `command_processor`; ejecución diferida `control_task`; API espera ACK (FW-F02/03, FW-H04) | V-RAW-01 separa accepted/completed/observed/ACK/app response. |
| DISC-07 | ZDO ASDU podía contarse incluyendo TSN | `zd_profile.c:2104-2106` separa el byte TSN | Schemas ZDO comienzan después de TSN. |
| DISC-08 | Reuso TI descrito como general | Diferentes licencias y parser source ausente (SDK-Z01/13, SDK-Z11/12) | Prioridad a implementación independiente y revisión por archivo si cambia. |

## 9. Mapa de evidencia a requisitos

| Requisito | IDs de fuente | Evidencia que justifican | Límite abierto |
|---|---|---|---|
| `R-BASE-01` | FW-H01/02/03, FW-F04/06/09/10 | Bytes y metadata actuales llegan a Python | Contexto PHY/FCS en wrapper; contract integrity requiere fixtures. |
| `R-MAC-01` | FW-F09/10/11, IEEE-D01 | MPDU+FCS disponible; MAC parser no existe | Revisión MAC vinculada con R22 no resuelta. |
| `R-ID-01` | FW-H01, FW-F05, CSA-D01 | Transporte raw sin identidad Zigbee; perfil normativo requerido | Provenance de captures corpus futuro. |
| `R-NWK-01` | SDK-Z11, TI-D04, CSA-D01 | Máscaras visibles, parser TI no definido en fuente | Exact Core R22 y corpus. |
| `R-APS-01` | SDK-Z12/13, TI-D04 | Campos APS visibles y límites de licencia | Layout exacto/fragment/security y Core R22. |
| `R-ZDO-01` | SDK-Z08/09/10, TI-D04 | IDs, TSN split, field order TI | Normativa status/count exacta. |
| `R-ZCL-01` | SDK-Z03/07, TI-D04 | Header helper, flags y tipos SDK | ZCL7 exacta; app profile enable. |
| `R-ZCL-02` | SDK-Z05/06/07, TI-D04 | IDs commands/types/helpers | Dirección/status/string sentinel normativos. |
| `R-SEC-01` | SDK-Z12, SDK-M06, IEEE-D01, TI-D04 | Indicios/campos de seguridad | CCM*, MIC level, AAD/key context quedan fuera baseline. |
| `R-RAW-01` | FW-H01, FW-F06, FW-F10 | Buffer y packet fields existentes | Ownership/lifetime del wrapper futuro. |
| `R-ERR-01` | SDK-Z03–10, FW-H03 | Helpers de entrada hostil no todos bounded | API futura de issues. |
| `R-VAL-01` | EXT-01/05, CSA-D01, IEEE-D01 | Opciones de comparator y autoridad normativa | Releases/corpus no están fijados/ejecutados. |

## 10. Vacíos de evidencia pendientes

1. Texto autorizado Core R22 exacto, ZCL7 y revisión IEEE correlacionada, incluyendo status, flags, strings y MIC lengths.
2. Fuente oficial exacta que sustente perfil de aplicación candidato `0x0104`; permanece deshabilitado hasta entonces.
3. Corpus legal y versionado con provenances para Zigbee known captures, no-Zigbee, secured, malformed, manufacturer-specific y fragmented.
4. Confirmación byte-exacta FCS/PHR/length y metadata para firmware/artifact identificado; timestamp wrap/continuidad y pérdida bajo carga.
5. Versión y config fijada de dissector independiente y comparación ejecutada.
6. Revisión legal file-by-file si se reutiliza código TI; esta propuesta sólo recomienda implementación independiente.
7. Throughput/buffer backlog host bajo captura sostenida antes de estado pasivo.
8. Completion RF correlacionado no se expone; cualquier futuro claim terminal requiere interfaz o instrumento independiente claramente definido.

Estos vacíos no cambian el sentido arquitectónico host-side. Sí bloquean detalles semánticos, assertions de validación y cualquier claim de Zigbee interoperability.
