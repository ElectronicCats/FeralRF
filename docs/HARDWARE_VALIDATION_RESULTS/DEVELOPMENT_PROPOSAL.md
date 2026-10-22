# Propuesta priorizada de desarrollo para FeralRF

**Carácter:** propuesta técnica; no autoriza implementación ni ensayos RF  
**Base:** [estado actual](PROJECT_STATUS_REPORT.md), [matriz](VALIDATION_MATRIX_CURRENT.md), [readiness de validación](PENDING_VALIDATION_ASSESSMENT.md) y [brechas](FUNCTIONAL_GAPS.md)  
**Corte:** 2026-10-22

## 1. Modelo de priorización

La prioridad combina cinco factores cualitativos: impacto demostrado, severidad/seguridad, número de capacidades bloqueadas, confianza de evidencia y costo/riesgo. Una ausencia opcional no recibe P0. Las incertidumbres de causa reciben prioridad de **investigación** independiente de la prioridad de **implementación**.

| Prioridad | Regla |
|---|---|
| P0 — bloqueador crítico | Contrato incorrecto o falta de reproducibilidad que invalida resultados transversales; actuar antes de ampliar alcance |
| P1 — alta | Defecto confirmado o estabilización que afecta operaciones centrales y desbloquea validación |
| P2 — media | Mejora importante pero no bloquea el núcleo PHY/raw, o depende de requisitos/hardware |
| P3 — opcional/exploratoria | Extensión sin requisito aprobado o con beneficio acotado |

Supuestos de complejidad: equipo de 1–2 desarrolladores familiarizados con C/TI-RTOS y Python; acceso a dos CatSniffer, entorno RF autorizado y equipo de medición cuando se indique. `Baja` significa cambio local con pruebas acotadas; `Media`, cambio multi-capa o HIL; `Alta`, arquitectura/protocolo/hardware con especificación y validación externas. No se proporcionan fechas porque faltan requisitos y disponibilidad de laboratorio.

Todos los umbrales numéricos de esta propuesta —por ejemplo 100 ciclos, 9/10, 10/10 o un SLA temporal— son **criterios provisionales** hasta que el asesor apruebe el nivel de evidencia, la semántica de éxito y los recursos de laboratorio. Expresan qué debe poder medirse, no una autorización de ejecución ni una especificación final.

## 2. Resumen de prioridades

| Propuesta | Categoría | Investigar | Implementar | Justificación corta |
|---|---|---:|---:|---|
| DEV-01 Manifest de build/dispositivo | Infraestructura/identidad | P0 | P0 | Sin identidad no hay regresión reproducible |
| DEV-02 Contrato unificado de eventos/responses | Correctiva/API | P0 | P0 | Pérdida determinista de RX y base de lifecycle |
| DEV-03 Diagnóstico y corrección de RX_STOP | Estabilización | P0 | P1 provisional | Bloquea switching y cleanup; mecanismo completo no cerrado |
| DEV-04 Corregir metadata proprietary | Correctiva | P0 | P0 | Defecto determinista que invalida campos RX |
| DEV-05 Configuración tipada, capabilities y readback | API/correctiva | P1 | P1 | Evita ACK engañoso y bandas/modos imposibles |
| DEV-06 Completion y telemetría TX | Estabilización/API | P1 | P1 | BURST/CONTINUOUS no son verificables |
| DEV-07 Integración segura Cat-Shell/board | Hardware/API | P1 | P1 | Reset/banda dependen de heurística de puerto |
| DEV-08 Observabilidad end-to-end | Infraestructura | P1 | P1 | Necesaria para localizar drops/timeouts |
| DEV-09 Gating de 169/433/2.4/reservados | Correctiva/hardware | P0 | P1 | Previene operación no interpretable o insegura |
| DEV-10 OOK lifecycle | Completitud | P1 | P2 provisional | OTA raw 868 histórica 10/10 reportada; falta repetición actual y cierre seguro del lifecycle |
| DEV-11 Crypto/KillerBee contratos | API/integración | P2 | P2 | Crypto tiene 9/9 histórico reportado pero contrato parcial; KillerBee no fue reproducible por packaging |
| DEV-12 HIL y regresión por niveles | Validación | P0 | P1 | Convierte hallazgos en gates repetibles |
| DEV-13 Taxonomía/documentación canónica | Documentación | P1 | P1 | Evita confundir preset, PHY y stack |
| DEV-14 MIOTY feasibility | Extensión protocolaria | P2 | P3/pendiente | TS-UNB ausente; depende del rol del producto |

### 2.1 Por qué cada frente P0 es crítico

| Frente P0 | Qué bloquea y capacidades afectadas | Naturaleza y evidencia | Riesgo / dependencias |
|---|---|---|---|
| DEV-01 identidad | Comparar cualquier regresión RF, lifecycle, metadata o protocolo | Bloqueador de **reproducibilidad**, no prueba de imposibilidad RF; campañas positivas y negativas no identifican todos los binarios | Sin manifest se puede “corregir” una diferencia de artefacto; depende del pipeline y board ID |
| DEV-02 eventos/responses | RX sostenido, callbacks, KillerBee, switching y comandos concurrentes | Bloqueador **funcional transversal**: source descarta un evento RX válido con `seq` no esperado | Cambia contrato/concurrencia; depende de versionado y backpressure |
| DEV-03 investigación RX_STOP | Cleanup, switching 25→20→25, transición IEEE↔Sub-GHz y HIL automático | Síntoma reproducible en RF-01/RF-03/OP-09B, pero causa end-to-end no cerrada; P0 para investigar, implementación P1 provisional | Preseleccionar el arreglo puede ocultar fallos de cola, tarea o bridge; la instrumentación inicial puede empezar antes de DEV-02 completo |
| DEV-04 metadata | RSSI, CRC/status, timestamp, filtros, análisis Sub-GHz e integraciones | Bloqueador de **exactitud de datos**: layout parser/SDK incompatible de forma determinista | Cambiar wire sin versión rompe consumidores; depende del contrato RX e identidad |
| DEV-09 investigación/gating | Evitar interpretar 169, 2440 y `modType=4/5/6` como RF válida | Riesgo de configuración no interpretable o en banda distinta; fuente confirma fallback 868/reservados | El rechazo seguro es local; habilitar 2.4/433 depende de hardware, backend e instrumentos |
| DEV-12 baseline de regresión | Evitar que los defectos anteriores reaparezcan mientras se amplía la plataforma | Bloqueador metodológico: 68/193 registros se apoyan en informes, resúmenes o claims sin todos los artefactos primarios; eso limita inspección independiente, no borra la observación reportada | El baseline inicial no depende de construir el banco HIL completo; la automatización física avanzada sí depende de DEV-01/02/08 y laboratorio |

## 3. Propuestas detalladas

### DEV-01 — Manifest reproducible de firmware, SDK, placa y unidad

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-01 |
| 2. Problema | `1.0.0` y `FERALRF1` no identifican binario/unidad; campañas no preservan hashes CC/UF2 |
| 3. Comportamiento actual | `GET_INFO` devuelve versión/capabilities/serial constantes; RP source puede exponer metadata, pero no existe manifest experimental obligatorio |
| 4. Evidencia | `ControlTask_getInfoPayload()`; F-05; contradicciones F29/0/70/60/60 |
| 5. Componentes | Build CC, protocolo, Python, RP Cat-Shell, run harness |
| 6. Enfoque | Inyectar Git SHA, dirty flag, SDK revision, target, feature bitmap y build hash; registrar hash de binarios y board ID/revision en cada run |
| 7. Alternativas | TLV extendible en `GET_INFO`; comando `GET_BUILD_INFO`; manifest firmado/JSON externo. Preferir TLV versionado + manifest de run |
| 8. Resultado esperado | Cada observación une source→artifact→device→run |
| 9. Interfaces | Extensión versionada de info; API `get_build_info()`; Cat-Shell reporta pairing/board metadata |
| 10. Dependencias | Pipeline de build y estrategia de serial por unidad |
| 11. Riesgos | Compatibilidad con host anterior y tamaño del frame |
| 12. Hardware | Lectura de board revision puede requerir provisioning, no autodetección |
| 13. Validación | Comparar campos contra artefactos hash; dos builds distintos deben ser distinguibles |
| 14. Aceptación | Criterio provisional: 100% de runs HIL guardan SHA CC/RP, SDK, hashes, dirty state y board ID sin entrada manual ambigua |
| 15. Complejidad | Media; multi-capa, sin algoritmo RF |
| 16. Prioridad | Investigación P0 / implementación P0 |
| 17. Confianza | Alta; queda decidir formato y compatibilidad |

### DEV-02 — Separar responses síncronas de eventos asincrónicos

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-02 |
| 2. Problema | RX usa secuencias que el host interpreta como responses stale y puede descartarlas |
| 3. Comportamiento actual | `_read_response()` mezcla parsing, correlación y dispatch; sólo `seq=0` se bufferiza de forma general |
| 4. Evidencia | GAP-02; `radio.py` L278–397; `data_task.c` L59–65 |
| 5. Componentes | Wire protocol, Python transport, DataTask/OutputIF |
| 6. Enfoque | Definir bit/tipo de evento independiente de SEQ, dispatcher único, cola bounded, callbacks/iterador y política de overflow |
| 7. Alternativas | Reservar `seq=0` para todo evento; usar canal/opcode de evento; reader thread. Preferir tipo de frame/event flag versionado con compatibilidad de lectura |
| 8. Resultado esperado | Ningún RX válido se pierde porque coincide con un comando síncrono |
| 9. Interfaces | Protocolo v2 o capability-negotiated; API de eventos consistente |
| 10. Dependencias | DEV-01/version negotiation; política de backpressure |
| 11. Riesgos | Cambios de concurrencia, memoria y compatibilidad con binarios antiguos |
| 12. Hardware | Ninguna modificación física |
| 13. Validación | Interleaving exhaustivo: RX seq 0…254 con cada comando; saturación y orden |
| 14. Aceptación | Criterio provisional: 0 eventos perdidos/reclasificados en pruebas deterministas; responses siempre correlacionadas |
| 15. Complejidad | Alta; cambia contrato y arquitectura host |
| 16. Prioridad | P0/P0 |
| 17. Confianza | Alta en el defecto; diseño final pendiente |

### DEV-03 — Instrumentar y estabilizar `RX_STOP`

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-03 |
| 2. Problema | Stop intermitente bloquea switching/cleanup; no se conoce el punto exacto de pérdida |
| 3. Comportamiento actual | Firmware agenda stop y ACK inmediato; host puede consumir `0x90` y agotar retries |
| 4. Evidencia | RF-01/RF-03 20–21-oct, OP-09B, GAP-03 |
| 5. Componentes | Python, HostIFTask, OutputIF/PacketQueue, DataTask, RP bridge |
| 6. Enfoque | Fase A independiente: tracing con command/seq/timestamps, estado y ocupación, sin esperar el rediseño completo de eventos. Fase B: confirmar el punto de pérdida. Fase C: ajustar contrato/lifecycle, hacer stop idempotente y definir drain. Fase D: regresión bajo tráfico |
| 7. Alternativas | ACK después de detener backend; ACK inmediato + `RX_STOPPED` completion. Preferir completion explícito para no bloquear UART task |
| 8. Resultado esperado | Stop siempre termina o falla con razón observable en tiempo acotado |
| 9. Interfaces | Evento `RX_STOPPED`, query de estado, counters; compatibilidad de `stop_rx()` |
| 10. Dependencias | La investigación inicial sólo requiere instrumentación mínima de DEV-08; la solución contractual y regresión completa se coordinan con DEV-02/08 |
| 11. Riesgos | Deadlock si el handler espera driver; pérdida deliberada de paquetes si drain no se define |
| 12. Hardware | HIL con tráfico sostenido; sin cambio de PCB |
| 13. Validación | Criterio provisional: ≥100 ciclos RX/stop por PHY/carga, switching 25→20→25 y cross-PHY, con fault injection |
| 14. Aceptación | Criterio provisional: 100% stop finaliza dentro del SLA que apruebe el asesor; cero ACK ambiguos; estado idle confirmado |
| 15. Complejidad | Investigación media-alta; implementación provisional media |
| 16. Prioridad | Investigar P0; implementar P1 hasta confirmar mecanismo |
| 17. Confianza | Alta en el síntoma, media en la solución |

### DEV-04 — Corregir y versionar metadata proprietary

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-04 |
| 2. Problema | Orden de campos no coincide con flags TI; canal/LQI carecen de semántica útil |
| 3. Comportamiento actual | Parser toma status antes de RSSI/timestamp y filtra CRC sobre byte equivocado |
| 4. Evidencia | GAP-01; RF-02; SDK local |
| 5. Componentes | `radio_if.c`, DataTask response, `Packet` Python/docs |
| 6. Enfoque | Parser basado en layout declarado; helpers de offset; metadata version/flags; frecuencia solicitada y/o efectiva separada de channel |
| 7. Alternativas | Mantener payload wire y corregir campos; nuevo TLV metadata. Preferir versión/flags para compatibilidad |
| 8. Resultado esperado | CRC/RSSI/timestamp reproducibles; campo no aplicable no se inventa |
| 9. Interfaces | `Packet` y `RSP_RX_PACKET` pueden requerir versión/capabilities |
| 10. Dependencias | DEV-01 y definición header/CRC |
| 11. Riesgos | Romper consumers que dependen accidentalmente del layout incorrecto |
| 12. Hardware | Peer/generador conocido y captura temporal |
| 13. Validación | Vectores sintéticos + OTA con RSSI/timestamp comparados a referencia |
| 14. Aceptación | Criterio provisional: todos los offsets unit-tested; CRC conocido; RSSI dentro de la tolerancia que se apruebe; sin channel residual |
| 15. Complejidad | Media |
| 16. Prioridad | P0/P0 |
| 17. Confianza | Alta |

### DEV-05 — Configuración RF tipada, validada y con readback

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-05 |
| 2. Problema | ACK no distingue solicitado/aceptado/efectivo; combinaciones imposibles se aceptan |
| 3. Comportamiento actual | `SET_PROP_CONFIG` muta estructuras y retorna `void`; potencia puede aproximarse |
| 4. Evidencia | GAP-05/06/07/14/15 |
| 5. Componentes | Presets, builders, command processor, RadioIF, docs |
| 6. Enfoque | Schema por backend/target, enums válidos, capability matrix y `GET_EFFECTIVE_CONFIG`; bloquear 169, reserved y 2440 hasta soporte real |
| 7. Alternativas | Rechazo estricto vs clamping explícito. Preferir rechazo por defecto y clamping opt-in reportado |
| 8. Resultado esperado | El host sabe qué fue solicitado, aplicado o rechazado |
| 9. Interfaces | Nuevos error codes/config response; deprecar `format_conf` crudo o envolverlo en tipo avanzado |
| 10. Dependencias | Hardware target y command/patch por modulación |
| 11. Riesgos | Presets antiguos pasan de ACK a error; requiere migration notes |
| 12. Hardware | Tabla por board/front-end; no prometer PA/banda global |
| 13. Validación | Boundary tests + readback + analizador por backend soportado |
| 14. Aceptación | Ningún valor reservado/169/2440 defectuoso recibe ACK “válido”; readback coincide con registros/medición dentro de tolerancia |
| 15. Complejidad | Alta |
| 16. Prioridad | P1/P1; gating inmediato P0/P1 |
| 17. Confianza | Alta en necesidad; detalles de hardware pendientes |

### DEV-06 — Operaciones TX observables

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-06 |
| 2. Problema | BURST/CONTINUOUS no reportan progreso, fallo ni completion; count no tiene semántica verificable |
| 3. Comportamiento actual | Scheduler decrementa count después de `RadioIF_transmitRaw()`; al fallar limpia estado silenciosamente |
| 4. Evidencia | `control_task.c` L360–408; 1/5 y 1/40 |
| 5. Componentes | ControlTask, RadioIF status, wire, Python API |
| 6. Enfoque | Operation state machine con attempts/accepted/completed/error status TI; eventos y stop idempotente |
| 7. Alternativas | Polling de `GET_OPERATION`; push events. Preferir ambos: events + query recuperable |
| 8. Resultado esperado | Host distingue scheduled, RF-completed, stopped y failed |
| 9. Interfaces | `operation_id`, result object/events; métodos legacy pueden esperar sólo ACK con warning |
| 10. Dependencias | DEV-02 y definición de éxito |
| 11. Riesgos | Más tráfico/estado; wrap de IDs; compatibilidad |
| 12. Hardware | Receptor/analizador para separar completion RF de OTA |
| 13. Validación | Burst N={1,5,40}, intervalos, continuous 0/250ms, stop/fail injection |
| 14. Aceptación | Conteos internos exactos y evento terminal único; discrepancia OTA reportada, no ocultada |
| 15. Complejidad | Alta |
| 16. Prioridad | P1/P1 |
| 17. Confianza | Alta en brecha; semántica a aprobar |

### DEV-07 — Abstracción de placa y Cat-Shell

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-07 |
| 2. Problema | Reset y band selection dependen de puerto derivado/scripts externos |
| 3. Comportamiento actual | Bridge+2; no reply validation; FeralRF CC no conoce U2 |
| 4. Evidencia | GAP-10; COM88/COM87; hardware report |
| 5. Componentes | Python device discovery, RP Cat-Shell/USB descriptors, examples |
| 6. Enfoque | Descubrir interfaces por parent/location/serial; objeto Board con bridge/shell explícitos; comandos versionados y estado leído |
| 7. Alternativas | Sólo `shell_port` obligatorio. Útil como fallback, pero no suficiente para auto-discovery |
| 8. Resultado esperado | Reset/banda afectan inequívocamente a la unidad correcta |
| 9. Interfaces | `Radio(..., shell_port=...)`/`CatSnifferBoard`; `set_band()` y `get_board_status()` |
| 10. Dependencias | Descriptores por SO y DEV-01 |
| 11. Riesgos | Reenumeración Windows y firmware RP antiguo |
| 12. Hardware | Dos o más unidades simultáneas, variantes RP/SAMD si entran en alcance |
| 13. Validación | Windows/Linux/macOS, desconexión/reconexión y pairing negativo |
| 14. Aceptación | 100% pairing correcto con ≥2 placas; no se usa aritmética de COM |
| 15. Complejidad | Media-alta |
| 16. Prioridad | P1/P1 |
| 17. Confianza | Alta |

### DEV-08 — Telemetría de colas y transporte

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-08 |
| 2. Problema | Drops y ocupación no son visibles end-to-end |
| 3. Comportamiento actual | Métricas RF parciales; output drop interno; RP sólo cuenta parte de dirección; host ignora parse errors |
| 4. Evidencia | GAP-08/09 |
| 5. Componentes | RadioIF, PacketQueue, HostIF, RP bridge, Python |
| 6. Enfoque | Contadores monotónicos por etapa, high-water marks, last-error, timestamps y snapshot atómico/consistente |
| 7. Alternativas | Logging debug vs counters wire. Preferir counters siempre disponibles y tracing compilable |
| 8. Resultado esperado | Cada pérdida se localiza a una etapa |
| 9. Interfaces | Stats v2/TLV y Cat-Shell board stats |
| 10. Dependencias | DEV-01/02 |
| 11. Riesgos | Overhead e inconsistencias de snapshot |
| 12. Hardware | Stress USB/UART real |
| 13. Validación | Saturación inducida conocida y reconciliación de contadores |
| 14. Aceptación | Bytes/frames in/out/drop cuadran bajo test; overflow nunca es silencioso |
| 15. Complejidad | Media |
| 16. Prioridad | P1/P1 |
| 17. Confianza | Alta |

### DEV-09 — Gating seguro de bandas/modulaciones y backend 2.4

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-09 |
| 2. Problema | Presets 169, 2440 y modType reservados pueden recibir ACK; 433 no tiene evidencia de placa |
| 3. Comportamiento actual | 2440 cae a 868; 169 usa ruta incoherente; 4/5/6 reservados; 433 depende de hardware |
| 4. Evidencia | GAP-05/14/15/16; `presets.py`, `radio_if.c`, SDK TI y [HARDWARE_INTEGRATION.md](HARDWARE_INTEGRATION.md) |
| 5. Componentes | Presets, validation host/C, RadioIF backends, board capabilities |
| 6. Enfoque | Bloqueo inmediato/capability gating; investigar backend proprietary 2.4 dedicado; catalogar 433 por revisión; retirar 169 del target P7 |
| 7. Alternativas | Ocultar presets vs conservar con `unsupported_reason`. Preferir conservar para procedencia, pero impedir ejecución sin backend/capability |
| 8. Resultado esperado | Ningún ACK implica una banda/modulación distinta a la solicitada |
| 9. Interfaces | `UnsupportedConfigurationError`, preset metadata y capabilities |
| 10. Dependencias | DEV-05 y matriz de hardware |
| 11. Riesgos | Cambios incompatibles en scripts históricos; documentar override de laboratorio sólo cuando sea seguro |
| 12. Hardware | 2.4 y 433 requieren frontend/antena/instrumento; 169 exige otro hardware |
| 13. Validación | Tests negativos inmediatos; positivos sólo tras backend y medición |
| 14. Aceptación | Reserved/169/2440 actual se rechazan antes de RF; 433 muestra gate explícito |
| 15. Complejidad | Gating baja; backend 2.4 alta; caracterización 433 alta |
| 16. Prioridad | Investigar P0; gating P1; extensiones P2 |
| 17. Confianza | Alta en gating, media en viabilidad 2.4/433 |

### DEV-10 — Lifecycle OOK seguro y validación compuesta

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-10 |
| 2. Problema | OOK puede dejar el RF core en estado no reversible por software; existe una observación histórica 868 MHz 10/10, pero RF-05 no se ejecutó en la campaña reciente |
| 3. Comportamiento actual | Patch genook y ruta existen; source exige power-cycle para volver a otros modos. El informe agregado histórico respalda entrega raw, no la seguridad del lifecycle actual ni la frecuencia/modulación radiadas |
| 4. Evidencia | GAP-13; `radio_if.c`; [informe histórico agregado y RF-05 NOT RUN §5.4](EXPERIMENTAL_EVIDENCE_REFERENCE.md); [readiness OOK](PENDING_VALIDATION_ASSESSMENT.md#4-ook-868-procedimiento-controlado-reconstruible) |
| 5. Componentes | RadioIF, API state, Cat-Shell/power procedure, HIL |
| 6. Enfoque | Tratar OOK como sesión terminal explícita; advertencia/gate; ejecutar RF-05 sólo con identidad y recuperación aprobadas; estudiar aislamiento backend |
| 7. Alternativas | Deshabilitar OOK por defecto; mantener modo experimental con `force=True`. Preferir feature gate explícito |
| 8. Resultado esperado | El usuario conoce el costo de recuperación; no se continúa en estado ambiguo |
| 9. Interfaces | State/capability `requires_power_cycle`; error en cambios posteriores |
| 10. Dependencias | DEV-01/05/07 y hardware 868 |
| 11. Riesgos | Lock que exige intervención física; regulación RF |
| 12. Hardware | Dos placas, 868, caja apantallada, power-cycle controlado |
| 13. Validación | RF-05, sólo con autorización separada: ≥9/10 + power-cycle + preflight + IEEE control PASS; la repetición aporta trazabilidad actual y recuperación, no determina por sí sola la forma de onda |
| 14. Aceptación | Resultado compuesto completo; cero sesión posterior no autorizada |
| 15. Complejidad | Media para lifecycle; alta si se rediseña patch/backend |
| 16. Prioridad | Investigar P1; implementar P2 provisional |
| 17. Confianza | Alta en la restricción y en la observación histórica reportada; media en la repetibilidad actual porque RF-05 sigue NOT RUN |

### DEV-11 — Cerrar contratos Crypto y KillerBee

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-11 |
| 2. Problema | Crypto acepta combinaciones imposibles y su resultado histórico 9/9 no tiene todos los primarios accesibles; KillerBee no es instalable/reproducible con el extra actual |
| 3. Comportamiento actual | El informe agregado registra 9/9 en vectores Crypto; el contrato conserva ECDSA Curve25519 vs P-256 y límites CCM/GCM. La dependencia `killerbee>=3.0.0` no se resolvió en RF-04 |
| 4. Evidencia | GAP-17/19; [evaluación de evidencia](EXPERIMENTAL_EVIDENCE_REFERENCE.md); [readiness](PENDING_VALIDATION_ASSESSMENT.md) |
| 5. Componentes | Python validation/packaging, crypto_engine, adapter KillerBee |
| 6. Enfoque | Separar workstreams: schema crypto + vectors; seleccionar fork/version KillerBee + contrato FCS/metadata/errors |
| 7. Alternativas | Retirar extra KillerBee hasta soporte; documentar integración manual. Preferir paquete reproducible o etiqueta experimental clara |
| 8. Resultado esperado | Solicitudes crypto válidas y adapter reproducible |
| 9. Interfaces | Error types/limits; adapter API y extras versionados |
| 10. Dependencias | DEV-02/03 para KillerBee; decisión de soporte |
| 11. Riesgos | Dependencia upstream y diferencias de API |
| 12. Hardware | Vectores crypto en CC; peer IEEE para KillerBee |
| 13. Validación | Repetición identificada de los 9 vectores actuales y vectores conocidos adicionales; instalación limpia + captura pasiva + PCAP para KillerBee. Inyección sólo con requisito, red aislada y autorización separados |
| 14. Aceptación | Todas las combinaciones documentadas pasan/fallan localmente de forma definida; RF-04 reproducible |
| 15. Complejidad | Media cada workstream |
| 16. Prioridad | P2/P2 |
| 17. Confianza | Alta en brechas; soporte KillerBee depende de upstream |

### DEV-12 — Banco HIL y criterios por nivel

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-12 |
| 2. Problema | Evidencia desigual, binarios no identificados y claims mezclan ACK/OTA/protocolo/física |
| 3. Comportamiento actual | Scripts/reportes heterogéneos; varias observaciones sólo son inspeccionables mediante informes narrativos o resúmenes; no hay instrumentación RF para claims físicos |
| 4. Evidencia | Matriz de 193 ejecuciones: 125 con registro detallado inspeccionado y 68 sustentadas por informe/resumen/claim sin todos los primarios; contradicciones Sub-GHz preservadas |
| 5. Componentes | CI/HIL, manifests, dos placas, instrumentos, report generator |
| 6. Enfoque | Dos niveles. **Baseline inicial:** tests deterministas host/C, fixtures de protocolo, chequeos estáticos de contrato, vectores metadata y controles mínimos de hardware documentados. **HIL completo posterior:** multi-board automatizado, manifests, telemetría por etapa, fault injection, instrumentos y reportes normalizados |
| 7. Alternativas | CI sólo host/mocks cubre el baseline lógico y debe coexistir; no sustituye la validación OTA/física del HIL completo |
| 8. Resultado esperado | Regresiones comparables y claims acotados |
| 9. Interfaces | Harness CLI, artifact manifest y formato de resultados versionado |
| 10. Dependencias | Baseline inicial: ninguna de DEV-01/02/08 completa. HIL completo: DEV-01/02/08, identidad de placas y laboratorio |
| 11. Riesgos | Flakiness RF y costos de instrumentación |
| 12. Hardware | Baseline: controles mínimos reproducibles y puertos/unidades documentados. Completo: dos o más placas identificadas, fixture, carga/antenas, analizador/power meter según claim |
| 13. Validación | Baseline: canary host, fixtures, metadata y controles estáticos. Completo: control IEEE, proprietary baseline, negative controls, recovery, fault injection e instrumentos |
| 14. Aceptación | Criterio provisional: cada test registra configuración, evidencia, stage status y cleanup; ningún ACK se clasifica OTA; la cobertura física exigida se aprueba por nivel |
| 15. Complejidad | Alta |
| 16. Prioridad | Investigar P0; implementar P1 |
| 17. Confianza | Alta |

### DEV-13 — Taxonomía y mantenimiento documental

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-13 |
| 2. Problema | Documentos y nombres mezclan preset, PHY, stack, emulación y validación |
| 3. Comportamiento actual | 31 presets; matrices históricas y docs de épocas distintas; helper W-MBus obsoleto |
| 4. Evidencia | F-16, B-03/B-05, esta consolidación |
| 5. Componentes | README, API docs, presets metadata, ejemplos, reportes canónicos |
| 6. Enfoque | Declarar nivel de soporte por capability; enlaces canónicos; deprecation notes; conservar históricos sin reescribirlos |
| 7. Alternativas | Renombrar todos los presets rompe scripts; preferir metadata/deprecation gradual |
| 8. Resultado esperado | El lector distingue configuración, raw, framing, procedimientos e interoperabilidad |
| 9. Interfaces | Docs y posiblemente `PresetCapabilities` |
| 10. Dependencias | Decisión de alcance y DEV-05 |
| 11. Riesgos | Duplicación si no se fija ownership documental |
| 12. Hardware | Documentar por revisión |
| 13. Validación | Link checker + revisión cruzada source/matrix |
| 14. Aceptación | Cada feature pública declara implementation status y validation level; cero claims de stack por preset |
| 15. Complejidad | Media |
| 16. Prioridad | P1/P1 |
| 17. Confianza | Alta |

### DEV-14 — Estudio de factibilidad MIOTY antes de implementación

| Campo requerido | Propuesta |
|---|---|
| 1. ID | DEV-14 |
| 2. Problema | El preset no implementa TS-UNB; se desconoce costo/viabilidad del RF core y rol requerido |
| 3. Comportamiento actual | El preset pide 868000000 Hz, `mod_type=0`, 396 baud, `deviation=1`, `rx_bw=0x4A` y sync `0x930B51DE`. El backend calcula `rateWord=259` con `pre_scale=15` (395.20263671875 baud teóricos) y una desviación nominal teórica de 250 Hz; sigue siendo packet FSK genérico sin readback. El claim 0/10 no evalúa MIOTY |
| 4. Evidencia | [ETSI TS 103 357-2](https://www.etsi.org/deliver/etsi_ts/103300_103399/10335702/02.01.01_60/ts_10335702v020101p.pdf); `presets.py`, `radio.py`, `commands.py`, `command_processor.c`, `radio_if.c`, SDK TI y GAP-21 |
| 5. Componentes | RF command/patch o modem externo, encoder/decoder, splitting, coding/interleaving, scheduler temporal/frecuencial, reensamble, API/procedimientos/security y test peer |
| 6. Enfoque | Definir rol/modo/revisión; gap normativo; consultar ruta TI/vendor; prototipo offline de splitting/coding/interleaving y decoder; evaluar scheduler/backend y recepción/reensamble antes de RF; sólo después decidir implementación |
| 7. Alternativas | Integrar modem/stack externo certificado; declarar MIOTY fuera de alcance; ambas pueden ser mejores que implementarlo en CC |
| 8. Resultado esperado | Decisión go/no-go con arquitectura, riesgos y criterios |
| 9. Interfaces | Ningún cambio público antes del go; luego API de telegram/role, no simple preset |
| 10. Dependencias | Aprobación del asesor, acceso normativo, soporte TI/vendor o backend viable, base station/peer independiente y presupuesto |
| 11. Riesgos | Alta complejidad, propiedad intelectual/certificación, timing y seguridad |
| 12. Hardware | Confirmar modulación, precisión/timing y front-end 868 |
| 13. Validación | Vectores ETSI, encoder/decoder offline, distribución de sub-ráfagas verificable, analizador y base station independiente |
| 14. Aceptación | Criterio provisional: informe go/no-go responde RF, splitting/coding/interleaving, scheduling, reensamble, procedimientos/security, hardware, ruta vendor y costo; implementación sólo con requisitos aprobados |
| 15. Complejidad | Investigación alta; implementación muy alta/indeterminada |
| 16. Prioridad | Investigar P2; implementar P3/pendiente de decisión |
| 17. Confianza | Alta en ausencia actual; baja-media en viabilidad final |

## 4. Decisión de alcance del producto

| Dirección | Núcleo requerido | Qué no sería obligatorio | Cómo cambia prioridades |
|---|---|---|---|
| A. Toolkit PHY/raw | Config efectiva, metadata, lifecycle, TX/RX observable, framing documentado | Pilas Zigbee/Thread/Wi-SUN/Sidewalk/W-MBus/MIOTY | DEV-01…13; protocolos quedan P3 o fuera de alcance |
| B. Toolkit de framing/parsing | Lo anterior + encoders/decoders versionados por protocolo | Asociación/red/security completa si no se promete | Después de estabilizar, seleccionar 1–2 familias; tests con vectores |
| C. Plataforma de emulación | Lo anterior + state machines, timing, replies, identidad y recovery por dispositivo | Certificación universal | Exige HIL/peers y fuerte diseño de lifecycle |
| D. Implementaciones interoperables seleccionadas | Stack/rol completo, seguridad y conformance del protocolo elegido | Otros protocolos no seleccionados | Programa separado por protocolo; MIOTY sólo tras DEV-14 |

Recomendación provisional: consolidar primero **A**. La arquitectura actual y la evidencia respaldan ese rol; A no impide evolucionar a B/C/D, pero evita que capas opcionales compitan con defectos transversales.

## 5. Secuencia técnica por etapas

### Etapa 1 — Corrección y reproducibilidad

- **Objetivo:** que cada build/run sea identificable y que configuraciones imposibles no parezcan válidas.
- **Cambios:** DEV-01, DEV-04, gating inicial de DEV-09, actualización DEV-13 y baseline inicial de DEV-12: tests host/C, fixtures de protocolo, chequeos estáticos, vectores metadata y controles mínimos de hardware.
- **Prerrequisitos:** formato de versionado y política de compatibilidad.
- **Entregables:** build manifest, metadata PROP corregida/versionada, rejection matrix y baseline de regresión ejecutable con reporte versionado.
- **Riesgos:** romper scripts que dependan de aceptación permisiva.
- **Gate:** source↔artifact↔device trazable; reserved/169/2440 actual rechazados; vectores metadata y baseline lógico pasan.
- **Definition of done:** no hay claim de configuración/metadata que dependa de un campo conocido incorrecto.

### Etapa 2 — Lifecycle y observabilidad

- **Objetivo:** eliminar pérdidas silenciosas y producir estados finales verificables.
- **Cambios:** DEV-02, DEV-03, DEV-06, DEV-07 y DEV-08.
- **Prerrequisitos:** Etapa 1 y contrato de eventos aprobado.
- **Entregables:** dispatcher, operation IDs, stop completion, stats v2 y Board abstraction.
- **Riesgos:** cambios multi-capa y compatibilidad de protocolo.
- **Gate:** criterio provisional: ≥100 ciclos RX/stop; interleaving sin pérdidas; BURST/CONTINUOUS con evento terminal y conteos reconciliados.
- **Definition of done:** ningún comando central termina sólo en timeout/ACK ambiguo sin estado diagnosticable.

### Etapa 3 — Configuración RF y completitud del toolkit

- **Objetivo:** alinear requested/accepted/effective y validar features existentes.
- **Cambios:** DEV-05 completo, power readback, DEV-10 OOK, crypto/KillerBee seleccionados, estudio 2.4/433.
- **Prerrequisitos:** HIL completo de DEV-12 y hardware/revisión identificados; el baseline inicial ya existe desde Etapa 1.
- **Entregables:** capability matrix por board, config readback, resultados instrumentados y lifecycle OOK.
- **Riesgos:** hardware/regulación y patches TI.
- **Gate:** cada backend soportado tiene negative tests, peer test y medición aplicable.
- **Definition of done:** los presets soportados tienen alcance y evidencia individual; los demás fallan explícitamente.

### Etapa 4 — Extensiones seleccionadas

- **Objetivo:** implementar sólo framing/emulación/interoperabilidad aprobados.
- **Cambios:** según dirección B/C/D; DEV-14 para MIOTY; posibles IEEE MAC, W-MBus u otra familia elegida.
- **Prerrequisitos:** requisitos de rol/versión, acceso a especificación y peer independiente.
- **Entregables:** especificación, encoder/parser/state machine, security si aplica y suite de conformance acotada.
- **Riesgos:** complejidad, certificación, ecosistema y mantenimiento.
- **Gate:** vectores oficiales + interoperabilidad por rol; no sólo CatSniffer↔CatSniffer.
- **Definition of done:** criterios del protocolo seleccionado cumplidos; ningún nombre de preset sustituye la evidencia.

## 6. Decisiones requeridas al asesor

1. Aprobar el rol A/B/C/D y qué se excluye explícitamente.
2. Definir qué significa “TX exitoso” y qué nivel de validación es suficiente.
3. Aprobar o descartar soporte de 433, proprietary 2.4, OOK y high-PA según hardware/regulación.
4. Seleccionar, como máximo inicialmente, una familia de protocolo superior con rol concreto.
5. Decidir si MIOTY merece estudio de factibilidad P2 o se documenta fuera de alcance.
6. Aprobar recursos HIL/instrumentación y una política de compatibilidad del wire protocol.

