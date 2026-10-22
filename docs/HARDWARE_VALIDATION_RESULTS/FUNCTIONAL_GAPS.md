# Evaluación de brechas funcionales de FeralRF

**Corte:** 2026-10-22  
**Fuente:** `feature/hardware-validation` @ `fbff1b7f5077964418fb63a3fdf346a27c35a003`

## 1. Criterio de clasificación

Este registro distingue defecto, incompletitud, limitación y falta de evidencia. La ausencia de una pila opcional no se considera defecto salvo que un requisito aprobado o la superficie pública prometa ese rol.

La ausencia de stdout/JSONL no convierte un reporte experimental narrativo en “no evidencia”. Cuando un reporte documenta condiciones, conteos y observaciones, éstas se conservan como resultados reportados; la falta de primarios afecta inspección independiente, reproducción exacta o explicación causal. Véanse la [taxonomía de evidencia](EXPERIMENTAL_EVIDENCE_REFERENCE.md) y la [evaluación de readiness](PENDING_VALIDATION_ASSESSMENT.md).

| Categoría | Uso en este documento |
|---|---|
| Defecto confirmado | El source viola un contrato o layout definido |
| Defecto probable / mecanismo no confirmado | Existe un mecanismo plausible; falta demostrar su efecto runtime |
| Funcionalidad existente incompleta | La ruta existe, pero no cumple todo su propósito anunciado |
| Limitación de API | La API no expresa, valida u observa suficientemente una capacidad |
| Limitación de soporte protocolario | Sólo existen capas inferiores o parámetros nominales |
| Incertidumbre de hardware | No está establecida la capacidad física de la revisión evaluada |
| Brecha de validación | La función puede existir, pero no ha sido demostrada adecuadamente |
| Discordancia documental | La afirmación excede source/evidencia o describe otra revisión |
| Mejora opcional | Útil, pero no es requisito actual confirmado |

## 2. Registro consolidado

### GAP-01 — Layout de metadata proprietary

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado**, fuente + síntoma experimental |
| Subsistema / capa | CC1352P7; `RadioIF` RX proprietary |
| Comportamiento actual | RX configura `bAppendRssi`, `bAppendTimestamp`, `bAppendStatus`; el RF core agrega RSSI→timestamp→status, pero `RadioIF_processPropPackets()` calcula `status_idx=pdu_len`, `rssi_idx=status+1`, `timestamp_idx=rssi+1`. Además LQI se fija en cero y `channel` refleja estado residual |
| Comportamiento requerido | Interpretar el orden exacto del RF command/SDK, definir si header/length/CRC forman parte de `Packet.data` y reportar metadata con unidades/semántica verificables |
| Evidencia | `radio_if.c`, `RadioIF_startSub1ghzRfBackend()` L1631–1642 y `RadioIF_processPropPackets()` L1706–1754; RF-02 reportó `channel=25` y RSSI incoherente |
| Consecuencia | CRC/filter, RSSI y timestamp pueden ser incorrectos; análisis de enlace y automatización no son confiables |
| Capacidades afectadas | Todo RX proprietary, stats, presets, integraciones y validación Sub-GHz |
| Dependencias | Contrato de bytes RX; SDK exacto; identidad del binario |
| Validación necesaria | Vectores/paquetes controlados, comparación con layout TI y captura independiente |
| Implicación de desarrollo | Corregir parser C, tests unitarios de layout y posiblemente versionar el formato RX |

### GAP-02 — Multiplexado y secuencia de eventos RX

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado de contrato host**; relación completa con `RX_STOP` no confirmada |
| Subsistema / capa | Python transport/API + protocolo wire |
| Comportamiento actual | Firmware emite `RSP_RX_PACKET` con secuencia propia 0…254; `_read_response()` sólo preserva como asincrónicos los `seq=0` y descarta respuestas con secuencia distinta a `_last_seq` durante una espera síncrona |
| Comportamiento requerido | Todo evento RX válido que compita con un comando síncrono se entrega exactamente una vez sin depender de `seq`; un canal de eventos inequívoco o dispatcher único correlaciona cada response/operación |
| Evidencia | `data_task.c` L59–65; `radio.py:_read_response()` L278–397; RF-01/RF-03/OP-09 muestran `last=0x90` durante timeouts |
| Consecuencia | Pérdida host-visible de paquetes; stop y comandos concurrentes poco confiables |
| Capacidades afectadas | RX, RX_STOP, callbacks, KillerBee, transiciones PHY |
| Dependencias | Diseño de versión/compatibilidad del protocolo; política de drain |
| Validación necesaria | Tests de interleaving y HIL con command/seq/event IDs y carga sostenida |
| Implicación de desarrollo | Dispatcher, cola de eventos y semántica separada para responses/events |

### GAP-03 — `RX_STOP` intermitente

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto experimental confirmado; mecanismo no confirmado** |
| Subsistema / capa | Python↔CC lifecycle, colas y transporte |
| Comportamiento actual | Tras tráfico IEEE, `stop_rx()` puede agotar retries/timeout mientras consume `RSP_RX_PACKET`; ocurrió en RF-01, RF-03 y OP-09B |
| Comportamiento requerido | Stop idempotente y acotado, ACK correlacionado, política explícita para paquetes pendientes y estado final observable |
| Evidencia | Reportes RF-01/RF-03 del 20 y 21-oct; código host/firmware |
| Consecuencia | Bloquea switching estable y transición IEEE↔Sub-GHz; cleanup global BLOCKED |
| Capacidades afectadas | RX, switching, automatización HIL, recuperación |
| Dependencias | GAP-02, GAP-08 y telemetría de colas |
| Validación necesaria | Trazas simultáneas host/UART/OutputIF/RP, ocupación y contadores por etapa |
| Implicación de desarrollo | No atribuir todavía una única corrección; instrumentar primero, luego ajustar drain/ACK/estado |

### GAP-04 — Resultado diferido de TX y completion

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Funcionalidad existente incompleta / limitación de API**, confirmada |
| Subsistema / capa | Python, wire protocol, `ControlTask`/`RadioIF` |
| Comportamiento actual | RAW/BURST/CONTINUOUS responden ACK al aceptar estado; la ejecución ocurre después. BURST sólo decrementa `remaining` si `RadioIF_transmitRaw()` devuelve éxito y, ante fallo, se limpia en silencio; CONTINUOUS también se limpia sin evento. RAW puede emitir error genérico, pero no hay progreso/fin para BURST/CONTINUOUS |
| Comportamiento requerido | `operation_id` y eventos `started/progress/completed/failed/stopped`, con conteo definido |
| Evidencia | `command_processor.c` L210–285; `control_task.c` L249–408; BURST 1/5 y 1/40 |
| Consecuencia | El host no distingue programado, intento ejecutado o recibido; un marker peer no equivale a un intento TX y la validación de `count` es imposible |
| Capacidades afectadas | RAW, FRAME, BURST, CONTINUOUS, TX_STOP, emulación |
| Dependencias | Definir semántica de éxito; versionado wire |
| Validación necesaria | Status TI + events + receptor independiente y fault injection |
| Implicación de desarrollo | Máquina de operación TX y contrato backwards-compatible |

### GAP-05 — Modulaciones reservadas expuestas como válidas

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado de API/documentación** |
| Subsistema / capa | Presets Python, configuración CC y contrato TI |
| Comportamiento actual | Presets usan `mod_type=4/5/6` para MSK/4-FSK/4-GFSK; firmware copia el valor a `CMD_PROP_RADIO_DIV_SETUP.modType` |
| Comportamiento requerido | Rechazar valores reservados o implementar el comando/patch TI específico que produzca la modulación |
| Evidencia | `presets.py`; SDK 8.30.01.01 `rf_prop_cmd.h` L475–478/L581–584 (`0=FSK,1=GFSK,2=OOK`, otros reservados) |
| Consecuencia | ACK y pruebas estáticas pueden generar claims físicos falsos |
| Capacidades afectadas | Seis presets nominales: `msk_433_50k`, `4fsk_433_50k`, `4gfsk_433_50k`, `msk_868_50k`, `4fsk_868_50k`, `4gfsk_868_50k`, además de sus tests |
| Dependencias | Decidir si esas modulaciones son requisito |
| Validación necesaria | Analizador/receptor conocido después de seleccionar backend oficial |
| Implicación de desarrollo | Corregir enums/presets/tests; no remapear números arbitrariamente |

### GAP-06 — Semántica y validación de `SET_PROP_CONFIG`

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Limitación de API / funcionalidad incompleta**, confirmada |
| Subsistema / capa | Python builder, command processor, `RadioIF` |
| Comportamiento actual | Parámetros se copian con validación semántica mínima; `RadioIF_setPropConfig()` retorna `void`; se responde ACK sin aceptación del RF core ni readback |
| Comportamiento requerido | Validar combinaciones por backend/banda, devolver requested/accepted/effective y errores específicos |
| Evidencia | `command_processor.c` L168–185; `radio_if.c:RadioIF_setPropConfig()` |
| Consecuencia | 169/2440/reservados pueden parecer configurados; debugging depende de inferencias |
| Capacidades afectadas | Todo proprietary y presets |
| Dependencias | Modelo tipado de configuración, hardware targets |
| Validación necesaria | Boundary tests, query/readback y medición RF |
| Implicación de desarrollo | Nuevo response de configuración o `GET_CONFIG`; compatibilidad a definir |

### GAP-07 — Potencia efectiva no observable

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Limitación de API**, confirmada |
| Subsistema / capa | CC `RadioIF`, API Python, hardware |
| Comportamiento actual | El valor solicitado se resuelve contra tablas y puede aproximarse a otra entrada; no se devuelve valor efectivo ni tipo de PA |
| Comportamiento requerido | Rechazo o readback explícito del valor efectivo y límites por PHY/placa/región |
| Evidencia | `radio_if.c:RadioIF_resolveTxPowerValue()`, F-17 |
| Consecuencia | ACK no sustenta claims de dBm; riesgo técnico/regulatorio |
| Capacidades afectadas | TX, CW/PRBS, jamming, high-PA |
| Dependencias | BOM/front-end y política regional |
| Validación necesaria | Power meter por banda/PA/carga |
| Implicación de desarrollo | Capability table + effective config response |

### GAP-08 — Métricas y pérdidas de transporte incompletas

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Funcionalidad existente incompleta**, confirmada |
| Subsistema / capa | CC queues, RP bridge, Python |
| Comportamiento actual | `GET_STATS` cubre métricas RF/LL; `PacketQueue` mantiene drops internos no incluidos; RP cuenta CC→USB pero no verifica USB→CC; host no expone parse/drop counters |
| Comportamiento requerido | Contadores monotónicos/ocupación por etapa, snapshot correlacionado y reset explícito |
| Evidencia | `packet_queue.c`, `output_if.c`, RP `main.c` L202–251, `shell_commands.c` L235–274 |
| Consecuencia | No puede localizarse una pérdida o timeout |
| Capacidades afectadas | RX_STOP, RX continuo, respuestas y HIL |
| Dependencias | GAP-02 y versionado stats |
| Validación necesaria | Saturación controlada/fault injection |
| Implicación de desarrollo | Extender stats CC/RP/host y conservar compatibilidad |

### GAP-09 — Cola compartida sin sincronización visible

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto probable / mecanismo no confirmado** |
| Subsistema / capa | CC TI-RTOS, `PacketQueue` |
| Comportamiento actual | `head`, `tail`, `count` y buffer son accedidos por tareas RF/UART sin mutex, critical section o atomics visibles |
| Comportamiento requerido | SPSC formalmente seguro o protección TI-RTOS documentada |
| Evidencia | `packet_queue.c`; callers `OutputIF`/`HostIFTask` |
| Consecuencia | Riesgo de data race, corrupción o drop bajo interleaving |
| Capacidades afectadas | Todas las responses y eventos |
| Dependencias | Modelo de memoria/toolchain y scheduler real |
| Validación necesaria | Stress/instrumentación; análisis de concurrencia |
| Implicación de desarrollo | Reemplazar por mecanismo SPSC/RTOS sólo después de confirmar supuestos |

### GAP-10 — Descubrimiento de Cat-Shell y control de banda fuera de la API

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado de integración / limitación de API** |
| Subsistema / capa | Python + RP2040 |
| Comportamiento actual | Shell = Bridge + 2; reset no valida replies; band selection sólo se realiza con scripts externos |
| Comportamiento requerido | Asociar interfaces por dispositivo/serial/location, aceptar puerto explícito y leer estado/resultados versionados |
| Evidencia | `radio.py` L184–246; COM88/COM87; RP Cat-Shell source |
| Consecuencia | Riesgo de controlar interfaz/unidad incorrecta; automatización no portable |
| Capacidades afectadas | Reset, recovery, 2.4/Sub-G, HIL |
| Dependencias | Descriptores/metadata RP y soporte por SO |
| Validación necesaria | Windows/Linux/macOS, múltiples unidades y reenumeración |
| Implicación de desarrollo | Objeto Device/Board con Bridge/Shell explícitos |

### GAP-11 — Identidad de firmware y hardware insuficiente

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Funcionalidad existente incompleta**, confirmada |
| Subsistema / capa | Build CC, protocolo info, RP metadata, procedimiento experimental |
| Comportamiento actual | CC devuelve versión/serial constantes; RP source puede reportar build, pero los runs no preservan hashes; revisión P7 exacta/BOM no está cerrada |
| Comportamiento requerido | Git SHA, dirty flag, SDK, target, feature bitmap, hash binario y board ID/revision |
| Evidencia | `ControlTask_getInfoPayload()`; historial de campañas; esquema P1 vs marcado P74 |
| Consecuencia | No hay regresión reproducible ni comparación causal entre campañas |
| Capacidades afectadas | Toda validación y soporte |
| Dependencias | Pipeline de build y provisioning |
| Validación necesaria | Manifest leído desde dispositivo y verificado contra artefacto |
| Implicación de desarrollo | Header/version response ampliado y formato de manifest |

### GAP-12 — `RADIO_INIT` no reinicializa `RadioIF`

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Incompletitud frente a posible contrato**, confirmada por fuente |
| Subsistema / capa | CC lifecycle |
| Comportamiento actual | `RadioIF_init()` se llama en `DataTask_init()`; `CMD_RADIO_INIT` resetea control/stats y detiene RX, pero no reinicializa backend |
| Comportamiento requerido | Definir si init es reset lógico o reinicialización RF; implementar y documentar una sola semántica |
| Evidencia | `data_task.c` L68–74; `control_task.c` L211–223 |
| Consecuencia | El usuario puede esperar una recuperación más fuerte que la ejecutada |
| Capacidades afectadas | Recovery, OOK, cambios de modo |
| Dependencias | Restricciones TI/OOK y compatibilidad |
| Validación necesaria | Estado sucio/fallos inducidos y power-cycle baseline |
| Implicación de desarrollo | Posible comando `RESET_STATE` separado de `INIT` |

### GAP-13 — OOK lifecycle y recuperación

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Funcionalidad incompleta + brecha de validación** |
| Subsistema / capa | CC RF patches, Python/operación, hardware |
| Comportamiento actual | Modo OOK existe; un reporte histórico declara 10/10 raw a 868 MHz. Los patches `genook` no pueden descargarse con seguridad; `s_prop_ook_active` fuerza `Prop0_modeOok` y cambiar después de modo/frecuencia requiere power-cycle. RF-05 no corrió porque sus gates y autorización específica no se cumplieron |
| Comportamiento requerido | Lifecycle explícito, gate de entrada, estado terminal y recuperación verificable |
| Evidencia | `radio_if.c:RadioIF_getPropMode()` L173–185 y `RadioIF_setPropConfig()`; [`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) §4/§5; [RF-05 §5.4](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| Consecuencia | Sesión puede quedar inutilizable para otros modos; riesgo de confundir control con enlace |
| Capacidades afectadas | OOK y pruebas posteriores |
| Dependencias | Hardware 868, power-cycle controlado y build identity |
| Validación necesaria | Repetición RF-05 compuesta con power-cycle y control IEEE posterior; no para invalidar el histórico, sino para cubrir build/lifecycle actuales |
| Implicación de desarrollo | Exponer estado/restricción; investigar backend o aislamiento de sesión |

### GAP-14 — Proprietary 2.4 GHz cae a 868 MHz

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado**, fuente |
| Subsistema / capa | CC proprietary backend |
| Comportamiento actual | `configure_prop(2440 MHz)` almacena 2440 MHz y `loDivider=0`, pero `RadioIF_applySub1ghzChannelConfig()` sólo aplica `frequency_hz` si es `<1000 MHz`; para la ruta proprietary GFSK de ejecución termina seleccionando el fallback 868 MHz con configuración/overrides Sub-GHz |
| Comportamiento requerido | Backend/patch/structures de proprietary 2.4 GHz y selección `band1`, o rechazo explícito |
| Evidencia | `radio_if.c` L1578–1612; historial contiene una ruta `smartrf_prop_2_4ghz` retirada |
| Consecuencia | Posible transmisión en banda distinta de la solicitada; ensayo actual inseguro/no interpretable |
| Capacidades afectadas | `gfsk_2440_*` |
| Dependencias | Diseño RF, frontend 2.4, coexistencia handles |
| Validación necesaria | Analizador de espectro y peer tras corrección |
| Implicación de desarrollo | Bloquear presets ahora; evaluar/restaurar backend dedicado con revisión |

### GAP-15 — Ruta 169 MHz incompatible/incoherente

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Hardware no soportado + defecto de configuración** |
| Subsistema / capa | Silicio/placa + CC backend |
| Comportamiento actual | 169.45 MHz selecciona modo 433; overrides 169 se escriben en estructura compartida mientras RX/TX usa estructuras 433; banda fuera del rango defendible del P7/U4 publicado |
| Comportamiento requerido | Rechazo por target o hardware/backend diseñado para 169 MHz |
| Evidencia | `radio_if.c:RadioIF_getPropMode()` L173–185 y `RadioIF_setPropConfig()` L2148–2159; [integración §6](HARDWARE_INTEGRATION.md) |
| Consecuencia | Presets aceptados sin capacidad física defendible |
| Capacidades afectadas | `wireless_mbus_n_169_*` |
| Dependencias | Decisión de target alterno |
| Validación necesaria | No ejecutar RF en la placa actual; primero diseño/BOM |
| Implicación de desarrollo | Capability gating y catálogo por hardware |

### GAP-16 — Hardware 433 MHz no establecido

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Incertidumbre de hardware** |
| Subsistema / capa | PCB/front-end/antena |
| Comportamiento actual | Ruta 433 existe; el reporte histórico consigna GFSK 6–10/10, FSK 1/10 y OOK 0/10, además de otros claims OOK contradictorios. No hay BOM/ECO/medición que permita trasladar causas o cobertura a las placas P7 actuales |
| Comportamiento requerido | Matriz board-revision/front-end/antenna y medición pasiva/OTA controlada |
| Evidencia | Esquema histórico P1, U8 físico P74 y análisis de [integración §1/§6](HARDWARE_INTEGRATION.md) |
| Consecuencia | Falso FAIL o riesgo de emitir con mala adaptación |
| Capacidades afectadas | FSK/GFSK/OOK 433 |
| Dependencias | Identidad de hardware y antena |
| Validación necesaria | Inspección física, VNA/analizador y entorno autorizado |
| Implicación de desarrollo | No corregir firmware antes de cerrar hardware |

### GAP-17 — API crypto inconsistente y límites desiguales

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado de API + brecha de validación** |
| Subsistema / capa | Python crypto, wire, CC `crypto_engine` |
| Comportamiento actual | Host permite Curve25519 para ECDSA; firmware implementa P-256. Validación de tamaños es desigual, especialmente CCM/GCM frente al frame de 255 bytes. Un reporte histórico documenta 9/9 hardware smoke; no se repitió sobre el build actual |
| Comportamiento requerido | Combinaciones válidas por operación/curva y límites antes del I/O; errores específicos |
| Evidencia | `radio.py:ecdsa_sign/verify()`, `crypto_engine.c`, `protocol.h` |
| Consecuencia | Solicitudes imposibles o error tardío. El 9/9 reportado se conserva, pero su ausencia de primarios/build ID limita inspección y reproducción |
| Capacidades afectadas | ECDSA, CCM/GCM y seguridad futura |
| Dependencias | Contrato de chunking/streaming |
| Validación necesaria | Vectores conocidos ejecutados en hardware |
| Implicación de desarrollo | Corregir validación y tests HIL; no equiparar primitivas con seguridad de protocolo |

### GAP-18 — Builders truncan parámetros / validación parcial

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Defecto confirmado de API** |
| Subsistema / capa | Python `commands.py`, parsing responses |
| Comportamiento actual | Algunos campos se enmascaran a enteros menores; no todos los máximos por PHY/comando se validan; parseo RX no valida exhaustivamente longitudes |
| Comportamiento requerido | Rechazo local consistente con rango/nivel de cada comando |
| Evidencia | `commands.py`; `radio.py:read_packets()`; F-10/F-12 |
| Consecuencia | Error del caller convertido en valor distinto, frame rechazado o parseo ambiguo |
| Capacidades afectadas | Configuración, TX, crypto, RX |
| Dependencias | Esquema único de comandos |
| Validación necesaria | Boundary/fuzz/property tests host↔C |
| Implicación de desarrollo | Tipos/rangos declarativos y tests espejo |

### GAP-19 — KillerBee no es una integración reproducible

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Funcionalidad existente incompleta** |
| Subsistema / capa | Packaging Python, adapter, lifecycle IEEE |
| Comportamiento actual | Existe `KBCatSniffer` y patch, pero el extra pide `killerbee>=3.0.0` no resoluble en RF-04; FCS/metadata/error handling y cleanup no están cerrados |
| Comportamiento requerido | Fork/versión soportada instalable, contrato FCS/canal/metadata, errores y PCAP reproducible |
| Evidencia | `python/pyproject.toml`, `integrations/killerbee.py`, RF-04 |
| Consecuencia | Función anunciada no se puede validar desde instalación estándar |
| Capacidades afectadas | Captura Zigbee/IEEE, inject/jam |
| Dependencias | GAP-02/03 y decisión de distribución |
| Validación necesaria | Entorno limpio + peer + PCAP + controles negativos |
| Implicación de desarrollo | Vendor/fork/adapter package o documentación de instalación exacta |

### GAP-20 — Emuladores y nombres de protocolo exceden el nivel real

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Limitación de soporte protocolario + discordancia documental** |
| Subsistema / capa | Presets/helpers/documentación |
| Comportamiento actual | Helpers IEEE/W-MBus/OOK producen tramas/patrones fijos y un reporte histórico documenta entrega de firmas raw. W-MBus usa preset MSK reservado; Wi-SUN/Sidewalk/MIOTY son parámetros nominales |
| Comportamiento requerido | Etiquetar como raw/experimental o implementar framing, parser, procedimientos, seguridad e interoperabilidad del rol aprobado |
| Evidencia | `presets.py`, `emulation/*` y [`PROJECT_STATUS_REPORT.md` §7](PROJECT_STATUS_REPORT.md) |
| Consecuencia | Riesgo de presentar presets o una entrega raw históricamente válida como stacks/protocolos interoperables |
| Capacidades afectadas | W-MBus, Wi-SUN, Sidewalk, MIOTY, Zigbee/Thread, OOK devices |
| Dependencias | Decisión de alcance del producto |
| Validación necesaria | Peer independiente y versión/rol por protocolo |
| Implicación de desarrollo | Primero corregir taxonomía; luego seleccionar sólo protocolos requeridos |

### GAP-21 — MIOTY carece de TS-UNB

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Limitación de soporte protocolario**, no defecto obligatorio |
| Subsistema / capa | Preset, RF backend y capas superiores |
| Comportamiento actual | `mioty_868_tsunb` pide 868000000 Hz, FSK (`mod_type=0`), 396 baud, `deviation=1`, `rx_bw=0x4A` y sync genérico `0x930B51DE`. Con `pre_scale=15`, el cálculo produce `rateWord=259`, o 395.20263671875 baud teóricos; con `deviationStepSz=0`, la desviación nominal teórica es 250 Hz. No hay readback ni medición |
| Comportamiento requerido | Si se aprueba: rol/modo [ETSI TS 103 357-2](https://www.etsi.org/deliver/etsi_ts/103300_103399/10335702/02.01.01_60/ts_10335702v020101p.pdf), splitting, coding/interleaving, scheduler temporal/frecuencial de sub-ráfagas, decoder/reensamble, procedimientos, seguridad y peer/base station |
| Evidencia | `presets.py` L236–243; `radio.py:configure_prop()`; `commands.py:set_prop_config()` (`<IBIHBIH`); `command_processor.c` L168–185; `radio_if.c` L2020–2170; SDK `rf_prop_cmd.h` |
| Consecuencia | El nombre sugiere una capacidad inexistente; no hay ruta TS-UNB alternativa y el claim raw 0/10, sin primario preservado, no evalúa MIOTY |
| Capacidades afectadas | MIOTY |
| Dependencias | Requisito académico/producto y estudio RF core |
| Validación necesaria | Nivel A: control/configuración; nivel B opcional: marker FSK genérico; niveles C/D: vectores TS-UNB y peer/base station independiente tras implementar las capas ausentes |
| Implicación de desarrollo | Investigación antes de estimar: prototipo offline, factibilidad TI/vendor, scheduler/backend y decoder/reensamble; después decidir implementación, modem externo o exclusión |

### GAP-22 — Spectrum, jamming avanzado y stacks opcionales

| Campo | Evaluación |
|---|---|
| Categoría / certeza | **Mejora opcional**, salvo requisito futuro |
| Subsistema / capa | Firmware/API/seguridad/UX |
| Comportamiento actual | Spectrum carece de backend; jamming sólo continuo y acotado; no hay pilas completas Zigbee/Thread/Wi-SUN/Sidewalk/W-MBus/BLE conectada |
| Comportamiento requerido | Sólo después de seleccionar un rol y criterios de seguridad/aceptación |
| Evidencia | `commands.py`, `protocol.h`, handlers firmware y [`PROJECT_STATUS_REPORT.md` §6–7](PROJECT_STATUS_REPORT.md) |
| Consecuencia | Ninguna para un producto definido como PHY/raw si la documentación es precisa |
| Capacidades afectadas | Expansiones futuras |
| Dependencias | Aprobación del asesor, regulación y recursos |
| Validación necesaria | Específica por feature/rol |
| Implicación de desarrollo | No priorizar en bloque ni prometer “todos los protocolos” |

## 3. Brechas documentales transversales

- [`ARCHITECTURE.md`](ARCHITECTURE.md), [`PYTHON_API.md`](PYTHON_API.md), [`protocol.md`](protocol.md) y la matriz histórica contienen descripciones de distintas etapas. Deben conservarse, pero apuntar en el futuro a esta serie como estado canónico.
- Algunas referencias antiguas describen `RF_open()` completamente lazy; el source actual abre un handle 433 persistente al boot (`main_rtos.c` L153–170).
- “BLE raw” debe calificarse: el host entrega advertising data y el RF core construye framing.
- “TX_FRAME” no construye una trama MAC; usa la misma ruta que RAW.
- “Validated preset” debe indicar si significa ACK, entrega raw, modulación medida o interoperabilidad.

## 4. Atribución por componente

| Origen | Brechas dominantes |
|---|---|
| Python | Secuencias async, descubrimiento Cat-Shell, validación de parámetros, contratos crypto y resultados TX |
| Wire protocol | Identidad, versionado, operation IDs, completion, readback y stats |
| CC1352P7 firmware | Metadata PROP, configuración sin resultado, lifecycle, scheduler TX y observabilidad |
| RP2040 | Asociación de interfaces, control de banda fuera de API y contadores incompletos |
| Hardware | Revisión P1/P7, front-end 433/169/2.4, antena, PA y estado U2 |
| Integraciones | KillerBee y helpers/emuladores que reducen o sobreafirman contratos |

## 5. Dependencias críticas

1. Identidad de artefacto antes de declarar regresiones.
2. Contrato event/response antes de cerrar `RX_STOP`, KillerBee y switching.
3. Definición de metadata antes de análisis RSSI/canal y presets confiables.
4. Semántica de completion antes de corregir BURST/CONTINUOUS.
5. Readback/capabilities antes de ampliar bandas o modulaciones.
6. Revisión de hardware antes de ensayar 433 o high-PA; 169 permanece fuera de alcance.
7. Decisión PHY/raw vs protocolos antes de convertir capas ausentes en requisitos.

Véase [DEVELOPMENT_PROPOSAL.md](DEVELOPMENT_PROPOSAL.md) para el orden propuesto, riesgos y criterios de aceptación.

