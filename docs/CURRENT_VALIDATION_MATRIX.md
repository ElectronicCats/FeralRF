# Matriz canónica actual de capacidades y validación de FeralRF

> [!IMPORTANT]
> Esta matriz describe el estado de evidencia defendible a 2026-11-06 sobre el source baseline `686a87cc4bcebedf6a3122c3ad82c554813478f0`. No es una certificación de hardware ni identifica de forma inequívoca los binarios instalados en las campañas. Implementación y validación experimental son dimensiones separadas: una capacidad puede existir sin estar validada, y una observación histórica positiva no establece fiabilidad universal ni equivalencia con el checkout actual.

Esta es la referencia `CANONICAL` del estado actual de capacidades y validación. La antigua [`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) se conserva sin reinterpretar como baseline `HISTORICAL`.

## 1. Alcance

La matriz responde qué existe en el source actual, qué ruta de control fue ejercitada, qué comportamiento fue observado, qué propiedades físicas fueron medidas, qué nivel protocolario existe y si hubo interoperabilidad independiente.

No sustituye:

- el [contrato wire](protocol.md);
- la [referencia de API](PYTHON_API.md);
- la [documentación canónica de hardware](../hardware/README.md);
- el [Validation Runbook canónico](validation/runbook/README.md);
- los registros de cada ejecución.

La evidencia experimental conservada no enlaza de forma criptográfica todos los binarios CC1352/RP2040 instalados con este HEAD. Por eso «presente en source» no significa «ejecutado desde este mismo artefacto».

## 2. Regla metodológica

Estas dimensiones no son equivalentes:

```text
Implementación
≠ aceptación de control
≠ comportamiento funcional/OTA
≠ propiedad física RF
≠ framing o stack protocolario
≠ interoperabilidad externa
```

Un ACK demuestra aceptación o encolado del comando según el caso. No demuestra por sí solo ejecución RF, radiación, frecuencia, potencia, modulación, temporización, posición del switch ni recepción por un peer. Un marcador recibido por otro CatSniffer demuestra entrega host-visible bajo esas condiciones; no mide independientemente las propiedades RF ni demuestra el protocolo sugerido por el nombre de un preset.

## 3. Vocabulario de estados

| Campo | Estados usados | Significado |
|---|---|---|
| `Implementation` | `PRESENT`, `PARTIAL`, `NOT_IMPLEMENTED`, `REMOVED`, `UNKNOWN` | Existencia en el source actual de la capacidad nombrada, no del potencial teórico del chip. |
| `Control` | `VALIDATED`, `PARTIAL`, `FAILED`, `NOT_TESTED`, `NOT_APPLICABLE`, `UNKNOWN` | Demostración de la ruta host/firmware de configuración o comando. |
| `Functional / OTA` | `DEMONSTRATED`, `PARTIAL`, `FAILED`, `BLOCKED`, `NOT_TESTED`, `NOT_APPLICABLE`, `UNKNOWN` | Comportamiento observable, incluida entrega a un peer cuando aplica. |
| `Physical RF` | `MEASURED`, `PARTIAL`, `NOT_VALIDATED`, `NOT_APPLICABLE`, `UNKNOWN` | Medición independiente de frecuencia, potencia, modulación, timing, espectro, cese o ruta física. |
| `Protocol level` | `RAW_PHY`, `FRAMING`, `MAC_PARTIAL`, `STACK`, `NOT_IMPLEMENTED`, `NOT_APPLICABLE`, `UNKNOWN` | Máximo nivel sustentado; el nombre de un preset no eleva este nivel. |
| `Interoperability` | `DEMONSTRATED`, `PARTIAL`, `NOT_DEMONSTRATED`, `NOT_TESTED`, `NOT_APPLICABLE`, `UNKNOWN` | Interacción con una implementación externa independiente y conforme. |
| `Evidence strength` | `PRIMARY_RAW`, `PRIMARY_REPORT`, `CONSOLIDATED_STRONG`, `CONSOLIDATED_LIMITED`, `HISTORICAL_CLAIM`, `BLOCKED`, `NO_EVIDENCE` | Calidad de la evidencia disponible para la observación, no madurez de la implementación. |

`FAILED` se limita al criterio efectivamente ensayado. Por ejemplo, un resultado raw 0/10 de `mioty_868_tsunb` no significa «MIOTY falló», porque el stack MIOTY no está implementado. `BLOCKED` significa que preparación, estado o cleanup impidieron completar el objetivo; no equivale a fallo de la capacidad que no llegó a ejercitarse.

## 4. Jerarquía de evidencia

De mayor a menor fuerza práctica para actualizar un estado experimental:

1. `PRIMARY_RAW`: artefacto crudo identificable con comando, resultado y cleanup.
2. `PRIMARY_REPORT`: reporte primario con condiciones y resultado suficientes.
3. `CONSOLIDATED_STRONG`: síntesis trazable de ejecuciones suficientes y no duplicadas.
4. `CONSOLIDATED_LIMITED`: síntesis útil pero incompleta en identidad, raw, comparabilidad o cleanup.
5. `HISTORICAL_CLAIM`: afirmación histórica agregada sin artefactos suficientes para inspección actual.
6. `BLOCKED`: existe un intento, pero no llegó al criterio objetivo.
7. `NO_EVIDENCE`: no existe ejecución válida conocida.

La [matriz histórica de ejecuciones][UM02] es el inventario de lookup. No se reproducen aquí sus 193 unidades.

## 5. Matriz maestra de capacidades

### 5.1 Sesión, transporte y configuración

| Capability | Implementation | Control | Functional / OTA | Physical RF | Protocol level | Interoperability | Evidence strength | Last validated | Limitations | Sources |
|---|---|---|---|---|---|---|---|---|---|---|
| Conexión, `init()` e identificación | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | EV-03, 2026-10 | `1.0.0` y `FERALRF1` no identifican commit, binario ni placa física. | [API], [UM02], [UM03] |
| Desconexión y reapertura entre procesos | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | EV-03, 2026-10 | Probado entre procesos; no cubre pérdida USB ni reapertura de la misma instancia en todos los estados. | [API], [UM02] |
| Estadísticas `get_stats()` | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_LIMITED` | 2026-10-21 | Contadores no cubren todas las capas y el metadata propietario RX tiene defectos conocidos. | [API], [UM08], [P_GAPS] |
| Recovery manual mediante Cat-Shell | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | campañas 2026-10 | Recuperación funcional no identifica eléctricamente reset/boot ni el binario reiniciado. | [UM02], [UM03], [HW] |
| `reset_device()` automático | `PRESENT` | `PARTIAL` | `PARTIAL` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | EV-04, 2026-10 | Intenta reset de hardware del CC1352 mediante Cat-Shell; no es power-cycle. La inferencia Bridge→Shell `+2` no es generalizable y debe verificarse antes. | [API], [UM02], [HW_OPEN] |
| `set_phy()` / `set_channel()` | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | 2026-10-21 | ACK/configuración no prueban frecuencia efectiva; OTA existe sólo para subconjuntos. | [API], [UM02], [UM08] |
| `configure_prop()` | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | CP4–CP8, 2026-10-26 | El backend común no implementa todas las modulaciones/frecuencias nombradas por presets. | [API], [P_MATRIX], [P_GAPS] |

### 5.2 PHY BLE e IEEE 802.15.4

| Capability | Implementation | Control | Functional / OTA | Physical RF | Protocol level | Interoperability | Evidence strength | Last validated | Limitations | Sources |
|---|---|---|---|---|---|---|---|---|---|---|
| BLE raw PHY 1M/2M/Coded S8/S2 | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-OTA, 2026-04-08 | Conteos agregados sin raw/binario recuperado; stack BLE de conexión/GATT/scan fue removido. | [ENUMS], [UM02], [HIST] |
| IEEE RX canal 25 | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `CONSOLIDATED_STRONG` | OP-04, 2026-10-21 | Tráfico no atribuido a un peer estándar; `RX_STOP` no es robusto. | [API], [UM03], [UM08] |
| IEEE `TX_RAW` canal 25 | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `CONSOLIDATED_STRONG` | OP-06, 2026-10-21 | 9/10 y 10/10 recientes; marcadores repetidos no son PDR por secuencia. | [PROTO], [UM02], [UM08] |
| IEEE `TX_FRAME` | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `FRAMING` | `NOT_TESTED` | `CONSOLIDATED_LIMITED` | OCT-IEEE, 2026-10-06/12 | 10/10 histórico; comando exacto no quedó preservado y comparte backend RAW. No implica MAC completo. | [PROTO], [UM02], [P_GAPS] |
| IEEE `TX_BURST` | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | OP-08, 2026-10-21 | `scheduled`/ACK no prueban número emitido; 1/5 y 1/40 observados. | [API], [UM02], [UM08] |
| IEEE `TX_CONTINUOUS`, intervalo `0` | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | OP-09B, 2026-10-21 | Actividad repetida demostrada; conteo y temporización físicos no establecidos; cleanup puede agotar timeout. | [API], [UM02], [UM08] |
| IEEE `TX_CONTINUOUS`, intervalo positivo | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | OP-09A, 2026-10-21 | Corridas comparables produjeron un hit; causa no resuelta. | [API], [UM02], [UM08] |
| `TX_STOP` | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_LIMITED` | 2026-10-21 | ACK y algunas ventanas posteriores negativas no miden cese físico inmediato. | [API], [UM08] |
| `RX_STOP` | `PRESENT` | `PARTIAL` | `PARTIAL` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | RF-01/RF-03, 2026-10-21 | Timeouts reproducibles en algunas sesiones, a menudo con eventos RX `0x90` durante la espera. | [PROTO], [UM02], [UM08] |
| Switching IEEE 25→20→25 | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `CONSOLIDATED_STRONG` | RF-01, 2026-10-21 | Una ejecución: 10/10, 0/10, 10/10, 9/10; cleanup final bloqueado. | [UM08], [UM02] |
| Transición IEEE↔Sub-GHz sin reset | `PRESENT` | `PARTIAL` | `BLOCKED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `BLOCKED` | RF-03, 2026-10-20/21 | Ambas corridas se bloquearon en la primera detención IEEE; la transición objetivo no fue alcanzada ni refutada. | [UM02], [UM08] |

### 5.3 Propietaria Sub-GHz y modos TX

| Capability | Implementation | Control | Functional / OTA | Physical RF | Protocol level | Interoperability | Evidence strength | Last validated | Limitations | Sources |
|---|---|---|---|---|---|---|---|---|---|---|
| Entrega raw FSK/GFSK propietaria A→B | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | CP4–CP8, 2026-10-26 | Positiva para 14 presets exactos; no establece fiabilidad universal. Metadata RSSI/CRC/timestamp propietario no es fiable. | [P_EVID], [P_MATRIX], [UM04] |
| Entrega raw Sub-GHz B→A | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | RF-02, 2026-10-20 | 10/10 a parámetros solicitados de 902,2 MHz; frecuencia, ruta y metadata no medidos. | [UM02], [UM03], [HW] |
| Recuperación/cambio de estado Sub-GHz | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_APPLICABLE` | `CONSOLIDATED_STRONG` | CP8, 2026-10-26 | Éxitos con estados lógicos distintos no determinan CTF/U2 ni explican campañas negativas previas. | [P_EVID], [UM02], [UM04] |
| CW | `PRESENT` | `VALIDATED` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_LIMITED` | EV-13/OP-10, 2026-10 | ACK solamente; portadora, frecuencia, potencia y cese no medidos. | [API], [UM02], [UM03] |
| PRBS15 / PRBS32 | `PRESENT` | `VALIDATED` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_LIMITED` | EV-13/OP-10, 2026-10 | ACK solamente; patrón/espectro no medidos; no es BLE DTM PRBS9. | [API], [UM02], [UM03] |
| `TX_TEST_STOP` | `PRESENT` | `VALIDATED` | `PARTIAL` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `CONSOLIDATED_LIMITED` | EV-13/OP-10, 2026-10 | ACK no demuestra cese físico. | [API], [UM03] |

### 5.4 Criptografía y helpers de emulación

Las primitivas crypto no son capacidades RF; `Physical RF` es `NOT_APPLICABLE`. Su «comportamiento funcional» refleja únicamente el smoke histórico reportado, no certificación criptográfica ni reproducción sobre el build actual.

| Capability | Implementation | Control | Functional / OTA | Physical RF | Protocol level | Interoperability | Evidence strength | Last validated | Limitations | Sources |
|---|---|---|---|---|---|---|---|---|---|---|
| TRNG | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Smoke histórico; artefactos/vectores completos y certificación RNG ausentes. | [API], [PROTO], [UM02], [HIST] |
| AES-ECB | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | No repetido con build identificado; vectores primarios no disponibles. | [API], [PROTO], [UM02], [HIST] |
| AES-CTR | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Igual límite de procedencia. | [API], [PROTO], [UM02], [HIST] |
| AES-CBC | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Igual límite de procedencia. | [API], [PROTO], [UM02], [HIST] |
| AES-CCM | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Límites de payload/contrato no quedaron cubiertos exhaustivamente. | [API], [PROTO], [UM02], [HIST] |
| AES-GCM | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Límites de payload/contrato no quedaron cubiertos exhaustivamente. | [API], [PROTO], [UM02], [HIST] |
| SHA-256 | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Smoke one-shot histórico; artefactos primarios ausentes. | [API], [PROTO], [UM02], [HIST] |
| ECDH | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Claim para P-256/Curve25519 no repetido con build actual identificado. | [API], [PROTO], [UM02], [HIST] |
| ECDSA sign | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Firmware soporta P-256; el contrato host permite combinaciones que firmware rechaza. | [API], [PROTO], [UM02], [EVID_REF] |
| ECDSA verify | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NOT_TESTED` | `HISTORICAL_CLAIM` | APR-CRYPTO, 2026-04-30 | Sin reproducción actual ni peer/vector externo preservado. | [API], [PROTO], [UM02], [EVID_REF] |
| Personalidades IEEE de emulación | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `FRAMING` | `NOT_DEMONSTRATED` | `HISTORICAL_CLAIM` | APR-EMU, 2026-05-04 | Firmas/payloads fijos; no state machine, autenticación ni peer independiente. | [EMU], [UM02], [HIST] |
| Personalidades Sub-GHz de emulación | `PRESENT` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_DEMONSTRATED` | `HISTORICAL_CLAIM` | APR-EMU, 2026-05-04 | Incluye etiquetas protocolarias sin stack; no demuestra un dispositivo real. | [EMU], [UM02], [HIST] |
| Personalidades OOK de emulación | `PRESENT` | `PARTIAL` | `BLOCKED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_DEMONSTRATED` | `CONSOLIDATED_LIMITED` | CP7, 2026-10-26 | Helper existe, pero el lifecycle OOK reciente quedó ambiguo. El backend requiere reset real/power-cycle/reflash antes de otro modo; sólo el power-cycle físico fue demostrado en CP8. | [EMU], [P_EVID], [UM03] |

### 5.5 Integraciones, protocolos nombrados y funciones experimentales

| Capability | Implementation | Control | Functional / OTA | Physical RF | Protocol level | Interoperability | Evidence strength | Last validated | Limitations | Sources |
|---|---|---|---|---|---|---|---|---|---|---|
| KillerBee adapter | `PRESENT` | `VALIDATED` | `BLOCKED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_DEMONSTRATED` | `BLOCKED` | RF-04, 2026-10-20 | Código y tests host existen; dependencia bloqueó HIL y no se generó PCAP. | [KB], [UM02], [UM03] |
| Wireless M-Bus S/T/C etiquetado | `PARTIAL` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `CONSOLIDATED_STRONG` | 2026-10-15/16 | Sólo TI proprietary raw; no framing/coding/CRC EN 13757 ni peer estándar. | [P_MATRIX], [P_GAPS], [P_EVID] |
| Wi-SUN etiquetado | `PARTIAL` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `CONSOLIDATED_STRONG` | 2026-10-15/16 | Presets FSK raw a 902,2 MHz solicitados; no SUN-FSK/PHR, FAN, hopping, RPL, IPv6 ni seguridad. | [P_MATRIX], [P_GAPS], [P_EVID] |
| Sidewalk FSK etiquetado | `PARTIAL` | `VALIDATED` | `DEMONSTRATED` | `NOT_VALIDATED` | `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `CONSOLIDATED_STRONG` | 2026-10-15/16 | Entrega raw común; no framing, MAC, autenticación, seguridad ni gateway Sidewalk. | [P_MATRIX], [P_GAPS], [P_EVID] |
| MIOTY etiquetado | `PARTIAL` | `VALIDATED` | `FAILED` | `NOT_VALIDATED` | `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `CONSOLIDATED_STRONG` | CP5, 2026-10-26 | 0/10 es fallo de entrega raw del preset genérico; no es un fallo de TS-UNB/MIOTY implementado. | [P_MATRIX], [P_GAPS], [P_EVID] |
| Zigbee/Thread stack | `NOT_IMPLEMENTED` | `NOT_APPLICABLE` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NO_EVIDENCE` | not validated | El path raw IEEE no aporta NWK/APS/ZDO/ZCL, commissioning, Thread/6LoWPAN ni seguridad de stack. | [P_GAPS], [UM03] |
| Jamming continuo `start_jam()` / `stop_jam()` | `PRESENT` | `VALIDATED` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `HISTORICAL_CLAIM` | baseline histórico; fecha no trazada | ACK de ruta experimental; no se validó efecto de interferencia, frecuencia, potencia ni cese. | [API], [PROTO], [HIST] |
| Jamming reactivo/patrón | `NOT_IMPLEMENTED` | `NOT_APPLICABLE` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NO_EVIDENCE` | not validated | Sólo IDs pendientes; no métodos ni handler/backend actual. | [API], [ENUMS] |
| Spectrum / RSSI scan | `NOT_IMPLEMENTED` | `NOT_APPLICABLE` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_APPLICABLE` | `NOT_APPLICABLE` | `NO_EVIDENCE` | not validated | Existen tipos/builders residuales, pero no API pública, IDs asignados, handler ni data path firmware utilizable. | [API], [ENUMS], [EVID_REF] |
| OOK raw | `PARTIAL` | `PARTIAL` | `BLOCKED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `CONSOLIDATED_STRONG` | CP7/CP8, 2026-10-26 | Configuración existe; CP7 dio 1 ACK, luego timeout/errores, 0/10 y `RX_STOP` timeout. El backend exige reset real/power-cycle/reflash antes de otro modo; CP8 sólo demostró recuperación tras power-cycle físico. | [P_EVID], [P_GAPS], [UM04] |
| MSK / 4-FSK / 4-GFSK nombrados | `NOT_IMPLEMENTED` | `PARTIAL` | `NOT_TESTED` | `NOT_VALIDATED` | `NOT_IMPLEMENTED` | `NOT_TESTED` | `CONSOLIDATED_STRONG` | source review, 2026-10-26 | Wire/firmware aceptan y copian `mod_type=4/5/6`, pero el comando TI activo reserva esos valores; ACK histórico no prueba las modulaciones nombradas. | [P_MATRIX], [P_GAPS], [SRC_RF] |
| Propietaria 2,4 GHz | `PARTIAL` | `PARTIAL` | `NOT_TESTED` | `NOT_VALIDATED` | `RAW_PHY` | `NOT_TESTED` | `CONSOLIDATED_STRONG` | source review, 2026-10-26 | Se solicita 2440 MHz, pero el backend actual cae a configuración 868; no existe validación 2,4 válida. | [P_MATRIX], [P_GAPS], [SRC_RF] |

## 6. Matriz de presets y familias protocolarias

`Present in code` indica que la entrada existe en `PROP_PRESETS`; no afirma que el backend pueda realizar la modulación/frecuencia nombrada. `Raw peer delivery` describe únicamente payload identificable entre CatSniffer.

La cobertura actual llega a entrega raw para 14 presets propietarios exactos. Ningún preset propietario alcanza validación física independiente L3 ni interoperabilidad L4.

| ID / preset | Present in code | Control | Raw peer delivery | Protocol framing / stack | Interop | Physical validation | Evidence | Notes |
|---|---|---|---|---|---|---|---|---|
| P01 `gfsk_433_50k` | `YES` | `VALIDATED` histórico | `NOT_TESTED` actual | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `CONSOLIDATED_LIMITED`, EV-11 | 433 fuera del alcance físico defendible de la campaña actual. |
| P02 `gfsk_433_10k` | `YES` | `VALIDATED` histórico | `NOT_TESTED` actual | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `CONSOLIDATED_LIMITED`, EV-11 | Mismo límite de hardware/antena y procedencia. |
| P03 `fsk_433_50k` | `YES` | `VALIDATED` histórico | `NOT_TESTED` actual | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `HISTORICAL_CLAIM` | Claim antiguo no basta para estado funcional actual. |
| P04 `ook_433_4k8` | `YES` | `PARTIAL` | `NOT_TESTED` actual | TI proprietary raw | `NOT_TESTED` | `NOT_VALIDATED` | `HISTORICAL_CLAIM` | OOK y ruta 433 requieren ensayo aislado y recovery seguro. |
| P05 `ook_433_2k4` | `YES` | `PARTIAL` | `NOT_TESTED` actual | TI proprietary raw | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_LIMITED` | Sin evidencia raw reciente. |
| P06 `msk_433_50k` | `YES`; named backend `NO` | `PARTIAL` | `NOT_TESTED` | `NOT_IMPLEMENTED` | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source | Wire acepta y backend copia `mod_type=4`, pero TI lo reserva; no implementa MSK. |
| P07 `4fsk_433_50k` | `YES`; named backend `NO` | `PARTIAL` | `NOT_TESTED` | `NOT_IMPLEMENTED` | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source | Wire acepta y backend copia `mod_type=5`, pero TI lo reserva; no implementa 4-FSK. |
| P08 `4gfsk_433_50k` | `YES`; named backend `NO` | `PARTIAL` | `NOT_TESTED` | `NOT_IMPLEMENTED` | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source | Wire acepta y backend copia `mod_type=6`, pero TI lo reserva; no implementa 4-GFSK. |
| P09 `gfsk_868_50k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 repetido | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, CP4/CP6/CP8 2026-10-26 | Control conocido; ninguna medición L3. |
| P10 `gfsk_868_100k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Ninguna medición L3. |
| P11 `ook_868_4k8` | `YES` | `PARTIAL` | `BLOCKED`, 0/10 | TI proprietary raw | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, CP7/CP8 2026-10-26 | RF-05 no se ejecutó; CP7 mostró lifecycle inseguro y CP8 sólo recuperó P09 tras power-cycle físico. |
| P12 `msk_868_50k` | `YES`; named backend `NO` | `PARTIAL` | `NOT_TESTED` válido | `NOT_IMPLEMENTED` | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source | Wire acepta y copia `mod_type=4`, reservado por TI; descarta «MSK PASS» como claim físico. |
| P13 `wireless_mbus_s_868` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | TI proprietary; W-MBus `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | No EN 13757 framing/coding/CRC. |
| P14 `wireless_mbus_t_868` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | TI proprietary; W-MBus `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | No stack W-MBus. |
| P15 `wireless_mbus_c_868` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | TI proprietary; W-MBus `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | No stack W-MBus. |
| P16 `wireless_mbus_n_169_2k4` | `YES` | `VALIDATED` histórico | `NOT_TESTED` actual | W-MBus `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_LIMITED`, EV-11 | Ruta/frontend 169 y protocolo no demostrados. |
| P17 `wireless_mbus_n_169_4k8` | `YES` | `VALIDATED` histórico | `NOT_TESTED` actual | W-MBus `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_LIMITED`, EV-11 | Ruta/frontend 169 y protocolo no demostrados. |
| P18 `4fsk_868_50k` | `YES`; named backend `NO` | `PARTIAL` | `NOT_TESTED` válido | `NOT_IMPLEMENTED` | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source | Wire acepta y copia `mod_type=5`, reservado por TI; descarta «4-FSK PASS» físico. |
| P19 `4gfsk_868_50k` | `YES`; named backend `NO` | `PARTIAL` | `NOT_TESTED` válido | `NOT_IMPLEMENTED` | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source | Wire acepta y copia `mod_type=6`, reservado por TI; descarta «4-GFSK PASS» físico. |
| P20 `gfsk_915_50k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | 915 MHz es solicitado, no medido. |
| P21 `gfsk_902_50k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | TI proprietary raw | `NOT_APPLICABLE` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | 902,2 MHz es solicitado, no medido. |
| P22 `gfsk_2440_250k` | `YES`; valid backend `NO` | `PARTIAL` | `NOT_TESTED` válido | TI proprietary raw | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source, 2026-10-26 | Backend actual cae a 868; claim 2,4 histórico no es válido. |
| P23 `gfsk_2440_50k` | `YES`; valid backend `NO` | `PARTIAL` | `NOT_TESTED` válido | TI proprietary raw | `NOT_TESTED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG` source, 2026-10-26 | Backend actual cae a 868. |
| P24 `sidewalk_915_fsk_50k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | Sidewalk `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Sólo FSK raw común. |
| P25 `sidewalk_915_fsk_250k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | Sidewalk `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Sólo FSK raw común. |
| P26 `wisun_915_fsk_50k` | `YES` | `VALIDATED` | `DEMONSTRATED`, repetido | Wi-SUN `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/20 | El nombre dice 915; solicita 902,2 MHz. No FAN/stack. |
| P27 `mioty_868_tsunb` | `YES` | `VALIDATED` | `FAILED` raw, 0/10 | MIOTY/TS-UNB `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, CP5 2026-10-26 | Generic FSK 396 sym/s; no splitting/coding/interleaving/bursts TS-UNB. |
| P28 `wisun_915_fsk_100k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | Wi-SUN `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Solicita 902,2 MHz; raw solamente. |
| P29 `wisun_915_fsk_150k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | Wi-SUN `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Solicita 902,2 MHz; raw solamente. |
| P30 `wisun_915_fsk_200k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | Wi-SUN `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Solicita 902,2 MHz; raw solamente. |
| P31 `wisun_915_fsk_300k` | `YES` | `VALIDATED` | `DEMONSTRATED`, 10/10 | Wi-SUN `NOT_IMPLEMENTED` | `NOT_DEMONSTRATED` | `NOT_VALIDATED` | `CONSOLIDATED_STRONG`, 2026-10-15/16 | Solicita 902,2 MHz; raw solamente. |

La base detallada de cada P01–P31 y los huecos de framing están en [PRESET_MATRIX][P_MATRIX] e [IMPLEMENTATION_AND_PROTOCOL_GAPS][P_GAPS].

## 7. Contradicciones preservadas

| Área | Evidencia en tensión | Estado canónico actual |
|---|---|---|
| Sub-GHz | F29 70/70; campaña posterior 0/70 y 0/240; recuperación 60/60; después 13 presets con 10/10 y 14 presets exactos cubiertos en total | La entrega raw está demostrada para los presets exactos positivos; ni fiabilidad universal ni regresión permanente están establecidas. La causa de la variación sigue abierta. |
| OOK 868 | Claim histórico 10/10 frente a dos secuencias controladas posteriores con primer ACK, timeouts/errores, 0/10 y cleanup fallido | Implementación `PARTIAL`, resultado reciente `BLOCKED`; no se borra el claim histórico ni se lo promociona a estado actual. |
| Propietaria 2,4 GHz | Claim positivo antiguo frente a 0 peer posterior y fallback a 868 confirmado por source | No existe validación actual válida de 2,4 GHz; la frecuencia efectiva histórica es desconocida. |
| `band1`/`band2` | Entrega Sub-GHz positiva reportando estados lógicos distintos | El estado lógico no prueba CTF/U2 ni throw físico; la topología pertenece a hardware canónico. |
| IEEE channel switching | Una corrida no alcanzó el switching; otra completó 25→20→25 pero falló cleanup | Capacidad estrecha `PARTIAL`; experimento global no se convierte en PASS. |
| IEEE↔Sub-GHz | RF-03 se bloqueó en `RX_STOP` antes de cambiar PHY | `BLOCKED`; no se registra como fallo de la transición no alcanzada. |

## 8. Límites y gaps abiertos

1. No hay identidad completa source→artefacto→dispositivo para las campañas; `1.0.0` no es un build ID.
2. No hay medición independiente actual de frecuencia, potencia, modulación, desviación, bandwidth, espectro, timing, cese, CTF/U2 ni ruta de antena. Por eso ninguna fila RF de esta matriz usa `Physical RF = MEASURED`.
3. El parser de metadata propietario invierte el orden TI esperado; `crc_ok`, RSSI, timestamp, channel y LQI no sustentan claims físicos.
4. La semántica de completion de BURST, CONTINUOUS, STOP y TX ACK es insuficiente para contar emisiones físicas.
5. `RX_STOP` tiene timeouts reproducibles y bloquea campañas compuestas.
6. OOK requiere lifecycle/recovery seguro y caracterización física antes de ser capacidad estable.
7. Crypto necesita repetición con vectores y build identificados; los 9/9 permanecen como claim histórico.
8. KillerBee necesita dependencia reproducible, HIL y PCAP exitoso.
9. Los stacks Wi-SUN, Sidewalk, Wireless M-Bus, MIOTY, Zigbee y Thread no están implementados por el mero hecho de existir PHY/presets raw.
10. Spectrum carece de data path; jamming continuo carece de criterio de efecto físico.

## 9. Registro de fuentes

### Implementación actual

- [API]: [`python/feralrf/radio.py`](../python/feralrf/radio.py), superficie host y semántica pública.
- [ENUMS]: [`python/feralrf/enums.py`](../python/feralrf/enums.py), IDs y estado estable/experimental/pendiente.
- [PRESETS]: [`python/feralrf/presets.py`](../python/feralrf/presets.py), catálogo P01–P31.
- [PROTO]: [`docs/protocol.md`](protocol.md), contrato wire y fuentes firmware enlazadas.
- [SRC_RF]: [`firmware/cc1352/src/radio_if.c`](../firmware/cc1352/src/radio_if.c), backend RF consultado selectivamente.
- [EMU]: [`python/feralrf/emulation/`](../python/feralrf/emulation/), helpers de personalidades, no stacks.
- [KB]: [`python/feralrf/integrations/killerbee.py`](../python/feralrf/integrations/killerbee.py), adaptador host.

### Síntesis y trazabilidad experimental

- [P_MATRIX]: [`Presets_Reports/PRESET_MATRIX.md`](Presets_Reports/PRESET_MATRIX.md), matriz P01–P31.
- [P_GAPS]: [`Presets_Reports/IMPLEMENTATION_AND_PROTOCOL_GAPS.md`](Presets_Reports/IMPLEMENTATION_AND_PROTOCOL_GAPS.md), backend/framing y gaps de stack.
- [P_EVID]: [`Presets_Reports/EXPERIMENTAL_EVIDENCE.md`](Presets_Reports/EXPERIMENTAL_EVIDENCE.md), CP4–CP8 y cobertura raw actual.
- [UM02]: [`User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md`](<User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md>), inventario de 193 ejecuciones.
- [UM03]: [`User-Manual/03 - Capacidades demostradas y catálogo de presets.md`](<User-Manual/03 - Capacidades demostradas y catálogo de presets.md>), síntesis por nivel.
- [UM04]: [`User-Manual/04 - Evidencia, procedencia y preguntas abiertas.md`](<User-Manual/04 - Evidencia, procedencia y preguntas abiertas.md>), calidad, contradicciones y gaps.
- [UM08]: [`User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md`](<User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>), revalidación del 21-oct.
- [HIST]: [`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md), baseline histórico abril–mayo; no autoridad actual.
- [EVID_REF]: [`HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md`](HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md), referencia histórica auxiliar usada sólo para claims sin primarios completos.

### Hardware

- [HW]: [`hardware/README.md`](../hardware/README.md), entrada canónica de hardware.
- [HW_OPEN]: [`hardware/OPEN_HARDWARE_QUESTIONS.md`](../hardware/OPEN_HARDWARE_QUESTIONS.md), límites físicos y preguntas abiertas.

## 10. Regla de mantenimiento

Una fila o estado sólo puede cambiar si la actualización registra, hasta donde aplique:

1. baseline/commit del source y estado dirty;
2. identidad de hardware y revisión hasta donde sean conocidas, sin inferir población física;
3. script y versión/hash de procedimiento;
4. comando exacto y working directory;
5. resultado observado y criterio usado;
6. artefacto de evidencia preservado;
7. resultado de cleanup y estado posterior;
8. limitaciones y aquello que el ensayo no demuestra.

La ejecución reproducible, sus precondiciones y cleanup se mantienen en el [Validation Runbook](validation/runbook/README.md); esta matriz no duplica esos pasos.

Un resultado nuevo no elimina evidencia contradictoria. Debe relacionarse como:

- `reproduction`: repite el resultado bajo condiciones comparables;
- `contradiction`: produce un resultado incompatible sin causa resuelta;
- `supersession under changed conditions`: reemplaza una interpretación porque cambiaron condiciones o implementación identificadas;
- `new capability evidence`: cubre una dimensión antes no ejercitada.

Si cambia la implementación sin repetir el ensayo, puede cambiar `Implementation`, pero `Control`, `Functional / OTA`, `Physical RF` e `Interoperability` no se promueven automáticamente. No se usa un PASS único para resumir esas dimensiones.

[API]: ../python/feralrf/radio.py
[ENUMS]: ../python/feralrf/enums.py
[PRESETS]: ../python/feralrf/presets.py
[PROTO]: protocol.md
[SRC_RF]: ../firmware/cc1352/src/radio_if.c
[EMU]: ../python/feralrf/emulation/
[KB]: ../python/feralrf/integrations/killerbee.py
[P_MATRIX]: Presets_Reports/PRESET_MATRIX.md
[P_GAPS]: Presets_Reports/IMPLEMENTATION_AND_PROTOCOL_GAPS.md
[P_EVID]: Presets_Reports/EXPERIMENTAL_EVIDENCE.md
[UM02]: <User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md>
[UM03]: <User-Manual/03 - Capacidades demostradas y catálogo de presets.md>
[UM04]: <User-Manual/04 - Evidencia, procedencia y preguntas abiertas.md>
[UM08]: <User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>
[HIST]: VALIDATION_MATRIX.md
[EVID_REF]: HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md
[HW]: ../hardware/README.md
[HW_OPEN]: ../hardware/OPEN_HARDWARE_QUESTIONS.md
