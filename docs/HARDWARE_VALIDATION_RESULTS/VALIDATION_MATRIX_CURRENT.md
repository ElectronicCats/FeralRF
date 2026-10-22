# Matriz canónica de validación de FeralRF

**Corte:** 2026-10-22  
**Fuente auditada:** `feature/hardware-validation` @ `fbff1b7f5077964418fb63a3fdf346a27c35a003`  
**Registros:** 49  
**Propósito:** separar presencia en código, resultado experimental y fuerza de evidencia sin borrar resultados históricos contradictorios.

## 1. Leyenda y reglas

### Estado de implementación

| Valor | Definición |
|---|---|
| PRESENTE | Existe una ruta ejecutable acorde con el alcance estrecho declarado |
| PARCIAL | Existe ruta, pero falta parte del contrato o del comportamiento anunciado |
| AUSENTE | No existe ruta funcional equivalente |
| NO SOPORTADO | La interfaz acepta/nomina algo que el comando, driver o hardware auditado no soporta |
| CONDICIONAL | Depende de hardware, entorno o decisión aún no establecidos |

### Estado experimental

| Valor | Definición |
|---|---|
| PASS | Cumplió el criterio definido para esa fila y completó las etapas aplicables |
| PARTIAL | Demostró una parte, pero no todo el criterio |
| FAIL | Se ejecutó de forma interpretable y no cumplió el criterio |
| BLOCKED | No alcanzó la etapa objetivo por prerrequisito o fallo previo |
| NOT RUN | No fue ejecutado |
| INCONCLUSIVE | La observación existe, pero procedencia/estado impide la conclusión pretendida |

`PASS` nunca asciende automáticamente de raw/PHY a protocolo o interoperabilidad. `ACK` es evidencia de aceptación; OTA exige un peer/captura. Una propiedad física exige instrumentación independiente.

Una celda puede contener estados compuestos como `PASS histórico reportado; NOT RUN reciente` cuando las campañas y niveles de validación son distintos. Esto no crea un estado nuevo: evita borrar cobertura histórica o promover raw a protocolo.

Abreviaturas de evidencia: **P** = registro primario; **R** = reporte experimental documentado; **S** = resumen consolidado; **H** = claim histórico de implementación; **C** = evidencia estática de source/SDK; **I** = medición física/peer independiente. La ausencia de **P** reduce inspectabilidad y reproducción, pero no invalida automáticamente una observación de **R**. No hay evidencia **I** preservada para frecuencia, potencia o modulación.

## 2. Resumen ejecutivo

| Clase | Capacidades principales |
|---|---|
| Demostradas | Descubrimiento/init; reapertura entre procesos; RX IEEE; IEEE RAW/FRAME; exclusión TX durante RX; switching IEEE en una ejecución; proprietary FSK/GFSK en campañas positivas; RF-02 reverso |
| Parciales | Reset automático, BURST, CONTINUOUS, TX_STOP, CW/PRBS, readback/configuración, metadata, switching estable, recuperación |
| Fallidas | Criterios BURST 5/40; CONTINUOUS lento; campañas proprietary 0/70 y 0/240; criterio post-stop en ejecuciones dirigidas |
| Bloqueadas | RF-03 transición IEEE↔Sub-GHz; RF-04 KillerBee; 433 sin evidencia de placa; 169; proprietary 2.4; MIOTY |
| No ejecutadas | RF-05 OOK reciente; medición RF instrumental; crypto reciente en hardware; jamming/spectrum bajo esta campaña |
| No soportadas/ausentes | MSK/4-FSK/4-GFSK por `modType=4/5/6`; spectrum funcional; pilas Zigbee/Thread/Wi-SUN/Sidewalk/W-MBus/MIOTY completas |

## 3. Lifecycle, transporte y observabilidad

| ID | Capability | Implementation status | Experimental status | Tested configuration | Historical evidence | Recent evidence | Observed result | Acceptance criterion | Evidence strength | Limitations | Open issue | Source reference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VM-DEV-01 | Device discovery / apertura | PRESENTE | PASS | Cat-Bridge en Windows, dos placas | EV-00 y múltiples campañas | Preflights 20–21-oct | Puertos bridge abribles; mapeo multi-CDC no siempre inferible | Identificar y abrir bridge correcto | P+S+C | Identidad USB por unidad incompleta | Asociación Bridge/Shell | `radio.py:connect()`; [evidencia §4.2](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-DEV-02 | Initialization, info, stats | PRESENTE | PASS | `RADIO_INIT`, `GET_INFO`, `GET_STATS` | EV-01 indirecta; repetido en EV/campañas | Preflights RF-01…03 | ACK, info y stats obtenidos repetidamente | Respuestas válidas y control recuperable | P+S+C | `1.0.0`/`FERALRF1` no identifican artefacto | Build identity | `radio.py:init()`, `control_task.c:ControlTask_getInfoPayload()` |
| VM-DEV-03 | Reconnection | PRESENTE | PASS | Cinco procesos separados | EV-03: 5/5 | Recuperaciones posteriores | Reapertura/init completados | 5/5 aperturas limpias | P+S | No probó USB loss ni misma instancia | Recovery automático | [Evidencia EV-03 §4.2](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-DEV-04 | Cat-Shell reset automático | PARCIAL | PARTIAL | `boot`/`exit`; COM88 bridge, COM87 shell | EV-04 manual 1/1; API habría elegido COM90 | Power-cycle/preflight usados fuera de API | Recuperación manual; `_get_shell_port()` inseguro | Reset por API en la unidad correcta y respuesta verificada | P+C | No valida respuesta Shell ni identidad | Descubrimiento por identidad USB | `radio.py:_get_shell_port(), reset_device()` |
| VM-DEV-05 | Firmware/build identity | PARCIAL | INCONCLUSIVE | CC reporta `1.0.0`; RP reportado `v3.1.0.1` | Campañas históricas sin hashes instalados | Reportes 21-oct sin hash CC/UF2 | No es posible unir campaña a commit/binario | Manifest source→artifact→device | P+S+C | Metadata RP disponible en source no prueba lo instalado | Inyectar SHA/hash/SDK/target | `control_task.c:ControlTask_getInfoPayload()`; [limitaciones §2](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-TRN-01 | COBS/CRC16/framing | PRESENTE | PASS | Tráfico de comandos/respuestas | Uso transversal | Uso transversal | Frames válidos intercambiados | Parseo y CRC correctos en operaciones observadas | P+C | Sin retransmisión/negociación | Versionado y límites | `protocol.py`, `protocol.c`, `protocol.h` |
| VM-TRN-02 | Async RX sequence handling | PARCIAL | FAIL (contrato estático) | RX activo mientras comandos síncronos esperan | Timeouts y respuestas `0x90` | RF-01/RF-03 y OP-09B | Un `RSP_RX_PACKET` válido con `seq!=0` que no coincide con la espera puede descartarse | Todo evento RX válido que compita con un comando síncrono se entrega exactamente una vez, cualquiera que sea su `seq`; la response conserva correlación | C+P | La violación está confirmada en source; no demuestra que todo timeout `RX_STOP` tenga esa única causa | F-02; política multiplexado/drenado | `radio.py:_read_response()` L278–397; `data_task.c` L59–65 |
| VM-TRN-03 | Error propagation | PARCIAL | PARTIAL | Rechazo TX/RX; fallos RF diferidos | EV-06 `0x05`; ACKs TX | OP-05 confirmó `error_code=0x05` | Error síncrono correcto; RAW tiene error async genérico; burst/continuous silencian fallo posterior | Error correlacionado por operación y causa | P+C | Async error sin operation ID | Completion/event contract | `command_processor.c`, `control_task.c` L332–407 |
| VM-TRN-04 | Transport counters | PARCIAL | PARTIAL | `GET_STATS`, Cat-Shell `status` | Stats repetidos | OP-10: contadores RP cero al consultar | Métricas RF y algunos contadores RP disponibles | Drops/ocupación por cada etapa, snapshot correlacionado | P+C | No cubre output queue ni USB→CC; consulta no simultánea | Telemetría end-to-end | `command_processor.c:send_stats()`, RP `main.c` L202–251 |

## 4. Configuración, IEEE y modos TX

| ID | Capability | Implementation status | Experimental status | Tested configuration | Historical evidence | Recent evidence | Observed result | Acceptance criterion | Evidence strength | Limitations | Open issue | Source reference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VM-CFG-01 | PHY configuration | PRESENTE | PASS | PHY 0–7 por control | EV-10 8/8 | Usado en RF-01…03 | ACK para configuraciones ensayadas | Comando aceptado/rechazado correctamente | P+C | No prueba RF de cada PHY | Validación física individual | `commands.py:set_phy()`, `command_processor.c` L128–139 |
| VM-CFG-02 | IEEE channel selection | PRESENTE | PASS (estrecho) | Canales 25 y 20 | Inicialmente no cerrado | 21-oct: 25/25 10/10; 25/20 0/10; 20/20 10/10; 25/25 9/10 | La selectividad de canal cumplió: positivos en match, negativo en mismatch y retorno positivo | Clasificación estrecha: match/mismatch/retorno; el cleanup se evalúa en VM-IEEE-04 | P | El `RX_STOP` final falló, por lo que RF-01 global no es PASS | Repetibilidad y RX_STOP | [`EXPERIMENTAL_EVIDENCE_REFERENCE.md`](EXPERIMENTAL_EVIDENCE_REFERENCE.md) §5.1 |
| VM-CFG-03 | Power configuration | PRESENTE | PASS control; NOT RUN físico | -20 y 0 dBm solicitados | Varias campañas | OP-05/06 y RF recientes | ACK; enlaces posibles | Readback efectivo + potencia medida | P+C | Tabla puede aproximar; sin power meter | Requested vs effective | `radio_if.c:RadioIF_resolveTxPowerValue()` |
| VM-CFG-04 | `SET_PROP_CONFIG` | PARCIAL | PARTIAL | 31 presets/varias frecuencias | EV-11 control; campañas OTA | Verificación estática actual | Config aceptada; algunos enlaces raw | Validación semántica, error driver y readback | P+C | Handler ACK tras función `void` | F-07; config efectiva | `command_processor.c` L168–185; `radio_if.c:RadioIF_setPropConfig()`; [GAP-06](FUNCTIONAL_GAPS.md) |
| VM-CFG-05 | Band selection | PARCIAL | PASS lógico; INCONCLUSIVE físico | `band1`, `band2`, estados 0/1 | 3A–4A; 3D/3E/4A funcionaron con Band 0 | Verificación lógica 21-oct | GPIO solicitados y texto correcto; ruta física no medida | Estado lógico+eléctrico+RF concordantes | P+C | Estado textual no prueba U2 | Revisar placa/medir CTF/U2 | RP `shell_commands.c`, overlay GPIO8–10 |
| VM-IEEE-01 | IEEE 802.15.4 RX | PRESENTE | PASS | PHY4, canal25, ventanas 30 s | EV-02/05: 41/43/43; OCT baselines | OP-04: 54 paquetes | Recepción repetida con CRC reportado | Tráfico recibido y cleanup | P | Fuente ambiental no atribuida en todos los casos | Metadata/timestamp independientes | [Evidencia EV-05 §4.2 y OP-04 §5.5](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-IEEE-02 | IEEE TX RAW OTA | PRESENTE | PASS | ch25, marcadores dirigidos | 20/20 y 10/10; recovery 9/10 | OP-06: 9/10 y 10/10 | Entrega bilateral en configuraciones documentadas | Umbral definido + control negativo + cleanup | P | Repetición de marker no es PDR por secuencia | Añadir IDs únicos | [Experimento 01](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/01_ieee_raw_frame_burst.md); [OP-06 §5.5](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-IEEE-03 | IEEE TX FRAME OTA | PRESENTE | PASS | ch25, bytes MAC aportados por host | 10/10 | Sin nueva ejecución separada | Marker recibido | 10/10 y cleanup | P | `TX_FRAME` no construye MAC; RF core añade PHR/FCS | Contrato de nombre | `ControlTask_onTxFrame()`; Experimento 01 |
| VM-IEEE-04 | IEEE channel switching | PRESENTE | BLOCKED | 25→20→25 | RF-01 20-oct bloqueado en primer stop | 21-oct completó las cuatro etapas RF, pero el stop final agotó timeout | La selectividad/switching estrechos se observaron; el experimento completo no terminó | Toda la secuencia, cleanup y repetición deben completar | P | El resultado estrecho de canal está separado en VM-CFG-02 | Estabilidad stop/transición | [`EXPERIMENTAL_EVIDENCE_REFERENCE.md`](EXPERIMENTAL_EVIDENCE_REFERENCE.md) §5.1 |
| VM-IEEE-05 | IEEE↔Sub-GHz transition | PRESENTE por componentes | BLOCKED | IEEE→Sub-GHz→IEEE | RF-03 20-oct: 10/10 y stop timeout | 21-oct: 7/10 y stop timeout | Nunca llegó a Sub-GHz; `completed_stages=[]` en histórica | Completar tres etapas y cleanup | P | No implica fallo del cambio PHY | Resolver RX_STOP primero | [Resumen RF-03 §5.3](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-TX-01 | TX exclusion during RX | PRESENTE | PASS | TX solicitado con RX activo | EV-06 dos ejecuciones | OP-05 confirmó `0x05` | Rechazo y recuperación correctos | `ERR_INVALID_STATE=0x05`, stop/stats válidos | P+C | Sólo estado probado | Unificar exclusión jam/TX | `command_processor.c` L188–220 |
| VM-TX-02 | BURST | PARCIAL | FAIL | 5×250 ms y 40×25 ms | 1/5 y 1/40 | OP-08 reprodujo 1/5 | ACK/scheduled, un marker peer | Entregar umbral solicitado y completion/count | P+C | No se conoce emisiones reales | F-04; scheduler/telemetría | `ControlTask_processTxBurst()`; EV-12 |
| VM-TX-03 | CONTINUOUS con intervalo | PARCIAL | PARTIAL | 250000 µs | Un hit | OP-09A un hit | Actividad mínima, repetición insuficiente | Tasa/ventana y count definidos | P+C | Sin completion ni contador TX | Temporización scheduler | `ControlTask_processTxContinuous()`; OP-09 |
| VM-TX-04 | CONTINUOUS intervalo 0 | PARCIAL | PARTIAL | `interval_us=0` | 30 o 99/102 | OP-09B: 37 hits CRC-good | Actividad repetida abundante | Tasa esperada, stop y cleanup medidos | P+C | Conteos no comparables; stop timeout | Completion/cadence/cese | [Evidencia §3.1, §4.2 y §5.5](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-TX-05 | TX_STOP | PRESENTE | PASS control; NOT RUN cese físico | Durante continuous/test | ACK en campañas | ACK; controles posteriores sin marker en sesiones documentadas | Control acepta stop | Evento de detenido + cese RF medido | P+C | No mide instante por aire | Semántica de stop | `ControlTask_onTxStop()` |
| VM-TX-06 | CW / PRBS15 / PRBS32 | PRESENTE | PASS control; NOT RUN físico | ACK de modos y stop | EV-13 4/4 control | OP-10 consulta posterior | Comandos aceptados/posteados | Frecuencia, potencia, patrón y cese medidos | P+C | Sin instrumento; estado Band posterior no retroactivo | HIL/instrumentación | `RadioIF_runTxTest()`, EV-13 |

## 5. Proprietary Sub-GHz: resultados que deben coexistir

| ID | Capability | Implementation status | Experimental status | Tested configuration | Historical evidence | Recent evidence | Observed result | Acceptance criterion | Evidence strength | Limitations | Open issue | Source reference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VM-PROP-01 | F29 siete presets | PRESENTE como raw | INCONCLUSIVE como reproducción; claim preservado | Sidewalk/Wi-SUN nominales, 10/preset | Claim commit `f5863c3`: 70/70 | Contradicho por campañas posteriores | Claim positivo de implementación sin reporte/transcript/binarios equivalentes | Reporte o primarios + identidad + peer | H+C | No invalida el claim; no permite tratarlo como corrida reproducible ni protocolo | Procedencia F29 | [Claim y límites §4.1](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-PROP-02 | Repetición subconjunto F29 | PRESENTE como raw | FAIL | Siete presets, 70 TX | Campaña octubre | — | 0/70 peer | ≥1/10 o criterio por preset, según plan | P | Causa RF inconclusa | Estado físico/binarios | [Contradicciones §6.1](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |
| VM-PROP-03 | Campaña proprietary ampliada fallida | PRESENTE como raw | FAIL | 24 corridas, 868/915/2440 nominal | 240 ACK, 0 peer | — | Host aceptó TX; receptor no vio marker | Entrega peer por corrida | P | ACK no prueba radiación; algunos presets inválidos | Causa abierta | `OCT-PROP-0` matriz histórica |
| VM-PROP-04 | Recovery `wisun_915_fsk_50k` | PRESENTE como raw FSK | PASS | 902.2 MHz solicitados, seis checkpoints | 3A…4A: 60/60 | — | 10/10 en cada checkpoint | 10/10 por corrida y cleanup | P | No Wi-SUN; no frecuencia medida | Estado previo divergente | Recovery audit/run |
| VM-PROP-05 | Campaña positiva de 13 presets | PRESENTE FSK/GFSK raw | PASS | 868, 868.3, 868.95, 902.2, 915 MHz nominales | E02…E24: 13 presets 10/10; cinco corridas BLOCKED | — | Entrega raw en cada preset positivo | 10/10 marker+CRC y cleanup | P | No pila, no instrumento | Repetibilidad por artefacto | `VALIDATION_MATRIX_261016` |
| VM-PROP-06 | RF-02 reverse delivery | PRESENTE | PASS | #3→#2, `wisun_915_fsk_50k`, 902.2 MHz solicitados | RF-02 20-oct | — | 10 ACK, 10 marker, stop/cleanup | 10/10 y cleanup | P | `channel=25`, RSSI incoherente; sin medida física | Metadata PROP | RF-02 report/JSONL |
| VM-PROP-07 | Proprietary RX metadata | PRESENTE defectuosa | FAIL | RF-02 packets | RSSI/channel anómalos | Verificación estática actual | Parser invierte orden status/RSSI/timestamp; LQI=0; channel residual | Campos contra vector/layout conocido | C+P | Binario histórico no identificado | GAP-01 | `radio_if.c:RadioIF_processPropPackets()` L1706–1754; [GAP-01](FUNCTIONAL_GAPS.md) |
| VM-PROP-08 | Generic FSK/GFSK raw | PRESENTE | PARTIAL | Subconjunto 868–915/902.2 | Positivos y negativos arriba | RF-02 reverso | Capacidad demostrada, fiabilidad no universal | Matriz por preset/build y repetición | P+S+C | No extrapolar a protocolo o RF física | HIL reproducible | [Evidencia Sub-GHz §3 y §6.1](EXPERIMENTAL_EVIDENCE_REFERENCE.md) |

## 6. Presets excluidos, bandas y niveles de protocolo

| ID | Capability | Implementation status | Experimental status | Tested configuration | Historical evidence | Recent evidence | Observed result | Acceptance criterion | Evidence strength | Limitations | Open issue | Source reference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VM-PRE-01 | OOK | PARCIAL/CONDICIONAL | PASS histórico reportado (raw); NOT RUN reciente | `ook_868_4k8`; RF-05 previsto | Reporte agregado abril: 10/10 raw con recuperación declarada | RF-05 no autorizado/no ejecutado porque no cumplió gates RF-01…03/recuperación | La observación 10/10 se conserva; faltan repetición actual y recuperación post-power-cycle del build actual | Criterio provisional: ≥9/10 + power-cycle + control IEEE posterior | R+C | Sin primarios por paquete; no protocolo OOK ni caracterización física | Lifecycle/recovery y procedimiento standalone | `radio_if.c:RadioIF_getPropMode()`; [`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) §4/§5; [readiness OOK](PENDING_VALIDATION_ASSESSMENT.md#4-ook-868-procedimiento-controlado-reconstruible) |
| VM-PRE-02 | MIOTY / TS-UNB | NO SOPORTADO como protocolo | INCONCLUSIVE (raw histórico); NOT RUN TS-UNB | `mioty_868_tsunb` | Comentario/claim 0/10 de marker raw | Verificación estática actual; sin prueba TS-UNB | Sólo packet FSK: 395.2026 baud teóricos y 250 Hz de desviación nominal teórica; no existe ruta TS-UNB | A/B: control o marker FSK; C/D: splitting/coding/interleaving, scheduler, reensamble y peer/base station | H+C | 0/10 no prueba hardware incompatible; A/B no prueban MIOTY ni medición física | Factibilidad RF core/patch/vendor | `presets.py`; [`PROJECT_STATUS_REPORT.md` §7.4](PROJECT_STATUS_REPORT.md); [niveles de prueba](PENDING_VALIDATION_ASSESSMENT.md#3-mioty-por-niveles-de-prueba) |
| VM-PRE-03 | MSK / 4-FSK / 4-GFSK | NO SOPORTADO | NOT RUN válido | Seis presets: `msk_433_50k`, `4fsk_433_50k`, `4gfsk_433_50k`, `msk_868_50k`, `4fsk_868_50k`, `4gfsk_868_50k`; `mod_type=4/5/6` | Claims 10/10 no caracterizan modulación | Prueba prohibida bajo contrato actual | El SDK reserva los tres valores usados por los seis presets | Comando/patch TI válido + analizador/peer | C | ACK/bytes no prueban modulación | Elegir backend oficial | SDK `rf_prop_cmd.h` L475–478/L581–584; [GAP-05](FUNCTIONAL_GAPS.md) |
| VM-PRE-04 | 433 MHz FSK/GFSK/OOK | CONDICIONAL | PARTIAL histórico reportado; BLOCKED actual | Presets 433 | Reporte: GFSK 6–10/10, FSK 1/10, OOK 0/10; otros claims OOK contradictorios | Verificación estática, sin caracterización P7 actual | La ejecución histórica se conserva; ruta código existe, pero placa/front-end/antena P7 no están establecidos | Revisión/BOM + medición pasiva + OTA FSK controlada | R+C+H | No atribuir marginalidad a antena/firmware sin medida | Hardware exacto | [`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) §4/§5; [`HARDWARE_INTEGRATION.md` §6](HARDWARE_INTEGRATION.md); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-PRE-05 | 169 MHz | NO SOPORTADO/INCOMPATIBLE | NOT RUN | W-MBus N 169.45 | Sólo control nominal | Verificación estática actual | Fuera de rango defendible; override/ruta incoherentes | Hardware distinto + backend validado | C | No ensayar en placa actual | Retirar/bloquear preset por target | `radio_if.c:RadioIF_getPropMode()`/`RadioIF_setPropConfig()`; [GAP-15](FUNCTIONAL_GAPS.md) |
| VM-PRE-06 | Proprietary 2.4 GHz | PARCIAL defectuosa | INCONCLUSIVE histórico; BLOCKED actual | 2440 MHz presets | Reporte antiguo 10/10 y reporte posterior 0 peer | Verificación estática actual | Se conservan ambos resultados; backend actual `<1000 MHz` cae a 868 MHz antes de RX/TX | Backend dedicado + frecuencia medida | R+C | Ensayo actual sería no interpretable y no resuelve el conflicto histórico | Restaurar/diseñar ruta 2.4 | `radio_if.c:RadioIF_applySub1ghzChannelConfig()` L1578–1612; [GAP-14](FUNCTIONAL_GAPS.md) |
| VM-PROT-01 | BLE-related functionality | PARCIAL | PASS histórico reportado (PHY/raw); NOT RUN reciente | 1M/2M/Coded advertising | Reporte: 1M 10/10, 2M 8/10, Coded S8/S2 10/10 | Sin campaña BLE externa dedicada; motivo específico no establecido | Fuente implementa advertising limitado; cobertura histórica no equivale a stack actual | Repetición raw por build o peer BLE externo por PHY/PDU y cleanup | R+C | Sin primarios por frame; no conexiones/GAP/GATT/SMP | Definir alcance BLE | [`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) §4/§5; `radio_if.c:RadioIF_transmitBle*`; [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-PROT-02 | Wireless M-Bus | PARCIAL nominal | PASS raw S/T/C; NOT RUN interoperabilidad | S/T/C/N presets, helper T1 | Reportes agregados históricos; S/T/C 868 recientes 10/10 raw | Sin peer EN 13757; N169 no ejecutado físicamente | Framing/CRC por bloques/parser/procedimientos ausentes; helper T1 usa MSK reservado | Peer W-MBus y versión/rol definidos | P+R+C | Raw no es W-MBus; N169 bloqueado por target | Corregir claim o implementar rol | `emulation/sub1ghz_device.py`; [campaña §3.2](EXPERIMENTAL_EVIDENCE_REFERENCE.md); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-PROT-03 | Wi-SUN | PARCIAL nominal | PASS raw; NOT RUN interoperabilidad | Cinco presets FSK | F29 claim; recovery raw | 60/60 recovery, presets 10/10 y RF-02 reverso 10/10 raw | Sin 802.15.4g/FAN, hopping, IPv6 o seguridad | Perfil/rol y peer Wi-SUN | P+R+H+C | Enlace raw no es Wi-SUN; otra repetición raw no cubre el gap | Decisión de producto | `presets.py`; [evidencia §3.2/§6.1](EXPERIMENTAL_EVIDENCE_REFERENCE.md); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-PROT-04 | Sidewalk | PARCIAL nominal | PASS raw; NOT RUN interoperabilidad | 50k/250k FSK | F29 claim raw | Campaña reciente 10/10 raw por preset | 50k usa FSK, no GFSK; sin framing/hopping/seguridad | Rol/región/ecosistema y peer | P+H+C | Etiqueta excede implementación; otra repetición raw no cubre el gap | Reetiquetar o diseñar soporte | `presets.py`; [evidencia §3.2](EXPERIMENTAL_EVIDENCE_REFERENCE.md); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-PROT-05 | Zigbee / Thread | AUSENTE como stacks | PASS IEEE raw; NOT RUN stacks/interoperabilidad | Base IEEE raw | RX/TX IEEE documentados | RF-04 no llegó a hardware | Sólo bytes IEEE; sin capas superiores | Rol/stack/peer y seguridad | P+R+C | Captura KillerBee raw no sería stack | Alcance de producto | [`PROJECT_STATUS_REPORT.md` §7.2–7.3](PROJECT_STATUS_REPORT.md); [GAP-20](FUNCTIONAL_GAPS.md); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |

## 7. Crypto, integraciones y funciones avanzadas

| ID | Capability | Implementation status | Experimental status | Tested configuration | Historical evidence | Recent evidence | Observed result | Acceptance criterion | Evidence strength | Limitations | Open issue | Source reference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VM-ADV-01 | Cryptography | PARCIAL | PASS histórico reportado; NOT RUN reciente | TRNG, AES, SHA, ECDH, ECDSA | Reporte APR-CRYPTO: 9/9 | Sin vectores hardware recientes; motivo específico no establecido | Rutas TI y script de vectores presentes; ECDSA host admite Curve25519 pero firmware P-256 | Repetir vectores conocidos en CC/build identificado y límites por operación | R+C | Primarios históricos ausentes; no invalida 9/9 reportado ni certifica TRNG | GAP-17 | [`smoke_f25_crypto.py`](../python/examples/lab/smoke_f25_crypto.py); `crypto_engine.c`; [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-ADV-02 | Jamming | PARCIAL | PASS control histórico reportado; NOT RUN físico | BLE/IEEE continuo, duración acotada | ACK mencionado en reporte histórico; sin efecto físico medido | No ejecutado recientemente; razón específica no establecida | Ruta continua existe; reactivo/pattern ausentes | Política autorizada, recinto/carga, RF medida, cooldown/stop | R+C | Riesgo legal/regulatorio; smoke actual sólo prueba ACK y usa 20 dBm por defecto | GAP-22; alcance | `ControlTask_onJamContinuous()`; [`smoke_jam_phase1.py`](../python/examples/lab/smoke_jam_phase1.py); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-ADV-03 | Spectrum functionality | AUSENTE | NOT RUN | Builders/IDs residuales | Ninguna | Ninguna | No hay handler/backend utilizable | Scan con bins/calibración/stop definidos | C | No anunciar como capacidad | Decisión opcional | `commands.py`, `protocol.h`, ausencia de handler; [GAP-22](FUNCTIONAL_GAPS.md) |
| VM-ADV-04 | Emulation | PARCIAL | PASS histórico reportado (firmas raw); NOT RUN interoperabilidad | IEEE, W-MBus y OOK helpers | Reporte APR-EMU/F17 con conteos por familia | Sin peer externo reciente; motivo de no repetición raw no establecido | Tramas/patrones fijos, no state machines; script histórico mezcla rutas ahora condicionadas | Peer independiente y rol/procedimientos definidos | R+C | “Emulation” sobreafirma helpers; raw reportado no equivale a dispositivo interoperable | Renombrar o completar | `python/feralrf/emulation/`; [`smoke_f17_emulation.py`](../python/examples/smoke_f17_emulation.py); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| VM-ADV-05 | KillerBee | PARCIAL | BLOCKED | RF-04 captura pasiva Zigbee | Adapter/patch presentes | `pip` no resolvió `killerbee>=3.0.0`; no hardware/PCAP | Bloqueo de packaging, no fallo RF | Entorno reproducible + captura/PCAP + cleanup | P+C | API/FCS/metadata/ciclo de vida parciales | F-13 | `integrations/killerbee.py`, RF-04 |

## 8. Fuentes de evidencia

- [Referencia autocontenida de evidencia, contabilidad y limitaciones](EXPERIMENTAL_EVIDENCE_REFERENCE.md).
- [Matriz histórica local del repositorio](VALIDATION_MATRIX.md).
- [Matriz y reportes preservados del 12-oct](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/validation_matrix_20261012_113056.md).
- [Matriz y reportes preservados del 16-oct](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/VALIDATION_MATRIX_261016).
- [Estado técnico y trazabilidad de implementación](PROJECT_STATUS_REPORT.md).
- [Brechas funcionales con evidencia de fuente](FUNCTIONAL_GAPS.md).
- [Integración física y de firmware](HARDWARE_INTEGRATION.md).
- [Readiness, procedimientos y prerrequisitos de validaciones pendientes](PENDING_VALIDATION_ASSESSMENT.md).
- [Contrato protocolario local](protocol.md), [arquitectura local](ARCHITECTURE.md) y [API Python](PYTHON_API.md).

## 9. Lectura final

La matriz demuestra una base funcional real, pero también tres límites metodológicos: no hay identidad completa de artefactos, no hay instrumentación RF independiente y los resultados Sub-GHz varían entre campañas. Cualquier nueva ejecución debe añadir registros; no debe sobrescribir ni “promediar” los resultados incompatibles aquí preservados.

