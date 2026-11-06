# Consolidación y evaluación experimental de FeralRF — índice

## Propósito

Esta carpeta constituye la línea base de **Fase I**: reconstruye qué se ejecutó, qué se observó y qué puede afirmarse experimentalmente sobre FeralRF. No es una auditoría exhaustiva de implementación, una investigación de causa raíz ni un plan de desarrollo.

La lectura recomendada es:

1. [[01 - Informe consolidado de evaluación experimental]] — síntesis defendible y contexto de hardware.
2. [[02 - Matriz histórica de experimentos y ejecuciones]] — inventario, estados y trazabilidad.
3. [[03 - Capacidades demostradas y catálogo de presets]] — alcance actual por capacidad.
4. [[04 - Evidencia, procedencia y preguntas abiertas]] — fuentes, contradicciones y entrega a Fase II.
5. [[05 - Manual práctico de uso de FeralRF]] — entrada operativa, preparación, puertos y recuperación.
6. [[06 - Procedimientos IEEE, TX y modos de prueba]] — recetas IEEE/control con terminales, orden, salida y cleanup.
7. [[07 - Procedimientos Sub-GHz y experimentos condicionados]] — presets, recuperación OTA, RF-02, KillerBee y OOK gated.
8. [[08 - Validación reciente del manual frente a evidencia histórica]] — disposición de los nueve reportes del operador y comparación 21-oct frente a campañas previas.

## Instantáneas locales revisadas

| Repositorio | Rama | HEAD | Estado inicial relevante |
|---|---|---|---|
| `FeralRF/` | `feature/hardware-validation` | `fbff1b7f5077964418fb63a3fdf346a27c35a003` | Modificado: `python/examples/killerbee_sniff.py`. Sin seguimiento: dos runbooks, scripts `next_rf_validation`, `quick_fsk_tx` y nueve JSONL de la campaña del 20-oct-2026. |
| `Understanding-CatSniffer/` | `main` | `c07aeeba4baff7c1c022d49c2449af82944533bb` | Limpio antes de crear esta carpeta. |

El SHA de FeralRF describe sólo los archivos versionados en HEAD. **No identifica** la versión de los scripts sin seguimiento ni la modificación local de `killerbee_sniff.py` usada o disponible durante la campaña reciente. El firmware CC1352P7 reportó `1.0.0`; este valor tampoco enlaza el binario instalado con un commit. El RP2040 de la campaña reciente sí reportó `v3.1.0.1`, Git `c0cd5a4 (clean)`, compilación `2026-09-04T00:40:22Z`, pero no se conservó hash del UF2 instalado.

Git se consultó con una excepción `safe.directory` por comando; no se modificó la configuración global ni el estado de los repositorios.

## Alcance documental

Se inventariaron los contenidos anidados de `05 - Evaluación/` y `06 - Experimentos/`: **49 Markdown y 1 CSV**, es decir, **50 artefactos históricos primarios**. Se detectaron seis pares de copias byte a byte dentro del reporte del 12-oct-2026; esas copias no se contaron como evidencia independiente.

También se inspeccionaron como apoyo:

- nueve JSONL locales del 20-oct-2026;
- reportes, matrices, runbooks y artefactos del 12 y 16-oct-2026 en `FeralRF/docs/`;
- Git de ambos repositorios;
- `docs/PYTHON_API.md`, `docs/ARCHITECTURE.md`, `docs/VALIDATION_MATRIX.md`;
- `python/feralrf/`, `python/examples/`, `python/examples/lab/` y los scripts sin seguimiento de validación reciente;
- firmware RP2040 local, overlay de pines y notas de hardware del Vault.

## Método de evaluación

Jerarquía práctica: registro crudo y salida literal > script y argumentos ejecutados > informe específico con medidas > auditoría trazable > matriz general > README o declaración de capacidad.

Estados usados:

- `PASS`: el criterio aplicable fue satisfecho en una ejecución completada.
- `PARTIAL`: se completó, pero faltó al menos un requisito material.
- `FAIL`: la ejecución terminó sin satisfacer el criterio aplicable.
- `BLOCKED`: infraestructura, preparación, cleanup o control impidió terminar el objetivo.
- `NOT RUN`: no hubo ejecución.
- `INCONCLUSIVE`: la evidencia no permite resolver la cuestión.

Un estado puede ser suficiente para una observación estrecha y, simultáneamente, insuficiente para la capacidad amplia. Por ejemplo, diez marcadores recibidos sustentan entrega entre dos CatSniffer; no miden frecuencia, potencia, modulación física ni interoperabilidad con terceros.

## Regla de conteo

La matriz identifica **193 unidades de ejecución** en **ocho familias/campañas**. Una unidad es una corrida, fila de preset, proceso repetido o prueba host expresamente documentada. Las filas agregadas declaran `n`; no se suman de nuevo sus copias o resúmenes. No se cuentan como ejecución los pasos sólo planificados, RF-05 no ejecutado ni EV-01 como ensayo dedicado inexistente.

- **125 unidades** tienen evidencia suficiente para sostener al menos su observación o bloqueo estrecho.
- **68 unidades** dependen de resumen, mensaje de commit, stdout faltante o procedencia incompleta.

“Suficiente” no significa validación integral: un ACK literal puede ser evidencia suficiente de control y seguir siendo insuficiente como prueba RF. Los conteos y su desglose están en [[02 - Matriz histórica de experimentos y ejecuciones#Resumen contable]].

## Exclusiones

- No se ejecutaron pruebas RF, scripts seriales ni comandos contra hardware.
- No se instalaron dependencias, no se construyó ni se flasheó firmware.
- No se modificaron firmware, Python, scripts, logs ni documentos históricos.
- No se hizo una auditoría causal completa de `RX_STOP`, RSSI, canal propietario o backend de presets; son preguntas de Fase II.
- No se validaron afirmaciones de pila Zigbee, Thread, Wi-SUN, Sidewalk o Wireless M-Bus por el nombre de un preset.

## Revisión operativa del 21-oct-2026

Los reportes aportados posteriormente por el operador se integran sin alterar la línea base histórica. Corrigen rutas/criterios de los manuales, reproducen varios resultados IEEE y añaden evidencia de switching de canal 25→20→25, pero conservan `RX_STOP` como limitación recurrente. El documento 07 reconstruye procedimientos Sub-GHz a partir de historia y código actual; todavía no fue validado por el operador. La trazabilidad detallada está en [[08 - Validación reciente del manual frente a evidencia histórica]].

## Cobertura resumida

La evidencia más fuerte es: inicialización/control repetido; recepción IEEE 802.15.4; TX RAW/FRAME IEEE con marcador de peer; entrega propietaria Sub-GHz en campañas de recuperación y 13 presets; recuperación de sesiones y controles post-incidente. Las restricciones principales son: timeout reproducible de `RX_STOP` en la campaña del 20-oct; conteos BURST/CONTINUOUS no consistentes en varias corridas; contradicción histórica 70/70, 0/70 y recuperación posterior; metadatos propietarios anómalos; falta de medidas independientes de frecuencia, potencia, switch y cese; KillerBee bloqueado por dependencia; OOK reciente no ejecutado.

