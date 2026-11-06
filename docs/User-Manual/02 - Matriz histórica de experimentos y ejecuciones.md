# Matriz histórica de experimentos y ejecuciones

## Cómo leerla

`n` es el número de unidades de ejecución identificadas en la fila. Una fila agregada no implica que todas las unidades tengan igual calidad documental. `Suf.` indica cuántas tienen material suficiente para sostener la observación estrecha; `Inc.` indica resumen, procedencia o salida incompleta. Los documentos duplicados del reporte del 12-oct no suman evidencia.

## Resumen contable

| Familia/campaña | n | Suf. | Inc. |
|---|---:|---:|---:|
| Baseline abril 2026 (control, OTA, crypto, emulación) | 46 | 0 | 46 |
| F29 mayo 2026 | 7 | 0 | 7 |
| EV-00…EV-14, sin duplicar las corridas OTA empaquetadas después | 63 | 52 | 11 |
| Campañas detalladas 6–12-oct (IEEE, propietaria y baseline nuevo) | 43 | 39 | 4 |
| Recuperación y campaña extendida 15–16-oct | 29 | 29 | 0 |
| Campaña RF 20-oct | 5 | 5 | 0 |
| **Total** | **193** | **125** | **68** |

Los 46 del baseline son afirmaciones de matriz/commit sin logs crudos localizados; “Inc.” no afirma que no ocurrieran. RF-04 cuenta como intento experimental bloqueado antes del hardware. EV-01 no suma porque no hubo corrida dedicada. RF-05 figura como `NOT RUN` pero no suma.

## Matriz A — historia por ejecución o grupo homogéneo

| ID consolidado | Fecha/campaña | n | Objetivo/configuración | Resultado histórico | Evaluación actual | Evidencia y limitación principal |
|---|---:|---:|---|---|---|---|
| APR-CTRL | 08-abr-2026 | 18 | Baseline de control | 18/18 PASS | INCONCLUSIVE como inventario individual | `FeralRF/docs/VALIDATION_MATRIX.md`; sin logs/argumentos por fila. |
| APR-OTA | 08-abr-2026 | 17 | BLE/IEEE/prop/OOK/WMBus entre dos placas | PASS/marginal/FAIL por fila | Afirmación histórica | Conteos en matriz; condiciones/binarios/raw no recuperados. No aceptar causas físicas declaradas sin medición. |
| APR-CRYPTO | 30-abr-2026 | 9 | TRNG, AES, SHA, ECDH, ECDSA | 9/9 PASS | Afirmación de hardware pendiente de artefactos | Fuera del foco RF; conservar para cobertura histórica. |
| APR-EMU | abr–may-2026 | 2 | Personalidades IEEE y Sub-GHz | 20/20 por familia | Afirmación histórica | No equivale a interoperabilidad externa de protocolo. |
| F29-7P | 03-may-2026 | 7 | Sidewalk/Wi-SUN raw, diez marcadores/preset | 70/70 | INCONCLUSIVE como reproducción; claim preservado | Commit `f5863c3`, script recuperable; transcript y binarios ausentes. |
| EV-00 | antes del 05-oct | 1 | Enumeración/mapeo | Provisional | PARTIAL | Catnip agrupó COM88/86/87; falta identidad USB completa. |
| EV-02/05 | antes del 05-oct | 5 | IEEE RX canal 25 | PASS-RF | PASS acotado | Dos preliminares 48/100 y tres formales 41/43/43; no PCAP/bytes completos. |
| EV-03 | antes del 05-oct | 5 | Reconexión entre procesos | PASS | PASS-control | Cinco `init()`/cierres; no USB loss ni misma instancia. |
| EV-04 | antes del 05-oct | 1 | `boot`/`exit` y reset | PASS parcial | PARTIAL / API BLOCKED | Manual recuperó 1/1; `reset_device()` calculó COM90 frente a COM87 y no se ejecutó completo. |
| EV-06 | antes del 05-oct | 2 | TX solicitado durante RX | PASS | PASS-control | Dos ejecuciones válidas; 0x05 y recuperación. Intentos de quoting/import no cuentan. |
| EV-10 | antes del 05-oct | 8 | PHY 0–7 por control | 8/8 PASS | PASS-control | Reset entre filas; sin evidencia RF por PHY. |
| EV-11 | antes del 08-oct | 28 | 27 presets únicos + repetición GFSK868 | 27/27 control | 19 ejecuciones auditables; 9 resumidas | No OTA; nombres MSK/4FSK/4GFSK no prueban modulación efectiva. |
| EV-12-CTRL | antes del 06-oct | 4 | RAW/FRAME/BURST/CONTINUOUS | 4/4 ACK | PASS-control | Sin observador RF en esa etapa. |
| EV-13 | después de EV-12 | 4 | CW, PRBS15, PRBS32, stop idle | PASS-control | PARTIAL | ACK; sin instrumento ni cese físico. |
| EV-14 | después de EV-13 | 5 | IEEE baseline, 3 cambios BLE→IEEE, tests host | PASS-HW/SW | 3 con salida suficiente; 2 repeticiones narradas | Error RF real no inducido; firma negativa inconclusa. |
| OCT-IEEE | 06–12-oct | 15 | RAW/FRAME/BURST/CONTINUOUS/STOP | Mixto | PASS RAW/FRAME; PARTIAL/FAIL/INCONCLUSIVE repetición/stop | Matriz CSV y documentos 01/02/05. Incluye un comando inválido y una ventana sin TX. |
| OCT-PROP-0 | 08-oct | 24 | Propietaria 868/915/2440, 10 TX por corrida | 240 ACK, 0 peer | FAIL del criterio receptor; causa INCONCLUSIVE | No equivale a cero radiación; varios presets tienen taxonomía física no válida. |
| OCT-BASE | 12-oct | 4 | Dos RX IEEE y RAW en ambos sentidos | PASS | PASS acotado | Nuevo harness sincronizado; binario exacto aún no identificado. |
| REC-3A…4A | 15-oct | 6 | Recuperación `wisun_915_fsk_50k` | 60/60 | PASS-RF funcional | 3A, 3A-R, 3C, 3D, 3E, 4A; metadatos y switch físico pendientes. |
| SUBG-E02…E24 | 15–16-oct | 23 | Trece presets, controles e incidentes | 18 PASS, 5 BLOCKED | Igual | E01 es 3A y no se duplica; E09/E15/E18/E22/E23 bloqueados. |
| RF-01-A | 20-oct | 1 | IEEE 25→20→25 | BLOCKED | BLOCKED | 10/10 en control canal 25; timeout `RX_STOP`; etapas posteriores no ejecutadas. |
| RF-01-R | 20-oct | 1 | Control tras recuperación | BLOCKED | BLOCKED | 10/10; timeout reproducido tras preflight PASS. |
| RF-02 | 20-oct | 1 | Sub-GHz reverso, 902,2 MHz solicitados | PASS | PASS funcional | 10/10, stop/cleanup; `channel=25`, RSSI inconsistente, sin medida física. |
| RF-03 | 20-oct | 1 | IEEE→Sub-GHz→IEEE | BLOCKED | BLOCKED | 10/10 IEEE inicial; stop falla; `completed_stages=[]`; cambio no alcanzado. |
| RF-04 | 20-oct | 1 | KillerBee pasivo | BLOCKED | BLOCKED | Dependencia no resoluble; hardware/captura no iniciados. |
| RF-05 | 20-oct | 0 | OOK 868 opcional | No ejecutado | NOT RUN | Gates RF-01..03 y recuperación no satisfechos. |

## Detalle de la campaña IEEE 6–12-oct

| Caso | Observación | Estado evaluado |
|---|---|---|
| RAW COM33→COM88 | 20/20 marcadores | PASS |
| RAW COM88→COM33 | 10/10 | PASS |
| FRAME COM33→COM88 | 10/10 | PASS |
| BURST 40×25 ms | 1 match observado | FAIL del umbral; causa/conteo físico INCONCLUSIVE |
| BURST 5×250 ms | 1 match, repetición y host abierto sin mejora | FAIL del umbral; INCONCLUSIVE sobre emisiones |
| CONT 250 ms | 1 match | PARTIAL; no prueba repetición suficiente |
| CONT 0 | 30 hits en matriz; otra EV registra 99/102 bajo variante | PASS de actividad repetida; conteo físico no establecido |
| CONT/STOP dirigido | A observado; B posterior 0/3 y 0/10×2 | FAIL del criterio B / causa INCONCLUSIVE |
| RAW recovery | 9/10 | PARTIAL |
| Comando RAW con argumentos inexistentes | parser rechazó | INVALID; no es fallo RF |

## Detalle de recuperación 15-oct

| Checkpoint | Reset explícito | Estado lógico RP | Resultado | Interpretación |
|---|---|---|---|---|
| 3A | Sí | `Band: 1` | 10/10 | Recuperación funcional. |
| 3A-R | Sí | `Band: 1` | 10/10 | Repetibilidad inmediata una vez. |
| 3C | No | `Band: 1` | 10/10 | Reset no necesario en ese estado cálido. |
| 3D | No | `Band: 0` | 10/10 | Estado lógico band2 no necesario para enlace cercano observado. |
| 3E | No | `Band: 0`, helper legado | 10/10 | Procedimiento legado puede funcionar en estado actual. |
| 4A | Power-cycle; sin reset/band | `Band: 0` | 10/10 | Éxito después de arranque frío; estado histórico divergente sigue desconocido. |

## Correcciones y contradicciones materiales

1. `ACK` fue reinterpretado correctamente como evidencia de control, no de RF.
2. EV-11 pasó de “27 presets validados” a “27 registrados por control; 18 individuales y 9 resumidos; RF no validada”.
3. EV-12/BURST/CONTINUOUS pasó de criterios débiles de un hit a evaluación separada de repetición, stop y conteo.
4. F29 70/70, octubre 0/70 y recuperación 60/60 son incompatibles como resultado, pero no equivalentes en procedencia/estado; no confirman regresión.
5. La hipótesis de que `band2` lógico es necesario quedó contradicha por 3D/3E/4A bajo enlace cercano, sin resolver el estado eléctrico de U2.
6. RF-03 no demuestra fallo del switching porque nunca completó la primera detención.
7. El bloqueo KillerBee es de dependencia; RF-05 es ausencia de ejecución, no fallo funcional.

## Fuentes primarias

- [Auditoría EV](<../../05 - Evaluación/Auditoría técnica de validación FeralRF - EV ejecutadas.md>)
- [Matriz CSV del 12-oct](<../EXPERIMENT_REPORT_2026-10-12/validation_matrix_20261012_113056.csv>)
- [Auditoría causal Sub-GHz](<../Pruebas OTA SubGHz/Auditoría causal Codex - comparación histórica OTA SubGHz.md>)
- [RF-01](<../EXPERIMENT_REPORT_2026-10-20/RF-01 — IEEE 802.15.4 Channel Switching.md>)
- [RF-02](<../EXPERIMENT_REPORT_2026-10-20/RF-02 — Validación OTA Sub-GHz inversa.md>)
- [RF-03](<../EXPERIMENT_REPORT_2026-10-20/RF-03 — Validación de transición IEEE 802.15.4 ↔ Sub-GHz.md>)
- [RF-04](<../EXPERIMENT_REPORT_2026-10-20/RF-04 — Captura pasiva Zigbee mediante KillerBee.md>)

