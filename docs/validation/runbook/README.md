# FeralRF Validation Runbook

Este es el punto de entrada `CANONICAL` para ejecutar validaciones de FeralRF con el checkout actual. Su objetivo es que el operador pueda seleccionar una operación, conocer sus dependencias, aplicar cleanup y conservar evidencia sin reconstruir la campaña histórica.

La autoridad se divide deliberadamente:

- [`CURRENT_VALIDATION_MATRIX.md`](../../CURRENT_VALIDATION_MATRIX.md) responde **qué estado de capacidad es defendible**.
- este Runbook responde **cómo ejecutar una validación** y si hoy existe una entrada reproducible;
- [`User-Manual/02`](<../../User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md>) inventaría **qué ocurrió en ejecuciones anteriores**;
- [`HARDWARE_VALIDATION_RESULTS/`](../../HARDWARE_VALIDATION_RESULTS/) y los artefactos raw conservan **la evidencia registrada**.

Un estado de capacidad positivo no vuelve ejecutable una receta cuyo script falta. A la inversa, que un comando se ejecute no eleva por sí solo la evidencia física o protocolaria.

## Estado de los procedimientos

| Estado | Significado |
|---|---|
| `CURRENT_VALIDATED` | La entrada, argumentos, dependencias y cleanup existen en el checkout, y la secuencia fue ejecutada con evidencia suficiente bajo las condiciones documentadas. |
| `CURRENT_PARTIAL` | La entrada existe y fue ejercitada, pero el objetivo, cleanup, identificación o evidencia sólo cubre una parte del procedimiento. |
| `RECONSTRUCTED_UNVALIDATED` | La secuencia es compatible por inspección estática con API/scripts actuales, pero el comando compuesto no fue reejecutado y no debe presentarse como receta validada. |
| `BLOCKED` | Falta una dependencia, script, estado seguro o ruta de cleanup necesaria; no se alcanzó el criterio objetivo. No significa que la capacidad haya fallado. |
| `HISTORICAL_ONLY` | El material se conserva por trazabilidad, pero no es una entrada operativa del checkout actual. |

El resultado de **una ejecución** se registra aparte:

- `PASS`: se alcanzó el criterio específico y el cleanup terminó aceptablemente.
- `PARTIAL`: se alcanzó sólo una parte o la evidencia quedó incompleta.
- `FAIL`: se ejercitó el criterio objetivo y no se cumplió.
- `BLOCKED`: preparación, dependencia, estado o cleanup impidió alcanzar el criterio.

No use `FAIL` cuando el objetivo no llegó a ejercitarse.

## Reglas de seguridad y evidencia

1. Use antena/carga apropiada, autorización y regulación aplicable antes de cualquier TX.
2. Registre los puertos reales: nunca derive Cat-Shell mediante aritmética sobre Cat-Bridge.
3. No infiera revisión, MCU, switch, throw, frecuencia, potencia o modulación física desde software. La autoridad física es [`hardware/`](../../../hardware/README.md).
4. Un ACK demuestra control/aceptación; no demuestra emisión ni recepción. Un marcador peer demuestra entrega host-visible; tampoco mide propiedades RF.
5. Empiece con un estado limpio y no continúe tras cleanup ambiguo. Conserve la salida antes de recuperar.
6. OOK se ejecuta sólo como experimento aislado: consulte la advertencia de [recovery OOK](common.md#ook-estado-aislado-y-peligroso) y [OP-16](subghz.md#op-16).
7. Sustituya siempre los COM de ejemplo y use markers nuevos, preferiblemente con ID de secuencia.

## Selección rápida

| Necesidad | Procedimiento | Estado |
|---|---|---|
| Identificar interfaces | [OP-01](common.md#op-01) | `CURRENT_PARTIAL` |
| Abrir/cerrar y reconectar | [OP-02](common.md#op-02) | `CURRENT_VALIDATED` |
| Recovery manual por Shell | [OP-03](common.md#op-03) | `CURRENT_PARTIAL` |
| RX IEEE canal 25 | [OP-04](ieee.md#op-04) | `CURRENT_VALIDATED` |
| Exclusión TX durante RX | [OP-05](ieee.md#op-05) | `CURRENT_VALIDATED` |
| OTA IEEE `TX_RAW` | [OP-06](ieee.md#op-06) | `CURRENT_VALIDATED` |
| OTA IEEE `TX_FRAME` | [OP-07](ieee.md#op-07) | `RECONSTRUCTED_UNVALIDATED` |
| `TX_BURST` | [OP-08](ieee.md#op-08) | `CURRENT_PARTIAL` |
| `TX_CONTINUOUS` / `TX_STOP` | [OP-09](ieee.md#op-09) | `CURRENT_PARTIAL` |
| CW / PRBS / test stop | [OP-10](ieee.md#op-10) | `RECONSTRUCTED_UNVALIDATED` |
| Switching IEEE / cross-PHY | [OP-11](ieee.md#op-11) | `BLOCKED` |
| Control `gfsk_868_50k` | [OP-12](subghz.md#op-12) | `CURRENT_VALIDATED` |
| OTA Sub-GHz reusable | [OP-13](subghz.md#op-13) | `RECONSTRUCTED_UNVALIDATED` |
| Helper coordinado archivado | [OP-13C](subghz.md#op-13c) | `HISTORICAL_ONLY` |
| Sub-GHz inverso B→A | [OP-14](subghz.md#op-14) | `BLOCKED` |
| KillerBee HIL/PCAP | [OP-15](ieee.md#op-15) | `BLOCKED` |
| OOK 868 aislado | [OP-16](subghz.md#op-16) | `BLOCKED` |

El setup, las convenciones de markers, el cleanup y el recovery comunes están en [`common.md`](common.md). Las operaciones RF se dividen entre [`ieee.md`](ieee.md) y [`subghz.md`](subghz.md).

## Registro canónico de ejecución

Copie este bloque por ejecución. Use `UNKNOWN` cuando no conozca una identidad; no la infiera.

```text
Test ID:
Date/time:
Operator:
Repository branch:
Source commit:
Git dirty state:
CC1352 binary identity:
RP2040 binary identity:
DUT identifier:
Peer identifier:
Known PCB revision:
Bridge port:
Shell port:
Command:
Working directory:
Procedure version:
Parameters:
Expected result:
Observed result:
Cleanup result:
Artifacts:
PASS / PARTIAL / FAIL / BLOCKED:
What this proves:
What this does not prove:
Notes:
```

`Procedure version` debe identificar al menos el commit y el path/anchor del procedimiento. Preserve stdout/stderr sin editar, exit code, timestamps, markers, conteos, archivos generados y el estado posterior.

## Mantenimiento

- Actualice un procedimiento sólo con un cambio verificable de entrada, argumentos, dependencia, precondición, criterio o cleanup.
- Una reejecución nueva no borra evidencia contradictoria: clasifíquela como reproducción, contradicción, supersesión bajo condiciones cambiadas o evidencia de una dimensión nueva.
- Si cambia implementación sin reejecución, actualice compatibilidad estática pero no promueva el estado del procedimiento ni la matriz experimental.
- Los documentos `User-Manual/` son drafts operativos y síntesis históricas preservadas; este Runbook es la ruta operacional canónica.
- Cualquier cambio de estado de capacidad se realiza en la [matriz canónica](../../CURRENT_VALIDATION_MATRIX.md), no aquí.
