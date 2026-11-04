# Propuesta técnica revisada para integrar decodificación Zigbee selectiva en FeralRF

**Revisión:** 1.2
**Fecha:** 2026-11-04
**Repositorio verificado:** `f0a64ef9a00438757d474fd037c0d1847e6596e4` (`feature/hardware-validation`)
**SDK fijado:** `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`
**Target:** CC1352P7
**Registro de evidencia:** [FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md](FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md)
**Revisión arquitectónica incorporada:** [FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md](FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md)
**Revisión normativa y de licencias:** [FERALRF_ZIGBEE_NORMATIVE_AND_LICENSE_REVIEW_2026-11-04.md](zigbee_proposal/FERALRF_ZIGBEE_NORMATIVE_AND_LICENSE_REVIEW_2026-11-04.md)

## 1. Resumen ejecutivo

Se propone añadir al paquete Python de FeralRF una capacidad opt-in de decodificación Zigbee pasiva, con parsing acotado y perfiles de protocolo versionados. Consumirá los bytes recibidos por la ruta IEEE 802.15.4 existente y mantendrá el MPDU original y su provenance. La capacidad no requiere cambios en el firmware inicial, en COBS, en la API pública `Radio`/`Packet`, en RF Driver ni en otras PHY.

La revisión 1.2 conserva el enfoque host-side. Incorpora un solo parser NWK acotado antes de clasificar, separa el estado del parse de la evidencia de origen y sustituye `confirmed_by_context` por `verified_by_provenance`. `CaptureContext` lleva valor, fuente, época y nivel de evidencia; PHY solicitada/derivada del estado host no se presenta como confirmación del hardware. El ACK TX sigue significando aceptación/encolado, no finalización RF.

El baseline propuesto incluye `CaptureContext` conceptual; parsing MAC; una única lectura NWK; clasificación conservadora; NWK/APS plaintext bounded con límites de seguridad; seis mensajes ZDO; header ZCL y ocho foundation commands con tipos acotados. El Core objetivo es Zigbee R22 1.0 (`05-3474-22`), ZCL7 (`07-5123-07`) y IEEE 802.15.4-2015, referida explícitamente por R22 Annex D. `0x0000` + endpoint 0 se reserva a ZDO. Se recomienda `0x0104` como primer contexto APS para foundation ZCL genérica, sujeto a aprobación/versionado, plaintext, APS/ZCL context coherente y manufacturer-specific desactivado; no activa clusters HA. Sin contexto/profile autorizado se preserva ASDU raw, sin interpretación semántica.

La implementación preferida es independiente y basada en textos normativos autorizados. Las funciones ZCL y parsers ZDO visibles de TI son cross-checks de campo/algoritmo; sus firmas y ownership no son APIs Python seguras. `NLDE_ParseMsg()` y `APSDE_ParseMsg()` sólo están declaradas en el SDK fijado. No se integra ZMAC/TIMAC, AF, BDB, NV, seguridad operacional ni el runtime Z-Stack.

**Dictamen heredado de la revisión crítica:** arquitectura adecuada con cambios documentales; factibilidad de implementación condicional a normas, perfiles y fixtures; ninguna capacidad propuesta está implementada o validada. Esta versión es apta para evaluación del supervisor, conservando expresamente las decisiones pendientes.

## 2. Historial de revisión

| Revisión | Fecha | Cambios |
|---|---|---|
| 1.0 | 2026-11-03 | Propuesta raw-first basada en auditorías del código y SDK. |
| 1.1 | 2026-11-04 | Incorpora la revisión crítica: contrato `CaptureContext`, `R-ID-01`, security gates por capa, límites y TSN ZDO, subset ZCL explícito, política APS profile, TX completion corregido, y registro de licencias/provenance. |
| 1.2 | 2026-11-04 | Verifica HEAD actual y ausencia de cambios de firmware/Python desde el baseline anterior; unifica parser NWK y clasificación; define provenance/snapshot de contexto; fija Core R22 1.0, ZCL7 e IEEE 802.15.4-2015 con límites de acceso; incorpora hallazgos ZCL7 y evaluación TI archivo-por-archivo; recomienda `0x0104` condicionalmente; formaliza Scope A sin descifrado y riesgos de distribución. Nada se implementa ni valida. |

## 3. Objetivo, alcance y criterio de evidencia

### 3.1 Objetivo rector

Extender FeralRF para que el host pueda interpretar, con precisión declarada, un subconjunto pasivo de tramas IEEE 802.15.4 compatibles con Zigbee. La integración es un componente opt-in del paquete FeralRF que consume el `Packet` existente; no una herramienta externa desconectada ni una implementación de nodo Zigbee.

### 3.2 Restricciones y exclusiones

- FeralRF sigue siendo el único firmware y CC1352P7 el target.
- No se introduce una imagen/persona ZNP, ZED o Zigbee separada.
- No se reemplaza RadioIF/RF Driver por el runtime completo de Z-Stack.
- No se enlazan librerías binarias TIMAC/Z-Stack sin evidencia de compatibilidad con el target GNU actual.
- No se implementan joining, commissioning, association, auto-ACK, CSMA-CA, retries, routing, coordinator/router/end device, BDB, key lifecycle ni endpoint operacional.
- El baseline no descifra, construye ni transmite tramas Zigbee-aware; no afirma interoperabilidad ni conformidad.
- Estado pasivo persistente, decryption autenticado, builders/raw injection y primitivas RF temporales permanecen condicionales.

### 3.3 Estados de evidencia

**CONFIRMADO-CÓDIGO** identifica comportamiento visible en el checkout; **CONFIRMADO-SDK** identifica código/definición visible en el SDK fijado; **DOCUMENTADO-TI/CSA/IEEE** identifica una afirmación oficial versionada; **EVIDENCIA EXPERIMENTAL HISTÓRICA** es una observación limitada a su setup; **INFERENCIA ARQUITECTÓNICA** deriva de evidencia; **FACTIBLE-CONDICIONAL** requiere decisiones/implementación; **NO VALIDADO** indica que no se ha probado la capacidad propuesta; **NO SOPORTADO/NO RESUELTO** limita claims. Referencias cortas remiten al registro de fuentes.

## 4. Baseline verificado y reconciliación

El HEAD actual es `f0a64ef9a00438757d474fd037c0d1847e6596e`, rama `feature/hardware-validation`; el submódulo SDK es `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`. La comparación `59816be..HEAD` no muestra cambios en `firmware/` ni `python/`; se conservan los hallazgos del código anterior, sin afirmar equivalencia general de todo el árbol. Target CC1352P7; CMake configura GNU Arm (`arm-none-eabi-gcc/g++`), cuya versión instalada no se verificó. El árbol tenía documentos de investigación complementarios no rastreados; se preservan [BASE-01–08].

Se conserva la discrepancia del audit de fuentes: `source/ti/zstack/stack/nwk/nwk.c` no existe en el submódulo. `NLDE_ParseMsg()` aparece sólo como declaración en `nwk_util.h:319-321`; `APSDE_ParseMsg()` sólo como declaración en `aps_util.h:110-113`. No se les atribuye una implementación reusable [DISC-01, SDK-Z11–14].

## 5. Arquitectura FeralRF existente

### 5.1 Responsabilidades y frontera de extensión

| Componente | Runtime | Comportamiento actual | Uso propuesto |
|---|---|---|---|
| `Radio`, `Packet` | PC/Python | Control RX/TX y objeto recibido (`radio.py:18-30,70-120,599-750`) | Entrada opt-in; no se cambia su API pública. |
| `protocol.py` / `protocol.c` | Python + CC1352P7 | COBS y CRC16 del transporte | Sin cambios; no es framing Zigbee. |
| `CommandProcessor`, `ControlTask` | CC1352P7 | Valida/encola comandos; ACK puede anteceder ejecución RF | Sin nueva semántica en baseline. |
| `DataTask` | CC1352P7/TI-RTOS | Drena RX y emite `RSP_RX_PACKET`; hasta ocho por poll | Mantiene la ruta raw actual. |
| `PhyManager`, `LLManager` | CC1352P7 | IEEE pasa sin parser MAC/Zigbee; sólo BLE se clasifica | No se añade parsing embedded. |
| `RadioIF` | CC1352P7 | Único owner de RF Driver, modos, colas, buffers y callbacks | Sin cambio en baseline. |
| RF Core/SmartRF | CC1352P7 | RX IEEE incluye PHR/FCS y metadata; TX genera PHR/FCS | Provee el bearer raw, no servicio MAC completo. |
| Decodificador conceptual | Host Python | **PROPOSED — NOT IMPLEMENTED** | Parser opt-in con schemas y resultados preservando raw. |

### 5.2 Ruta RX/TX confirmada

```text
RX: RF Core → RadioIF queue → RadioIF_poll/processIeee154Packets
    → RadioIF_RxPacket → LLManager (IEEE pass-through) → DataTask
    → COBS/UART → Radio.read_packets() → Packet.data

TX: Radio.transmit/transmit_frame → COBS command → CommandProcessor ACK/queue
    → ControlTask/DataTask → RadioIF_transmitIeee154Raw → RF Core
```

RX solicita PHR, FCS, RSSI, correlación y RAT (`radio_if.c:1335-1355`); firmware valida longitud, elimina PHR, descarta CRC-bad y expone MPDU con FCS (`radio_if.c:1497-1568`). El TX IEEE acepta MPDU sin FCS, 1–125 bytes, y RF Core genera PHR/FCS (`radio_if.c:468-488`; `smartrf_ieee_15_4_0.c:146-163`). `TX_FRAME` no construye una trama MAC.

La cola RF tiene 3 entradas y la cola de aplicación 8; el polling limita el drenaje. Filtering, auto-ACK y CCA están deshabilitados en SmartRF (`smartrf_ieee_15_4_0.c:95-125`). RX/TX no son servicio MAC operacional. La API host no expone completion RF correlacionado: ACK significa aceptado/encolado (`command_processor.c:188-258`; `control_task.c:332-357`; `radio.py:725-750`).

## 6. Contrato de captura y propiedad de bytes

`Packet.data` contiene los bytes recibidos, pero `Packet` no incluye PHY ni un flag FCS dedicado. `Radio` conserva su PHY seleccionada internamente; eso no equivale a que un objeto `Packet` autocontenga ese contexto. El backend determina PHR/FCS policy. Timestamp y `lqi` también tienen limitaciones documentadas [FW-H01–07, FW-F12–15].

### 6.1 Contratos conceptuales

Todos los nombres de esta tabla son **PROPOSED — NOT IMPLEMENTED**.

| Modelo | Campos/contrato conceptual | Obtención y límite |
|---|---|---|
| `CaptureContext` | Para cada valor: value, evidencia, source, capture/session identity y epoch; provenance `declared_by_caller`, `derived_from_session`, `verified_from_source` o `unknown`; PHY; PHR/FCS/CRC policy; firmware/build identity opcional; fuente/unidad de timestamp y metadatos | Snapshot al recibir/encolar cada packet; valor derivado de `Radio` nunca se rotula hardware-confirmed. `Packet` actual no expone PHY/FCS policy ni epoch RF por paquete. No añadir método/campo público en baseline. |
| `RawCapture` | Referencia inmutable a `Packet.data`, `CaptureContext`, metadata original y policy aprobada | El input owner sigue siendo `Packet`/caller; wrapper no altera bytes. Si el lifetime no está garantizado, conserva una copia inmutable en el host. |
| `LayerResult` | Campos tipados, clasificación, status, raw reference, spans absolutos `[start,end)`, payload restante y issues | Los spans siempre se miden desde el inicio de `Packet.data`, nunca desde un buffer temporal sin conversión. |
| `DecodeIssue` | Layer, code, severity, absolute offset y bytes requeridos/disponibles; detalle no sensible | Resultado por frame; input malformado no termina el stream. |
| `SchemaProfile` | Revisiones IEEE/Core/ZCL; APS profile allowlist; tablas ZDO/ZCL; feature coverage | Inmutable por decode y parte de provenance del resultado. No se mezcla con estado observado. |

Si PHY falta, el decoder devuelve `missing_context` y conserva raw sin intentar MAC/Zigbee; no la infiere del canal. La PHY host configurada o con ACK es `derived_from_session`, no `verified_from_source`. Congelar epoch al recibir/encolar, no leer estado mutable al decodificar. Si RF cambia y el paquete buffered no puede asociarse con un epoch, usar contexto unknown/mismatch y raw-only. FCS `unknown` impide calcular límite MAC/FCS: no quitar bytes por heurística. `crc_ok=false` no se espera en el flujo IEEE actual porque firmware descarta CRC-bad; si llega de otra fuente, permanece raw y no entra al parser por defecto. Una fixture marcada por caller no constituye una emisión OTA verificada.

### 6.2 Metadatos

Preservar `timestamp_us`, channel, RSSI, `lqi` reportado, `crc_ok`, bytes originales, policy FCS/PHR y procedencia. El campo `lqi` refleja siete bits de correlación del backend; no se afirma calibración LQI Zigbee. Timestamp procede de RAT/4; no se asume wall-clock, continuidad o precisión sin validación [FW-F13].

## 7. Capacidad propuesta y protocolo de identificación

### 7.1 Requisitos baseline

| ID | Capacidad propuesta | Ejecución | Estado/decisión |
|---|---|---|---|
| `R-BASE-01` | Crear `RawCapture` con `CaptureContext` explícito, ownership, FCS/PHR policy y raw intacto | Python | Propuesta; no modificar `Packet`. |
| `R-MAC-01` | Parsing bounded IEEE 802.15.4 del profile aprobado: frame type/version, addresses/PAN, sequence suppression, security aux header, IEs y spans MAC/payload/FCS | Python | Requiere texto IEEE exacto. |
| `R-ID-01` | Elegibility gate MAC, un parser NWK bounded y clasificación sobre un único `NwkResult`; nunca prueba origen por semejanza | Python | Cambiar arquitectura del requisito; classification y parse status independientes. |
| `R-NWK-01` | Parsing bounded de NWK elegible: FCF, direcciones, radius/sequence, campos opcionales, security boundary y tipo Data/Command | Python | No incluye routing/service. |
| `R-APS-01` | Parsing bounded de APS plaintext: Data/Command/ACK, delivery mode, endpoints/group, cluster/profile, counter, extensión, security y fragmentación | Python | Fragmentos no reensamblados paran dispatch app. |
| `R-ZDO-01` | Seis schemas ZDO indicados en §10 | Python | Sólo payload plaintext, APS profile/endpoint coherente. |
| `R-ZCL-01` | Header ZCL general: FC, manufacturer code opcional, TSN y command ID | Python | Header no acredita por sí solo que el payload sea ZCL. |
| `R-ZCL-02` | Ocho foundation commands y lista candidata de tipos de §11 | Python | No incluye cluster-specific; requiere profile allowlist aprobado. |
| `R-SEC-01` | Security state y spans estructurales por MAC/NWK/APS; preservar opaque ciphertext/MIC; detener plaintext | Python | Sin decryption. MIC exacto sólo con perfil/nivel que lo determine. |
| `R-RAW-01` | Preservar bytes, metadata, unknown remainder y spans absolutos en todo resultado | Python | Obligatorio. |
| `R-ERR-01` | Errores tipados incluyendo truncation, malformed, unsupported, encrypted/opaque, missing context, ambiguous protocol, context mismatch y schema unknown | Python | No excepciones opacas por frame normal inválido. |
| `R-VAL-01` | Corpus versionado y comparator secundario, divergencias resueltas por fuente normativa | Herramientas offline | No ejecutado; no implica validación actual. |

### 7.2 `R-ID-01`: clasificaciones y semántica

El MAC/context gate decide `not_eligible` antes de invocar NWK si el frame no es Data, falta payload, PHY/FCS-context no permite delimitar el MAC payload, la versión PHY/MAC está fuera de coverage o MAC security impide plaintext. De lo contrario se invoca una sola vez al parser NWK con el span MAC payload, bounds y `SchemaProfile`; el parser no necesita origen Zigbee confirmado. Devuelve NWK fields/opcionales, spans absolutos, remaining payload, security boundary, issues y estado valid/partial/malformed/unsupported. El clasificador sólo consume este resultado y provenance; no calcula campos/offsets NWK nuevamente.

La clasificación es independiente del parse status. `declared_by_caller` no prueba origen. Channel, bytes “parecidos”, profile por defecto o nombre no confirman Zigbee.

| Resultado | Evidencia/condición | Semántica y acción |
|---|---|---|
| `verified_by_provenance` | `verified_from_source` aporta evidencia externa independiente de procedencia Zigbee y el único NWK parse es válido para profile activo | Identificación sólo respecto a la procedencia documentada; no certificado ni autenticado. `declared_by_caller` y `derived_from_session` no bastan. Puede continuar a APS si no hay security stop. |
| `structurally_consistent` | Estructura soportada cuadra con MAC Data, NWK control/version, opcionales y límites; no hay provenance de origen | Candidato compatible, nunca claim de origen Zigbee. Parsing posterior se etiqueta tentativo y preserva clasificación. |
| `ambiguous` | Contexto insuficiente, profile no fijado, revisión no soportada o parser partial sin contradicción normativa | Raw/candidato más issue; no dispatch semántico. |
| `invalid_for_profile` | El caller solicita un profile explícito, pero bits reservados, versión o longitudes contradicen una regla aprobada | Resultado parcial + issue y raw remainder; stop en capa que detectó contradicción. No equivale a “no Zigbee” global. |
| `not_eligible` | MAC/context gate: MAC no Data/sin payload, PHY/FCS/PHR context desconocido, versión fuera de coverage o MAC security no resuelta | Resultado IEEE raw y motivo; no se invoca NWK parser. |

El clasificador no escoge Zigbee por heurística. `structurally_consistent` permite APS tentativa si el parser informa NWK Data plaintext y profile/límites están aprobados. `verified_by_provenance` requiere evidencia de fuente documentada y continúa separada de autenticación criptográfica. Una captura conocida o fixture etiquetada sólo prueba su provenance declarado; una captura OTA controlada exige fuente externa que verifique la emisión/observación.

## 8. Pipeline y límites por protocolo

```text
Packet + snapshot CaptureContext → MAC bounded parser → MAC eligibility gate
  ├─ not eligible / MAC security / context absent → raw + issue/security; no NWK call
  └─ candidate MAC payload → single bounded NWK structural parser
       → NwkResult + classifier(result, provenance, SchemaProfile)
         ├─ ambiguous / invalid_for_profile → raw/candidate; STOP
         └─ verified_by_provenance o structurally_consistent (tentative)
              ├─ NWK security → visible header + opaque protected payload; STOP
              ├─ NWK Command → NWK command/opaque; no APS
              └─ NWK Data plaintext → APS parser
                   ├─ APS security → visible header + opaque ciphertext/MIC; STOP
                   ├─ APS Command/ACK → APS result; no ZDO/ZCL data dispatch
                   ├─ APS fragment → fragment metadata + opaque payload; STOP (sin reassembly)
                   └─ APS Data plaintext → dispatch por profile/endpoint/cluster
                        ├─ ZDO profile 0x0000, endpoints 0 → seis schemas ZDO
                        ├─ app profile aprobado + ZCL frame type 0 → ZCL header/foundation
                        └─ profile/context desconocido → raw app payload; STOP
```

Los flags NWK activan multicast/source route/extended addresses y cambian los offsets (`nwk_util.h:111-155`). El parser debe consumir opcionales en orden especificado y confirmar límites antes de cada lectura. APS frame type y delivery mode determinan campos presentes; extended header/fragment control añade otros (`aps_mede.h:91-124`). No se pasa un NWK Command como APS ni un APS Command/ACK como application data.

## 9. Security boundaries y política de datos

El baseline nunca llama a parser plaintext sobre ciphertext. Seguridad indicada no significa que el decoder conozca autenticidad. `R-SEC-01` se aplica en cada capa y produce un estado con raw spans.

| Límite | Campos potencialmente visibles | Datos necesarios / stop |
|---|---|---|
| IEEE MAC security | FCF security bit, auxiliary security control, frame counter, key identifier/address fields según security control | MAC parser necesita revisión IEEE para auxiliary header y payload/FCS boundary. MIC length depende de security level/profile; si desconocido, no afirmar división exacta ciphertext/MIC y parar NWK. |
| Zigbee NWK security | NWK security flag/header; security control, frame counter, source/key identifier según nivel/contexto | NWK header puede ser visible, pero payload protegido no se entrega a APS sin plaintext autenticado. MIC span se marca sólo si nivel soportado y norma exacta permiten determinarlo. |
| APS security | APS security flag y campos de extensión visibles conforme a formato aprobado | No se entrega ASDU protegido a ZDO/ZCL. APS encryption/security debe distinguirse de security indicada en NWK. |

Estados por capa: `not_indicated` (bits/nivel aprobado no indican security), `opaque` (hay security y payload no interpretable), `authentication_not_attempted` (estado descriptivo cuando no se ejecutó CCM*), `unsupported_security_level` o `missing_context`. No se afirma `authenticated` en baseline. Conservar header visible, frame counter/key-id visibles, ciphertext/remaining raw y issue; no registrar secretos. Sin nivel/contexto normativo, MIC permanece incluido en opaque remainder, no se calcula por conjetura.

APS fragmentation: decodificar sólo indicador/contador/span permitido; sin reassembly no hay una ASDU completa y se detiene ZDO/ZCL. Reassembly, deduplicación y state son extensión separada.

R22 describe NWK/APS security, incluida confidencialidad y autenticación según el security level. Con NWK confidentiality, los APS/ZDO/ZCL bytes no están disponibles como plaintext al sniffer pasivo sin CCM*; campos de NWK y auxiliary security pueden seguir visibles. Scope A sigue siendo útil para MAC/NWK visible, security boundaries, parser bounds, clasificación, fixtures plaintext y errores. No promete leer ZDO/ZCL habitual de redes protegidas. El Scope B decryption, con claves autorizadas, nonce/AAD, MIC, counters y pruebas criptográficas, es extensión distinta y exige aprobación.

## 10. Baseline ZDO: seis mensajes

Contexto obligatorio: APS Data plaintext; profile `ZDO_PROFILE_ID=0x0000`; source/destination endpoint ZDO `0`; APS cluster ID igual al comando seleccionado; destination/broadcast pattern compatible cuando la definición lo exija. El SDK define profile/endpoint (`zd_profile.h:86-87`). En entrada TI separa el Transaction Sequence Number del primer byte antes de asignar `asdu` y `asduLen` (`zd_profile.c:2104-2106`). Por tanto, en este modelo `ZdoEnvelope.tsn` es externo y los schemas de esta tabla empiezan después del TSN; nunca se cuenta dos veces.

R22 §2.4 confirma los cluster IDs y field order de los seis mensajes de esta tabla. Status-dependent fields, RequestType/original request, length/count bounds, APS endpoints/profile y malformed cases deben validarse campo por campo contra las tablas R22 citadas antes de cerrar fixtures. Los parsers TI sólo son cross-check y no son bounded seguros [SDK-Z08–10, NORM-01].

| Mensaje / cluster | Payload después de TSN (orden candidato) | Condiciones, límites y significado pasivo |
|---|---|---|
| `Device_annce` `0x0013` | NWK address `u16 LE`; IEEE/EUI-64 `8`; capability `u8` (11 bytes) | `ZDO_ParseDeviceAnnce()` consume esos campos (`zd_object.c:3607-3618`). Anuncio puede observarse sin request previo; no prueba que el nodo permanezca unido. Truncation issue; trailing bytes preservados y reportados. |
| `NWK_addr_rsp` `0x8000` | status `u8`; success: EUI-64 `8`; NWK address `u16 LE`; address-list count/start/list según request/mode aplicable | `ZDO_ParseAddrRsp()` presenta base de 11 bytes e inspecciona count en índice 11 sólo si `asduLen > 11`; para count no cero consume count, start index y `count×u16` (`zd_object.c:3005-3057`). Longitud/count/status normativos aún provisionales; no leer lista si no cabe. |
| `IEEE_addr_rsp` `0x8001` | Igual wire candidate address response: status; en success EUI-64, NWK address y lista opcional | Cluster IDs de `zd_profile.h:164-173`; la función TI se comparte. Interpretación depende del request/mode y no sólo del layout. Correlación fuera del parser. |
| `Node_Desc_rsp` `0x8002` | status `u8`; NWK address `u16 LE`; si success, Node Descriptor (13 bytes según lecturas TI) | `ZDO_ParseNodeDescRsp()` lee descriptor lógico/capabilities/manufacturer/transfers/server mask (`zd_object.c:3070-3101`). Longitud y tratamiento no-success deben cotejarse con R22. |
| `Simple_Desc_rsp` `0x8004` | status `u8`; NWK address `u16`; descriptor length `u8`; descriptor si success: endpoint, profile ID `u16`, device ID `u16`, version/reserved, input count + `2×nIn`, output count + `2×nOut` | `ZDO_ParseSimpleDescRsp/Buf()` muestra orden de campos y asignación heap (`zd_object.c:3147-3160,3543-3594`). Candidate descriptor length `8+2×nIn+2×nOut`; debe concordar con bytes disponibles y campo length; confirmar normativa. |
| `Active_EP_rsp` `0x8005` | status `u8`; NWK address `u16`; endpoint count `u8`; endpoint IDs `count` | `ZDO_ParseEPListRsp()` (`zd_object.c:3172-3194`); candidate total 4+count. TI no demuestra bounds completos. No contiene endpoints ZCL data; es descriptor pasivo. |

Todos los enteros listados como `u16` son wire little-endian según `BUILD_UINT16` en TI; confirmar contra norma aprobada. Para status de error no se asumen campos de success. Si una longitud/status exacta no está resuelta, el registro devuelve status + remainder opaque y `schema_incomplete`, no una estructura completada por default. Unknown cluster permanece APS raw. Extra bytes se preservan y se marcan `trailing_data`; nunca descartar silenciosamente.

TSN ZDO se guarda por separado de NWK sequence, APS counter y ZCL TSN. Correlación request/response no es requisito del parser pasivo; si se añade, requiere cluster, direcciones, perfil, endpoints, TSN y ventana temporal/provenance. Fixture mínimo por mensaje: ejemplo válido provenance conocido, success/error donde corresponda, truncation en cada frontera, extra bytes, count 0/1/máximo que cabe/count overrun; simple descriptor con counts combinados; casos de TSN presente una sola vez y contexto APS incorrecto.

## 11. Baseline ZCL: header, commands y tipos

### 11.1 APS profile y dispatch

El profile `0x0000` y endpoint 0 se habilitan para ZDO por `zd_profile.h:86-87`. El SDK contiene clusters ZCL, pero no define una allowlist de perfiles de aplicación interoperables en los headers inspeccionados. `af.c` enruta según el `ProfileID` declarado por cada endpoint; eso no establece que todo profile admita este decoder (`af.c:463-486`).

Política de revisión 1.1:

| Profile/contexto | Tratamiento |
|---|---|
| APS profile `0x0000` + endpoint ZDO 0 | Sólo dispatch de seis clusters ZDO; no ZCL. |
| Perfil de aplicación `0x0104` (Common/HA) | **Recomendado condicionalmente como primer contexto ZCL foundation**, no habilitado por defecto. R21 Table 2.44 documenta Common (HA); el literal no aparece en el Core R22 extraído y el SDK no es fuente del profile. Requiere conservar fuente/profile histórico aplicable, aprobación del supervisor y fixtures R7. No incorpora esquemas cluster-specific. |
| Otros perfiles estándar | Unsupported hasta añadir individualmente al `SchemaProfile` con fuente/version aprobada. |
| `0xFFFF` wildcard, profile unknown o falta contexto | No usar para afirmar ZCL; preservar APS ASDU raw y `missing_context`/`unsupported_profile`. |
| Manufacturer-specific schema/profile | Payload opaco salvo schema explícito, versionado y aprobado. |

ZCL foundation interpretation requiere APS Data plaintext, profile exacto allowlisted/versionado (propuesto `0x0104`), cluster y endpoints preservados, sin fragmentación, security gate satisfecho, ZCL frame type global, header bounded, manufacturer-specific bit apagado y command/type dentro del subset. El profile no demuestra que cada ASDU sea ZCL. Activación/semántica es decisión del supervisor; fixture sintética prueba sólo parser, no origen OTA ni interoperabilidad.

### 11.2 Header separado

ZCL header contiene Frame Control, Manufacturer Code opcional, Transaction Sequence Number y Command ID. TI `zclParseHdr()` lee 3 o 5 bytes sin argumento de longitud (`zcl.c:2412-2446`); una reimplementación propuesta usa cursor+remaining. Header de 3/5 bytes con frame type no foundation no activa los schemas de esta sección. Manufacturer-specific permanece header+payload raw.

### 11.3 Ocho foundation commands

IDs provienen de `zcl.h:509-557`; estructuras candidatas en `zcl.h:844-899,971-1016`. Dirección es la convención ZCL; el receptor debe validar flags/contexto según R7 aprobado, sin reinterpretar command ID por dirección inesperada.

| Command (ID) | Dirección usual | Payload candidato | Condicionalidad/errores |
|---|---|---|---|
| Read Attributes `0x00` | Client→server | attribute IDs `u16 LE` repetidos hasta fin; sin count explícito | Longitud remanente múltiplo de 2; parcial final = truncated. |
| Read Attributes Response `0x01` | Server→client | records attr ID `u16`, status `u8`; en success, datatype `u8` + value | Status error omite datatype/value; exactos según R7. Type desconocido detiene records posteriores y preserva resto. |
| Write Attributes `0x02` | Client→server | records attr ID, datatype, value hasta fin | Value length depende del datatype y profile. Unknown datatype: no adivinar límite del siguiente record. |
| Write Attributes Response `0x04` | Server→client | Success total: un status SUCCESS con Attribute ID omitido; errores: status + Attribute ID por record según regla ZCL7 | Regla en ZCL7 §2.5.5.1.2. Parsar variantes bounded, conservar mismatch/truncation; no usar simplificación de sólo un status byte sin record. |
| Report Attributes `0x0A` | Dirección depende del role cliente/servidor del cluster | records attr ID, datatype, value hasta fin | No imponer una dirección universal en parser genérico; no inferir semántica de cluster. |
| Default Response `0x0B` | Respuesta al command previo; dirección opuesta a la transacción correspondiente | command ID respondido `u8`, status `u8` | Sin correlación se decodifican campos, no se afirma que responde a un frame capturado. |
| Discover Attributes `0x0C` | Client→server | start attribute `u16`, max attribute IDs `u8` | Longitud fija 3 después del header. |
| Discover Attributes Response `0x0D` | Server→client | discovery-complete `u8`; records attr ID `u16` + datatype `u8` | Prefijo 1 y resto múltiplo de 3; flags/status exactos de R7. |

Estas estructuras son schemas candidatos y no afirmación normativa completa mientras falte la especificación R7 autorizada. Cluster-specific commands no se interpretan. Invalid/truncated/mismatched direction produce issue y raw remainder. Se distinguen ZCL TSN, APS counter y ZDO TSN.

### 11.4 Allowlist inicial candidato de tipos

Los IDs están confirmados por `zcl.h:597-652`; tamaños fijos deben cotejarse con la tabla normativa ZCL7 y el helper TI (`zcl.c:3195-3283`). Esta es **lista candidata**, no aprobación final.

| Tipo | ID | Longitud wire candidata | Nota |
|---|---:|---:|---|
| `NO_DATA` | `0x00` | 0 | No debe confundirse con tipo desconocido; contexto command debe permitirlo. |
| `BOOLEAN` | `0x10` | 1 | Interpretación de valores válidos requiere R7. |
| `BITMAP8/16/32/64` | `0x18/19/1B/1F` | 1/2/4/8 | No incluir bitmap24/40/48/56. |
| `UINT8/16/32/64` | `0x20/21/23/27` | 1/2/4/8 | Little-endian conforme al schema aprobado. |
| `INT8/16/32/64` | `0x28/29/2B/2F` | 1/2/4/8 | Sign extension definida por implementation; bytes originales preservados. |
| `ENUM8/16` | `0x30/31` | 1/2 | Valores no conocidos permanecen numéricos. |
| `OCTET_STR/CHAR_STR` | `0x41/42` | Prefijo `u8` + longitud indicada | ZCL7 §§2.6.2.11–12: `0x00` vacío y `0xFF` valor inválido; ambos con cero bytes de contenido. Otros prefijos deben caber en span bounded. |
| `CLUSTER_ID/ATTR_ID` | `0xE8/0xE9` | 2 | Campo wire 16-bit. |
| `IEEE_ADDR` | `0xF0` | 8 | Mantener bytes y convención de visualización documentada. |

Tipos compound (array, structure, set, bag), floating point, widths 24/40/48/56/long strings y key type quedan fuera del primer subset. String prefix mayor que remaining = truncated; vacío/sentinel inválido sigue ZCL7. Tipo no soportado devuelve `unsupported_data_type`, datatype offset y todo el resto raw. No se continúa hacia supuestos records posteriores.

Fixtures mínimos: headers 3/5; profile allowlisted/no allowlisted/missing; cada command en request/response context; dirección esperada y mismatch; cada tipo candidato en mínimo/normal/máximo; empty/null/truncated string; unknown type; manufacturer-specific opaque; trailing bytes y listas parciales.

## 12. Componentes propuestos y ownership

Las responsabilidades siguientes son **PROPOSED — NOT IMPLEMENTED** y no requieren un paquete separado cada una.

| Responsabilidad conceptual | Contrato | Contexto/dependencia | Ownership/error | Ejecución |
|---|---|---|---|---|
| Core capture/model | `Packet + CaptureContext → RawCapture` | PHY/FCS/PHR/provenance/profile | Referencia inmutable/copia si lifetime incierto; errores `missing_context`/`context_mismatch`; raw original | Python |
| Bounded cursor + issues | `bytes/span → read result + absolute offset` | Longitud del buffer y límites de subspan | No lee fuera de span; errores requeridos/disponibles | Python |
| Classifier/pipeline | `RawCapture + SchemaProfile → DecodeResult` | Resultado MAC, clasificación, security state | Stop por ambiguo/security/unsupported; no muta raw | Python |
| IEEE parser | MPDU+FCS policy → `MacResult` y payload span | Profile IEEE aprobado | FCS/metadata preservados; error por offset absoluto | Python |
| NWK / APS parsers | Payload elegible/plaintext → resultados de capa | Profile Zigbee, flags opcionales, frame type | Ciphertext/fragment no asciende | Python |
| ZDO / ZCL schemas | APS context + ASDU → record tipado u opaque | Profile, endpoint, cluster, command, types | Raw desconocido y offsets absolutos | Python |
| Validation adapters | fixtures/PCAP + resultados → diff reproducible | Corpus licenciado, comparator fijado | Separados del runtime; no cambian `Radio` | Host/tooling |

Forma de empaquetado mínima conceptual: un `core` (capture/cursor/issues/profile), un `pipeline` (classification/dispatch), parsers de capas y tablas de schemas; adapters offline en test tooling. Los nombres de módulos futuros son internos, no API pública comprometida. La biblioteca no debe abrir radio, leer serial ni importar KillerBee al importarse.

## 13. Reutilización Z-Stack, procedencia y límites

| Símbolo/área | Tratamiento propuesto | Motivo y evidencia |
|---|---|---|
| `zclParseHdr()` | Reimplementar bounded desde norma; TI como cross-check | Source 3/5 bytes, sin length (`zcl.c:2412-2446`). |
| `zclBuildHdr()` | Fuera baseline; referencia condicional para builder | `static`, sin capacity (`zcl.c:2459-2482`). |
| `zclGetDataTypeLength()` | Reexpresar como tabla fixed/variable/unsupported | Devuelve cero para varias categorías (`zcl.c:3195-3283`). |
| `zclGetAttrDataLength()` | No portar firma; usar reader bounded | Lee prefix sin available length (`zcl.c:3296-3322`). |
| `zclSerializeData()` | No baseline; cross-check si builder futuro | Usa layout C y carece de capacity (`zcl.c:2961-3076`). |
| `ZDO_Parse*` | Verificar field order; implementar Python independiente | `zdoIncomingMsg_t`, `asduLen`, OSAL allocation/ownership y funciones sin bounds completos (`zd_object.c:3005-3194,3543-3618`). |
| NWK/APS headers | Máscaras/structs cross-check secundarios | `NLDE_ParseMsg`/`APSDE_ParseMsg` sin source visible; no inventar función. |
| ZMAC/TIMAC/AF/BDB/security/NV | Excluidos | Wrappers y callbacks ligados a `MAP_MAC_*`, OSAL, task/config y bibliotecas por toolchain/rol; no servicio raw FeralRF equivalente. |

Política de provenance: implementación desde normas obtenidas/autorizadas y versionadas; TI sólo como cross-check técnico. La cabecera BSD-3-Clause de `zcl.c/.h` (`:9-37`) hace ciertas reutilizaciones potencialmente permitidas bajo notices, disclaimer y no-endorsement, pero sus APIs sin bounds no se recomiendan directamente. `zd_profile.*`, `zd_object.*`, `nwk_util.h`, `aps_mede.h`, `aps_util.h`, `aps.h` y `zmac*.c` inspeccionados tienen avisos TI `Limited License`; su alcance exacto en paquete Python host/distribución queda para asesoría legal. No copiar ni traducir esos archivos bajo el supuesto de que todo SDK es BSD. Root y Python declaran GPL-3.0-or-later/GPL-3.0; Scapy GPL-2.0-only no se integra al runtime. El análisis no es clearance legal.

## 14. Normativa y APS profile versioning

TI release notes para Z-Stack 8.30 declaran Zigbee PRO 2017/R22 y ZCL7. El documento CSA `05-3474-22` identifica R22 1.0 (2017-04-19); su Annex D §§D.1.1–D.1.2 dice que asume y complementa IEEE 802.15.4-2015. ZCL `07-5123-07` es Revision 7. Se fija esta tríada como target inicial acotado; no mezclar R22.1/R23 ni ZCL8 dentro del mismo schema.

Pendiente antes de implementar schemas: registrar copia de norma obtenida conforme a los términos aplicables, su hash y errata/corrigendum IEEE; aprobación de `SchemaProfile`; y completar crosswalk/fixtures para status/count/direction. La extracción oficial disponible confirma versión/relación y reglas ZCL citadas, no sustituye control institucional de documentos ni autorización de redistribución. IEEE muestra 2015 superseded por 2020; R22, sin embargo, refiere expresamente 2015, por lo que la edición posterior no se sustituye silenciosamente.

APS profile allowlist: `0x0000` + endpoint 0 confirmado para ZDO. `0x0104` tiene evidencia documental de profile Common (HA) en Core R21 Table 2.44 y material histórico de aplicación; no lo establece el SDK ni el texto R22 inspeccionado. Se recomienda como primer profile contextual ZCL foundation, condicionado a fuente/profile exacto y aprobación de supervisor. Esto habilita sólo los ocho foundation commands genéricos cuando APS/ZCL context, plaintext y flags lo permiten; no cluster-specific ni manufacturer-specific. Si no se conserva norma/perfil conforme a términos, deshabilitar semántica ZCL y mantener raw.

## 15. Compatibilidad y preservación de FeralRF

| Superficie | Efecto esperado del baseline | Condición de aceptación |
|---|---|---|
| `Radio`/`Packet` | Consumo por wrapper opt-in; API pública intacta | Imports sin side effects; `Packet.data` byte-identical; PHY/session context obligatorio. |
| COBS/UART | Sin comandos/respuestas nuevas | Prueba de regresión del protocolo al implementarse; decoder no toca serial automáticamente. |
| Firmware/RF Driver | Sin modificaciones | Inspección diff; no altera ownership ni colas. |
| Raw RX/TX | Bytes permanecen accesibles sin decoder | Unknown/non-Zigbee siempre pasa como raw; decoder no consume el iterador de fondo. |
| Otras PHY | No elegibles para Zigbee | Rechazo por PHY de contexto, nunca por heurística de canal. |
| KillerBee | Adapter independiente/lazy | No es dependencia del decoder; conserva ruta existente (`killerbee.py:1-19,95-119`). |
| Capacidad host | Añade CPU/RAM y puede acumular backlog | Medir throughput/pérdida/backlog antes de habilitar observation state; no asumir captura sin gaps. |

La compatibilidad es inferencia arquitectónica, no resultado probado. No se normalizan timestamp/LQI. Current Python `read_packets()` parsea el response length y hace slicing de `pkt_len`, pero no comprueba explícitamente que todos esos bytes estén presentes (`radio.py:673-707`); cualquier decoder adapter debe validar su input y devolver issue si contexto o longitudes no son suficientes. Esta propuesta no modifica esa API.

## 16. Requisitos funcionales verificables

| ID | Entrada/contexto | Salida | Error/stop | Capa/validación |
|---|---|---|---|---|
| `R-BASE-01` | `Packet` + `CaptureContext` con PHY, FCS/PHR policy, source/version/ownership | `RawCapture` raw inmutable, metadata y spans | Falta/conflicto context → issue y sólo raw | Python; V-RAW-01 + fixtures adapter |
| `R-MAC-01` | MPDU y FCS policy + revisión IEEE | Campos MAC, header/payload/FCS spans | No Data/unsupported/secured → result raw/security y stop | Python; V-MAC-01 |
| `R-ID-01` | MAC payload + provenance + schema profile | Una de cinco clasificaciones §7.2 | Ambiguous/not eligible/invalid se conservan y no autorizan decode afirmativo | Python; V-NWK-APS-01 |
| `R-NWK-01` | Payload clasificado y sin MAC security indicada; plaintext externo autenticado queda fuera del baseline | Header base/opcionales, frame type, payload/security spans | Security/unsupported version/command path para APS | Python; V-NWK-APS-01 |
| `R-APS-01` | NWK Data plaintext | APS type/delivery/context/counter/extensions/spans | security/command/ACK/fragment/malformed paran app dispatch | Python; V-NWK-APS-01 |
| `R-ZDO-01` | APS plaintext profile 0/endpoints 0/cluster allowlist | TSN separado + seis schemas §10 | Status/length/count provisional → partial/raw issue | Python; V-ZDO-01 |
| `R-ZCL-01` | APS Data plaintext + profile allowlisted | Header 3/5 y payload span | Missing/unsupported profile o manufacturer payload → opaque | Python; V-ZCL-01 |
| `R-ZCL-02` | ZCL foundation frame + command/type allowlist | Campos/value/span por §11 | Tipo/longitud desconocidos detienen records, preservan tail | Python; V-ZCL-01 |
| `R-SEC-01` | Security fields de cada capa | Security state + visible/ciphertext spans | Nunca plaintext sin authenticated context; MIC no supuesto | Python; V-SEC-01 |
| `R-RAW-01` | Cada resultado parcial o final | Bytes originales reconstruibles por spans | Preservar unknown/trailing bytes aun con error | Core; property fixtures |
| `R-ERR-01` | Todo input bounded | Issue estable por capa/offset | Frame inválido no mata siguiente frame | Core; suite negative |
| `R-VAL-01` | Corpus normativo versionado y comparator | Resultado/diff/provenance reproducible | Conflicto sin resolver bloquea claim | Offline; V-COMP-01 |

Todos son **propuestos/no implementados**. La cobertura se declara por profile y command. “Unsupported” es distinto de “malformed”; “structurally consistent” no significa interoperable.

## 17. Validación reproducible requerida (futura; no ejecutada)

| Test | Prerrequisitos y estímulos | Observables/criterio | Límite |
|---|---|---|---|
| `V-RAW-01` | Build/firmware identificado; capturas de entrada conocidas; sniffer OTA independiente si se evalúa TX | Confirmar por interfaces actuales: RX bytes y PHR/FCS policy según captura externa; channel y metadata documentadas. TX separa (1) ACK accepted, (2) ausencia de async failure dentro de ventana declarada como evidencia negativa débil, (3) OTA observed byte-exact. Terminal RF completed correlacionado: no disponible en host actual; no inferir de ausencia de error. MAC ACK y respuesta app son observables distintos, no requisitos del baseline. | No prueba Zigbee ni delivery. No inventar umbral temporal. OTA receiver necesario para afirmación de emisión. |
| `V-MAC-01` | IEEE profile autorizado; fixtures byte-exactos | Non-Data; combinaciones addressing; versiones supported/unsupported; PAN compression; sequence suppression; aux security; IE policy; FCS present/unknown; truncation cada offset; malformed. Expects spans/issues exactos y raw conservado. | No RF stack service. |
| `V-NWK-APS-01` | Zigbee Core profile aprobado y known captures con provenance | R-ID-01 positive contextual, structural unconfirmed, non-Zigbee, ambiguous, invalid NWK, context missing; NWK optional fields/data-vs-command; APS Data/Command/ACK, delivery modes, profiles unsupported; security MAC/NWK/APS; fragment stops; unknown/truncated. | Comparator no sustituye norma. |
| `V-ZDO-01` | ZDO schemas R22 autorizados | Seis clusters, TSN exactly once, status success/error, count/length, request/response context sólo para correlation, APS mismatch, unknown cluster, trailing/truncation. Expected bytes/offsets exactos. | Las reglas TI de status/count no son oracle. |
| `V-ZCL-01` | ZCL7 autorizada y APS allowlist aprobada | Header 3/5; profile allowed/unsupported/missing; ocho commands; candidate types; direction mismatch; manufacturer opaque; unknown type; string sentinel/limits/truncation; invalid records. | Sin semántica cluster-specific. |
| `V-SEC-01` | Vectores estructurales de profile aprobado; sin claves para baseline | Security headers MAC/NWK/APS, missing context, unsupported level, opaque ciphertext/MIC handling, no plaintext calls; assertion de estado `authentication_not_attempted`. | Authenticated decryption excluida. |
| `V-COMP-01` | Release/config/DLT/FCS policy fijados de dissector independiente | Mismos fixtures/captures y diff campo a campo; cada diferencia explicada por norma o bloquea claim | Norma aprobada es autoridad. Scapy/Wireshark sólo comparator. |

Fixtures deben registrar provenance/licencia, revisión, FCS policy, expected classification, raw hashes, schema version y sensibilidad. Fuzzing offline será posterior; no se ejecutó aquí. Aceptación de arquitectura no cambia implementación o validación a “completa”.

### 17.1 Estados TX que no deben confundirse

`accepted` = ACK host de comando encolado; `async_failure` = evento error posterior, no necesariamente correlacionado; `completed` = terminación interna que no se expone correlacionadamente por la API; `observed_ota` = trama capturada por sniffer externo; `mac_acknowledged` = ACK IEEE recibido; `application_responded` = respuesta de capa superior. `Radio.transmit()` sólo espera ACK/ERROR (`radio.py:725-750`). Baseline pasivo no necesita una nueva completion API.

## 18. Mapa requisito → fuente → validación

| Requisito | Fuente principal | Verificación futura | Estado actual |
|---|---|---|---|
| `R-BASE-01` | `radio.py:18-30,455-478,673-707`; `radio_if.c:1335-1355,1497-1568` | Adapter fixtures + V-RAW-01 | Raw implementado; contexto completo no existe en `Packet`; decoder propuesto. |
| `R-MAC-01` | IEEE-D01; AUD-01; RF RX citado arriba | V-MAC-01 | No existe parser. Normativa pendiente. |
| `R-ID-01` | MAC pass-through `ll_manager.c:144-159`; policy §7 | V-NWK-APS-01 | Nuevo clasificador propuesto. |
| `R-NWK-01` | SDK-Z11/Z14 `nwk_util.h:111-155,319-321` | V-NWK-APS-01 | Definiciones visibles; implementación TI no visible. |
| `R-APS-01` | SDK-Z12 `aps_mede.h:91-124`; `aps_util.h:110-113` | V-NWK-APS-01 | Definiciones visibles; parser TI no visible. |
| `R-ZDO-01` | SDK-Z08–10 `zd_profile.h/.c`, `zd_object.c/.h` | V-ZDO-01 | Field order visible; parser propuesto, no validado. |
| `R-ZCL-01` | SDK-Z03/07 `zcl.c:2412-2446`, `zcl.h:822-842` | V-ZCL-01 | Algoritmo visible sin bounds; propuesta Python. |
| `R-ZCL-02` | SDK-Z05/06 `zcl.c:2961-3076,3195-3322`; `zcl.h:509-652` | V-ZCL-01 | IDs/helpers visibles; schema pendiente de ZCL7. |
| `R-SEC-01` | SDK-Z12/M08; `ssp.h`; IEEE profile | V-SEC-01 | No decryption ni validación propuesta. |
| `R-RAW-01` | `Packet`, `RadioIF_RxPacket`; FW-F13 | Spans/property fixtures | Modelo común propuesto. |
| `R-ERR-01` | Parse hazards SDK-Z03–10 | Negative suites | Issues propuestos. |
| `R-VAL-01` | EXT-01/05; normativa | V-COMP-01 | No comparador ejecutado. |

## 19. Extensiones condicionales

| ID | Extensión | Gate explícito |
|---|---|---|
| `C-OBS-01` | Estado pasivo mappings/transactions/topology | Parser aceptado; provenance, gaps, conflicts, expiry y pérdidas modelados. |
| `C-DEC-01` | Decryption Zigbee autorizado | CCM* exacto, keys autorizadas, nonce/AAD, frame counters, MIC known-answer y política secretos; fuera baseline. |
| `C-BLD-01` | Builders ZCL/ZDO/NWK/APS/MAC y raw injection | Normas y schemas aprobados, serializers bounded, FCS contract, V-BLD-01; inyección no equivale a nodo. |
| `C-RF-01` | Timed TX/CCA observacional | Requisito funcional, ownership, terminal events, timing, recursos y regresión multi-PHY aprobados. Auto-ACK/CSMA/RX_ACK requieren estudio MAC separado. |

## 20. Riesgos y dependencias no resueltas

1. Controlar copias/hash y condiciones de acceso de Core R22, ZCL7, IEEE 802.15.4-2015 y documento de perfil; comprobar errata/corrigendum.
2. Aprobar `SchemaProfile`; habilitar `0x0104` sólo condicionalmente para foundation genérica tras comprobar contexto APS y fixtures; perfil no añade clusters.
3. Fijar definición de MAC versions/IE/security cubiertas, incluidos PAN compression y sequence suppression.
4. Validar `Packet` layout, FCS policy, metadata, drops y firmware identity con artefacto identificado antes de claims físicos.
5. Implementar con provenance independiente desde norma; cualquier código TI traducido/copied requiere revisión legal específica por archivo/distribución.
6. Medir CPU/memoria/backlog host antes de estado pasivo; colas actuales pueden perder frames.
7. No hay completion RF correlacionado en host; cualquier futuro requisito requiere interfaz o evidencia externa diferenciada.
8. Corpus, comparadores y schemas no se han construido/ejecutado; divergencia queda bloqueante.

## 21. Decisiones para supervisor

| Decisión | Recomendación | Bloquea |
|---|---|---|
| Arquitectura | Aprobar decoder opt-in del paquete Python FeralRF y sin firmware inicial | Decisión principal. |
| `R-ID-01` | Aprobar eligibility MAC → un solo NWK parser → classifier sobre `NwkResult`; parse status separado | Semántica identificatoria y corpus. |
| `CaptureContext` | Aprobar provenance de 4 niveles y snapshot por packet/epoch; sin confirmar hardware por host ACK | Adapter/corpus; no cambio a `Packet`. |
| Profile Core/ZCL/IEEE | Aprobar Core R22 1.0 / ZCL7 / IEEE 802.15.4-2015 como perfil inicial acotado | Crosswalk de normas legalmente accesibles y errata. |
| Aplicación APS/ZCL | Aceptar condicionalmente `0x0104` sólo para foundation genérica; deshabilitar semántica si perfil/contexto no está aprobado | Payload ZCL typed y fixtures. |
| ZDO/ZCL subset | Mantener seis mensajes y ocho commands; aplicar reglas ZCL7 confirmadas; completar crosswalk de status/count/types/direction | Corpus norma-derived. |
| Seguridad | Mantener Scope A sin descifrado; decidir si headers/fixtures bastan para el objetivo académico | Tráfico real ZDO/ZCL secured requiere Scope B autorizado. |
| Source policy | Implementación independiente desde specs autorizadas; TI BSD file-by-file, TI-restricted reference-only | Revisión legal previa a cualquier reuse/redistribución. |
| Validación TX | Baseline no exige completion host; OTA observación externa sólo si se evalúa TX | Ninguno para decoder; completion claim queda bloqueado. |
| Extensiones | Mantener observation state, decryption, builder/injection y RF primitives condicionales | Sus ramas respectivas. |

## 22. Recomendación final

Se recomienda aprobar la dirección **raw-first, host-side y versionada**, con el alcance ya corregido: adapter/contexto de captura, MAC bounded, clasificación conservadora, NWK/APS plaintext con límites de seguridad, seis mensajes ZDO, header ZCL y foundation subset sólo bajo contexto/profile permitido. El baseline permanece en Python y no altera firmware, `Radio`, `Packet`, COBS, RF ownership ni otras PHY.

La aplicación ZCL no recibe profile habilitado por defecto. Debe aprobarse un APS profile de aplicación y su revisión normativa antes de decodificar payload semántico; `0x0104` es candidato, no hecho confirmado en el SDK examinado. R22/ZCL7 permanece provisional y no reemplaza los textos normativos. La implementación independiente desde norma reduce dependencia de OSAL, ABI y condiciones de licencia.

El éxito futuro requiere corpus con provenance, límites/negativos por capa, clasificación no heurística, security stops, comparación secundaria y divergencias resueltas por norma. V-RAW-01 puede usar interfaces actuales para ACK accepted y observación externa OTA; no puede afirmar completion correlacionado. Ningún resultado histórico raw acredita Zigbee, joining, seguridad o interoperability.

## 23. Respuesta final

La capacidad justificable es un decodificador pasivo, bounded y opt-in integrado en el paquete Python FeralRF, sobre el MPDU IEEE 802.15.4 que ya llega al host. Exige un snapshot explícito de contexto/provenance, clasifica después de una sola lectura NWK, conserva raw y para en security, fragmentación o profile/contexto desconocido. Propone seis mensajes ZDO y ocho foundation commands bajo APS `0x0104` sólo si el supervisor aprueba el perfil/fuente y el frame es plaintext con contexto compatible; no incluye clusters HA.

El SDK TI justifica campos y cross-checks, pero `NLDE_ParseMsg()`/`APSDE_ParseMsg()` son declarations-only y no demuestra un runtime MAC reusable. `zcl.c/.h` tienen aviso BSD por archivo; las unidades ZDO/NWK/APS/ZMAC citadas tienen TI Limited License y necesitan revisión legal antes de reuso. La propuesta recomienda implementación independiente desde textos normativos obtenidos conforme a licencia; traducir código TI no se llama automáticamente independiente. Scope A no descifra tráfico protegido. Cada capacidad sigue **propuesta/no implementada/no validada**.

## 24. Referencias

La evidencia de código por ruta/símbolo/línea y el registro de fuentes está en [FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md](FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md). La resolución de licencia, normativa, perfil y contrato único NWK se detalla en [FERALRF_ZIGBEE_NORMATIVE_AND_LICENSE_REVIEW_2026-11-04.md](zigbee_proposal/FERALRF_ZIGBEE_NORMATIVE_AND_LICENSE_REVIEW_2026-11-04.md); el review arquitectónico previo permanece como antecedente.
