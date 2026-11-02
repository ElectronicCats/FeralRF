# Registro de fuentes — convergencia de APIs FeralRF + TI Z-Stack

**Fecha de acceso externo:** 2026-11-02

**Repositorio:** `bfe056cc323911b6fc4c6f224d1f4f626ee15942`

**SDK fijado:** `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`

**Uso:** registro trazable para [ZSTACK_FERALRF_API_CONVERGENCE_PROPOSAL.md](ZSTACK_FERALRF_API_CONVERGENCE_PROPOSAL.md). Los rangos son los del checkout examinado. No se copiaron documentos completos ni se ejecutó firmware.

## 1. Criterios de fuente

1. Código FeralRF actual para afirmar implementación.
2. Submódulo fijado y documentación TI de la misma versión para arquitectura Z-Stack.
3. CSA para alcance normativo Zigbee/ZCL/BDB; su disponibilidad no demuestra conformance.
4. Reportes históricos para observación experimental, conservando límites.
5. Terceros sólo para alternativas de decoder/interop, nunca para compatibilidad del runtime TI.

## 2. Baseline Git y reconciliación

| ID | Fuente/comprobación | Resultado | Uso |
|---|---|---|---|
| BASE-01 | `git rev-parse HEAD`, 2026-11-02 | `bfe056cc323911b6fc4c6f224d1f4f626ee15942` | Línea base actual. |
| BASE-02 | `git status --short --branch` antes de crear estos documentos | Rama `feature/hardware-validation`; modificación preexistente en `docs/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md` | Preservación del trabajo local. |
| BASE-03 | `git submodule status --recursive` | SDK `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`lpf2-8.30.01.01`) | Identidad del SDK. |
| BASE-04 | `git diff --stat 8a4a8d8..HEAD -- firmware python hardware docs` | Sólo se añadió la auditoría del 30 de octubre; implementación sin cambios | Aplicabilidad de auditorías previas. |
| BASE-05 | `git diff -- docs/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md` | Correcciones A–F presentes en working tree, no confirmadas | Reconciliación documental. |

## 3. Auditorías primarias

| ID | Ruta | Secciones relevantes | Uso y cautela |
|---|---|---|---|
| AUD-01 | [`FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md`](FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md) | Arquitectura; IEEE; Zigbee; frame ownership; historial; fuentes | Baseline detallado de FeralRF. Fue ejecutado sobre `3409955`; código permanece igual. |
| AUD-02 | [`ZSTACK_INTEGRATION_FEASIBILITY_AUDIT_2026-10-27.md`](ZSTACK_INTEGRATION_FEASIBILITY_AUDIT_2026-10-27.md) | §§2–11, 13, 16–18 | Fronteras runtime/toolchain/RF y alternativas. No es prueba de build. |
| AUD-03 | [`ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md`](ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md) | §§2–11 y `Audit Corrections and Evidence Reconciliation` | Reuso por función, contrato `Packet`, decoder y correcciones. Archivo local modificado; incertidumbres preservadas. |

## 4. Código FeralRF actual

### 4.1 Python y transporte

| ID | Ruta, símbolo y líneas | Evidencia obtenida |
|---|---|---|
| FW-01 | [`python/feralrf/radio.py`](../python/feralrf/radio.py): `Packet` 18–30; `Radio.set_phy()` 455–466; `set_channel()` 468–478; `start_rx()` 599–607; `stop_rx()` 609–634; `read_packets()` 636–709; `transmit()` 725–735; `transmit_frame()` 737–749 | Contrato público, metadatos, RX async y comandos raw. |
| FW-02 | [`python/feralrf/enums.py`](../python/feralrf/enums.py): `PHY` 8–18; `Command`/`Response` | Identidad `IEEE_802_15_4=4` y IDs wire. |
| FW-12 | [`python/feralrf/protocol.py`](../python/feralrf/protocol.py): 11–90; [`firmware/cc1352/src/protocol.c`](../firmware/cc1352/src/protocol.c): `protocol_build_frame()` 115–143, `protocol_parse_frame()` 145–180 | COBS + frame `CMD|SEQ|LEN|PAYLOAD|CRC16`; distinto de NPI/MT. |
| HOST-01 | [`python/examples/killerbee_sniff.py`](../python/examples/killerbee_sniff.py): 122–137; [`python/feralrf/integrations/killerbee.py`](../python/feralrf/integrations/killerbee.py): 81–109 | PCAP DLT 195 y adaptación FCS/metadata existente; no validación end-to-end. |

### 4.2 Firmware, tareas y RF

| ID | Ruta, símbolo y líneas | Evidencia obtenida |
|---|---|---|
| FW-03 | [`firmware/cc1352/src/command_processor.c`](../firmware/cc1352/src/command_processor.c): `handle_command()` 101–270; `CMD_RX_START` 188–199, `CMD_RX_STOP` 201–208, `CMD_TX_RAW` 210–223, `CMD_TX_FRAME` 245–258 | Validación y ACK de comandos; RX/TX diferidos. |
| FW-04 | [`firmware/cc1352/src/control_task.c`](../firmware/cc1352/src/control_task.c): init 194–223; set PHY 225–237; `ControlTask_onTxRaw()` 249–259; `ControlTask_onTxFrame()` 262–264; RX 440–454 | Estado de control, buffers/eventos y alias FRAME→RAW. |
| FW-05 | [`firmware/cc1352/src/data_task.c`](../firmware/cc1352/src/data_task.c): `DataTask_emitRxPacket()` 28–65; `DataTask_poll()` 76–139 | Layout de respuesta y orden command/control/RF/LL/emisión. |
| FW-06 | [`firmware/cc1352/src/phy_manager.c`](../firmware/cc1352/src/phy_manager.c): tabla 9–24; selección 38–75; [`include/phy_manager.h`](../firmware/cc1352/include/phy_manager.h): 14–28 | Mapeo PHY/backend/link layer. |
| FW-07 | [`firmware/cc1352/src/ll_manager.c`](../firmware/cc1352/src/ll_manager.c): `LLManager_processRxPacket()` 40–159 | BLE se clasifica; IEEE/otros se pasan sin decoder Zigbee. |
| FW-08 | [`firmware/cc1352/src/radio_if.c`](../firmware/cc1352/src/radio_if.c): constantes/colas 32–75; `RadioIF_startIeee154RfBackend()` 1335–1371; `RadioIF_stopRfBackend()` 1373–1395 | Configuración RX, colas, metadatos y propiedad RF. |
| FW-09 | [`firmware/cc1352/src/radio_if.c`](../firmware/cc1352/src/radio_if.c): `RadioIF_processIeee154Packets()` 1497–1568; `RadioIF_poll()` 2354–2389; `RadioIF_popRxPacket()` 2392–2400 | PHR, FCS, CRC filtering, metadata y entrega. |
| FW-10 | [`firmware/cc1352/src/radio_if.c`](../firmware/cc1352/src/radio_if.c): `RadioIF_transmitIeee154Raw()` 468–488; `RadioIF_transmitRaw()` 2182–2204 | MPDU TX máximo 125; dispatch por PHY. |
| FW-11 | [`firmware/cc1352/src/smartrf_ieee_15_4_0.c`](../firmware/cc1352/src/smartrf_ieee_15_4_0.c): RX 81–144; TX 146–163 | Sin filtering/auto-ACK; RF Core genera PHR/FCS en TX según configuración. |
| FW-13 | [`firmware/cc1352/src/main_rtos.c`](../firmware/cc1352/src/main_rtos.c): tasks/stacks 54–77, 110–178, 220–232 | Modelo de tareas FeralRF y stacks estáticos. |
| FW-14 | [`firmware/cc1352/CMakeLists.txt`](../firmware/cc1352/CMakeLists.txt): target 3–18, sources/includes/libs 81–184, definitions 225–253 | CC1352P7/GNU; ausencia de Z-Stack/TIMAC/OSAL config en target. |
| FW-15 | [`firmware/cc1352/linker/cc1352p7.ld`](../firmware/cc1352/linker/cc1352p7.ld): heap 33–34; memory 43–47; `.priheap` 265–271; [`syscfg/ti_sysbios_config.c`](../firmware/cc1352/syscfg/ti_sysbios_config.c): 83–110 | Regiones y heap activo; no mide memoria libre. |

### 4.3 Hardware y configuración generada

| ID | Ruta y líneas | Evidencia obtenida |
|---|---|---|
| HW-01 | [`firmware/cc1352/syscfg/ti_drivers_config.c`](../firmware/cc1352/syscfg/ti_drivers_config.c): board 1–8; RF global callback 119–160; antenna switching 202–303; [`ti_drivers_config.h`](../firmware/cc1352/syscfg/ti_drivers_config.h): 21, 44–54 | Configuración LP_CC1352P7_4 y DIO28–30 para front-end. |
| HW-02 | [`hardware/PINOUT.md`](../hardware/PINOUT.md): 3–17, 31–73; [`README.md`](../README.md): 62–65 | UART/RP2040, reset y RF pins documentados. La fuente RP2040 no está en este repo. |

## 5. SDK TI fijado

Todas las rutas de esta sección son relativas a `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/`.

### 5.1 Codecs y capas Zigbee

| ID | Ruta, símbolo y líneas | Evidencia obtenida |
|---|---|---|
| SDK-01 | `source/ti/zstack/stack/zcl/zcl.c`: licencia 1–38; includes 40–55 | `zcl.c` BSD-3-Clause y dependencias de translation unit. |
| SDK-02 | `source/ti/zstack/stack/zcl/zcl.c`: `zclParseHdr()` 2412–2446; `zclBuildHdr()` 2459–2490; `zclSerializeData()` 2961–3117; `zclGetDataTypeLength()` 3195–3279; `zclGetAttrDataLength()` 3296–3322; `zcl_ProcessMessageMSG()` alrededor de 2140–2230 | Helpers de bytes y falta de bounds; acoplamiento del procesamiento completo. |
| SDK-03 | `source/ti/zstack/stack/zdo/zd_object.c`: `ZDO_Parse*` 2914–3764; `zd_object.h` 399–602 | Field order, allocation/ownership y dependencia de `zdoIncomingMsg_t`. |
| SDK-06 | `source/ti/zstack/stack/nwk/nwk_util.h`: `NLDE_ParseMsg()` 321; `source/ti/zstack/stack/nwk/aps_util.h`: `APSDE_ParseMsg()` 112 | Declaraciones sin definición visible en fuente. |
| SDK-07 | `source/ti/zstack/stack/api/zstacktask.c`: `ZStackTaskProcessEvent()` 724–820 y handlers posteriores; `zstackapi.c/h` | API message/task-based, AF/ZDO indications y OSAL messages. |
| SDK-12 | `source/ti/zstack/stack/nwk/aps.h`: licencia 9–46; manifest oficial 8.30.01.01 | Componentes con licencia TI-device-only además de archivos BSD. |

### 5.2 Build, startup, estado y librerías

| ID | Ruta, símbolo y líneas | Evidencia obtenida |
|---|---|---|
| SDK-04 | `source/ti/zstack/.meta/zstack.syscfg.js`: library selection 150–226; directorios `source/ti/zstack/lib/{ticlang,iar}/m4f/`; `source/ti/ti154stack/lib/...` | TIMAC y Z-Stack por device/role/toolchain; ZNP usa `nwk_all`. No GCC demostrado. |
| SDK-05 | `source/ti/zstack/zmac/f8w/zmac.c`, `zmac_cb.c`; `source/ti/zstack/stack/nwk/nwk.c` interfaces | Frontera ZMAC callbacks y acoplamiento MAC→NWK; core TIMAC binario. |
| SDK-08 | `source/ti/zstack/startup/main.c`: CCFG address 319–339; NVOCMP 341–349; stack init 351–358; NPI/app task 360–389 | Identidad, NV, stack task, NPI y rol. |
| SDK-11 | `source/ti/zstack/rtos/app_release.cfg`: heap 669–758; `source/ti/ti154stack/common/osal_port/...` | Heap OSAL 0x1800 del config Z-Stack y servicios OSAL Port; inactivos en FeralRF. |

### 5.3 NPI/MT/ZNP

| ID | Ruta, símbolo y líneas | Evidencia obtenida |
|---|---|---|
| SDK-09 | `source/ti/zstack/npi/npi_frame.h`: callback/framing 69–94; `npi_task.c`, `npi_tl_uart.c` | NPI task y framing separado de COBS FeralRF. |
| SDK-10 | `source/ti/zstack/mt/mt_rpc.h`: frame 83–126, command types 139–148; `mt.h`: subsystems/commands; `mt_znp.c` | MT usa length/cmd0/cmd1/data y SREQ/SRSP/AREQ, no contrato FeralRF. |

### 5.4 RF y DMM

| ID | Ruta, símbolo y líneas | Evidencia obtenida |
|---|---|---|
| SDK-13 | `source/ti/drivers/rf/RFCC26X2_multiMode.c`: `N_MAX_CLIENTS=2` 140; client arrays 327–328; `RF_open()` 4599–4712; `source/ti/dmm/apps/common/zstack/source/main.c`: DMM init/register alrededor de 550–580 | Dos clientes RF y arquitectura DMM explícita. El límite no prueba coexistencia. |
| SDK-14 | `source/ti/dmm/apps/dmm_zed_switch_remote_display`, `dmm_zc_switch_remote_display`, `dmm_zr_light_remote_display`; `source/ti/dmm/policy` y scheduler | Ejemplos de combinaciones TI soportadas; no contienen un cliente FeralRF. |

## 6. Documentación oficial externa

| ID | Documento, versión y URL oficial | Fecha/publicación | Conclusión soportada |
|---|---|---|---|
| DOC-TI-01 | [CC1352P7 product page](https://www.ti.com/product/CC1352P7) | Acceso 2026-11-02 | 704 KiB flash, 152 KiB RAM agregada, Zigbee/IEEE y DMM como capacidades de plataforma. No prueba fit de imagen. |
| DOC-TI-02 | [CC1352P7 datasheet, Rev. A](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf) | Nov. 2021; acceso 2026-11-02 | Arquitectura de MCU, memoria, RF y multiprotocolo. |
| DOC-TI-03 | [Z-Stack 8.30.00 Hardware Architecture](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/cc13xx_cc26xx/hardware-architecture.html) | © 2005–2024; acceso 2026-11-02 | System core ejecuta link layer→app; RF Driver gestiona radio; CM0 implementa PHY; 704/144+8 KiB. |
| DOC-TI-04 | [Zigbee Configuration, Z-Stack 8.30.00](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/sysconfig/zigbee.html) | © 2005–2024; acceso 2026-11-02 | Roles predeterminados por proyecto, configuración generada, channels, keys, BDB y endpoints. |
| DOC-TI-05 | [MT Interface, Z-Stack 8.30.00](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/zigbee/mt_interface.html) | Acceso 2026-11-02 | MT_AF/ZDO/SYS/UTIL/APP, NPI task y UART 115200 default. |
| DOC-TI-06 | [SimpleLink SDK 8.30.01.01 release notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | SDK 8.30.01.01; acceso 2026-11-02 | Paquete contiene Zigbee 3.0 suite, TI-RTOS y DMM; no implica integración FeralRF. |
| DOC-TI-07 | [DMM User's Guide — Dynamic Multiprotocol Manager](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/7.40.00.77/exports/docs/dmm/dmm_user_guide/html/dmm-guide/dmm-index-cc13xx_cc26xx.html) y [8.30 DMM quick start](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/simplelink_mcu_sdk/html/quickstart-guide/dmm-quick-start.html) | Guide 5.40 / SDK docs; acceso 2026-11-02 | DMM arbitra RF y TI sólo caracteriza subconjuntos; requiere políticas, estados y scheduling. La versión del guide no coincide nominalmente con SDK, por lo que el código local tiene prioridad. |
| DOC-TI-08 | [SDK 8.30.01.01 manifest](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | Acceso 2026-11-02 | Mezcla de software binary/limited source y BSD; revisar licencias por archivo. |
| DOC-TI-09 | [Custom Hardware — DMM guide](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.00.121/exports/docs/dmm/dmm_user_guide/html/cc13xx_cc26xx/custom-hardware-cc13xx_cc26xx.html) | DMM guide 6.30; acceso 2026-11-02 | RF front-end/board SysConfig debe coincidir; configuración incorrecta degrada RF. |

## 7. Especificaciones y documentación CSA

| ID | Fuente oficial | Versión/fecha | Uso y límite |
|---|---|---|---|
| CSA-01 | [Alliance Specification Downloads](https://csa-iot.org/developer-resource/specifications-download-request/) | Catálogo accedido 2026-11-02; ofrece Zigbee Core R22.1/R23.1/R23.2, ZCL R7/R8 y BDB 3.0.1/3.1 | Define las familias normativas que deben fijarse antes de un decoder. No se descargó/copió una especificación completa. |
| CSA-02 | [Zigbee standard overview](https://csa-iot.org/all-solutions/zigbee/) | Acceso 2026-11-02 | Zigbee como solución full-stack y con validación de múltiples capas; apoya no equiparar PHY raw con Zigbee. |
| CSA-03 | [ZUTH certification tool](https://csa-iot.org/certification/tools/zuth/) | Acceso 2026-11-02 | La certificación usa herramientas/casos formales; ninguna ejecución ZUTH existe en FeralRF. |
| CSA-04 | [ZigBee Cluster Library Specification, doc. 075123 rev. 6](https://csa-iot.org/wp-content/uploads/2019/12/07-5123-06-zigbee-cluster-library-specification.pdf) | 2016 | Referencia histórica pública de ZCL. No debe sustituir la revisión objetivo actual aún no seleccionada. |

## 8. Evidencia experimental histórica

| ID | Fuente | Observación retenida | Limitación que debe acompañarla |
|---|---|---|---|
| EXP-01 | [`docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/SUMMARY.md`](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/SUMMARY.md): 16–32, 47–53 | IEEE RAW bidireccional, FRAME en una dirección y repetición interval=0 bajo setup probado | Dos CatSniffer, canal 25; no range/PER/conformance/Zigbee; otras operaciones parciales. |
| EXP-02 | [`experiments/01_ieee_raw_frame_burst.md`](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/01_ieee_raw_frame_burst.md) y [`historical_command_audit.md`](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/historical_command_audit.md): 18–21, 51 | Conteos 20/20 y 10/10, y `TX_FRAME` comparte backend raw | Comandos literales no siempre preservados; ACK no prueba completion. |
| EXP-03 | [`docs/HARDWARE_VALIDATION_RESULTS/EXECUTIVE_SUMMARY.md`](HARDWARE_VALIDATION_RESULTS/EXECUTIVE_SUMMARY.md): tabla de capacidades; [`VALIDATION_MATRIX_CURRENT.md`](HARDWARE_VALIDATION_RESULTS/VALIDATION_MATRIX_CURRENT.md): VM-PROT-05 | RX/TX IEEE presente; Zigbee/Thread ausentes como stacks | Campañas posteriores encontraron stop/cleanup y packaging gaps; RF-04 no llegó a hardware. |
| EXP-04 | [`docs/HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md`](HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md) | Índice de artefactos, runs y clasificación | Debe consultarse antes de elevar cualquier claim experimental. |

## 9. Fuentes de terceros — sólo comparación

Estas fuentes no acreditan compatibilidad del runtime TI ni fueron instaladas durante esta investigación.

| ID | Fuente/revisión | Uso permitido en la propuesta | Estado |
|---|---|---|---|
| EXT-01 | [Scapy v2.8.0, commit `e2e35c0`](https://github.com/secdev/scapy/tree/e2e35c07bce2f40e7513e1b29a6d300d5329dbc3), `dot15d4.py`/`zigbee.py` | Comparar dissectors, FCS y cobertura | Fijado por auditoría; no probado con FeralRF. |
| EXT-02 | [zigpy 2.4.1, commit `10b6837`](https://github.com/zigpy/zigpy/tree/10b68377bcbd9a8b15cbbfb506cebdc450704614) | Comparar schemas ZDO/ZCL y modelo controller | No es entrada raw directa demostrada. |
| EXT-03 | [WHAD 1.2.18, commit `9d48016`](https://github.com/whad-team/whad-client/tree/9d48016375cd2fdae58517fe6f2fb97022c3bb61) | Comparar connector/sniffer | No existe connector FeralRF demostrado. |
| EXT-04 | [KillerBee upstream](https://github.com/riverloopsec/killerbee) | Comparar integración existente y APIs legacy | Snapshot exacto no fijado; packaging bloqueó validación histórica. |
| EXT-05 | [Wireshark IEEE 802.15.4/Zigbee display references](https://www.wireshark.org/docs/dfref/z/zbee_nwk.html) | Decoder independiente y campos esperados | Documentación live no fijada; usar versión concreta al validar. |

## 10. Mapa conclusión → evidencia

| Conclusión | Evidencia principal | Naturaleza |
|---|---|---|
| `Packet.data` IEEE contiene MPDU+FCS, no PHR | FW-08, FW-09, FW-11, AUD-03 | Código actual |
| TX recibe MPDU sin FCS y RF Core genera framing | FW-10, FW-11 | Código actual/SDK command semantics |
| No hay decoder Zigbee ni Z-Stack en target | FW-07, FW-14 | Código/build actual |
| RAW OTA funciona dentro de alcance limitado | EXP-01, EXP-02 | Histórica, no Zigbee |
| NWK/APS/TIMAC no son parsers source-reusable | SDK-04, SDK-05, SDK-06 | SDK fijado |
| Helpers ZCL/ZDO requieren hardening/adaptación | SDK-01, SDK-02, SDK-03 | SDK fijado |
| Z-Stack operacional exige task/OSAL/NV/config | SDK-07, SDK-08, SDK-11, DOC-TI-04 | SDK/documentación oficial |
| COBS y NPI/MT no son wire-compatible | FW-12, SDK-09, SDK-10, DOC-TI-05 | Dos contratos confirmados |
| ZNP se alinea mejor con host API | SDK-08–10, DOC-TI-05 | Inferencia técnica condicionada |
| DMM no es automático | SDK-13, SDK-14, DOC-TI-07 | SDK/documentación oficial |
| Board/antena deben portarse | HW-01, DOC-TI-09 | Código actual/documentación oficial |
| Imagen única no está demostrada | FW-14, SDK-04, SDK-11, SDK-13 | Ausencia + dependencias confirmadas |

## 11. Vacíos de evidencia no resueltos

- revisión normativa exacta y features Zigbee requeridos;
- corpus de capturas legal, versionado y representativo;
- timestamp RF exacto, rollover y pérdida de frames bajo carga;
- definiciones visibles pero implementaciones ocultas de NWK/APS/TIMAC;
- ABI/link de archivos Z-Stack con el GNU target actual;
- proyecto ZNP/ZED CC1352P7 completo y mapa de recursos;
- board file CatSniffer para una persona Z-Stack;
- compatibilidad del RP2040 bridge con NPI/MT y reset;
- NV layout, persistence y security lifecycle;
- decoder/decryption implementado y validado;
- tool versions externos realmente desplegados;
- conclusión legal sobre redistribución/combinación.

Estos vacíos no invalidan el contrato raw ni la recomendación de separar personas; sí impiden afirmar que ZNP/ZED/DMM ya funciona o que existe interoperabilidad Zigbee.
