# Registro de evidencia — propuesta de integración Zigbee nativa en FeralRF

**Revisión:** 1.0  
**Fecha:** 2026-11-03  
**Repositorio:** `fc0de99b61cdb3d37701cdc625980b8882ed92b2`  
**SDK:** `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`  
**Documento asociado:** [FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md](FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md)

## 1. Propósito y reglas

Este registro permite auditar independientemente las afirmaciones de la propuesta. Los rangos corresponden al checkout indicado. Las rutas son relativas a la raíz `FeralRF/`, salvo que se indique expresamente `SDKROOT`, que significa `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/`.

Estados de evidencia usados:

- **CURRENT IMPLEMENTATION:** demostrado por código FeralRF actual.
- **SDK-CONFIRMED MECHANISM:** mecanismo o definición visible en el SDK fijado; no demuestra integración.
- **HISTORICAL EXPERIMENTAL EVIDENCE:** observación previa limitada a su artefacto y condiciones.
- **ARCHITECTURAL INFERENCE:** consecuencia razonada de fuentes verificadas.
- **CONDITIONALLY FEASIBLE:** requiere desarrollo o validación identificados.
- **UNRESOLVED:** la evidencia no resuelve la pregunta.
- **UNSUPPORTED:** no debe afirmarse como implementado, validado o disponible.

No se ejecutó firmware, hardware, build ni paquete externo para producir este registro.

## 2. Baseline Git y SDK

| ID | Comprobación | Resultado | Uso |
|---|---|---|---|
| BASE-01 | `git rev-parse HEAD`, 2026-11-03 | `fc0de99b61cdb3d37701cdc625980b8882ed92b2` | Baseline documental actual. |
| BASE-02 | `git branch --show-current` | `feature/hardware-validation` | Rama examinada. |
| BASE-03 | `git status --short --branch`, antes de crear los entregables | Árbol limpio | No había cambios locales que reconciliar. |
| BASE-04 | `git submodule status --recursive` | SDK `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`lpf2-8.30.01.01`) | Único SDK usado. |
| BASE-05 | `SDKROOT/.metadata/product.json:4` | `8.30.01.01` | Versión del paquete. |
| BASE-06 | `git diff 31f9535..HEAD -- firmware python hardware` | Sin diferencias | `fc0de99` sólo incorpora la evaluación documental; implementación sin cambios. |
| BASE-07 | `firmware/cc1352/CMakeLists.txt:12-17,35-43,123-124` | CC1352P7; GNU Arm `arm-none-eabi-gcc/g++`; libs driver GCC | Toolchain configurado. |
| BASE-08 | Resolución de ejecutable en shell | `arm-none-eabi-gcc` no está en `PATH` | Versión efectiva no verificada; no es un resultado de build. |

## 3. Informes previos reconciliados

| ID | Ruta y secciones | Uso | Cautela |
|---|---|---|---|
| AUD-01 | `docs/FERALRF_NATIVE_ZIGBEE_CAPABILITY_ASSESSMENT.md:1-421` | Baseline nativo, clasificación A–E, límites RF/MAC y discrepancia `nwk.c` | Es evaluación, no implementación. |
| AUD-02 | `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_PROPOSAL.md:1-535` | Objetivos de convergencia, alternativas y límites de runtime | Su solución multi-persona no se selecciona en esta propuesta. |
| AUD-03 | `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md:1-186` | Registro previo de código, SDK, documentos y experimentos | `:92` cita erróneamente un `stack/nwk/nwk.c` inexistente. |
| AUD-04 | `docs/Z-Stack_Reports/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md:1-603` | Reuso por función, alternativas host, licencias y correcciones A–F | Herramientas externas no fueron instaladas/probadas. |
| AUD-05 | `docs/Z-Stack_Reports/ZSTACK_INTEGRATION_FEASIBILITY_AUDIT_2026-10-27.md:1-938` | Fronteras Z-Stack/TIMAC/toolchain/RF/RTOS | No es una prueba de compilación ni compatibilidad ABI. |
| AUD-06 | `docs/FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md:1-1084` | Arquitectura completa, contratos RF y limitaciones de evidencia | Incluye otros PHY; aquí sólo se usa lo necesario para preservar regresión. |

Las correcciones A–F de AUD-04 están incorporadas en `HEAD`. Permanecen abiertas revisión normativa, corpus, timestamp, memoria libre, ABI, CCM* y versiones de herramientas.

## 4. Código FeralRF actual

### 4.1 Host, API y transporte

| ID | Ruta, símbolo y líneas | Evidencia |
|---|---|---|
| FW-H01 | `python/feralrf/radio.py:18-30`, `Packet` | Modelo RX: timestamp, canal, RSSI, LQI, CRC, bytes y metadata LL opcional. |
| FW-H02 | `python/feralrf/radio.py:455-478`, `Radio.set_phy()`, `set_channel()` | Selección PHY/canal mediante comandos host. |
| FW-H03 | `python/feralrf/radio.py:599-709`, `start_rx()`, `stop_rx()`, `read_packets()` | Control RX y entrega asíncrona de `Packet`/errores. |
| FW-H04 | `python/feralrf/radio.py:725-750`, `transmit()`, `transmit_frame()` | TX host existente; respuesta ACK/ERROR. |
| FW-H05 | `python/feralrf/radio.py:943-1004`, AES-CCM | Primitiva crypto host→firmware; nonce 7–13, tag par 4–16. |
| FW-H06 | `python/feralrf/protocol.py:11-90` | Framing/COBS y CRC16 de transporte. |
| FW-H07 | `python/feralrf/enums.py:8-18,21-89` | `PHY.IEEE_802_15_4=4`, comandos y respuestas. |
| FW-H08 | `python/feralrf/integrations/killerbee.py:54-62,81-120` | Adapter de captura/inyección; no decoder Zigbee completo. |
| FW-H09 | `python/examples/killerbee_sniff.py:122-137` | Escritura PCAP DLT 195 con `Packet.data`; usa timestamp host y pierde parte de metadata RF. |

### 4.2 Firmware y recorrido de datos

| ID | Ruta, símbolo y líneas | Evidencia |
|---|---|---|
| FW-F01 | `firmware/cc1352/src/protocol.c:115-180` | Frame con campos `CMD`, `SEQ`, `LEN`, `PAYLOAD` y `CRC16`; distinto de Zigbee y NPI/MT. |
| FW-F02 | `firmware/cc1352/src/command_processor.c:101-270` | Validación/despacho; `SET_PHY`, `SET_CHANNEL`, RX y TX. |
| FW-F03 | `firmware/cc1352/src/command_processor.c:188-258` | RX/TX reciben ACK tras aceptación/encolado, no tras terminación RF. |
| FW-F04 | `firmware/cc1352/src/control_task.c:194-264,332-358,440-454` | Estado, buffers y eventos; `TX_FRAME` delega a raw; fallo TX posterior es error asíncrono. |
| FW-F05 | `firmware/cc1352/src/data_task.c:28-65,76-139` | Layout RX y orden command/control/RF/LL/emisión. |
| FW-F06 | `firmware/cc1352/src/phy_manager.c:9-75` | IEEE selecciona RF backend y link layer por defecto. |
| FW-F07 | `firmware/cc1352/src/ll_manager.c:40-159` | Sólo BLE se clasifica; IEEE pasa sin decoder MAC/Zigbee. |
| FW-F08 | `firmware/cc1352/include/radio_if.h:15-28,55-68` | `RadioIF_RxPacket`, buffer 242 y API raw/RX. |
| FW-F09 | `firmware/cc1352/src/radio_if.c:32-75,108-193` | Cola app 8, cola RF 3, TX IEEE 125 y buffers/ownership estáticos. |
| FW-F10 | `firmware/cc1352/src/radio_if.c:324-381` | `RF_runCmd()` y handle persistente para TX; status interno no correlacionado al ACK host. |
| FW-F11 | `firmware/cc1352/src/radio_if.c:457-488` | Canal IEEE y TX: copia MPDU, `TRIG_NOW`, `CMD_IEEE_TX`. |
| FW-F12 | `firmware/cc1352/src/radio_if.c:1080-1144,1335-1397` | RX `RF_postCmd`, configuración PHR/FCS/metadata, stop y ownership. |
| FW-F13 | `firmware/cc1352/src/radio_if.c:1497-1568` | PHR removido; MPDU+FCS copiado; CRC malo descartado; RAT/4, RSSI, correlación. |
| FW-F14 | `firmware/cc1352/src/radio_if.c:2182-2204,2297-2400` | TX rechazado con RX activo; start/stop/poll/pop. |
| FW-F15 | `firmware/cc1352/src/smartrf_ieee_15_4_0.c:81-163` | Filtering/auto-ACK/CCA apagados; TX sin PHR/FCS aportados por caller. |
| FW-F16 | `firmware/cc1352/src/main_rtos.c:54-77,110-178,220-232` | Tareas UART/RF prioridad 3, stacks 4096, cooperación. |
| FW-F17 | `firmware/cc1352/src/crypto_engine.c:197-246`; `command_processor.c:357-414` | AES-CCM genérico one-shot y rechazo de MAC/tag inválido. |
| FW-F18 | `firmware/cc1352/CMakeLists.txt:81-184,225-253` | Target no incluye Z-Stack/TIMAC/AF/BDB/OSAL de rol. |

### 4.3 Hardware, memoria y frontend

| ID | Ruta y líneas | Evidencia |
|---|---|---|
| HW-01 | `firmware/cc1352/syscfg/ti_drivers_config.c:119-160,202-303`; `ti_drivers_config.h:21,44-54` | Callback RF y selección de frontend/antena mediante DIO. |
| HW-02 | `firmware/cc1352/syscfg/ti_radio_config.c:5-22`; `ti_radio_config_433.c:5-18` | Configuración generada CC1352P7 y diseños RF seleccionados. |
| HW-03 | `firmware/cc1352/linker/cc1352p7.ld:33-47,265-271`; `syscfg/ti_sysbios_config.c:83-110` | Regiones y heap configurado; no miden margen libre ni high-water. |
| HW-04 | `hardware/PINOUT.md:3-17,31-73`; `README.md:62-65` | Conexión UART/RP2040 y pines RF documentados; firmware RP2040 no está en este repo. |

## 5. SDK TI fijado

### 5.1 Codecs y definiciones Zigbee

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-Z01 | `source/ti/zstack/stack/zcl/zcl.c:1-38` | Aviso BSD-3-Clause del archivo; revisión legal sigue siendo necesaria. |
| SDK-Z02 | `source/ti/zstack/stack/zcl/zcl.c:40-62` | Translation unit depende de config, AF, BDB, Green Power y ports. |
| SDK-Z03 | `source/ti/zstack/stack/zcl/zcl.c:2412-2446`, `zclParseHdr()` | Algoritmo de 3/5 bytes; no recibe longitud. |
| SDK-Z04 | `source/ti/zstack/stack/zcl/zcl.c:2459-2482`, `zclBuildHdr()` | Serializer pequeño `static`; no recibe capacidad. |
| SDK-Z05 | `source/ti/zstack/stack/zcl/zcl.c:2961-3076`, `zclSerializeData()` | Serializa escalares/strings; no recibe capacidad y asume layouts C. |
| SDK-Z06 | `source/ti/zstack/stack/zcl/zcl.c:3195-3322`, length helpers | Tabla de tamaños; strings leen prefijo sin bytes restantes. |
| SDK-Z07 | `source/ti/zstack/stack/zcl/zcl.h:1927-1994` | Declaraciones/tipos asociados; `zclBuildHdr()` no es pública. |
| SDK-Z08 | `source/ti/zstack/stack/zdo/zd_object.c:69-96` | Translation unit acoplada a NV, NWK, APS, AF, security, BDB y globals. |
| SDK-Z09 | `source/ti/zstack/stack/zdo/zd_object.c:2914-3733`; `source/ti/zstack/stack/zdo/zd_object.h:399-602` | `ZDO_Parse*`: orden de campos; varios usan `OsalPort_malloc` y carecen de bounds completos. |
| SDK-Z10 | `source/ti/zstack/stack/zdo/zd_object.c:3005-3057,3147-3194,3225-3284,3543-3618` | Addr, simple descriptor, endpoints, LQI y announce: ejemplos concretos de count/ownership. |
| SDK-Z11 | `source/ti/zstack/stack/nwk/nwk_util.h:111-201,286-321` | Máscaras/estructuras y declaración `NLDE_ParseMsg()`; definición no visible. |
| SDK-Z12 | `source/ti/zstack/stack/nwk/aps_mede.h:91-173,231-234`; `aps_util.h:86-117` | Definiciones APS y declaración `APSDE_ParseMsg()`; definición no visible. |
| SDK-Z13 | `source/ti/zstack/stack/nwk/aps.h:38-46` | Condición de licencia TI-device-only en este archivo; no conclusión legal global. |
| SDK-Z14 | `source/ti/zstack/stack/nwk/nwk.h`; `source/ti/zstack/stack/nwk/nwk_bufs.h`; `source/ti/zstack/stack/nwk/nwk_globals.c`; `source/ti/zstack/stack/nwk/nwk_globals.h`; `source/ti/zstack/stack/nwk/nwk_util.h` | Inventario NWK realmente presente. `nwk.c` no existe. |

### 5.2 MAC, runtime y build

| ID | Ruta SDKROOT, símbolo y líneas | Evidencia y límite |
|---|---|---|
| SDK-M01 | `source/ti/zstack/zmac/f8w/zmac.c:480-560` | `ZMacInit()` delega a `MAP_MAC_*` y estado TIMAC. |
| SDK-M02 | `source/ti/zstack/zmac/f8w/zmac.c:870-917,1160-1218` | Association/data/purge wrappers delegan a MLME/MCPS binario. |
| SDK-M03 | `source/ti/zstack/zmac/f8w/zmac_cb.c:165-240` | Callbacks, ownership y mensajes OSAL hacia NWK. |
| SDK-M04 | `source/ti/zstack/.meta/zstack.syscfg.js:150-226` | Selección binaria MAC/Z-Stack por device, role, ISA y toolchain. |
| SDK-M05 | `source/ti/zstack/lib/iar/`; `source/ti/zstack/lib/ticlang/` | No hay archivo de rol Z-Stack GCC demostrado. |
| SDK-M06 | `source/ti/zstack/startup/main.c:319-389` | CCFG, NVOCMP, stack task, NPI/app; runtime operativo acoplado. |
| SDK-M07 | `source/ti/zstack/stack/api/zstacktask.c:724-820`; `zstackapi.c/h` | API interna task/message, no codec wire independiente. |
| SDK-M08 | `source/ti/zstack/stack/sec/`; `stack/zdo/zd_sec_mgr.c`; `stack/nwk/nwk_globals.h:144-145,458-466` | Keys, counters, política y estado de seguridad. |
| SDK-RF01 | `source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:80-207` | RX, filtering, auto-ACK, CCA y direcciones como mecanismo RF Core. |
| SDK-RF02 | `source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:266-394` | TX temporal, `CMD_IEEE_CSMA`, `CMD_IEEE_RX_ACK`; no integración FeralRF. |
| SDK-RF03 | `source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:548-620` | CCA request, output RX y corr/CRC. |
| SDK-RF04 | `source/ti/drivers/rf/RFCC26X2_multiMode.c:140,4599-4712,4931,5287` | RF Driver admite clientes/comandos; no demuestra scheduling Zigbee+FeralRF. |

## 6. Documentación oficial

**Fecha de consulta web:** 2026-11-03.

| ID | Documento oficial | Versión/fecha | Conclusión soportada |
|---|---|---|---|
| TI-D01 | [Z-Stack 8.30.00 User’s Guide](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/zigbee-users-guide.html) | SDK 8.30.01.01 / Z-Stack 8.30.00 | Arquitectura, tareas, configuración y alcance del stack completo. |
| TI-D02 | [Hardware Architecture, SDK 8.30](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/cc13xx_cc26xx/hardware-architecture.html) | © 2005–2024 | CM0 implementa PHY; system core ejecuta link layer→app; RF Driver media el radio. |
| TI-D03 | [Zigbee Configuration, SDK 8.30](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/html/sysconfig/zigbee.html) | 8.30 | Roles, channels, BDB, keys, endpoints y configuración generada. |
| TI-D04 | [Z-Stack 8.30.00 release notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/release_notes_zigbee_8_30_00.html) | 8.30.00 | Declara Zigbee PRO 2017/R22 y ZCL 7; base provisional alineada con el SDK. |
| TI-D05 | [SimpleLink SDK 8.30.01.01 release notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/release_notes_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | 8.30.01.01 | Paquete/tooling y componentes distribuidos. |
| TI-D06 | [SDK 8.30.01.01 manifest](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | 8.30.01.01 | Licencias por componente; no sustituye revisión archivo por archivo. |
| TI-D07 | [CC1352P7 product page](https://www.ti.com/product/CC1352P7) y [datasheet Rev. A](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf) | Nov. 2021 / acceso 2026-11-03 | Recursos y capacidades del dispositivo; no prueba fit ni integración. |
| IEEE-D01 | [IEEE Std 802.15.4-2020](https://standards.ieee.org/ieee/1159/7029/) | Publicado 2020-07-23; hoy superseded por 802.15.4-2024 | Familia normativa MAC/PHY; la revisión exacta compatible con Zigbee R22 debe aprobarse/obtenerse. |
| CSA-D01 | [Alliance Specification Downloads](https://csa-iot.org/developer-resource/specifications-download-request/) | Acceso 2026-11-03 | Ofrece Zigbee Core R22.1/R23.1/R23.2, ZCL R7/R8 y BDB 3.0.1/3.1. |
| CSA-D02 | [Zigbee Specification doc. 05-3474-23](https://csa-iot.org/wp-content/uploads/2023/04/05-3474-23-csg-zigbee-specification-compressed.pdf) | R23 draft/public document | Demuestra disponibilidad de otra revisión; no se mezcla con R22 por defecto. |
| CSA-D03 | [ZCL Specification 075123 rev. 6](https://csa-iot.org/wp-content/uploads/2019/12/07-5123-06-zigbee-cluster-library-specification.pdf) | 2016 | Referencia histórica pública; no sustituye ZCL 7 objetivo. |

**Estrategia normativa provisional:** alinear la primera cobertura con lo declarado por TI-D04 —Zigbee PRO 2017/R22 y ZCL 7—, mantener schemas versionados y no afirmar compatibilidad R23/ZCL8. La especificación exacta y la base IEEE asociada requieren aprobación y acceso normativo antes de implementar.

## 7. Evidencia experimental histórica

| ID | Fuente | Observación preservada | Limitación |
|---|---|---|---|
| EXP-01 | `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/SUMMARY.md:16-37` | IEEE raw/FRAME observado en canal 25 bajo setup registrado | No Zigbee, rango, PER, conformance ni firmware actual identificado criptográficamente. |
| EXP-02 | `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/01_ieee_raw_frame_burst.md:13,358-443,891-906` | RAW 20/20 y 10/10 históricos; FRAME 10/10 unidireccional | `TX_FRAME` comparte raw; ACK no equivale por sí solo a RF. |
| EXP-03 | `docs/HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md:47-59` | Índice consolidado de IEEE RAW/FRAME | Conserva límites y provenance. |
| EXP-04 | `docs/HARDWARE_VALIDATION_RESULTS/VALIDATION_MATRIX_CURRENT.md:57-59,67,71-79` | Transporte, canal, RX/TX, completion y fallos actuales documentados | Algunos estados son parciales/bloqueados; no promover. |
| EXP-05 | `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/execution_report.md:77-107` | Repetición sincronizada 10/10 bilateral y declaración explícita “No Zigbee stack was validated” | Sólo canal 25, dos CatSniffer y bytes marcados. |

## 8. Herramientas externas — comparación, no autoridad TI

| ID | Fuente fijada | Uso admisible | Límite/licencia registrada |
|---|---|---|---|
| EXT-01 | [Scapy v2.8.0, commit `e2e35c07`](https://github.com/secdev/scapy/tree/e2e35c07bce2f40e7513e1b29a6d300d5329dbc3) | Oráculo comparativo para FCS/MAC/NWK/APS/ZDP/ZCL | GPL-2.0-only; cobertura parcial; no probado con FeralRF. |
| EXT-02 | [zigpy 2.4.1, commit `10b68377`](https://github.com/zigpy/zigpy/tree/10b68377bcbd9a8b15cbbfb506cebdc450704614) | Comparar schemas ZDO/ZCL | GPL-3.0; espera contexto de controller/capa superior, no raw MPDU. |
| EXT-03 | [WHAD v1.2.18, commit `9d480163`](https://github.com/whad-team/whad-client/tree/9d48016375cd2fdae58517fe6f2fb97022c3bb61) | Comparar abstracción de sniffer | MIT; no hay connector FeralRF. |
| EXT-04 | [KillerBee upstream](https://github.com/riverloopsec/killerbee) | Comparar flujo legacy y adapter existente | Snapshot exacto no fijado; packaging histórico bloqueado. |
| EXT-05 | [Wireshark Zigbee NWK fields](https://www.wireshark.org/docs/dfref/z/zbee_nwk.html) | Análisis PCAP externo e independiente | Página live no fijada; fijar release antes de validar. |

La arquitectura seleccionada no depende en runtime de estas herramientas. Se usan como comparadores, y la especificación aprobada resuelve divergencias.

## 9. Discrepancias y correcciones

| ID | Discrepancia | Verificación actual | Tratamiento |
|---|---|---|---|
| DISC-01 | AUD-03 `:92` cita `source/ti/zstack/stack/nwk/nwk.c` | No existe; inventario real en SDK-Z14 | No citarlo como implementación. Refuerza ausencia del core/parser NWK fuente. |
| DISC-02 | Informes anteriores describían la evaluación nativa como untracked | `fc0de99` ahora la incluye y árbol inicial está limpio | Baseline documental actualizado; implementación no cambió. |
| DISC-03 | “LQI” podría interpretarse como métrica Zigbee normalizada | FW-F13 expone siete bits de corr/CRC como `lqi` | Describir como correlación/LQI reportada; calibración no validada. |
| DISC-04 | ACK host puede leerse como terminación TX | FW-F03/F04 muestran aceptación antes de RF | Separar accepted/completed/observed/acknowledged/responded. |
| DISC-05 | Catálogo CSA actual ofrece R23/ZCL8, SDK declara R22/ZCL7 | CSA-D01 frente a TI-D04 | No combinar; schema profile versionado y aprobación normativa. |

## 10. Mapa evidencia → requisitos

Los identificadores corresponden a la propuesta principal.

| Requisito | Evidencia principal | Estado soportado |
|---|---|---|
| `R-BASE-01` contrato raw | FW-H01–H04, FW-F05, FW-F09, FW-F12–F15, EXP-01–05 | Implementación fuente; observación histórica acotada; confirmación actual pendiente. |
| `R-MAC-01` parser MAC bounded | R-BASE-01, IEEE-D01, AUD-01 | Propuesto/no implementado; revisión exacta pendiente. |
| `R-NWK-01` parser NWK | SDK-Z11/Z14, TI-D04, CSA-D01 | Propuesto; implementación TI no visible. |
| `R-APS-01` parser APS | SDK-Z12/Z13, TI-D04, CSA-D01 | Propuesto; implementación TI no visible. |
| `R-ZDO-01` subset ZDO | SDK-Z08–Z10, TI-D04 | Field order disponible; parser bounded propuesto. |
| `R-ZCL-01` header ZCL | SDK-Z02–Z04/Z07 | Algoritmo adaptable, sin bounds en original. |
| `R-ZCL-02` foundation/types | SDK-Z05–Z07, TI-D04 | Algoritmos/tipos adaptables; cobertura seleccionada pendiente de schema aprobado. |
| `R-SEC-01` frontera de seguridad | SDK-Z12, SDK-M08, FW-H05/F17 | Header estructural propuesto; no decryption baseline. |
| `R-RAW-01` preservación | FW-H01, FW-F13, AUD-01 | Contrato existente; wrapper/resultado propuesto. |
| `R-ERR-01` errores tipados | AUD-01; riesgos SDK-Z03–Z10 | Propuesto/no implementado. |
| `R-VAL-01` comparación independiente | EXT-01/05, FW-H09 | Flujo posible; no ejecutado. |
| `C-OBS-01` estado pasivo | SDK-Z10, R-NWK/R-APS/R-ZDO | Condicional; requiere gaps/provenance. |
| `C-DEC-01` decryption | FW-H05/F17, SDK-M08 | Condicional; CCM*/nonce/AAD/keys/vectores pendientes. |
| `C-BLD-01` builders/raw TX | FW-H04, FW-F11/F14/F15, SDK-Z04–Z06, EXP-01/02/05 | TX raw existe; builder no; entrega Zigbee no soportada. |
| `C-RF-01` primitiva temporal | SDK-RF01–RF03, FW-F10–F16 | Mecanismo SDK; necesidad, integración y temporización no resueltas. |

## 11. Vacíos de evidencia restantes

1. Documento normativo exacto de Zigbee R22/ZCL7 y revisión IEEE asociada, aprobado para el proyecto.
2. Corpus legal, versionado y representativo con frames plaintext, secured, truncados, manufacturer-specific y fragmentados.
3. Confirmación del contrato PHR/FCS/longitudes con firmware actual y sniffer independiente.
4. Semántica exacta, wrap y continuidad del timestamp RAT; pérdida bajo carga y telemetry end-to-end.
5. Versiones fijadas y ejecutadas de los comparadores externos.
6. Cobertura ZDO/ZCL exacta que aprobará el supervisor.
7. Vectores Zigbee CCM*, key provenance, nonce/AAD y niveles soportados.
8. Conclusión legal sobre adaptación/redistribución de código TI y dependencias GPL en runtime.
9. Map file, margen RAM/flash y high-water de stacks para cualquier cambio embedded.
10. Completion correlacionado de RX/TX y timing medido para cualquier primitiva MAC/RF.
11. Regresión BLE/IEEE/Sub-1 GHz y frontend/antena si se modifica firmware.

Estos vacíos no impiden aprobar la arquitectura host pasiva. Sí impiden describir cualquiera de sus componentes como implementado o validado, y bloquean todo claim de participación o interoperabilidad Zigbee.
