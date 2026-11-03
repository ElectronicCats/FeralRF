# Propuesta técnica de integración Zigbee selectiva y nativa en FeralRF

**Revisión:** 1.0  
**Fecha:** 2026-11-03  
**Repositorio:** `fc0de99b61cdb3d37701cdc625980b8882ed92b2`  
**Rama:** `feature/hardware-validation`  
**SDK fijado:** `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`  
**Target:** CC1352P7  
**Registro de evidencia:** [FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md](FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md)

## 1. Resumen ejecutivo

Se recomienda extender FeralRF con un **pipeline pasivo, acotado y versionado de decodificación Zigbee ejecutado en Python**, sobre el flujo IEEE 802.15.4 raw que el firmware ya entrega. El baseline propuesto no modifica el firmware, el protocolo COBS, `Radio`, `Packet`, la propiedad RF ni los otros PHY. Conserva siempre el MPDU original y añade resultados derivados con offsets, contexto, errores tipados y revisión de schema.

El mínimo inicial seleccionado comprende:

1. normalización explícita del contrato `Packet.data` —MPDU recibido con FCS—;
2. parsing bounded de MAC IEEE 802.15.4 para el perfil normativo aprobado;
3. parsing bounded de Zigbee NWK y APS;
4. parsing del header ZCL y un subset de foundation commands/tipos;
5. decoding de un subset ZDO orientado a identificación y descriptores;
6. detección/interpretación estructural de seguridad suficiente para preservar header, ciphertext y MIC y detener el plaintext pipeline de forma correcta;
7. preservación raw, unknown fields, provenance y errores por capa;
8. corpus versionado y comparación con un dissector independiente como parte inseparable de la aceptación.

No se seleccionan para el baseline: estado pasivo persistente, descifrado, builders, inyección Zigbee-aware ni cambios RF/MAC. Esas capacidades quedan como extensiones condicionadas y separadas. Joining, formation, routing, coordinator/router/end-device operacional, BDB, key lifecycle, conformance y delivery Zigbee quedan excluidos.

La recomendación reutiliza Z-Stack de forma selectiva: `zclParseHdr()`, `zclBuildHdr()`, `zclGetDataTypeLength()`, `zclGetAttrDataLength()` y `zclSerializeData()` son referencias algorítmicas adaptables con bounds; varios `ZDO_Parse*` aportan field order, pero su dependencia de `zdoIncomingMsg_t`, OSAL heap y falta de validación completa favorece una reimplementación host. `NLDE_ParseMsg()` y `APSDE_ParseMsg()` sólo están declaradas; ZMAC delega en TIMAC binario. Ninguno de esos componentes operacionales se incorpora.

La propuesta ofrece valor académico porque convierte una capacidad raw demostrable en una herramienta protocol-aware verificable sin fingir que FeralRF es un nodo Zigbee.

## 2. Objetivo, alcance y criterio de evidencia

### 2.1 Objetivo rector

Extender el proyecto FeralRF existente con un subconjunto Zigbee mantenible y verificable, preservando el CC1352P7 y la arquitectura RF actual. Las funciones host forman parte de FeralRF, pero se identifican siempre como ejecución Python, no CC1352P7.

### 2.2 Alcance seleccionado

| Categoría | Decisión |
|---|---|
| Firmware | FeralRF sigue siendo el único firmware; baseline sin cambios embedded. |
| Host | Nuevos componentes internos Python, todos **NO IMPLEMENTADOS ACTUALMENTE**. |
| API existente | `Radio` y `Packet` se reutilizan sin cambio obligatorio. |
| Transporte | COBS/UART y respuestas actuales se preservan. |
| Z-Stack | Reuso de algoritmos/definiciones pequeños o referencia; no runtime ni translation units completas. |
| Semántica | Decoder/constructor/injector/MAC/nodo son capacidades distintas. |
| Validación | Ninguna capacidad propuesta se considera validada por aprobar esta arquitectura. |

### 2.3 Estados de evidencia

Se emplean los estados definidos en el registro: **CURRENT IMPLEMENTATION**, **SDK-CONFIRMED MECHANISM**, **HISTORICAL EXPERIMENTAL EVIDENCE**, **ARCHITECTURAL INFERENCE**, **CONDITIONALLY FEASIBLE**, **UNRESOLVED** y **UNSUPPORTED**. Cada recomendación principal referencia IDs del registro de evidencia.

## 3. Baseline verificado y reconciliación

El `HEAD` actual es `fc0de99`; el commit añade la evaluación nativa previa. No hay cambios en `firmware/`, `python/` ni `hardware/` desde `31f9535`, y el SDK permanece en `5b31d0a...`. El árbol estaba limpio antes de estos dos entregables. El target continúa configurado para GNU Arm/CC1352P7, aunque el ejecutable del compilador no está en `PATH` y no se verificó su versión [BASE-01–08].

Se preserva una corrección importante: el `source/ti/zstack/stack/nwk/nwk.c` citado por un registro anterior no existe. El inventario real contiene `nwk.h`, `nwk_bufs.h`, `nwk_globals.c`, `nwk_globals.h` y `nwk_util.h`; `NLDE_ParseMsg()` sigue sin definición visible. La ausencia fortalece la decisión de implementar NWK independientemente y no se corrige retroactivamente el informe anterior [DISC-01][SDK-Z11][SDK-Z14].

## 4. Arquitectura relevante actual

### 4.1 Responsabilidades

| Componente existente | Procesador/runtime | Responsabilidad verificada | Límite relevante |
|---|---|---|---|
| `Radio` / `Packet` | PC, Python | Control, RX asíncrono, TX y representación de bytes/metadata | No hay modelo MAC/Zigbee. [FW-H01–H05] |
| `protocol.py` / `protocol.c` | PC + CC1352P7 | COBS, CRC16 y frame de comando/respuesta | No es framing RF ni Zigbee. [FW-H06][FW-F01] |
| RP2040 bridge | Hardware/firmware externo al repo | Puente PC↔UART documentado | Source no disponible aquí; no atribuirle decoding. [HW-04] |
| `CommandProcessor` | CC1352P7 | Valida y acepta comandos | ACK puede preceder terminación RF. [FW-F02–F03] |
| `ControlTask` | CC1352P7/TI-RTOS | Estado, buffers y eventos de control | `TX_FRAME` es raw; fallo posterior asíncrono. [FW-F04] |
| `DataTask` | CC1352P7/TI-RTOS | Ejecuta control/RF, drena colas y emite RX | Máximo 8 paquetes por poll para no bloquear UART. [FW-F05] |
| `PhyManager` | CC1352P7 | Selecciona PHY/backend/LL | IEEE usa LL default. [FW-F06] |
| `LLManager` | CC1352P7 | Clasificación BLE | IEEE pasa sin parsing. [FW-F07] |
| `RadioIF` | CC1352P7 | Único owner de RF Driver, colas, RX/TX y metadata | Compartido por BLE/IEEE/sub-GHz; alto riesgo de regresión. [FW-F08–F14] |
| RF Driver/RF Core | CC1352P7 system core/CM0 | Programa operaciones y radio | Hardware command ≠ servicio MAC. [SDK-RF01–04][TI-D02] |
| Crypto engine | CC1352P7 | AES-CCM genérico one-shot | No construye contexto Zigbee ni key lifecycle. [FW-H05][FW-F17] |

### 4.2 RX actual

```text
RF Core
  → cola RF (3 entradas)
  → callback que marca evento
  → RadioIF_poll()/RadioIF_processIeee154Packets()
  → cola RadioIF (8 paquetes)
  → LLManager (IEEE pass-through)
  → DataTask_emitRxPacket()
  → COBS/UART/RP2040/PC
  → Radio.read_packets()
  → Packet
```

`RadioIF_startIeee154RfBackend()` configura PHR y FCS incluidos y añade RSSI, corr/CRC y timestamp. `RadioIF_processIeee154Packets()` elimina PHR, conserva el MPDU incluido su FCS, descarta CRC-bad y construye `RadioIF_RxPacket` [FW-F12–F13]. El baseline propuesto comienza después de `Packet`, no altera esa cadena.

### 4.3 TX actual

```text
Radio.transmit(bytes)
  → COBS command
  → CommandProcessor ACK de aceptación
  → ControlTask event/buffer
  → DataTask_poll()
  → RadioIF_transmitIeee154Raw()
  → CMD_IEEE_TX inmediato
  → RF Core añade PHR/FCS
```

El caller entrega 1–125 bytes de MPDU sin FCS. TX es rechazado si RX está activo. No existen CCA/CSMA, espera de ACK, retry, association ni completion host correlacionado [FW-H04][FW-F03–F04][FW-F11][FW-F14–F15].

## 5. Contrato IEEE 802.15.4 consumido

| Campo/propiedad | Estado actual | Consecuencia para la propuesta | Validación pendiente |
|---|---|---|---|
| PHY/canal | `PHY.IEEE_802_15_4=4`; canal aplicado a RX/FS | El contexto de captura debe acompañar cada resultado | Confirmar canales/límites con firmware actual. |
| PHR RX | Incluido en cola RF, removido antes de `Packet` | Parser MAC nunca recibe PHR | Fixture/captura byte-exacta. |
| MPDU RX | `Packet.data` conserva MPDU | Input primario inmutable | Confirmación OTA independiente. |
| FCS RX | Incluido; CRC-bad descartado | `fcs_present=true`; parser separa últimos 2 bytes, no los adivina | Longitudes frontera y bad-FCS policy futura. |
| TX input | MPDU sin FCS, 1–125 bytes | Builder condicional debe excluir FCS | Captura independiente del FCS generado. |
| CRC | Hardware/firmware filtra inválidos | Baseline no analiza bad-FCS | Aclarar si se desea modo forensic posterior. |
| RSSI | Byte appended expuesto | Metadata, no semántica Zigbee | Correlación con instrumento no demostrada. |
| `lqi` | Siete bits de correlación expuestos como `lqi` | Nombrar “correlación/LQI reportada”; no normalizar sin evidencia | Calibración/semántica pendiente. |
| Timestamp | RAT de 32 bits dividido entre 4 | Contexto relativo; no asumir wall clock ni continuidad | Evento exacto, wrap y loss bajo carga. |
| Colas | 3 RF, 8 app; hasta 8 drains/poll | Captura puede tener gaps; estado pasivo no puede asumir exhaustividad | Stress/telemetry de drops. |
| RX/TX | Mutuamente excluyentes | Construcción no implica respuesta/ACK temporal | Validar error y recuperación. |
| ACK host | Aceptación/encolado | No usar como “transmitido” | Completion contract futuro. |

Estados: la representación anterior es **CURRENT IMPLEMENTATION** [FW-F09–F15]; RAW OTA es sólo **HISTORICAL EXPERIMENTAL EVIDENCE** en canal 25 [EXP-01–05].

## 6. Subconjunto funcional seleccionado

### 6.1 Baseline propuesto

| ID | Capacidad | Cobertura inicial | Razón de selección |
|---|---|---|---|
| `R-BASE-01` | Adaptador de contrato raw | MPDU+FCS, metadata y schema profile explícitos | Evita heurísticas y desacopla captura de parsing. |
| `R-MAC-01` | MAC bounded | FCF, frame type/version aprobado, sequence, addressing, PAN compression, aux security boundary, payload/FCS; IE no soportada se detecta y preserva | Fundamento de todas las capas. |
| `R-NWK-01` | NWK bounded | Frame control/type/version, addresses, radius, seq, ext addresses, multicast/source route y security boundary | Alta utilidad pasiva; definitions disponibles. |
| `R-APS-01` | APS bounded | Type, delivery mode, endpoints/group, cluster, profile, counter, ext/fragment/security boundary | Entrega contexto a ZDO/ZCL. |
| `R-ZDO-01` | ZDO seleccionado | `Device_annce`, `NWK_addr_rsp`, `IEEE_addr_rsp`, `Node_Desc_rsp`, `Active_EP_rsp`, `Simple_Desc_rsp` | Identidad y descriptors con field-order TI visible. |
| `R-ZCL-01` | Header ZCL | FC, manufacturer code, direction, TSN, command ID | Algoritmo TI pequeño y claro. |
| `R-ZCL-02` | Foundation subset | Read/Write Attributes req/rsp, Report Attributes, Default Response, Discover Attributes req/rsp; tipos escalares y strings cubiertos por el schema aprobado | Valor analítico, cobertura bounded y testable. |
| `R-SEC-01` | Frontera de seguridad | Extraer campos visibles, longitud MIC esperada cuando esté definida, preservar ciphertext/MIC y detener plaintext | Evita falsos decodes; no decryption. |
| `R-RAW-01` | Preservación | Bytes originales, spans/offsets y unknown remainder en cada capa | Auditabilidad y forward compatibility. |
| `R-ERR-01` | Errores tipados | `truncated`, `malformed`, `unsupported_revision`, `unsupported_field`, `encrypted`, `missing_context`, `schema_unknown` | Sin excepciones opacas por frame. |
| `R-VAL-01` | Comparación independiente | Corpus versionado + dissector fijado + resolución contra norma | Aceptación reproducible. |

**No se promete cobertura completa** de MAC moderno, ZDO, ZCL ni manufacturer-specific clusters. Un comando desconocido se conserva raw y produce un resultado estructurado, no semántica inventada.

### 6.2 Extensiones condicionadas

| ID | Capacidad | Gate antes de aprobar |
|---|---|---|
| `C-OBS-01` | Estado pasivo: devices, mappings, transactions, fragments, topology/attributes observados | Parsers aceptados; modelado de gaps, provenance, expiry y conflicto. |
| `C-DEC-01` | Descifrado autenticado autorizado | Revisión/security levels, keys autorizadas, nonce/AAD, CCM* known-answer y política de secretos. |
| `C-BLD-01` | Serializers/builders y raw TX | Schemas aceptados, round-trip, FCS contract, estados accepted/completed/observed separados. |
| `C-RF-01` | Timed raw TX o CCA observacional | Requisito funcional real, timebase/completion, recursos y regresión demostrables. |

Auto-ACK, CSMA, TX→RX_ACK y RX→TX quedan investigación posterior a `C-RF-01`, no parte implícita de ella.

### 6.3 Exclusiones operacionales

Joining, formation, coordinator/router, endpoint operacional, NWK routing, BDB, key lifecycle, MAC completo, conformance y delivery guarantee requieren runtime/estado/temporización ausentes. Se excluyen, no se posponen silenciosamente.

## 7. Reuso selectivo de Z-Stack

### 7.1 Evaluación por función

| Candidato | Input/output real | Entorno/dependencias | Decisión | Hardening/prueba requerida |
|---|---|---|---|---|
| `zclParseHdr()` | `uint8_t*`→`zclFrameHdr_t` + puntero payload | Tipos/macros ZCL; sin longitud | **Adaptar algoritmo** en Python, no compilar archivo | Buffer length 3/5; bits reservados; vectors y truncación. [SDK-Z03] |
| `zclBuildHdr()` | struct + buffer→puntero final | `static`; sin capacidad | Referencia para `C-BLD-01`, no baseline | Writer bounded y round-trip. [SDK-Z04] |
| `zclGetDataTypeLength()` | datatype→0/1…16 | Constantes; cero ambiguo | Adaptar como tabla typed fixed/variable/unsupported | Exhaustividad contra ZCL7 aprobado. [SDK-Z06] |
| `zclGetAttrDataLength()` | datatype+data→length | Lee prefijo 1/2 sin bytes restantes | Adaptar regla, nunca firma original | Bounds, overflow y content available. [SDK-Z06] |
| `zclSerializeData()` | C attr→bytes | Layout C, endian, sin capacidad | Referencia condicional para serializer | Writer bounded, tipos y round-trip. [SDK-Z05] |
| `ZDO_ParseDeviceAnnce()` | `zdoIncomingMsg_t`→struct | ASDU sin length check; OSAL types | Reimplementar desde schema; usar field order como evidencia | Longitud exacta/extra bytes y endianness. [SDK-Z10] |
| `ZDO_ParseAddrRsp()` | incoming→heap object/list | `OsalPort_malloc`, count/ownership | Reimplementar bounded | Counts vs remaining, allocation caps. [SDK-Z10] |
| `ZDO_ParseEPListRsp()` | incoming→heap endpoints | Count leído antes de comprobar ASDU | Reimplementar bounded | 0/max/truncated counts. [SDK-Z10] |
| `ZDO_ParseSimpleDescRsp/Buf()` | incoming→descriptor+dos listas | Heap/free rules; sin buffer length | Reimplementar bounded | Descriptor length y cluster counts. [SDK-Z10] |
| `NLDE_ParseMsg()` | declaración buffer+len→frame format | Definición no visible | **No reutilizable**; parser independiente | Norma aprobada + corpus. [SDK-Z11/Z14] |
| `APSDE_ParseMsg()` | declaración NWK frame→APS format | Definición no visible | **No reutilizable**; parser independiente | Norma aprobada + corpus. [SDK-Z12] |
| ZMAC/TIMAC | MLME/MCPS wrappers/callbacks | Binarios, OSAL, tasks, PIB/state | Excluido | No hay adaptación parcial equivalente. [SDK-M01–05] |

### 7.2 Direct source vs. definición

- **Direct source reuse:** no seleccionado para baseline.
- **Adapted source algorithm:** sólo helpers ZCL pequeños si la revisión de licencia lo permite y tras reescritura bounded.
- **Protocol-definition reuse:** constantes/máscaras ZCL/ZDO/NWK/APS como referencia secundaria.
- **Independent reimplementation:** MAC, NWK, APS y ZDO; preferida para ownership, errors y mantenimiento.
- **Dependiente del runtime/no soportado:** ZMAC/TIMAC, AF, ZDO/ZCL operacionales, BDB, security/NV y Z-Stack API.

`zcl.c` contiene aviso BSD-3-Clause, mientras `aps.h` incluye condición TI-device-only; FeralRF es GPL-3.0. La compatibilidad/redistribución debe revisarse archivo por archivo. La disponibilidad de fuente no resuelve la cuestión legal [SDK-Z01/Z13][TI-D06].

## 8. Estrategia normativa y versionado

TI declara para Z-Stack 8.30 Zigbee PRO 2017/R22 y ZCL7 [TI-D04]. El catálogo CSA actual ofrece R22.1/R23.x y ZCL7/8 [CSA-D01]. IEEE 802.15.4-2020 está hoy superseded por 2024 [IEEE-D01]. Por tanto:

1. el profile provisional de implementación será **R22/ZCL7 alineado al SDK**, pendiente de aprobación y acceso a los textos exactos;
2. el profile declarará separadamente revisión MAC, Core, ZCL y schemas de cluster;
3. no se mezclará R23/ZCL8 en el mismo parser sin un profile nuevo;
4. frame version/IE no cubierto producirá `unsupported_revision` o `unsupported_field` conservando raw;
5. manufacturer-specific sin schema se entrega como header+payload raw;
6. la ZCL rev.6 pública sólo puede ayudar históricamente; no es oráculo de ZCL7.

Esta decisión provisional requiere aprobación del supervisor antes de implementar `R-MAC-01` y schemas superiores.

## 9. Arquitectura propuesta

### 9.1 Principio

Python procesa pasivamente `Packet.data`; el firmware permanece intacto. La hipótesis está soportada porque el MPDU y metadata ya llegan al host y ninguna operación de parsing exige timing RF [FW-H01][FW-F05/F13].

### 9.2 Componentes

Todos los elementos marcados “propuesto” son **NO IMPLEMENTADOS ACTUALMENTE** y sus nombres son candidatos internos, no API pública comprometida.

| Componente | Estado | Ubicación | Responsabilidad/input/output | Firmware/recursos RF |
|---|---|---|---|---|
| `Radio`, `Packet`, transporte | Existente | Python/firmware | Captura, metadata y bytes | Sin cambio. |
| `zigbee.model` conceptual | Propuesto | Python | Tipos inmutables de span/context/result/error/schema | Ningún cambio. |
| `zigbee.pipeline` conceptual | Propuesto | Python | Orquesta MAC→NWK→APS→ZDO/ZCL y stops | Ningún cambio. |
| `zigbee.mac` conceptual | Propuesto | Python | `RawCapture`→`MacResult` | CPU/RAM host. |
| `zigbee.nwk` conceptual | Propuesto | Python | MAC payload→`NwkResult` | CPU/RAM host. |
| `zigbee.aps` conceptual | Propuesto | Python | NWK plaintext→`ApsResult` | CPU/RAM host. |
| `zigbee.zdo` conceptual | Propuesto | Python | APS ZDO context→selected schema result | CPU/RAM host. |
| `zigbee.zcl` conceptual | Propuesto | Python | APS app context→header/foundation result | CPU/RAM host. |
| `zigbee.schemas` conceptual | Propuesto | Python/data | Profiles versionados y registries | Revisión/mantenimiento host. |
| adapter PCAP/comparator | Propuesto para validación | Python/tool externo | Corpus/PCAP→comparación | No runtime obligatorio. |
| observation/security/builders | Condicional | Python | Estado, decryption, serializers | Sin firmware salvo elección explícita futura. |
| timed RF primitive | Condicional | CC1352P7 | Operación temporal estrecha | Cambio de alto riesgo; fuera baseline. |

### 9.3 Contratos internos conceptuales

- **`RawCapture` conceptual:** referencia inmutable a MPDU, `fcs_present`, canal, RSSI, corr/LQI, timestamp y profile.
- **`LayerResult` conceptual:** status, campos tipados, spans `[start,end)`, payload/remaining, issues y raw reference.
- **`DecodeIssue` conceptual:** layer, code, severity, offset, required/available y detail no sensible.
- **`SchemaProfile` conceptual:** IDs/revisiones aprobadas y feature flags.

Estos contratos no sustituyen `Packet`; lo envuelven. El decoder nunca modifica bytes y no copia salvo que ownership/lifetime lo exija. Cada capa consume un span de la anterior y conserva offsets absolutos al MPDU.

### 9.4 Política de errores

Por frame, truncation/unsupported/encrypted son resultados, no fallos del stream. Sólo errores de programación/configuración elevan excepciones. El pipeline se detiene en la última capa segura:

- malformed MAC: no intenta NWK;
- payload no Zigbee: termina con clasificación raw;
- NWK/APS secured sin plaintext: devuelve security metadata+ciphertext/MIC;
- revisión no soportada: devuelve campos previos+raw remainder;
- contexto ZCL faltante: header opcional, payload raw y `missing_context`;
- comando desconocido: preserva command ID/payload, `schema_unknown`.

## 10. Flujos de datos

### 10.1 A — Recepción y decode pasivo

```text
RF Core → RadioIF → DataTask → COBS/UART → Radio.read_packets() → Packet
                                                               │
                                                               ▼
                                                      RawCapture (propuesto)
                                                               │
                  ┌──────────── malformed/unsupported ─────────┤
                  ▼                                            ▼
             DecodeIssue ← MAC → NWK → [security gate] → APS → dispatch
                                                           ├─ ZDO subset
                                                           └─ ZCL subset
```

La detección Zigbee se basa en contexto/estructura aprobados; no en que toda trama IEEE sea Zigbee. Security gates impiden interpretar ciphertext como APS/ZDO/ZCL.

### 10.2 B — Estado pasivo condicional

```text
LayerResult + capture provenance
        → observation service (C-OBS-01, NO IMPLEMENTADO)
             ├─ short↔EUI-64 mappings observados
             ├─ PAN/device observations
             ├─ transaction/fragment correlation
             └─ topology/attributes observados o inferidos
```

Cada registro conserva primera/última observación, source frame, confidence y gaps. “Observado” no significa miembro actual/autorizado de la red.

### 10.3 C — Construcción condicional

```text
campos+schema+contexto explícito
  → serializers ZCL/ZDO/APS/NWK/MAC
  → MPDU ≤125 sin FCS
  → Radio.transmit()
  → accepted
  → [completion RF hoy no correlacionado]
  → observed OTA
  → optional MAC ACK
  → optional application response
```

Cada estado es distinto. Sólo el primero está reflejado por el ACK host actual; los demás requieren evidencia propia.

### 10.4 D — Primitiva embedded condicional

```text
conceptual host command (versionado)
 → CommandProcessor: valida/operation-id
 → ControlTask: copia/estado/evento
 → DataTask: serializa ejecución
 → RadioIF: chain/trigger RF
 → RF Driver/Core
 → terminal result correlacionado
```

No se incluye en baseline. Antes debe existir necesidad funcional que no pueda resolverse en host, además de timebase, completion y regresión.

## 11. Compatibilidad

### 11.1 Protocolo e interfaz

| Dimensión | Compatibilidad fuente | Condición/límite |
|---|---|---|
| MPDU/FCS | Compatible con input pasivo | Contrato debe hacerse explícito; confirmación actual pendiente. |
| NWK/APS | Bytes preservados permiten parsing | Sólo plaintext después de security gate; revisión versionada. |
| ZDO/ZCL | Contexto APS contiene profile/cluster/endpoints | Subset/schema explícito; unknown raw. |
| `Radio` | Reutilizable sin cambio | Decoder puede ser servicio separado; no ocultar runtime. |
| `Packet` | Suficiente para baseline | LQI/timestamp tienen incertidumbres. |
| COBS/UART | Sin cambio | Parser corre después de entrega host. |
| Async delivery | Compatible | Gaps y multiplexing deben registrarse; no asumir lossless. |
| Errores | Decoder añade errores propios | No alterar silenciosamente `CommandError`/RF errors. |

### 11.2 Implementación/toolchain/licencia

La solución Python independiente evita ABI y librerías Z-Stack IAR/TI-clang, OSAL heap, globals, tasks y ownership embebido. Tampoco consume RAM/stack CC1352P7 ni cambia RF. Los algoritmos adaptados deben tener notice/provenance y revisión legal. Scapy/zigpy no son dependencias runtime baseline por cobertura, adapter y licencias [SDK-M04–05][EXT-01–02].

### 11.3 Runtime/hardware

Baseline no toca RF Driver, tasks, frontend/antena ni buffers embedded. Sí consume CPU/RAM del host por frame; se debe medir throughput y queueing en implementación. Exportar/decodificar no recupera frames ya perdidos. Cualquier futuro firmware change activa pruebas de memoria, timing y regresión BLE/IEEE/sub-GHz [HW-01–04].

## 12. Enfoque conceptual de implementación

Este apartado define separaciones, no pasos de código.

1. **Contrato primero:** un adapter valida metadata y expone MPDU/FCS sin heurísticas.
2. **Cursor bounded común:** cada parser lee mediante remaining/offset y nunca con puntero sin longitud.
3. **Resultados parciales:** un header válido se conserva aun si payload es unsupported.
4. **Schemas declarativos versionados:** comandos/tipos no quedan dispersos en conditionals opacos.
5. **Raw como autoridad:** campos derivados incluyen spans hacia los bytes originales.
6. **Security gate:** ninguna capa plaintext se invoca sobre ciphertext.
7. **Independent oracle:** fixtures esperados provienen de norma/corpus y se comparan con herramienta externa fijada.
8. **Feature discovery interna:** profile declara cobertura; ausencia de schema no se interpreta como malformed.
9. **Sin firmware baseline:** no añadir resultados Zigbee al wire protocol ni parsear en `LLManager`/`RadioIF`.

## 13. Especificación funcional baseline

### 13.1 Requisitos testables

| ID | Propósito; entrada/contexto | Salida y comportamiento válido | Malformed/unsupported/encrypted | Ubicación/dependencias | Validación y exclusiones |
|---|---|---|---|---|---|
| `R-BASE-01` | Convertir `Packet` IEEE en captura; requiere PHY, MPDU y policy FCS | Raw inmutable, metadata, payload length y FCS span | Length<minimum→typed issue; nunca inferir Zigbee | Python; `Packet`; componente propuesto | Fixtures de layout + captura independiente. No corrige RF metadata. |
| `R-MAC-01` | Parsear MAC del MPDU+FCS según profile | FCF/type/version/seq/address/PAN/security/IE flags, header/payload/FCS spans | Truncated exact offset; version/IE no soportada→raw remainder | Python; norma IEEE aprobada | Vectores de combinations, boundaries, fuzz, oracle. No MAC service. |
| `R-NWK-01` | Parsear payload MAC identificado como Zigbee | FC/type/version/dst/src/radius/seq/spans opcionales/frontera de seguridad | Un frame malformado detiene APS; una versión no soportada se preserva | Python; profile Zigbee | Flags combinadas, source route/multicast/ext addr y corpus negativo. Sin routing. |
| `R-APS-01` | Parsear NWK plaintext | Type/delivery/endpoints/group/cluster/profile/counter/ext/fragment/security | Ciphertext→security stop; missing fields typed issue | Python; `R-NWK-01` | Data/command/ack, unicast/group, ext headers, fuzz. No APS service/ACK. |
| `R-ZDO-01` | Decodificar payload APS profile `0x0000` seleccionado | TSN + estructura del comando; cluster desconocido raw | Inconsistencia count/length tipada; comando no soportado raw | Python; contexto APS; schemas R22 | Vectores para seis mensajes y counts cero/máximo/truncados. Sin ZDO completo. |
| `R-ZCL-01` | Parsear header app plaintext | FC/manufacturer/direction/TSN/command + payload span | <3/5 bytes→truncated; reserved preserved | Python; APS cluster/profile context | 3/5-byte vectors, directions/types/manufacturer. No command semantics por sí solo. |
| `R-ZCL-02` | Decodificar foundation subset y tipos aprobados | Typed records/values + unknown tail | Invalid length/count typed; unknown datatype raw | Python; ZCL7 schema provisional | Endian, scalar/string boundaries, each command, round-trip when serializer exists. No cluster-specific baseline. |
| `R-SEC-01` | Evitar falso plaintext y exponer security metadata | Visible control/key-id/counter/source context, ciphertext/MIC spans, auth=`not_attempted` | Missing EUI/context reported; nunca plaintext no autenticado | Python; NWK/APS results | Security-header vectors y malformed cases. No key acquisition/decryption. |
| `R-RAW-01` | Mantener auditabilidad | Original bytes + absolute spans + unknown raw en todo resultado | Siempre preserva incluso error | Python common model | Property tests: concatenación/spans reproducen input. |
| `R-ERR-01` | Errores consumibles sin matar stream | Layer/code/severity/offset/required/available | No stack trace para input normal inválido | Python common model | Cada negative fixture produce issue estable; decoder continúa siguiente frame. |
| `R-VAL-01` | Evidencia independiente | Resultado corpus propio + comparator + diff explicado | Divergencia no resuelta hasta norma | Test tooling, versión fijada | Cada fixture baseline tiene expectativa y resultado; sin umbral OTA inventado. |

### 13.2 Política común

- Output nunca afirma “dispositivo unido”, “mensaje entregado” o “autenticado” sin evidencia correspondiente.
- Unsupported revision es distinto de malformed.
- Unknown enum/command se retiene numéricamente.
- Manufacturer-specific sólo se interpreta con schema versionado explícito.
- El contexto necesario viaja entre capas; no se usa estado global implícito.
- Los limits de lista/string se derivan del frame y de caps defensivos documentados; caps de implementación deben aprobarse y probarse.

## 14. Extensiones condicionales: especificación de entrada

| ID | Condiciones mínimas | Output permitido | Prohibición |
|---|---|---|---|
| `C-OBS-01` | Baseline validado; provenance/gap/expiry model aprobado | Observations con confidence y source frames | No “authoritative topology” ni membership actual. |
| `C-DEC-01` | Keys autorizadas; exact nonce/AAD/level; known-answer CCM*; secret policy | Plaintext sólo con MIC válido; auth state explícito | No plaintext tras MIC failure; no key discovery. |
| `C-BLD-01` | Schema aprobado; bounded writers; parse/serialize round-trip; FCS contract | MPDU sin FCS y construction report | No claim de join/delivery/ACK por host ACK. |
| `C-RF-01` | Need aprobado; RAT/completion design; memory/regression plan | Una primitiva exacta con terminal result | No “MAC service” por agregar timed TX/CCA. |

## 15. Dependencias y riesgos

| Riesgo | Evidencia/causa | Impacto | Mitigación/decisión |
|---|---|---|---|
| Revisión normativa incorrecta | R22/ZCL7 vs R23/ZCL8/IEEE 2024 | Parser aparentemente correcto pero semántica mezclada | Profile aprobado antes de código; fixtures por versión. |
| Out-of-bounds | Helpers TI sin length/capacity | Seguridad/robustez | Cursor bounded, caps y fuzz; no firmas originales. |
| Falsa clasificación Zigbee | Toda trama IEEE no es Zigbee | Resultados engañosos | Dispatch estructural/contextual y `not_zigbee/unknown`. |
| Ciphertext interpretado | Security flag/context faltante | Datos falsos | Security gate obligatorio. |
| Pérdida de raw/provenance | Models sólo semánticos | No auditable | `R-RAW-01` obligatorio. |
| Gaps de captura | Colas/transport/timestamp | Estado/topología incompletos | Telemetry y explicit gaps; estado fuera baseline. |
| Licencia TI/GPL | Mixed files/tools | Redistribución/dependency risk | Implementación independiente; review legal; tools externos de test. |
| Drift de schemas | Zigbee/ZCL evoluciona | Mantenimiento | Registry versionado y cobertura declarada. |
| Oracle externo divergente | Scapy/Wireshark coverage/version | Falsa confianza | Pin version; norma resuelve discrepancia. |
| Exposición de claves | Decryption futura | Riesgo de seguridad | Fuera baseline; policy y negative tests antes de incluir. |
| Regresión RF | Cambio futuro a `RadioIF` | BLE/IEEE/sub-GHz afectados | No firmware baseline; gates estrictos para `C-RF-01`. |
| Semántica de completion | ACK anticipado | TX claim incorrecto | Estados separados; corregir antes de active claims. |

## 16. Preservación de FeralRF

El baseline satisface preservación por diseño:

- no modifica C, Python existente, CMake, SysConfig, linker ni submodule;
- no cambia `Radio`, `Packet`, command IDs o COBS;
- no consume CC1352P7 flash/RAM/stack ni RF slots;
- no altera BLE, IEEE capture, sub-GHz, antenna switch o task priorities;
- permite opt-in del decoder; quien use raw obtiene exactamente el contrato anterior;
- guarda el MPDU original aun cuando el decoder falle.

Antes de integrar cualquier componente se deberá demostrar con tests que importar/usar el decoder no cambia captura o TX existente. Si más adelante se cambia firmware, el baseline de regresión de todos los PHY pasa a ser gate obligatorio.

## 17. Estrategia de validación reproducible

### 17.1 Actividades

| ID | Qué se verifica | Herramientas/equipo y precondiciones | Estímulos | Observables | Criterio de aceptación | Evidencia retenida | Limitación |
|---|---|---|---|---|---|---|---|
| `V-RAW-01` | PHR/FCS/length/channel/RX/TX/ACK contract actual | Dos CatSniffer identificados, firmware hash, sniffer independiente; autorización RF | MPDUs únicos en límites definidos por source, channels aprobados, RX/TX conflict | Bytes capturados, FCS, terminal events, stats, logs | Igualdad byte-exacta esperada; estados accepted/completed separados; cleanup exitoso | Hashes, commands, PCAPNG, JSONL, stats, setup | No prueba Zigbee. |
| `V-MAC-01` | `R-MAC-01`, errors y raw spans | Offline; corpus normativo versionado + comparator fijado | Positivos por FCF/address/version; truncación en cada offset; unknowns | Result/spans/issues/diff | Todos los fixtures declarados producen expectativa exacta; ninguna lectura OOB/fuzz crash | Corpus, expected JSON, version/hash, fuzz seeds | Cobertura limitada al profile. |
| `V-NWK-APS-01` | `R-NWK-01/R-APS-01/R-SEC-01` | Offline; Zigbee profile aprobado | Flags opcionales combinadas, frames types/delivery, secured/truncated | Layer fields, stops, ciphertext/MIC spans | Igualdad con vectors/norma; secured nunca llega a plaintext parser | Fixtures/diffs/revision | No routing/APS service. |
| `V-ZDO-01` | Seis mensajes baseline | Offline; schemas versionados | count 0/max válido, inconsistent/truncated, unknown cluster | Typed records/raw/issues | Field/endian/count exactos; unknown preservado; no OOB | Fixture por command/case | No ZDO completo. |
| `V-ZCL-01` | Header/foundation/types | Offline; ZCL7 aprobado | Header 3/5, comandos seleccionados, límites scalar/string y manufacturer desconocido | Registros tipados, spans y remanente raw | Resultado exacto esperado; longitudes inválidas rechazadas; no soportado queda raw | Schema/version, vectores y logs de round-trip | Sin semántica cluster-specific. |
| `V-SEC-01` | Security structural y futuro decryption | Offline; authorized known-answer data | Headers, missing context; para futuro correct/wrong key/nonce/AAD/MIC | Auth state, ciphertext/plaintext policy | Baseline: no plaintext; futuro: plaintext sólo MIC válido y negatives fallan | Vector provenance, no secret logs | No key lifecycle. |
| `V-COMP-01` | Independencia del decoder | Release fijado de Scapy/Wireshark; misma captura | Corpus baseline | Diferencia campo a campo | Toda divergencia se explica por norma o queda bloqueante/no resuelta | Versiones, comando, salida y diff | El comparador no es autoridad normativa. |
| `V-BLD-01` | `C-BLD-01` | Offline round-trip; luego sniffer independiente | Campos boundary/unknown policy; MPDU TX autorizado | Bytes, FCS OTA, states | Serializer/parser equivalence y captura byte-exacta; no confundir ACK | Fixtures, PCAP, logs | No delivery/interoperability. |
| `V-RF-01` | `C-RF-01` si se aprueba | Analizador lógico/RF, dos peers, map/high-water, firmware hash | Early/late/wrap triggers, queue pressure, cancel, other PHY | Timing/status/drops/memory/frontend | Requisito temporal normativo aprobado y terminal result exacto; regresión pasa | Traces, map, high-water, PCAP, manifests | Criterio numérico se deriva de norma, no se inventa aquí. |

### 17.2 Confirmación del baseline existente

Los resultados históricos RAW/FRAME prueban delivery de bytes en canal 25 bajo condiciones documentadas, no la revisión actual instalada ni Zigbee [EXP-01–05]. Antes de usar hardware para validar builders o RF debe repetirse `V-RAW-01` con source→artifact→device identity. Timestamp, packet loss, RX_STOP/completion y telemetry permanecen gaps.

### 17.3 Corpus y fuzz

El corpus debe registrar licencia/provenance, revisión, expected layer, FCS policy y si contiene datos sensibles. Debe incluir cada branch opcional, límites, truncation byte a byte, count/length inconsistency, unknown enums, manufacturer payload, secured frames y fragments. Fuzzing debe ser determinista/reproducible y conservar seeds mínimos de crash/divergencia. “No crash” no sustituye corrección semántica.

## 18. Matriz de trazabilidad requisito–validación

| Capacidad | Requisito | Evidencia | Componente propuesto | Verificación | Criterio | Estado actual |
|---|---|---|---|---|---|---|
| Raw input | `R-BASE-01` | FW-H01–04, FW-F05/F09/F12–15, EXP-01–05 | adapter/model | `V-RAW-01` + fixtures | Layout/FCS/bytes exactos | Existente/source-confirmed; históricamente observado; confirmación actual requerida. |
| MAC | `R-MAC-01` | IEEE-D01, AUD-01 | `zigbee.mac` | `V-MAC-01` | Expectativa exacta y bounded | Propuesto/no implementado; revisión no resuelta. |
| NWK | `R-NWK-01` | SDK-Z11/Z14, TI-D04 | `zigbee.nwk` | `V-NWK-APS-01` | Campos/offsets exactos | Propuesto; parser TI unavailable. |
| APS | `R-APS-01` | SDK-Z12/Z13, TI-D04 | `zigbee.aps` | `V-NWK-APS-01` | Modes/types/context exactos | Propuesto; parser TI unavailable. |
| ZDO | `R-ZDO-01` | SDK-Z08–10 | `zigbee.zdo/schemas` | `V-ZDO-01` | Seis mensajes+negatives | Propuesto/no implementado. |
| ZCL header | `R-ZCL-01` | SDK-Z02–04/Z07 | `zigbee.zcl` | `V-ZCL-01` | 3/5 bytes+negatives | Propuesto; algoritmo adaptable. |
| ZCL foundation | `R-ZCL-02` | SDK-Z05–07, TI-D04 | `zigbee.zcl/schemas` | `V-ZCL-01` | Commands/types selected exact | Propuesto; schema approval required. |
| Security boundary | `R-SEC-01` | SDK-Z12/M08 | `zigbee.security` | `V-SEC-01` | No false plaintext; spans exactos | Propuesto/no decryption. |
| Raw preservation | `R-RAW-01` | FW-H01/F13 | common model | property tests | Input reproducible desde spans | Propuesto. |
| Typed errors | `R-ERR-01` | Risks SDK-Z03–10 | common model | all negative suites | Stable issue + next frame continues | Propuesto. |
| Independent check | `R-VAL-01` | EXT-01/05, FW-H09 | test adapter | `V-COMP-01` | Divergences resolved/blocked | Proposed; tools not run. |
| Passive state | `C-OBS-01` | Results baseline | observation service | future state corpus | Provenance/gaps/conflicts correct | Conditionally feasible/deferred. |
| Descifrado | `C-DEC-01` | FW-H05/F17, SDK-M08 | servicio de seguridad | `V-SEC-01` futuro | Plaintext sólo con MIC válido | Condicional/no resuelto. |
| Builder/TX | `C-BLD-01` | FW-H04/F11/F14/F15, SDK-Z04–06 | serializers | `V-BLD-01` | Round-trip+OTA exact; state separated | Conditional; raw TX exists, builder absent. |
| RF temporal | `C-RF-01` | SDK-RF01–03, FW-F10–16 | frontera firmware | `V-RF-01` | Temporización derivada de norma + regresión | Sólo mecanismo SDK; no resuelto. |

La aprobación de implementación no cambia ningún estado a “validado”.

## 19. Prioridades y dependencias

### 19.1 Orden lógico

```text
P0: revisión normativa + contrato raw + corpus skeleton
 └─ P1: common model/errors/raw spans
     └─ P2: MAC
         └─ P3: NWK + security boundary
             └─ P4: APS
                 ├─ P5a: ZDO subset
                 └─ P5b: ZCL header/foundation subset

Validation/oracle runs alongside P1–P5, not after them.
P6+: state / decryption / builders are independent conditional branches.
P7: embedded RF only after an approved need; it does not depend on completing all host branches.
```

### 19.2 Matriz de prioridad

| Capacidad | Valor | Evidencia | Complejidad | Riesgo integración | Validación | Mantenimiento/licencia | Prioridad |
|---|---|---|---|---|---|---|---|
| Contrato/profile/corpus | Muy alto | Alta/pendiente normativa | Baja-media | Baja | Media | Baja | **P0** |
| Common model/errors/raw | Alto | Alta arquitectura | Baja-media | Baja | Baja | Baja | **P1** |
| MAC bounded | Muy alto | Media-alta | Media | Baja | Media-alta | Normativa | **P2** |
| NWK/security boundary | Muy alto | Definiciones, parser TI ausente | Media | Baja | Alta | Normativa | **P3** |
| APS | Muy alto | Definiciones, parser TI ausente | Media | Baja | Alta | Normativa | **P4** |
| ZDO subset | Alto | Field order visible | Media | Baja | Media-alta | TI/spec review | **P5a** |
| ZCL subset | Alto | Helpers visibles | Media-alta | Baja | Alta | License/schema | **P5b** |
| Estado pasivo | Medio-alto | Inferencia | Alta | Baja | Alta | Modelo durable | **Condicional** |
| Decryption | Medio | Crypto primitive | Alta | Media seguridad | Muy alta | Secrets/spec | **Condicional posterior** |
| Builders/TX | Medio | TX existente | Alta | Media | Muy alta | Schema/license | **Condicional separada** |
| RF primitives | Bajo sin requirement | Mecanismo SDK | Muy alta | Muy alta | Muy alta | Embedded | **Diferida** |

No se asignan horas, memoria ni costes sin datos.

## 20. Alternativas dentro del alcance

| Alternativa | Ventajas | Dependencias/riesgos | Decisión |
|---|---|---|---|
| Decoder Python interno spec-driven | Contrato natural con `Packet`; bounded; testeable; sin firmware/toolchain | Mantener schemas y obtener normas | **Seleccionada**. |
| Metadata mínima embedded | Puede preclasificar antes de UART | Consume task RF, nuevo wire contract, poco valor porque bytes ya llegan | Rechazada baseline. |
| Copiar helpers ZCL C al firmware | Algoritmo TI conocido | Bounds/ABI/license/CPU MCU; no resuelve NWK/APS | Rechazada. Adaptar idea en host. |
| Compilar translation units Z-Stack | Mucho código visible | AF/BDB/OSAL/globals/libs/toolchain; riesgo extremo | Rechazada/no soportada. |
| Scapy in-process | Cobertura rápida y objetos existentes | GPL-2.0-only, coverage/version y adapter no validados | Sólo comparator/optional integration futuro. |
| Wireshark/PCAP | Independencia, análisis maduro | No API in-process; metadata/timestamps/version | Comparator/export path, no arquitectura principal. |
| zigpy schemas | ZDO/ZCL model rico | GPL-3.0, controller context, no raw entry | Referencia comparativa, no baseline. |
| Builder bounded | Round-trip y experimentación | Puede confundirse con nodo/delivery; seguridad/counters | Rama condicional. |
| Timed TX/CCA | Puede habilitar research temporal | Firmware/completion/RF regression | Diferida hasta requirement explícito. |

## 21. Exclusiones explícitas

| Capacidad excluida | Base de exclusión | Tipo |
|---|---|---|
| ZNP/ZED/segunda persona | Contradice objetivo de extender único FeralRF | Decisión de proyecto. |
| Runtime Z-Stack completo | Toolchain/libs/tasks/RF/NV no compatibles demostrados | Unsupported por arquitectura actual. |
| Joining/rejoining | Requiere scan, association, MAC/NWK/security/timers/state | No soportado. |
| Network formation/coordinator/router | Requiere routing, tables, permit join, persistence | No soportado. |
| Endpoint Zigbee operacional | Requiere AF/APS/ZDO/ZCL services, state y timing | No soportado. |
| MAC completo | Auto-ACK/CSMA/retry/association/PIB/confirmations ausentes | No soportado; mecanismos RF aislados no bastan. |
| NWK routing/APS delivery | Parser no es servicio; core fuente ausente | No soportado. |
| BDB commissioning | Runtime/state/security/NV completos | Fuera de alcance. |
| Key lifecycle activo | Counters, replay, key update, NV/power-loss | Fuera baseline y no soportado. |
| Zigbee certification/conformance | Requiere suite/proceso formal y stack operacional | Unsupported claim. |
| Garantía OTA/application delivery | ACK host no prueba RF/peer/app | Unsupported claim. |
| Multi-channel simultáneo | Backend actual usa un owner/canal; no evidencia | No soportado. |
| Manufacturer clusters genéricos | Schemas propietarios/no examinados | Unknown raw solamente. |

## 22. Decisiones del supervisor y preguntas abiertas

| Tipo | Decisión/pregunta | Recomendación provisional | Bloquea baseline |
|---|---|---|---|
| Establecida por alcance | Único firmware FeralRF | Confirmar sin cambio | No. |
| Recomendación técnica | Decoder Python interno y firmware unchanged | Aprobar | Sí, decisión arquitectónica. |
| Aprobación requerida | Profile Zigbee PRO 2017/R22 + ZCL7 y base IEEE exacta | Aprobar alineación SDK u ordenar otro profile separado | Sí para schemas. |
| Aprobación requerida | Subset ZDO de seis mensajes | Aprobar como mínimo; expansión posterior | Sí para `R-ZDO-01`. |
| Aprobación requerida | Foundation ZCL seleccionado; sin cluster-specific | Aprobar; schemas de cluster en change control | Sí para `R-ZCL-02`. |
| Aprobación requerida | Política de licencia: reimplementación vs adaptación ZCL | Preferir independiente; review legal antes de copiar | Sí para source reuse. |
| Decisión diferible | Estado pasivo | Después del decoder | No. |
| Decisión diferible | Authenticated decryption | Fuera primera implementación | No. |
| Decisión diferible | Builders/raw TX | Rama posterior separada | No. |
| Decisión diferible | Embedded RF primitive | No autorizar sin requirement | No. |
| Experimento futuro | Confirmar FCS/length/timestamp/loss/completion en build identificado | Ejecutar `V-RAW-01` tras autorización | No para diseño; sí para claims. |
| Recurso futuro | Zigbee devices/captures/keys autorizadas y sniffer independiente | Definir antes de validation campaign | No para diseño. |

## 23. Recomendación técnica final

La integración debe adoptar una arquitectura **raw-first, host-side y spec-versioned**. FeralRF continúa capturando y transmitiendo IEEE 802.15.4; Python añade un pipeline pasivo que convierte bytes en resultados MAC/NWK/APS/ZDO/ZCL auditables sin perder el original. Los helpers ZCL y field order ZDO de TI sirven como evidencia o algoritmos adaptados con bounds; NWK/APS se reimplementan desde la especificación porque sus parsers no están disponibles. TIMAC/ZMAC y el runtime operacional quedan fuera.

El primer objetivo técnico no es “hacer un nodo Zigbee”, sino demostrar que un MPDU FeralRF puede:

1. ser interpretado de forma bounded según un profile aprobado;
2. detenerse correctamente ante encryption, revisión o schema desconocidos;
3. conservar raw/provenance y producir errores reproducibles;
4. coincidir con vectors normativos y un decoder independiente;
5. hacerlo sin cambiar firmware ni degradar otras capacidades FeralRF.

Sólo después de esa aceptación deben evaluarse, como ramas independientes, state, decryption, builders/TX o primitives RF. Ninguna debe heredar un claim de interoperabilidad del decoder pasivo.

## 24. Respuesta a la pregunta final

La funcionalidad específica que debe integrarse es un decoder pasivo Python de MAC IEEE 802.15.4, Zigbee NWK y APS, acompañado por parsing ZCL header/foundation acotado, un subset ZDO de identificación/descriptores, security-aware boundaries, raw preservation, typed errors y validación independiente. Esta selección utiliza el transporte IEEE existente y no requiere firmware.

TI Z-Stack apoya la integración mediante algoritmos pequeños ZCL y definiciones/field order ZDO/NWK/APS; no aporta parsers source-reusable para NWK/APS ni un MAC independiente. La compatibilidad exige fijar revisión R22/ZCL7 o un profile alternativo, respetar MPDU/FCS, no compilar runtime TI con el target GNU sin evidencia, no trasladar ownership OSAL y revisar licencias.

El éxito se demuestra con contrato raw byte-exacto, corpus versionado, negative/boundary/fuzz tests, comparación externa fijada y criterios por requisito. Si se añade TX, se separan construcción, aceptación, completion, observación, ACK y respuesta. Si se añade RF embedded, timing, queues, memory, frontend y regresión multi-PHY son obligatorios. Nada de ello autoriza afirmar joining, routing, conformance o nodo Zigbee operacional.

## 25. Revisión de consistencia final

- [x] Extiende FeralRF; no selecciona firmware/persona separada.
- [x] Cada capacidad tiene propósito, ubicación, frontera y validación.
- [x] Interfaces nuevas están marcadas **NO IMPLEMENTADAS ACTUALMENTE**.
- [x] `nwk.c`, `NLDE_ParseMsg()`, `APSDE_ParseMsg()` y TIMAC se tratan según disponibilidad real.
- [x] Decoder, builder, raw TX, MAC y participación se distinguen.
- [x] Hardware mechanism no se presenta como firmware service.
- [x] Compatibilidad fuente no se presenta como validación experimental.
- [x] Preservación de FeralRF es requisito explícito.
- [x] Gaps tienen actividades de verificación.
- [x] El registro separado soporta claims y decisiones.
- [x] No se modificó implementación ni auditorías previas para esta propuesta.
