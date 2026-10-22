# Resumen ejecutivo — estado y propuesta de FeralRF

**Corte de evidencia:** 2026-10-22  
**Fuente principal:** `feature/hardware-validation` @ `fbff1b7f5077964418fb63a3fdf346a27c35a003`

## ¿Qué es CatSniffer/FeralRF?

CatSniffer es una plataforma con un RP2040 de interfaz, un CC1352P7 con radio integrado Sub-1/2.4 GHz y un SX1262 independiente. FeralRF es la biblioteca Python y el firmware TI-RTOS que controlan el CC1352P7. La ruta evaluada es:

`PC/Python ↔ Cat-Bridge USB ↔ RP2040 ↔ UART ↔ CC1352P7 ↔ RF core/front-end ↔ switch U2 ↔ antena`.

El RP2040 transporta los bytes y controla reset, boot y las líneas externas de selección de banda mediante Cat-Shell. El CC1352P7 interpreta el protocolo FeralRF, configura el PHY y ejecuta RX/TX. Cat-LoRa/SX1262 es otra ruta y no forma parte de las operaciones FeralRF del CC1352P7.

La fuente FeralRF auditada coincide con la revisión esperada. El SDK TI está fijado a 8.30.01.01. El CatSniffer-Firmware disponible es `v3.x` @ `c0cd5a4` (`v3.1.0.1`). La placa fue registrada como CatSniffer v3.1 y U8 se observó marcado `CC1352P74`, pero el esquema histórico v3.1 nombra un P1. No existe todavía una BOM/ECO que vincule sin ambigüedad ese esquema con toda placa P7 evaluada.

## Estado de capacidades

| Área | Implementación | Evidencia actual | Conclusión defendible |
|---|---|---|---|
| Conexión/init/info/stats | Presente | Uso repetido | Funcional; identidad del binario insuficiente |
| IEEE 802.15.4 RX | Presente | RX ambiental y peer | Demostrado como PHY/transporte de PSDU |
| IEEE RAW/FRAME TX | Presente | 20/20, 10/10 y otras corridas | Entrega raw demostrada entre dos CatSniffer |
| Switching IEEE | Presente | Una secuencia 25→20→25; cleanup final falló | Capacidad estrecha observada, estabilidad no demostrada |
| Proprietary FSK/GFSK | Presente | Campañas positivas y negativas | Entrega raw posible; fiabilidad/causa histórica no cerradas |
| BURST/CONTINUOUS | Presente con contrato incompleto | BURST 1/5 y 1/40; continuous rápido con actividad abundante | Programación/actividad, pero count/completion no verificables |
| CW/PRBS/TX_STOP | Presente | ACKs | Control demostrado; señal/cese físicos no medidos |
| BLE | Advertising limitado | Informe histórico agregado: 1M 10/10, 2M 8/10, Coded S8 10/10 y S2 10/10; sin peer BLE externo identificado | Entrega raw reportada; no es pila BLE completa ni interoperabilidad externa |
| OOK | Ruta implementada | Informe histórico 868 MHz 10/10; RF-05 reciente no ejecutado | Entrega raw histórica reportada; repetibilidad actual y lifecycle seguro pendientes |
| MSK/4-FSK/4-GFSK | Seis presets nominales (tres a 433 y tres a 868 MHz) | Valores 4/5/6 reservados en SDK | No soportados por el comando actual |
| Crypto | Primitivas TI presentes | Informe histórico agregado registra 9/9; primarios completos no accesibles y campaña reciente no repetida | Observación histórica válida para esos vectores; contrato aún parcial y requiere repetición identificada |
| KillerBee | Adapter presente | RF-04 bloqueado por dependencia | Integración no reproducible todavía |
| Spectrum | Sin backend funcional | No ejecutado | Ausente |
| Wi-SUN/Sidewalk/W-MBus/MIOTY | Parámetros/helpers nominales | Raw o análisis estático | No son pilas ni interoperabilidad de esos protocolos |

## Qué se ha demostrado

La evidencia sostiene que FeralRF:

- descubre, abre, inicializa y consulta el dispositivo en los entornos registrados;
- recibe IEEE 802.15.4 en canal 25;
- transmite RAW y FRAME IEEE a otro CatSniffer;
- rechaza TX durante RX con `ERR_INVALID_STATE=0x05` y recupera el control;
- cambia entre canales IEEE 25 y 20 en una ejecución reciente, incluido un control negativo 25/20;
- entrega paquetes raw proprietary FSK/GFSK entre dos CatSniffer para configuraciones concretas en 868, 868.3, 868.95, 902.2 y 915 MHz solicitados;
- invierte la dirección del enlace Sub-GHz en RF-02 con 10/10 y cleanup correcto;
- conserva como observaciones históricas reportadas BLE raw por cuatro PHY, OOK 868 10/10, crypto 9/9 y firmas de emulación raw, aunque sus artefactos primarios completos no estén incluidos.

Estas pruebas no certifican frecuencia, potencia o modulación radiadas; tampoco prueban Wi-SUN, Sidewalk, Wireless M-Bus, Zigbee, Thread o MIOTY. El peer usa la misma implementación, no una pila externa independiente.

La contabilidad completa comprende 193 ejecuciones/intentos: 125 con registro primario o reporte detallado inspeccionado y 68 sustentados por informes agregados, resúmenes o claims sin todos los artefactos primarios. Estos 68 no se consideran automáticamente inválidos ni “no ejecutados”: el tipo de fuente limita la inspección independiente, la reproducción exacta y la atribución causal. La procedencia está en la [referencia experimental autocontenida](EXPERIMENTAL_EVIDENCE_REFERENCE.md) y la factibilidad de lo pendiente en la [evaluación de readiness](PENDING_VALIDATION_ASSESSMENT.md).

## Fallos, bloqueos e incertidumbres principales

El problema operacional más importante es `RX_STOP`: varias sesiones reciben correctamente y luego agotan timeout esperando la confirmación. Las respuestas `0x90` compiten con la espera síncrona y el source confirma una política de secuencias que puede descartar eventos RX; aun así, falta instrumentación para identificar toda la cadena causal. Este problema bloqueó RF-03 antes de llegar a la transición IEEE↔Sub-GHz.

La metadata proprietary contiene un defecto determinista: el parser no interpreta los campos agregados por el RF core en el orden configurado. RF-02 observó `channel=25` residual y RSSI incoherente. Por tanto, la entrega del payload es evidencia válida, pero RSSI, CRC/filter y timestamp no deben usarse como mediciones confiables hasta corregir y validar el layout.

La historia Sub-GHz debe conservarse sin una explicación inventada: F29 afirmó 70/70 sin transcript; otra campaña produjo 0/70 y luego 0/240; los checkpoints de recuperación produjeron 60/60; trece presets lograron 10/10; RF-02 inverso logró 10/10. Esto demuestra capacidad real, no fiabilidad universal ni una regresión causalmente identificada.

Otros límites relevantes:

- ACK temprano sin progreso/completion para TX diferido;
- `SET_PROP_CONFIG` sin validación semántica ni readback efectivo;
- build/version/serial insuficientes para unir source, binario y dispositivo;
- Cat-Shell derivado como Bridge + 2, contradicho por COM88/COM87;
- 169 MHz fuera del alcance defendible y con backend incoherente;
- proprietary 2.4 GHz cae a 868 MHz en la ruta actual;
- 433 MHz requiere verificar placa/front-end/antena;
- OOK conserva una observación histórica 868 MHz 10/10, pero necesita una repetición identificada con lifecycle y power-cycle controlados;
- KillerBee quedó bloqueado antes de hardware; RF-05 OOK no se ejecutó.

MIOTY merece una precaución específica: el preset actual sólo configura un paquete FSK genérico a 868 MHz y ~395.20 baud efectivos teóricos, con desviación nominal teórica de 250 Hz y sin readback. No implementa la división, codificación/interleaving, planificación de sub-ráfagas ni reensamble de [ETSI TS 103 357-2](https://www.etsi.org/deliver/etsi_ts/103300_103399/10335702/02.01.01_60/ts_10335702v020101p.pdf). El claim histórico 0/10 no constituye una prueba MIOTY.

## Madurez actual

FeralRF es un **prototipo de investigación funcional y valioso como toolkit PHY/raw**, no un producto de protocolos completo ni una plataforma de medición RF calibrada. La arquitectura base y los enlaces positivos justifican continuar. La robustez de lifecycle, observabilidad, identidad y contrato API todavía no soporta una afirmación de operación universal o automatización sin supervisión.

## Prioridades preliminares

| Prioridad | Acción | Razón |
|---|---|---|
| P0 | Manifest source→artifact→device | Hace comparables todas las validaciones |
| P0 | Corregir metadata proprietary | Defecto determinista que invalida campos RX |
| P0 | Rediseñar contrato async response/event | Evita pérdida host-visible y habilita lifecycle estable |
| P0 investigación | Instrumentar `RX_STOP` end-to-end | El síntoma es reproducible, el mecanismo completo no |
| P1 | Validación/readback/capability gating | Evita ACK de bandas/modos imposibles |
| P1 | Completion/progreso para TX | Hace verificables BURST/CONTINUOUS/STOP |
| P1 | Integración segura de Cat-Shell | Elimina heurística de puertos y expone banda/reset |
| P0 baseline / P1 HIL completo | Regresión por niveles | El baseline host/C y metadata puede empezar ya; multi-board, fault injection e instrumentos vienen después |
| P2 | OOK, crypto y KillerBee | Revalidar OOK/Crypto con trazabilidad actual y cerrar lifecycle/contratos; desbloquear packaging KillerBee |
| P2 investigación / P3 implementación | MIOTY | TS-UNB está ausente y el costo depende del rol aprobado |

P0 no significa “implementar toda capacidad ausente”. Se reserva para correcciones transversales y reproducibilidad. La prioridad de investigar `RX_STOP` es mayor que la prioridad de aplicar una solución todavía no sustentada.

Los umbrales numéricos de aceptación propuestos —ciclos, ratios y SLA— son provisionales hasta aprobación del asesor.

## Decisiones que requieren aprobación del asesor

FeralRF puede evolucionar en cuatro direcciones con costos muy distintos:

1. toolkit PHY/raw;
2. toolkit de construcción y parseo de tramas;
3. plataforma de emulación con state machines;
4. implementaciones interoperables de protocolos seleccionados.

La recomendación provisional es consolidar primero la opción 1. Si se elige una opción superior, debe seleccionarse un rol y protocolo concretos, no “todos los protocolos”. Para MIOTY se requiere primero un estudio de factibilidad: modalidad ETSI TS-UNB, soporte real del RF core/patch o ruta vendor, división/codificación/interleaving, scheduler temporal/frecuencial, decoder/reensamble, procedimientos, seguridad y acceso a base station/peer independiente.

También deben aprobarse la semántica de “TX exitoso”, el nivel de evidencia requerido, la política de compatibilidad del protocolo wire, los recursos de laboratorio y el alcance de 433/OOK/proprietary 2.4/high-PA.

## Secuencia propuesta

1. **Corrección y reproducibilidad:** identidad, metadata, bloqueo de configuraciones inseguras, baseline de regresión.
2. **Lifecycle y observabilidad:** eventos, `RX_STOP`, completion TX, stats y abstracción Cat-Shell.
3. **Configuración y completitud:** readback, potencia efectiva, OOK, hardware 433/2.4, crypto/KillerBee.
4. **Protocolos seleccionados:** framing, emulación o stack sólo con rol, estándar, peer y criterios aprobados.

Cada etapa tiene gates verificables en [DEVELOPMENT_PROPOSAL.md](DEVELOPMENT_PROPOSAL.md). La evidencia por capacidad está en [VALIDATION_MATRIX_CURRENT.md](VALIDATION_MATRIX_CURRENT.md), su procedencia/contabilidad en [EXPERIMENTAL_EVIDENCE_REFERENCE.md](EXPERIMENTAL_EVIDENCE_REFERENCE.md) y la disposición técnica de ensayos pendientes en [PENDING_VALIDATION_ASSESSMENT.md](PENDING_VALIDATION_ASSESSMENT.md); las causas y categorías se desarrollan en [FUNCTIONAL_GAPS.md](FUNCTIONAL_GAPS.md); la arquitectura física está en [HARDWARE_INTEGRATION.md](HARDWARE_INTEGRATION.md); el análisis completo está en [PROJECT_STATUS_REPORT.md](PROJECT_STATUS_REPORT.md).

## Conclusión

FeralRF ya demuestra control RF y transferencia raw útil, pero necesita corregir contratos y observabilidad antes de ampliar promesas. La decisión técnica más sólida no es añadir muchos presets o stacks, sino volver reproducible, medible y estable el núcleo que ya funciona. Después, el asesor puede seleccionar con evidencia qué nivel de protocolo justifica el costo de desarrollo.

