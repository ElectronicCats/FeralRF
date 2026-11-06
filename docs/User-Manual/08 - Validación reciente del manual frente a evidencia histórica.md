# Validación reciente del manual frente a evidencia histórica

## Propósito y procedencia

Este addendum conserva la disposición de los nueve reportes aportados por el operador en `Reportes de pruebas realizadas/`. No reemplaza la matriz histórica ni convierte observaciones recientes en una nueva línea base universal. Fecha principal: 21-oct-2026; FeralRF fuente examinado: `feature/hardware-validation`, `fbff1b7f5077964418fb63a3fdf346a27c35a003`.

Los reportes permanecen sin modificación. No todos contienen transcript completo: `Investigación pendiente para Codex — Limitaciones de TX_BURST, TX_CONTINUOUS y RX_STOP.md` termina truncado en mitad de la pregunta 6.2; sus resultados anteriores al truncamiento sí son legibles, pero su investigación solicitada no está completa.

## Disposición feedback → receta

| Reporte | Observación | OP/área | Histórico | Actual/condiciones | Clasificación | Acción documental | Confianza |
|---|---|---|---|---|---|---|---|
| `Estandarización del directorio...` | Rutas mezclaban raíz y `FeralRF/python` | 05–07 | Ambos cwd aparecían | Operador trabajó desde `FeralRF/python` | Error documental/reproducibilidad | Normalizar cwd y rutas relativas | Sustentada |
| `Reporte complementario — Selección de banda...` | `band1→Band:0`, `band2→Band:1`; respuestas acumuladas no son bandas simultáneas | Prerrequisitos, OP-04 | Estado lógico sin correspondencia cerrada | RP2040 `v3.1.0.1`, COM35; OP-04 54 paquetes | Corrección documental + verificación lógica; física pendiente | Añadir `status`, separar ruta/frecuencia/switch | Sustentada para estado lógico |
| `Reporte complementario - OP-05...` | La excepción permite comprobar `error_code=0x05` | OP-05 | Rechazo descrito pero código no impreso | COM88, PHY4/ch25, -20 dBm; stop/stats correctos | Procedimiento incompleto | Validar código y propagar fallo de cleanup | Sustentada |
| `Observación experimental — OP-06...` | Corridas 9/10 y 10/10; negativo 0 hits | OP-06 | 20/20 y 10/10 en campaña anterior | COM33→COM88, ch25, 0 dBm, 60/90 s | Variación histórica; no regresión | Conservar ambas; aclarar `marker_hits`/PDR | Sustentada |
| `Observaciones experimentales — OP-08...` | BURST 5 programados, peer 1/5 | OP-08 | 1/5 | Mismos count/intervalo; marcador nuevo | Limitación reproducida | Mantener PARTIAL y distinguir scheduled/emisión | Sustentada |
| Mismo reporte, sección OP-09 | Sólo `--help` ejecutado en ese momento | OP-09 preparación | CLI ya usada históricamente | Parser reconoció argumentos; sin RF aún | Comprobación estática/ambiental | No presentarla como RF | Sustentada |
| `Investigación pendiente...` | OP-09A 1 hit; OP-09B 37 hits y timeout RX_STOP | OP-09/RX_STOP | Positivo lento ~1; rápido 30 o 99/102; timeouts previos | COM33/88, ch25; 250000 vs 0 µs | Discrepancia/configuración + limitación lifecycle | Añadir resultados y mantener causa abierta | Resultados sustentados; causa no resuelta |
| `OP-10_consulta_Cat-Shell...` | Consulta posterior mostró `Band:0`, contadores cero | OP-10 | ACK CW/PRBS/STOP, sin medición | Consulta posterior COM35; no simultánea | Evidencia temporalmente no comparable | No atribuir banda retrospectiva ni LoRa | Sustentada dentro de su instante |
| `RF-01 — Nueva evidencia...` | 25→20→25 entregó 10/10, 0/10, 10/10, 9/10; stop final timeout | OP-11/RF-01 | Bloqueo tras primera etapa 25 | Una ejecución completó recepción de 4 etapas | Nueva capacidad estrecha + bloqueo recurrente | Actualizar sin promover global a PASS | Sustentada |
| `Validación RF — Resultados...` | RF-03 7/10 y stop timeout antes de Sub-GHz; recuperaciones variables | OP-11/RF-03/recovery | 10/10 y bloqueo antes de transición | Primera etapa IEEE; no etapa Sub-GHz | Resultado diferente y no comparable integralmente | Preservar ambos; no afirmar fallo Sub-GHz | Sustentada |

## Comparación por receta

### OP-04 — RX IEEE

| Campo | Histórico EV-05 | Validación reciente |
|---|---|---|
| Bridge | COM88 | COM88 |
| PHY/canal | IEEE 4 / 25 | IEEE 4 / 25 |
| Ventana | 30 s | 30 s |
| Paquetes | 41, 43, 43 | 54 |
| CRC | Primeros reportados válidos | Primer paquete `crc_ok=True` |
| Cleanup | ACK | ACK |

Clasificación: **reproducido bajo condiciones parcialmente comparables**. No se conoce identidad binaria CC enlazada a un commit ni se atribuyó independientemente el tráfico.

### OP-05 — exclusión TX/RX

La ejecución reciente estrecha el resultado: `CommandError.error_code == 0x05`, `RX_STOP` correcto y stats válidos. Corrige el procedimiento, no el firmware. El contrato verificado es rechazo de TX mientras RX está activo para ese PHY/estado.

### OP-06 — RAW OTA

| Campo | Histórico | Reciente 1 | Reciente 2 |
|---|---:|---:|---:|
| TX solicitados/ACK | 10 o 20 | 10/10 | 10/10 |
| Ventana | no idéntica/procedencia parcial | 60 s | 90 s |
| Marker hits | 10/10 o 20/20 | 9/10 | 10/10 |
| Resultado umbral 10 | PASS | FAIL | PASS |
| RX_STOP | correcto | ACK | ACK |

Clasificación: segunda corrida reproduce entrega 10/10; la primera es **resultado diferente bajo condiciones no completamente controladas**. No se demuestra que 90 s causara la mejora. Como el marker se repitió, el conteo no es PDR por secuencia.

### OP-08 — BURST

| Indicador | Histórico | 21-oct |
|---|---:|---:|
| Count solicitado / scheduled | 5 | 5 |
| Intervalo | 250000 µs | 250000 µs |
| Hits | 1 | 1 |
| Criterio | 5 | 5 |
| Resultado OTA | PARTIAL/FAIL umbral | PARTIAL/FAIL umbral |

Clasificación: **reproducido bajo condiciones comparables en lo documentado**. Sólo queda demostrada una entrega; ni `scheduled=5` ni el ACK prueban cinco emisiones.

### OP-09 — CONTINUOUS

| Variante | Histórica | Reciente | Cleanup reciente |
|---|---|---|---|
| 250000 µs | 1 hit | 1 hit | TX_STOP ACK; RX_STOP ACK según reporte de variante |
| 0 µs | 30 o 99/102 según corrida | 37/37 CRC-good, separación media reportada 1118,7 µs | TX_STOP ACK; RX_STOP timeout con `last=0x90` |
| Post-stop | Ventanas sin marcador en casos documentados | Control posterior 0 hits | RX_STOP ACK en sesión nueva |

Clasificación: patrón de alta diferencia entre intervalo positivo/cero reproducido; conteos exactos no son directamente comparables. El timeout no prueba que RF siguiera activo. La corrida intermedia con un hit no tiene sincronización suficiente para atribuir transmisión después de STOP.

### OP-10 — modos de prueba

Los ACK de CW/PRBS15/PRBS32/STOP siguen siendo evidencia de control. `status` consultado después mostró `Band:0`, `uart_overrun=0` y `ring_dropped=0`, pero no registra la banda durante cada emisión ni todas las capas de pérdida. `Radio: LoRa` es etiqueta del Shell, no evidencia de modulación LoRa por FeralRF.

### OP-11 / RF-01

| Etapa | Histórico 20-oct | 21-oct |
|---|---|---|
| Control 25/25 | 10/10, stop timeout | 10/10, stop ACK |
| Mismatch 25/20 | No alcanzado | 0/10, stop ACK |
| Match 20/20 | No alcanzado | 10/10, stop ACK |
| Retorno 25/25 | No alcanzado | 9/10, stop timeout |
| Global | BLOCKED | BLOCKED |

Conclusión estrecha nueva: switching IEEE 25→20→25 observado en una ejecución. No se declara estabilidad ni PASS global porque faltó cleanup.

### OP-11 / RF-03 y recuperación

RF-03 histórico obtuvo 10/10 IEEE inicial y se bloqueó en stop; la corrida reciente obtuvo 7/10 y el mismo tipo de bloqueo. Ninguna alcanzó Sub-GHz: **no se ha repetido ni refutado switching IEEE↔Sub-GHz**.

Después de RF-01, power-cycle + preflight + control produjo 10/10 y PASS. Después de RF-03, preflight dio PASS, pero el control produjo 9/10, un ACK durante cleanup y resultado global BLOCKED por el timeout previo. Accesibilidad del dispositivo no equivale a recuperación funcional completa de RX.

## Semántica de aceptación usada al corregir los manuales

| Hito | Evidencia |
|---|---|
| Comando aceptado | ACK correlacionado |
| Operación programada | Estado interno/helper, p. ej. `scheduled=5` |
| Operación ejecutada | Requiere completion/status de driver, hoy incompleto para repetitivos |
| Paquete recibido | Marker del peer y CRC reportado |
| Experimento completado | Umbral + cleanup + exit/evento final |
| Propiedad física | Instrumentación independiente |

## Cuestiones que permanecen abiertas

- Causa del timeout intermitente de `RX_STOP`; `0x90` son eventos RX inesperados durante la espera, no una atribución causal completa.
- Por qué BURST y CONTINUOUS con intervalo positivo producen una entrega en las corridas observadas.
- Correspondencia real entre estado lógico `Band`, GPIO y posición del RF switch.
- Identidad exacta del binario CC1352P7 instalado y comparabilidad con HEAD.
- Potencia, frecuencia, modulación y temporización físicas.
- Validación del documento 07 reconstruido; ningún reporte reciente la realizó.

## Fuentes recientes

Los nueve archivos bajo `Reportes de pruebas realizadas/` constituyen la fuente primaria de este addendum. Para antecedentes: [[02 - Matriz histórica de experimentos y ejecuciones]], [[03 - Capacidades demostradas y catálogo de presets]], [[06 - Procedimientos IEEE, TX y modos de prueba]] y [[07 - Procedimientos Sub-GHz y experimentos condicionados]].
