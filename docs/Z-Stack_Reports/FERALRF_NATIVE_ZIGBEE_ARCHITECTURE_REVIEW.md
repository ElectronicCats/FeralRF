# Revisión crítica de arquitectura de la integración Zigbee nativa en FeralRF

**Fecha de revisión:** 2026-11-04  
**Documento revisado:** `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md`  
**Alcance:** revisión analítica; no se implementó, compiló, enlazó, flasheó ni validó por RF ninguna capacidad.

## 1. Dictamen ejecutivo

La dirección principal —decoder pasivo, raw-first, versionado y ejecutado en Python sobre la única persona firmware FeralRF— es **arquitectónicamente adecuada y está respaldada por el código actual**. El firmware ya entrega al host la MPDU IEEE 802.15.4 con FCS y metadatos básicos; el parsing pasivo no necesita temporización RF, servicios MAC operacionales ni el runtime Z-Stack. Colocar ese parsing en el host preserva RF Driver, tareas, memoria embebida, COBS y las demás PHY.

El documento, sin embargo, **debe revisarse antes de someterse a aprobación final**. No necesita un rediseño completo, pero cuatro omisiones afectan afirmaciones centrales:

1. `Packet` no incluye el PHY ni una política FCS explícita. `Packet.data` es suficiente como bytes, pero no como contexto autosuficiente; el adaptador debe recibir el estado de sesión o provenance externo.
2. Una trama IEEE 802.15.4 estructuralmente compatible no queda confirmada como Zigbee. Falta un clasificador conservador y un estado explícito de ambigüedad.
3. El despacho ZCL requiere un perfil APS permitido, además de cluster y endpoints. La propuesta no fija ese registro de perfiles y deja el subconjunto de tipos demasiado abierto.
4. `V-RAW-01` menciona eventos terminales que la API Python actual no expone de forma correlacionada. El éxito OTA sólo puede demostrarse externamente; el ACK host demuestra aceptación/encolado.

Por tanto, el veredicto global es **REVISE BEFORE APPROVAL**. La arquitectura se acepta **con cambios**; la factibilidad de implementación es alta para el baseline pasivo una vez fijadas las normas; la validación experimental de las capacidades propuestas continúa inexistente.

## 2. Alcance, método y estados de evidencia

Se contrastaron la propuesta, su registro de fuentes, los seis informes indicados, el código actual FeralRF y el SDK fijado. Los estados usados son:

- **CONFIRMADO-CÓDIGO:** comportamiento observable directamente en el checkout actual.
- **CONFIRMADO-SDK:** símbolo, estructura o dependencia visible en el submódulo fijado.
- **SOPORTADO-DOCUMENTACIÓN:** declaración de TI, CSA o IEEE identificada por versión.
- **FACTIBLE-CONDICIONAL:** arquitectura razonable, pero pendiente de norma, implementación o validación.
- **NO VALIDADO:** no hay ejecución ni evidencia experimental de la capacidad propuesta.
- **NO SOPORTADO/NO RESUELTO:** la evidencia disponible no permite aprobar el detalle.

Los informes anteriores se usaron como índice, no como sustituto del código. No se modificó ninguno.

## 3. Baseline verificado

| Elemento | Resultado verificado | Efecto en la revisión |
|---|---|---|
| Repositorio | `HEAD 59816be3460884481f2827eacb630d54cfff6ae8`, rama `feature/hardware-validation` | Commit posterior a `fc0de99`; añade únicamente la propuesta y su registro. |
| Diferencia desde `fc0de99b61cdb3d37701cdc625980b8882ed92b2` | Sólo dos documentos nuevos bajo `docs/Z-Stack_Reports/` | No cambió la implementación evaluada. |
| SDK | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` presente | No se sustituyó por otra instalación. |
| Commit SDK | `5b31d0a4903351e544546e23ef3330eaa4291ceb`, detached HEAD, limpio | Coincide con la propuesta. |
| Target | `CC1352P7`, `DeviceFamily_CC13X2X7` | `firmware/cc1352/CMakeLists.txt:12-18`; SysConfig identifica CC1352P7. |
| Toolchain configurado | GNU Arm: `arm-none-eabi-gcc/g++` | `firmware/cc1352/CMakeLists.txt:35-43`; no se comprobó instalación ni se compiló. |
| Árbol inicial | Limpio | La única creación de esta revisión es este documento. |

La discrepancia conocida se confirma: `source/ti/zstack/stack/nwk/nwk.c` **no existe**. El inventario visible contiene `nwk.h`, `nwk_bufs.h`, `nwk_globals.c`, `nwk_globals.h` y `nwk_util.h`. `NLDE_ParseMsg()` sólo está declarado en `nwk_util.h:319-321`; `APSDE_ParseMsg()` sólo en `aps_util.h:110-113`.

## 4. Veredicto de colocación: host frente a firmware

### 4.1 Suficiencia del host actual

El recorrido confirmado es:

```text
RF Core IEEE RX
  → cola RF: PHR + MPDU/FCS + RSSI + corr/CRC + RAT
  → RadioIF_processIeee154Packets()
       valida longitud, elimina PHR, descarta CRC-bad, conserva MPDU+FCS
  → cola RadioIF_RxPacket
  → LLManager_processRxPacket() (IEEE pasa sin semántica)
  → DataTask_emitRxPacket()
  → RSP_RX_PACKET sobre COBS/UART
  → Radio.read_packets()
  → Packet.data + timestamp/channel/RSSI/lqi/crc_ok
```

Evidencia: `firmware/cc1352/src/radio_if.c:1335-1371,1497-1568`; `firmware/cc1352/src/ll_manager.c:144-159`; `firmware/cc1352/src/data_task.c:28-65,76-139`; `python/feralrf/radio.py:18-30,636-709`.

La MPDU tiene límites suficientes para decodificación pasiva, pero el contexto no es autosuficiente:

- `Packet` no almacena `PHY`; `Radio` mantiene `_phy` internamente (`radio.py:113-120,455-466`).
- La presencia de FCS se deriva del backend actual, no de un campo de `Packet`.
- `lqi` son siete bits de correlación expuestos con ese nombre; no hay calibración Zigbee (`radio_if.c:1544-1548`).
- El timestamp es RAT/4 extendido a `uint64_t`, pero el origen sólo aporta 32 bits; no se demuestra continuidad, wrap ni wall clock (`radio_if.c:1538-1545`).
- `read_packets()` no verifica expresamente `13 + pkt_len <= len(payload)` antes del slice (`radio.py:675-703`). El firmware actual produce el layout correcto, pero la capa host no conserva la longitud declarada si una respuesta estuviera truncada.

Conclusión: **`Packet.data` es suficiente como buffer, pero `Packet` aislado no es un `RawCapture` completo**. `R-BASE-01` debe exigir contexto de sesión: PHY confirmado, política PHR/FCS, versión del contrato, fuente y opcionalmente identidad de firmware/captura.

### 4.2 Consecuencias de ejecutar el decoder en Python

Ventajas confirmables:

- no modifica COBS, comandos ni respuestas;
- no compite por el único RF Driver ni por buffers/tareas del CC1352P7;
- permite parsing acotado, tipos ricos, corpus, fuzzing y comparadores externos;
- conserva `Packet` y el consumo raw existente;
- evita ABI, OSAL, heaps y librerías TI no demostradas para GNU.

Costes y condiciones:

- CPU, RAM y backlog pasan al host; deben medirse con captura sostenida;
- no recupera drops en las colas RF/app/UART;
- estado pasivo no puede asumir una captura completa;
- una dependencia pesada o import con efectos laterales rompería la compatibilidad; el decoder debe ser opt-in y sus dependencias baseline deben limitarse a la biblioteca estándar o declararse expresamente;
- parsing host no aporta auto-ACK, CSMA, asociación, join ni respuesta temporal.

### 4.3 ¿Aporta ventaja el procesamiento embebido?

No hay ventaja demostrada para MAC/NWK/APS/ZDO/ZCL pasivos. El firmware sólo sería justificable si un requisito futuro necesitara una decisión dentro de la ventana RF, filtrado previo para reducir transporte, timestamping adicional o telemetría terminal correlacionada. Ninguno pertenece al baseline. Cualquier parser embebido aumentaría memoria, superficie de error y riesgo multi-PHY sin mejorar la exactitud del análisis offline.

### 4.4 Condiciones para reconsiderar la decisión

Reconsiderar sólo si se demuestra al menos una de estas condiciones:

1. el throughput host pierde sistemáticamente las tramas necesarias y no puede resolverse con buffering/telemetría;
2. aparece un requisito temporal imposible después del transporte UART;
3. se necesita filtrar o resumir en el dispositivo por una restricción cuantificada;
4. una función requiere estado RF que no puede exponerse sin firmware;
5. se aprueba una nueva revisión del contrato host y se dispone de presupuesto medido de flash/RAM/stack.

## 5. Revisión de límites de componentes

Los nombres de la propuesta son conceptuales. La separación por capa es razonable, pero faltan responsabilidades transversales y no es necesario que cada responsabilidad sea un paquete separado.

| Componente propuesto | Contrato y ownership recomendados | Dependencias/contexto | Veredicto |
|---|---|---|---|
| `zigbee.model` | Modelos inmutables que referencian un único buffer owned por `RawCapture`; spans absolutos `[inicio, fin)`; issues y classification | Ninguna capa protocolaria | **ACCEPT WITH CHANGES:** integrar aquí o junto a un `core` el cursor bounded y el contexto de captura. |
| `zigbee.pipeline` | `RawCapture + SchemaProfile → DecodeResult`; decide elegibilidad, dispatch y stops; nunca muta bytes | classifier, parsers, security gates | **ACCEPT WITH CHANGES:** debe poseer clasificación Zigbee explícita, no sólo orquestación. |
| `zigbee.mac` | MPDU+FCS policy → header/payload/FCS spans; genérico IEEE 802.15.4 | revisión IEEE, contexto PHY | **ACCEPT WITH CHANGES:** renombrar conceptualmente a límite IEEE, porque no es Zigbee. |
| `zigbee.nwk` | MAC payload elegible → NWK header, opcionales, security boundary y payload | profile Zigbee y classifier | **ACCEPT.** Mantener separado. |
| `zigbee.aps` | NWK plaintext data payload → APS context/security boundary/ASDU | resultado NWK y profile | **ACCEPT.** Mantener separado; no prometer APS service. |
| `zigbee.zdo` | ASDU ZDO plaintext + cluster/TSN/contexto → registro seleccionado u opaque | APS profile/endpoint/cluster | **ACCEPT WITH CHANGES:** schemas exactos y política status/count/length. |
| `zigbee.zcl` | ASDU aplicación plaintext + perfil/cluster/endpoints → header y, si aplica, foundation payload | registro de perfiles ZCL y tipos | **ACCEPT WITH CHANGES:** separar lógicamente header de payload/schema, aunque puedan residir juntos. |
| `zigbee.schemas` | Perfil normativo inmutable: revisiones, feature flags, APS profile allowlist, ZDO/ZCL commands y tipos | aprobación normativa | **ACCEPT WITH CHANGES:** no debe mezclar schemas normativos con estado observado. |

Componentes/roles faltantes:

- **capture adapter/context:** une `Packet` con PHY, FCS/PHR policy, fuente y revisión del contrato;
- **bounded cursor/reader:** lectura little-endian, remaining, subspans y error de offset común;
- **protocol classifier:** produce `confirmed_by_context`, `structurally_consistent`, `ambiguous`, `invalid_for_profile` o `not_eligible`;
- **security boundary:** responsabilidad explícita en MAC, NWK y APS; no sólo un módulo condicional de descifrado;
- **validation adapters:** fuera del runtime; convierten PCAP/fixtures/resultados del comparador sin contaminar los parsers.

Modelo mínimo recomendado:

```text
core = capture context + immutable raw/model + bounded cursor + issues + profile
pipeline = classifier + dispatch
ieee802154 = MAC/FCS
zigbee = NWK + APS + security boundaries
application = ZDO + ZCL schemas
validation = corpus/comparator adapters (sólo pruebas)
```

Esto conserva límites protocolarios sin obligar a ocho paquetes físicos.

## 6. Flujo de datos corregido

```text
Packet + estado de sesión/provenance
  → CaptureAdapter
      ├─ exige PHY=IEEE_802_15_4
      ├─ fija PHR ausente / FCS presente
      └─ conserva timestamp, channel, RSSI, corr/LQI, crc_ok y raw
  → IEEE 802.15.4 MAC parser
      ├─ frame no Data / sin payload / revisión no soportada → resultado raw; STOP
      ├─ MAC security presente y payload no autenticado → metadata+ciphertext; STOP
      └─ payload span
  → Zigbee classifier
      ├─ contexto explícito + estructura consistente → confirmed_by_context
      ├─ sólo estructura consistente → structurally_consistent
      ├─ evidencia insuficiente/ambigua → raw candidate; no semántica afirmativa
      └─ inconsistente → invalid_for_profile; STOP
  → NWK parser
      ├─ header base + opcionales según FCF
      ├─ NWK security → security header + ciphertext/MIC; STOP sin plaintext
      ├─ frame NWK command → resultado NWK command/raw; no APS
      └─ NWK data plaintext
  → APS parser
      ├─ data / command / ACK según frame control
      ├─ APS security → security header + ciphertext/MIC; STOP sin plaintext
      ├─ fragmento no reensamblado → metadata+fragment raw; STOP application
      └─ APS data plaintext + endpoints/group/cluster/profile/counter
  → application dispatch
      ├─ profile=0x0000 y endpoints ZDO coherentes → ZDO selected schema
      ├─ profile en allowlist ZCL y frame type foundation → ZCL header/payload
      └─ contexto desconocido/manufacturer/cluster-specific → header+opaque raw
```

Los límites MPDU/FCS se fijan en `RadioIF_processIeee154Packets()`; los límites MAC dependen de FCF, address modes, PAN compression, sequence suppression, auxiliary security e IEs según la revisión IEEE aprobada. NWK optional fields cambian offsets y deben consumirse sólo conforme a sus flags (`nwk_util.h:111-155`). APS cambia su layout por frame type, delivery mode y extended header (`aps_mede.h:91-124`). Ninguna capa debe calcular offsets desde una longitud fija.

## 7. Matriz de decisiones sobre requisitos baseline

| Decisión/Requisito | Propuesta actual | Evidencia | Veredicto | Cambio requerido | Dependencia de validación |
|---|---|---|---|---|---|
| `R-BASE-01` | Adapter de MPDU+FCS/metadata | `radio.py:18-30,673-707`; `radio_if.c:1497-1568` | **ACCEPT WITH CHANGES** | Incluir PHY/provenance/contract version; no afirmar que `Packet` solo lo contiene. | Fixtures de adapter y `V-RAW-01` para claims físicos. |
| `R-MAC-01` | MAC bounded | MPDU conservada; norma exacta pendiente | **ACCEPT WITH CHANGES** | Definir revisión/feature set; security gate MAC; frames no Data; PAN/seq/IE policy. | `V-MAC-01` normativo y negativos. |
| `R-NWK-01` | Header y opcionales NWK | `nwk_util.h:111-155`; parser TI ausente | **ACCEPT WITH CHANGES** | Anteponer clasificación conservadora y fijar protocol version; separar data/command. | Perfil R22 y `V-NWK-APS-01`. |
| `R-APS-01` | Frames/types/delivery/security | `aps_mede.h:91-124`; parser TI ausente | **ACCEPT WITH CHANGES** | Fijar layouts por frame type; fragmentos no pasan a app sin reassembly; security gate APS. | Corpus por layout y negativos. |
| `R-ZDO-01` | Seis mensajes | IDs/field order visibles en `zd_profile.h` y `zd_object.c` | **ACCEPT WITH CHANGES** | Añadir IDs, TSN, status-dependent lengths, counts, APS endpoint/profile context y malformed model. | Norma R22 + corpus detallado. |
| `R-ZCL-01` | Header general | `zcl.c:2412-2446`; `zcl.h:822-842` | **ACCEPT WITH CHANGES** | Requiere APS profile allowlist; manufacturer-specific queda opaque; bits reservados se reportan. | Headers 3/5 bytes, contexto válido/inválido. |
| `R-ZCL-02` | Foundation y tipos seleccionados | IDs/tipos visibles, cobertura no fijada | **ACCEPT WITH CHANGES** | Lista exacta de commands/types; dirección esperada; desconocido detiene records y preserva tail. | ZCL7 exacta y fixtures por command/type. |
| `R-SEC-01` | Metadata y stop ante ciphertext | Flags SDK; formato exacto pendiente de norma | **ACCEPT WITH CHANGES** | Aplicar también a MAC; distinguir `not_attempted`, `opaque`, `unsupported_security_level`; no prometer MIC length sin profile. | Vectores estructurales; no requiere claves para baseline. |
| `R-RAW-01` | Raw/spans en cada capa | `Packet.data` existente | **ACCEPT** | Aclarar lifetime/ownership y offsets absolutos. | Property tests de spans. |
| `R-ERR-01` | Issues tipados | Necesario por input no confiable | **ACCEPT WITH CHANGES** | Añadir `not_eligible`, `ambiguous_protocol`, `context_mismatch`; separar invalid/unsupported/truncated. | Todos los corpus negativos. |
| `R-VAL-01` | Corpus + comparator + norma | Comparadores son secundarios | **ACCEPT WITH CHANGES** | La norma/expected fixtures son autoridad; comparator sólo detecta divergencias. | Versiones, hashes y provenance. |

No se elimina ningún requisito baseline. Todos salvo raw preservation necesitan precisión documental; esto confirma que el scope es apropiado pero aún no es una especificación aprobable tal como está.

## 8. Evaluación del subconjunto ZDO

ZDO se identifica por contexto APS, no por inspeccionar únicamente el primer byte del payload. El SDK define `ZDO_EP=0` y `ZDO_PROFILE_ID=0` (`stack/zdo/zd_profile.h:86-87`). Z-Stack separa el primer byte como `TransSeq` antes de entregar `asdu` a los `ZDO_Parse*` (`stack/zdo/zd_profile.c:2104-2106`). Un decoder host debe modelar esa separación explícitamente.

| Mensaje | Cluster ID | Evidencia TI visible | Campos candidatos después del TSN | Restricción crítica | Decisión |
|---|---:|---|---|---|---|
| `Device_annce` | `0x0013` | `zd_profile.h:182`; `ZDO_ParseDeviceAnnce`, `zd_object.c:3607-3618` | NWK address `u16`, EUI-64 `8`, capability `u8` | ASDU fijo 11 bytes según fuente; no tiene respuesta. Útil pasivamente. | Mantener. |
| `NWK_addr_rsp` | `0x8000` | `zd_profile.h:164,171`; `ZDO_ParseAddrRsp`, `zd_object.c:3005-3057` | status; si success: EUI-64, NWK address; opcional count, start index, lista `u16` | Base candidata 11 bytes; si count>0 exigir `13 + 2×count`. TI no valida todo el remanente. | Mantener. |
| `IEEE_addr_rsp` | `0x8001` | `zd_profile.h:165,172`; misma función | Mismo wire candidate que address response | La semántica depende del request/cluster, no de la función común. | Mantener. |
| `Node_Desc_rsp` | `0x8002` | `zd_profile.h:166,173`; `ZDO_ParseNodeDescRsp`, `zd_object.c:3070-3101` | status, NWK address; en success, node descriptor | La fuente consume 13 bytes de descriptor; reglas exactas de status/longitud requieren R22. | Mantener condicional a norma. |
| `Simple_Desc_rsp` | `0x8004` | `zd_profile.h:168,175`; `ZDO_ParseSimpleDescRsp/Buf`, `zd_object.c:3147-3160,3543-3594` | status, NWK address, descriptor length; endpoint, profile, device, version, in/out cluster lists | `descriptor_length` debe concordar con `8 + 2×in_count + 2×out_count`; counts no pueden exceder bytes. | Mantener. |
| `Active_EP_rsp` | `0x8005` | `zd_profile.h:169,176`; `ZDO_ParseEPListRsp`, `zd_object.c:3172-3194` | status, NWK address, endpoint count, endpoint list | Longitud candidata `4 + count`; TI copia sin bounds. | Mantener. |

Las longitudes anteriores son **candidatos confirmados por la fuente TI**, no sustituyen la definición normativa R22. En status de error, el decoder sólo debe consumir campos que la norma aprobada exija; no debe extrapolar el comportamiento permisivo de TI.

Coherencia del subset:

- `Device_annce` aporta la observación pasiva más directa de short↔EUI-64 y capability.
- Las respuestas de address/descriptor/endpoints son valiosas sólo cuando aparecen en la captura; FeralRF no las solicita por sí mismo.
- `Active_EP_rsp` y `Simple_Desc_rsp` forman una pareja analítica coherente; `Node_Desc_rsp` complementa capacidades del nodo.
- No hace falta agregar requests para parsear una respuesta, pero la correlación fiable requiere observar el request y usar TSN, cluster, direcciones y tiempo. Esa correlación pertenece a `C-OBS-01`, no al parser baseline.
- Para el corpus mínimo sí deben incluirse fixtures de los requests correspondientes, al menos como contexto raw, para comprobar que una respuesta no se correlaciona sólo por TSN.

Representación malformed recomendada: conservar cluster, TSN si existe, campos completos previos, span restante y un issue con offset/required/available; no rellenar listas ni descriptores faltantes con ceros.

Corpus mínimo por mensaje: caso válido, truncación en cada frontera, trailing bytes, status no-success cuando aplique; además count 0/1/máximo que cabe/count excesivo para listas, y combinaciones de in/out counts para simple descriptor.

## 9. Evaluación del subconjunto ZCL

### 9.1 Header

`zclParseHdr()` confirma el layout de 3 bytes o 5 bytes manufacturer-specific: frame control, manufacturer code opcional little-endian, TSN y command ID (`zcl.c:2412-2446`; `zcl.h:822-842`). La función no recibe longitud; sólo es referencia algorítmica.

El header no basta para declarar ZCL. Antes se requiere:

- APS data plaintext;
- profile ID incluido en un allowlist aprobado;
- endpoints/cluster conservados;
- frame type ZCL foundation (`0x00`) para usar los schemas siguientes;
- payload manufacturer-specific opaco salvo schema específico.

### 9.2 Subset foundation mínimo recomendado

| Command | ID | Payload candidate visible en SDK | Dirección esperada | Veredicto |
|---|---:|---|---|---|
| Read Attributes | `0x00` | lista de attribute IDs `u16` | normalmente client→server | Mantener. |
| Read Attributes Response | `0x01` | records: attr ID, status; si success, datatype+value | normalmente server→client | Mantener. |
| Write Attributes | `0x02` | records: attr ID, datatype, value | normalmente client→server | Mantener. |
| Write Attributes Response | `0x04` | status records y semántica especial de success | normalmente server→client | Mantener, sujeto a regla exacta ZCL7. |
| Report Attributes | `0x0A` | records: attr ID, datatype, value | depende del lado que reporta | Mantener; no imponer dirección sin profile. |
| Default Response | `0x0B` | command ID respondido + status | opuesta al command originador | Mantener. |
| Discover Attributes | `0x0C` | start attribute `u16` + max IDs `u8` | normalmente client→server | Mantener. |
| Discover Attributes Response | `0x0D` | discovery complete + records attr ID/datatype | normalmente server→client | Mantener. |

IDs: `stack/zcl/zcl.h:509-557`. Estructuras candidatas: `zcl.h:844-899,971-1016`. La dirección es contexto de validación, no razón suficiente para reinterpretar el command ID. Hasta disponer de ZCL7 exacta, una dirección inesperada debe ser `context_mismatch` y no una mutación del payload.

No se incluyen `Write Attributes Undivided`, `Write Attributes No Response`, reporting configuration, discover commands ni cluster-specific commands. Son expansiones futuras, no huecos del baseline.

### 9.3 Tipos de atributo

La propuesta debe reemplazar “tipos escalares y strings cubiertos por el schema” por un allowlist explícito. El mínimo útil recomendado es:

- `NO_DATA (0x00)`;
- `BOOLEAN (0x10)`;
- `BITMAP8/16/32/64 (0x18,0x19,0x1B,0x1F)`;
- `UINT8/16/32/64 (0x20,0x21,0x23,0x27)`;
- `INT8/16/32/64 (0x28,0x29,0x2B,0x2F)`;
- `ENUM8/16 (0x30,0x31)`;
- `OCTET_STR/CHAR_STR (0x41,0x42)` con prefijo `u8` y sentinel/null definido por ZCL7;
- `CLUSTER_ID/ATTR_ID (0xE8,0xE9)` e `IEEE_ADDR (0xF0)`.

Los anchos 24/40/48/56, floats, long strings, array/structure/set/bag, time/date y security key deben quedar fuera del primer subset o representarse sólo como fixed raw cuando la norma aprobada permita determinar inequívocamente la longitud. `zclGetDataTypeLength()` demuestra tamaños para múltiples escalares (`zcl.c:3195-3283`), pero devolver cero mezcla no-data, variable y unsupported. `zclGetAttrDataLength()` lee prefijos sin remaining (`zcl.c:3296-3322`). Por ello no se deben portar sus firmas.

Ante datatype desconocido no es seguro buscar el siguiente record: el decoder conserva desde el datatype hasta el final como raw, emite `unsupported_data_type` y detiene el payload command. Strings cuya longitud exceda remaining producen `truncated`; caps defensivos deben ser explícitos.

Fixtures mínimos: cada command en ambos sentidos observables, header 3/5, manufacturer flag, listas vacías/no vacías, success/error records, cada tipo permitido en valores frontera, string 0/máximo que cabe/sentinel/truncada, unknown datatype y trailing bytes.

## 10. Estrategia conservadora de identificación Zigbee

El backend recibe todas las tramas aceptadas por RF porque filtering y auto-ACK están desactivados (`smartrf_ieee_15_4_0.c:95-125`; runtime en `radio_if.c:1347-1355`). No hay discriminador Zigbee en `Packet`.

Clasificación recomendada:

| Estado | Significado permitido | Regla mínima |
|---|---|---|
| `confirmed_by_context` | El origen/corpus/sesión declara Zigbee y la estructura es consistente | Requiere provenance explícito externo; el canal IEEE por sí solo no basta. |
| `structurally_consistent` | Bytes compatibles con el profile Zigbee seleccionado | MAC Data elegible, NWK protocol version/FC válido, opcionales caben y APS/command layout no contradice el profile. No prueba origen. |
| `ambiguous` | Más de una interpretación o contexto insuficiente | Conservar candidatos/raw; no emitir “trama Zigbee confirmada”. |
| `invalid_for_profile` | Pretende evaluarse bajo un profile, pero viola una regla estructural | Issue exacto y stop en la última capa segura. |
| `not_eligible` | MAC frame type, seguridad, revisión o ausencia de payload impiden intentar NWK | Resultado IEEE raw, sin error Zigbee falso. |

El parser debe detenerse cuando:

- la trama MAC no sea Data o su payload no esté disponible;
- la revisión MAC/IE/PAN compression no esté soportada;
- MAC security o NWK security oculten la siguiente capa sin plaintext autenticado;
- NWK protocol version/reserved bits/optional lengths contradigan el profile;
- un NWK command sea interpretado erróneamente como APS;
- APS command/ACK, fragmento no reensamblado o APS security impidan application dispatch;
- profile APS no esté en el registro ZDO/ZCL aprobado.

La clasificación estructural puede producir un parse tentativo para análisis, siempre que el resultado conserve el grado de evidencia y nunca lo eleve a “confirmed” por apariencia de bytes.

## 11. Revisión de reuso Z-Stack

| Candidato | Clasificación revisada | Decisión |
|---|---|---|
| `zclParseHdr()` | Algoritmo visible, pequeño, sin bounds | Preferir reimplementación especificación-driven; usar como cross-check. Adaptarlo literalmente sólo tras revisión de licencia/provenance. |
| `zclBuildHdr()` | Serializer `static`, sin capacity | Fuera del baseline; referencia sólo para builders condicionales. |
| `zclGetDataTypeLength()` | Tabla visible; cero ambiguo | Reexpresar como schema `{fixed, variable, unsupported}`; no copiar la API. |
| `zclGetAttrDataLength()` | Regla visible, lecturas no acotadas | Reimplementar con cursor+remaining; no adaptar firma. |
| `zclSerializeData()` | Visible, acoplado a layouts C y sin capacity | Fuera baseline; referencia condicional. |
| `ZDO_Parse*` seleccionados | Field order visible, ownership/bounds inseguros | Usar como evidencia secundaria; implementación Python independiente desde norma. |
| NWK/APS definitions | Máscaras/structs visibles; parsers ausentes | Útiles para cross-check, no sustituyen la especificación. |
| ZMAC/TIMAC | Wrappers `MAP_MAC_*`, OSAL/callbacks y binarios | Rechazado para integración selectiva; no equivale al RF backend actual. |

`zcl.c:9-37` usa términos permisivos tipo BSD-3-Clause, pero `zd_object.c:9-63`, `nwk_util.h:9-63` y `aps.h:9-63` contienen licencia limitada a TI Devices. FeralRF es GPL-3.0 (`python/pyproject.toml:5-11`). No se emite conclusión legal; la ruta de menor riesgo técnico y de provenance es implementar desde la especificación autorizada y citar TI como comparación. “Visible” no significa “portable” ni “copiable sin revisión”.

La ausencia de `nwk.c` refuerza la decisión de no afirmar reuso de un parser NWK TI. `ZMacInit()` y association/data delegan a `MAP_MAC_*` (`zmac.c:506-519,865-917,1160-1203`); callbacks usan ownership y colas OSAL (`zmac_cb.c:165-240`). No hay servicio equivalente dentro de FeralRF.

## 12. Compatibilidad con FeralRF actual

| Elemento preservado | Soporte de fuente | Condición pendiente |
|---|---|---|
| `Radio` y `Packet` | Decoder puede envolver `Packet` sin alterar métodos (`radio.py:70-120`) | Adapter debe recibir contexto PHY/FCS fuera de `Packet`. |
| Transporte COBS | Parsing ocurre después de `read_packets()` (`protocol.py:11-90`) | Ningún mensaje Zigbee nuevo en baseline. |
| Firmware CC1352P7/RF ownership | No hay cambio embedded | Confirmar por revisión de diff y regresión de paquete Python. |
| Raw RX/TX | Decoder opt-in no cambia bytes ni llamadas | No capturar/importar automáticamente al crear `Radio`. |
| Otras PHY | Decoder rechaza contexto no IEEE | No inferir PHY por rango de canal. |
| KillerBee | Adapter actual consume `pkt.data` y permanece independiente (`integrations/killerbee.py:95-119`) | No importar KillerBee desde el decoder; dependencia sigue opcional/lazy. |
| Ejemplos existentes | Continúan usando `Packet.data` | Tests de imports/API y raw passthrough. |

Compatibilidad arquitectónica no equivale a compatibilidad validada. Deben probarse imports sin side effects, ausencia de nuevas dependencias obligatorias, igualdad de raw bytes, comportamiento de unknown frames, memoria/CPU host y backlog. `python/pyproject.toml:28-44` sólo declara pyserial, asyncio-serial y COBS como runtime; cryptography y KillerBee son opcionales.

El modelo debe preservar timestamp, channel, RSSI, correlación/LQI reportada, `crc_ok`, FCS policy y raw. No debe prometer exactitud física de timestamp/LQI ni pérdida cero. El decoder no debe consumir el iterador en segundo plano salvo opt-in explícito.

## 13. Auditoría de la estrategia de validación

| Validación | Claim que sí puede demostrar | Evaluación | Corrección requerida |
|---|---|---|---|
| `V-RAW-01` | Layout RX/FCS y bytes TX observados OTA para build/setup identificados | **INCOMPLETE/MISALIGNED** | Separar accepted, async failure, OTA observed, MAC ACK y application response. La API no ofrece RF-completed correlacionado. |
| `V-MAC-01` | Correctitud del subset MAC offline | **SUFFICIENT IN PRINCIPLE** | Añadir eligibility, MAC security, FCS policy, PAN/seq/IE combinations y non-Zigbee negatives. |
| `V-NWK-APS-01` | Headers/opcionales/stops NWK/APS | **INCOMPLETE** | Incluir classifier, NWK command vs data, APS command/ACK, fragment stop y ambigüedad. |
| `V-ZDO-01` | Seis schemas bounded | **INCOMPLETE** | Añadir TSN, APS context mismatches, status success/error, declared lengths y request-context fixtures. |
| `V-ZCL-01` | Header y foundation subset | **INCOMPLETE** | Fijar commands/types, allowlist APS, direction, manufacturer-specific y unknown datatype stop. |
| `V-SEC-01` | No false plaintext y spans de security | **INCOMPLETE hasta norma** | Separar baseline structural de futuro decryption; incluir security MAC/NWK/APS y niveles desconocidos. |
| `V-COMP-01` | Divergencias con otro decoder | **SUFFICIENT AS SECONDARY CHECK** | Fijar release/config/DLT/FCS policy; nunca aceptar por mayoría frente a norma. |
| `V-BLD-01` | Round-trip y bytes OTA de builders | **DEFER** | No usar hasta aprobar `C-BLD-01`; no demuestra delivery Zigbee. |
| `V-RF-01` | Timing/estado de una primitiva embedded aprobada | **DEFER/UNRESOLVED** | Requiere nueva interfaz terminal correlacionada o instrumentos externos y requisito temporal exacto. |

### 13.1 Corrección obligatoria de TX completion

La secuencia real es:

```text
Radio.transmit[_frame]()
  → CommandProcessor valida y encola
  → RSP_ACK inmediato                         = accepted
  → DataTask/ControlTask ejecuta RF después
  → RF_runCmd termina internamente            = completed interno, no correlacionado al host
  → sólo si falla: RSP_ERROR asíncrono seq=0  = failure genérico tardío
  → sniffer independiente observa trama       = observed OTA
  → posible ACK MAC                           = acknowledged, no implementado/esperado por API
  → posible respuesta app                     = responded
```

Evidencia: `command_processor.c:210-258`; `control_task.c:332-357`; `radio_if.c:324-381`; `radio.py:725-750`. El ACK host no prueba RF completed. `V-RAW-01` debe aceptar únicamente:

1. `accepted`: ACK host;
2. `no_async_failure_within_window`: observación negativa débil, no success;
3. `observed_ota`: captura externa byte-exacta;
4. `completed`: **blocked** con la API actual, salvo que un instrumento externo permita definirlo operacionalmente sin atribuirlo a telemetría FeralRF.

No se requiere cambiar firmware para validar el decoder pasivo. Sí se requeriría una interfaz futura si el claim fuera “terminal completion reportado por FeralRF”.

## 14. Revisión de versionado normativo

Mantener provisionalmente **Zigbee PRO 2017/R22 + ZCL7**, porque TI declara esa alineación para Z-Stack 8.30 en las release notes oficiales registradas como TI-D04. El SDK también marca stack compliance rev. 22 en `stack/zdo/zd_object.h:90-95`.

La selección sigue **UNRESOLVED para implementación** hasta obtener y aprobar:

- documento Zigbee Core exacto R22/R22.1 aplicable;
- ZCL revision 7 exacta;
- revisión IEEE 802.15.4 referenciada por ese profile, incluidas reglas FCF/PAN compression/IE/security;
- schemas/errata de los mensajes ZDO y comandos ZCL seleccionados;
- política de coexistencia de captures R22/R23 y cómo se declara `unsupported_revision`.

IEEE 802.15.4-2020 o 2024 no debe elegirse sólo por ser más reciente. El documento oficial IEEE identificado en el registro confirma la familia/revisión, pero la correspondencia exacta con R22 no está establecida localmente. CSA ofrece R22.1/R23.x y ZCL7/8; esa disponibilidad no autoriza mezclar revisiones.

Recomendación: retener el profile provisional, registrar por separado `mac_revision`, `zigbee_core_revision`, `zcl_revision` y `schema_set`, y bloquear implementación semántica donde falte el texto exacto. El supervisor debe aprobar el profile final.

## 15. Hallazgos críticos y dependencias no resueltas

1. **Contexto incompleto en `Packet`:** no hay PHY ni flag FCS. Es corregible mediante adapter/contexto de sesión sin firmware.
2. **Identificación Zigbee insuficientemente especificada:** “contexto/estructura aprobados” debe convertirse en un contrato de clasificación y stop.
3. **Security gate incompleto:** la propuesta dibuja NWK security, pero MAC security también puede ocultar el payload; APS security y fragmentación necesitan stops propios.
4. **Dispatch ZCL ambiguo:** falta un allowlist de APS profile IDs. Cluster ID por sí solo no basta.
5. **ZCL types abiertos:** impide estimar cobertura y diseñar corpus de aceptación.
6. **ZDO lengths/status:** el field order TI es útil, pero no normativo ni bounded; faltan reglas exactas por status.
7. **TX completion:** el criterio actual no es observable correlacionadamente por la API.
8. **Norma objetivo:** R22/ZCL7 es razonable, pero el texto exacto y la base IEEE aún no están disponibles/aprobados.
9. **Licencia/provenance:** debe decidirse reimplementación desde norma frente a adaptación; se recomienda la primera.
10. **Throughput/gaps:** no invalida parsing por frame, pero limita estado/topología y debe medirse antes de `C-OBS-01`.

## 16. Correcciones recomendadas a la propuesta existente

Sin modificarla en esta revisión, se recomienda que una revisión posterior:

1. cambie `RawCapture` para requerir `CaptureContext` con PHY/FCS/provenance;
2. agregue `R-ID-01` o incorpore en `R-NWK-01` la clasificación conservadora definida aquí;
3. actualice el diagrama con security gates MAC/NWK/APS, frame-type dispatch y fragment stop;
4. defina el ownership/lifetime del buffer y cursor bounded común;
5. añada APS profile allowlist al `SchemaProfile`;
6. detalle IDs, TSN, status/count/length de los seis mensajes ZDO;
7. enumere exactamente ocho commands ZCL foundation y el allowlist de tipos inicial;
8. separe parsing de header ZCL de payload foundation y deje cluster-specific fuera;
9. cambie `V-RAW-01` para no exigir terminal completion a la API actual;
10. amplíe `V-NWK-APS-01` con tráfico IEEE no Zigbee, ambigüedad y stops;
11. declare que comparator es detector secundario, no oráculo;
12. conserve R22/ZCL7 como provisional y bloquee schemas hasta aprobación normativa.

## 17. Decisiones para aprobación del supervisor

| Pregunta | Recomendación | ¿Bloquea implementación baseline? |
|---|---|---|
| ¿Se acepta host-side raw-first como arquitectura? | Sí, con las correcciones de contexto/classification. | Sí. |
| ¿Se acepta que “integración en FeralRF” sea un módulo opt-in del paquete Python que consume `Packet`? | Sí; no es un decoder externo si comparte modelos, profile, packaging, pruebas y provenance FeralRF. | Sí. |
| ¿Qué profile normativo se adopta? | R22/ZCL7 alineado al SDK, con base IEEE exacta por confirmar. | Sí para schemas. |
| ¿Qué perfiles APS habilitan ZCL? | Aprobar allowlist inicial; no inferir por cluster ID. | Sí para `R-ZCL-01/02`. |
| ¿Se aprueba el subset ZDO de seis mensajes? | Sí, con IDs y reglas bounded; correlation queda condicional. | Sí para ZDO. |
| ¿Se aprueba el subset ZCL exacto? | Ocho foundation commands y allowlist de tipos propuesto. | Sí para ZCL. |
| ¿Se copia/adapta TI o se reimplementa desde norma? | Reimplementación independiente; TI como cross-check. | Sí por licencia/provenance. |
| ¿Se necesita estado pasivo, decryption, builders o RF embedded en primera entrega? | No; mantenerlos diferidos. | No. |
| ¿Se exige completion TX reportado por FeralRF? | No para baseline; si se exige después, aprobar cambio de interfaz separado. | No para decoder; sí para claim TX terminal. |
| ¿Qué evidencia constituye aceptación? | Corpus normativo + negativos + comparator fijado; OTA sólo para contrato raw/TX. | Sí para validación final. |

## 18. Conclusión final

### 18.1 Idoneidad arquitectónica

**ADECUADA CON CAMBIOS.** El pipeline host pasivo usa una frontera real de FeralRF, preserva el firmware único y evita dependencias operacionales Z-Stack. Es una integración legítima si se entrega como componente opt-in del paquete FeralRF y conserva `Packet`, raw bytes, metadatos, profiles y errores; no sería legítima si se publica como herramienta ajena sin contrato con FeralRF.

### 18.2 Factibilidad de implementación

**ALTA PERO CONDICIONAL** para parsing bounded MAC/NWK/APS, seis mensajes ZDO, header ZCL y ocho foundation commands. Depende de obtener la norma exacta, cerrar schemas, definir contexto/classification y decidir provenance/licencia. No hay fundamento para integrar TIMAC/ZMAC, servicios MAC operacionales ni el runtime Z-Stack.

### 18.3 Estado de validación

**NO VALIDADO.** La fuente confirma el transporte raw y los límites arquitectónicos; la evidencia histórica confirma sólo intercambios IEEE raw acotados. Ningún decoder propuesto, schema, identificación Zigbee, seguridad estructural o interoperabilidad ha sido ejecutado. No se realizó ninguna prueba en esta revisión.

### 18.4 Recomendación de aprobación

**REVISE BEFORE APPROVAL.** Aceptar la dirección y el alcance, pero corregir primero el contrato de captura, identificación protocolaria, security gates, dispatch ZCL, schemas exactos y semántica de validación TX. Tras esas correcciones documentales, la arquitectura puede presentarse al supervisor sin rediseño completo. Estado pasivo, decryption, builders y primitivas embedded deben continuar como ramas condicionales separadas.

## 19. Referencias primarias usadas

- Propuesta: `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md:1-581`.
- Registro: `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md:1-225`.
- Assessment nativo: `docs/FERALRF_NATIVE_ZIGBEE_CAPABILITY_ASSESSMENT.md:68-252`.
- Python: `python/feralrf/radio.py:18-30,70-120,455-478,599-750`; `python/feralrf/protocol.py:11-90`; `python/feralrf/integrations/killerbee.py:1-119`; `python/pyproject.toml:5-44`.
- Firmware RX/TX: `firmware/cc1352/src/command_processor.c:188-258`; `control_task.c:332-357,440-454`; `data_task.c:28-139`; `ll_manager.c:40-159`; `radio_if.c:324-381,457-488,1335-1397,1497-1568`; `smartrf_ieee_15_4_0.c:81-163`.
- ZCL: `source/ti/zstack/stack/zcl/zcl.c:2412-2482,2961-3076,3195-3322`; `zcl.h:490-652,822-1016` bajo el SDK fijado.
- ZDO: `source/ti/zstack/stack/zdo/zd_profile.h:86-87,154-186,229-241`; `zd_profile.c:2104-2106`; `zd_object.c:3005-3194,3543-3618`; `zd_object.h:90-145,200-205`.
- NWK/APS: `source/ti/zstack/stack/nwk/nwk_util.h:111-170,319-321`; `aps_mede.h:91-124,288-308`; `aps_util.h:110-113`.
- MAC TI: `source/ti/zstack/zmac/f8w/zmac.c:506-519,865-917,1160-1218`; `zmac_cb.c:165-240`.
- Documentación oficial versionada y URLs: IDs TI-D01–D06, IEEE-D01 y CSA-D01–D03 en `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md:137-155`.

