# FeralRF - Estado de Sesion

## Ultima actualizacion: 2026-02-17

## Resumen ejecutivo

1. Base host<->firmware estable a `921600` con `COBS + CRC16`.
2. Comandos de fase 2 operativos en hardware (`RADIO_INIT`, `GET_INFO`, `SET_PHY`, `SET_CHANNEL`, `SET_POWER`, `RX_START`, `RX_STOP`).
3. RX BLE real funcionando en CC1352 (sin backend sintetico), con parser robusto y filtro CRC en firmware.
4. Hopping BLE advertising basico activo (37/38/39) con dwell configurable.
5. Metricas base expuestas al host via `GET_STATS`:
   1. `rx_ok`
   2. `rx_crc_err`
   3. `rx_drop`
   4. `rx_overflow`

## Estado por fase

1. Fase 1: `COMPLETA`.
2. Fase 2: `COMPLETA`.
3. Fase 3: `COMPLETA (alcance MVP)`.
4. Fase 4: `COMPLETA (alcance MVP)`.
5. Fase 5: `PARCIAL`.
6. Fase 6: `PARCIAL` (captura funcional, falta cierre de estabilidad).
7. Fase 7: `EN PROGRESO` (metricas base implementadas).

## Validaciones recientes (hardware)

1. Smoke fase 2 en verde.
2. Captura BLE con paquetes reales (no patron fijo sintetico).
3. Prueba corta de metricas:
   1. `stats_before: ok=0 crc_err=0 drop=0 ovf=0`
   2. `packets: 2`
   3. `stats_after: ok=2 crc_err=0 drop=0 ovf=0`

## Bloqueo / riesgo abierto actual

1. En soak de 120s hay timeout intermitente al cierre (`RX_STOP`) y/o en `GET_STATS` durante RX continuo.
2. Se aplicaron mitigaciones host:
   1. timeout global robusto en lectura de respuestas.
   2. reintentos en `stop_rx()`.
   3. script soak tolerante a timeout puntual de stats.
3. Aun pendiente: endurecer lado firmware para ACK consistente de `RX_STOP` bajo carga.

## Scripts y comandos vigentes

1. Soak test:
   `PYTHONPATH=python python3 python/examples/soak_ble_30min.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --duration 120 --report-every 30 --stats-timeout 3 --stats-retries 5`
2. Smoke fase 2:
   `python examples/smoke_phase2.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --power 0`

## Archivos clave tocados en este bloque

1. Firmware:
   1. `firmware/cc1352/src/radio_if.c`
   2. `firmware/cc1352/src/command_processor.c`
   3. `firmware/cc1352/src/control_task.c`
   4. `firmware/cc1352/include/radio_if.h`
   5. `firmware/cc1352/include/config.h`
2. Python:
   1. `python/feralrf/radio.py`
   2. `python/feralrf/enums.py`
   3. `python/examples/soak_ble_30min.py`
3. Planes:
   1. `docs/PLAN_FASES_DESDE_REPORTE.md`
   2. `docs/PLAN_MAESTRO.md`

## Siguiente paso inmediato recomendado

1. Cerrar timeout de `RX_STOP` en firmware (estado/flush/restart) y repetir soak 120s.
2. Si 120s queda limpio, ejecutar soak 30min para cierre formal de Fase 6.
