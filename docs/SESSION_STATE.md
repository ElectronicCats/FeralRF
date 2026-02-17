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
6. Soak BLE de 30 minutos validado en hardware (`OK`).
7. Base de Fase 5 integrada en firmware (`phy_manager` tabular + `LL_DEFAULT/LL_BLE` pluggable).
8. `LL_BLE` ya aplica validacion minima de PDU (header+length), con recorte o descarte de paquetes inconsistentes.
9. `RSP_RX_PACKET` ya incluye metadata LL opcional (`ll_pdu_kind`, `ll_pdu_type`) con capability flag en `GET_INFO`.
10. `GET_STATS` extendido (compat): incluye contadores LL por tipo (`unknown/adv/scan/connect/data`) cuando capability lo anuncia.
11. Parser `LL_BLE` ampliado: subtipos advertising (`ADV_IND`, `ADV_SCAN_IND`, `ADV_EXT_IND`, etc.) y bandera de casos reservados.

## Estado por fase

1. Fase 1: `COMPLETA`.
2. Fase 2: `COMPLETA`.
3. Fase 3: `COMPLETA (alcance MVP)`.
4. Fase 4: `COMPLETA (alcance MVP)`.
5. Fase 5: `PARCIAL`.
6. Fase 6: `COMPLETA (MVP BLE validado en hardware)`.
7. Fase 7: `EN PROGRESO` (metricas base implementadas).

## Validaciones recientes (hardware)

1. Smoke fase 2 en verde.
2. Captura BLE con paquetes reales (no patron fijo sintetico).
3. Prueba corta de metricas:
   1. `stats_before: ok=0 crc_err=0 drop=0 ovf=0`
   2. `packets: 2`
   3. `stats_after: ok=2 crc_err=0 drop=0 ovf=0`
4. Soak BLE 30 minutos: `OK` (sin caida de sesion).
5. Soak BLE 60s con stats LL live: `OK`:
   1. `capabilities=0x07`
   2. `packets_total=9476`
   3. `stats_total ok=9669 crc_err=1291 drop=64 ovf=0`
   4. `ll_total unk=0 adv=7601 scan=2068 conn=0 data=0`
6. Validacion de parser LL ampliado: `OK` (captura real muestra subtipos `ADV_IND`, `ADV_NONCONN_IND`, `ADV_EXT_IND`, `SCAN_REQ`, `SCAN_RSP` en canales 37/38/39).

## Riesgo abierto actual

1. No hay bloqueo critico abierto para MVP BLE.
2. Riesgo residual en endurecimiento:
   1. mantener vigilancia sobre timeout intermitente de `RX_STOP/GET_STATS` bajo carga extrema.
3. Mitigaciones host ya aplicadas:
   1. timeout global robusto en lectura de respuestas.
   2. reintentos en `stop_rx()`.
   3. script soak tolerante a timeout puntual de stats.
4. Pendiente de calidad:
   1. automatizar soak canario y criterio de cierre limpio.

## Scripts y comandos vigentes

1. Soak test:
   `PYTHONPATH=python python3 python/examples/soak_ble_30min.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --duration 1800 --report-every 30 --stats-timeout 3 --stats-retries 5`
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

1. Cerrar Fase 5 (parser LL BLE mas completo `OK` + primer backend RF real no-BLE pendiente).
2. Cerrar Fase 7 (regresion automatizada con criterios de aceptacion).
3. Publicar `docs/protocol.md` con contrato vigente y capacidades.
