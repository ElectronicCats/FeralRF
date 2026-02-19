# FeralRF - Estado de Sesion

## Ultima actualizacion: 2026-02-19

## Fuente de plan activa

1. Plan activo detallado: `docs/NUEVO_PLAN_MAESTRO.md`.
2. Plan consolidado/historico: `docs/PLAN_MAESTRO.md`.

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
12. Backend RF real `IEEE_802_15_4` (RX) integrado en `radio_if` con settings SmartRF dedicados y fallback sintetico para PHY no soportados.
13. Validacion HW PHY 4: `OK` con captura real (`packets>0`, `delta_ok>0`) y barrido `11..26` en hardware.
14. `TX_RAW` fase 1 en PHY4: `OK` en smoke de control (`TX_RAW ACK`) con ejecucion diferida no bloqueante.
15. `TX_RAW` fase 1 en BLE (PHY0 ADV): `OK` en smoke de control (`TX_RAW ACK`, `TX BLE SMOKE PASS`).
16. `MULTI-PHY RELEASE GATE`: `OK` en hardware (BLE baseline + PHY4 RX + PHY4 TX + BLE TX en una sola corrida).
17. Validacion over-the-air cerrada para TX:
   1. PHY4: `OK` con `python/examples/ota_rx_probe.py` (marcador `a1b2c3d4`, `marker_hits=80`).
   2. BLE: `OK` con `python/examples/ota_rx_probe.py` (marcador `beef01`, `marker_hits=24`, `crc_ok=2110/2110`).
18. `CMD_TX_BURST` fase 1: `OK` en hardware (`TX_BURST ACK`, `TX BURST SMOKE PASS`) e integrado al gate multi-PHY (`PHY4 TX burst smoke PASS`).
19. `CMD_TX_CONTINUOUS` + `CMD_TX_STOP` fase 1: `OK` en hardware (`TX_CONTINUOUS ACK`, `TX_STOP ACK`, `TX CONTINUOUS SMOKE PASS`) e integrado al gate multi-PHY (`PHY4 TX continuous smoke PASS`).
20. `CMD_TX_FRAME` fase 1: `OK` en hardware (`TX_FRAME ACK`, `TX FRAME SMOKE PASS`) para PHY4 y BLE, e integrado al gate multi-PHY (`PHY4 TX frame smoke PASS`, `BLE TX frame smoke PASS`).
21. Evidencia OTA dedicada de `TX_FRAME`: `OK` en hardware con `ota_tx_frame.py` + `ota_rx_probe.py`:
   1. PHY4 CH25 marcador `a1b2c3d4`: `marker_hits=40`, `crc_ok=86/86`.
   2. BLE CH37 marcador `beef01`: `marker_hits=14`, `crc_ok=1427/1427`.
22. Jamming fase 1 implementado y validado:
   1. `CMD_JAM_CONTINUOUS` + `CMD_JAM_STOP` en PHY4 y BLE.
   2. Potencia TX aplicada con tabla real 2.4 GHz (incluye `20 dBm`).
   3. Ajustes de robustez para `JAM_STOP` en firmware/host bajo carga TX.
23. Modo seguro de laboratorio para JAM activo en firmware:
   1. Duracion maxima (`30 s`).
   2. Cooldown obligatorio entre JAMs (`2 s`) usando tiempo wall-clock (AON RTC).
   3. Canal explicito valido por PHY (BLE `37..39`, IEEE 802.15.4 `11..26`).
   4. Rechazo por estado invalido (`ERR_INVALID_STATE`) cuando aplica.

## Estado por fase

1. Fase 1: `COMPLETA`.
2. Fase 2: `COMPLETA`.
3. Fase 3: `COMPLETA (alcance MVP)`.
4. Fase 4: `COMPLETA (alcance MVP)`.
5. Fase 5: `COMPLETA`.
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
7. Barrido PHY 4 IEEE 802.15.4 (CH 11..26, 8s c/u): `OK` con deteccion real en CH 11, 12 y 25; caso objetivo CH25 validado con `packets=34`, `delta_ok=34`, `drop=0`, `ovf=0`.
8. Canary regresion (BLE, 60s): `OK` en hardware (`CANARY PASS`) con monotonia de stats, `RX_STOP ACK` y `packets_total=8150`.
9. Gate BLE unificado (`release_gate_ble.py`, 60s, `ci_manual`) corrida 1/2: `OK` (smoke + canary en verde, `packets_total=5304`, `stats_total ok=5390 crc_err=919 drop=37 ovf=0`).
10. Gate BLE unificado (`release_gate_ble.py`, 60s, `ci_manual`) corrida 2/2: `OK` (smoke + canary en verde, `packets_total=5623`, `stats_total ok=5733 crc_err=778 drop=37 ovf=0`).
11. TX smoke fase 1 (`smoke_tx_phase1.py`, PHY4 CH25): `OK` (`TX_RAW ACK`, `TX SMOKE PASS`).
12. TX smoke BLE fase 1 (`smoke_tx_ble_phase1.py`, PHY0 CH37): `OK` (`TX_RAW ACK`, `TX BLE SMOKE PASS`).
13. Gate multi-PHY creado (`release_gate_multi_phy.py`): orquesta BLE gate + PHY4 RX/TX smoke + BLE TX smoke en un comando.
14. Gate multi-PHY ejecutado y aprobado (`MULTI-PHY RELEASE GATE PASS`, 2026-02-18):
   1. BLE baseline gate: `PASS` (smoke + canary 60s, `packets_total=2862`, `stats_total ok=2921 crc_err=198 drop=7 ovf=0`).
   2. PHY4 RX smoke CH25: `PASS` (`packets=45`, `RX_STOP ACK`).
   3. PHY4 TX smoke CH25: `PASS` (`TX_RAW ACK`).
   4. BLE TX smoke CH37: `PASS` (`TX_RAW ACK`).
15. Gate multi-PHY actualizado con TX burst (`MULTI-PHY RELEASE GATE PASS`, 2026-02-18):
   1. `PHY4 TX burst smoke`: `PASS` (`TX_BURST ACK`, `count=5`, `interval_us=5000`).
   2. Corrida completa en verde con baseline BLE + PHY4 RX + PHY4 TX + PHY4 TX burst + BLE TX.
16. Gate multi-PHY actualizado con TX continuous (`MULTI-PHY RELEASE GATE PASS`, 2026-02-18):
   1. `PHY4 TX continuous smoke`: `PASS` (`TX_CONTINUOUS ACK`, `TX_STOP ACK`, `run_seconds=1.0`, `interval_us=5000`).
   2. Corrida completa en verde con baseline BLE + PHY4 RX + PHY4 TX + PHY4 TX burst + PHY4 TX continuous + BLE TX.
17. Gate multi-PHY actualizado con TX frame (`MULTI-PHY RELEASE GATE PASS`, 2026-02-18):
   1. `PHY4 TX frame smoke`: `PASS` (`TX_FRAME ACK`).
   2. `BLE TX frame smoke`: `PASS` (`TX_FRAME ACK`).
   3. Corrida completa en verde con baseline BLE + PHY4 RX + PHY4 TX + PHY4 TX frame + PHY4 TX burst + PHY4 TX continuous + BLE TX frame + BLE TX raw.
18. OTA dedicada de `TX_FRAME` validada (2026-02-18):
   1. PHY4 CH25: `packets_total=86`, `crc_ok=86`, `marker_hits=40`, `RX_STOP ACK`.
   2. BLE CH37: `packets_total=1427`, `crc_ok=1427`, `marker_hits=14`, `RX_STOP ACK`.
19. Validacion HW de modo seguro JAM (2026-02-19): `OK`.
   1. `invalid_channel` rechazado (`error_code=5`).
   2. `cooldown_immediate` rechazado (`error_code=5`).
   3. `cooldown_after_wait` aceptado (inicio JAM exitoso tras espera).
20. Validacion A/B JAM 5 dBm vs 20 dBm (2026-02-19): `OK`.
   1. PHY4 CH25: ambos niveles con `JAM_CONTINUOUS ACK` + `JAM_STOP ACK`.
   2. BLE CH37: ambos niveles con `JAM_CONTINUOUS ACK` + `JAM_STOP ACK`.
21. Verificacion post-JAM de estabilidad: `smoke_phase2` en verde (sin lockup).

## Riesgo abierto actual

1. No hay bloqueo critico abierto para MVP BLE.
2. Riesgo residual en endurecimiento:
   1. mantener vigilancia sobre timeout intermitente de `RX_STOP/GET_STATS` bajo carga extrema.
3. Mitigaciones host ya aplicadas:
   1. timeout global robusto en lectura de respuestas.
   2. reintentos en `stop_rx()`.
   3. script soak tolerante a timeout puntual de stats.
4. Pendiente de calidad:
   1. canario automatizado implementado y validado en corrida base HW.
   2. umbral por entorno implementado con perfiles (`lab/ci_manual/quiet`).
   3. gate operativo formalizado en comando unico (`python/examples/release_gate_ble.py`).
   4. `OK`: corridas 1/2 y 2/2 consecutivas del gate BLE en verde.
   5. `OK`: baseline BLE congelado para evitar regresiones antes de abrir nuevos verticales.
   6. `OK`: gate conectado a flujo manual/CI:
      1. workflow manual HW `ble_release_gate_hw.yml`.
      2. validacion CI de scripts gate en `build.yml`.

## Scripts y comandos vigentes

1. Soak test:
   `PYTHONPATH=python python3 python/examples/soak_ble_30min.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --duration 1800 --report-every 30 --stats-timeout 3 --stats-retries 5`
2. Smoke fase 2:
   `python examples/smoke_phase2.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --power 0`
3. Canary regresion:
   `PYTHONPATH=python python3 python/examples/canary_regression.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --power 0 --soak-duration 60 --report-every 15 --stats-timeout 2 --stats-retries 3 --profile ci_manual`
4. Gate release BLE (comando unico):
   `PYTHONPATH=python python3 python/examples/release_gate_ble.py -p /dev/ttyACM0 --baudrate 921600 --phy 0 --channel 37 --power 0 --soak-duration 60 --report-every 15 --stats-timeout 2 --stats-retries 3 --profile ci_manual`
5. Sweep IEEE 802.15.4:
   `PYTHONPATH=python python3 python/examples/sweep_phy4_ieee154.py -p /dev/ttyACM0 -b 921600 --ch-min 11 --ch-max 26 --duration 8 --retries 2`
6. TX smoke fase 1:
   `PYTHONPATH=python python3 python/examples/smoke_tx_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 0 --packet-hex 01020304 --tx-timeout 10`
7. TX smoke BLE fase 1:
   `PYTHONPATH=python python3 python/examples/smoke_tx_ble_phase1.py -p /dev/ttyACM0 -b 921600 --channel 37 --power 0 --payload-hex 020106 --tx-timeout 10`
8. TX burst smoke fase 1:
   `PYTHONPATH=python python3 python/examples/smoke_tx_burst_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 0 --packet-hex 01020304 --count 5 --interval-us 5000 --tx-timeout 10`
9. TX continuous smoke fase 1:
   `PYTHONPATH=python python3 python/examples/smoke_tx_continuous_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 0 --packet-hex 01020304 --interval-us 5000 --run-seconds 1.0 --tx-timeout 10`
10. TX frame smoke fase 1:
   `PYTHONPATH=python python3 python/examples/smoke_tx_frame_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 0 --frame-hex 01020304 --tx-timeout 10`
11. Gate multi-PHY (comando único):
   `PYTHONPATH=python python3 python/examples/release_gate_multi_phy.py -p /dev/ttyACM0 -b 921600 --ble-soak-duration 60 --ble-report-every 15 --ble-profile ci_manual --phy4-rx-channel 25 --phy4-rx-duration 10 --phy4-tx-channel 25 --phy4-tx-packet-hex 01020304 --phy4-tx-burst-count 5 --phy4-tx-burst-interval-us 5000 --phy4-tx-cont-interval-us 5000 --phy4-tx-cont-run-seconds 1.0 --ble-tx-channel 37 --ble-tx-payload-hex 020106`
12. OTA TX frame helper (transmisor):
   `PYTHONPATH=python python3 python/examples/ota_tx_frame.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 0 --payload-hex a1b2c3d4 --count 40 --interval-us 25000`
13. OTA RX probe helper (receptor):
   `PYTHONPATH=python python3 python/examples/ota_rx_probe.py -p /dev/ttyACM1 -b 921600 --phy 4 --channel 25 --duration 12 --marker-hex a1b2c3d4 --min-hits 1`
14. JAM smoke fase 1:
   `PYTHONPATH=python python3 python/examples/smoke_jam_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 20 --duration-ms 3000`
15. JAM A/B (PHY4/BLE, 5 dBm vs 20 dBm):
   1. `PYTHONPATH=python .venv/bin/python python/examples/smoke_jam_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 5 --duration-ms 1500`
   2. `PYTHONPATH=python .venv/bin/python python/examples/smoke_jam_phase1.py -p /dev/ttyACM0 -b 921600 --phy 4 --channel 25 --power 20 --duration-ms 1500`
   3. `PYTHONPATH=python .venv/bin/python python/examples/smoke_jam_phase1.py -p /dev/ttyACM0 -b 921600 --phy 0 --channel 37 --power 5 --duration-ms 1500`
   4. `PYTHONPATH=python .venv/bin/python python/examples/smoke_jam_phase1.py -p /dev/ttyACM0 -b 921600 --phy 0 --channel 37 --power 20 --duration-ms 1500`

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
   3. `docs/NUEVO_PLAN_MAESTRO.md`
4. Canary:
   1. `python/examples/canary_regression.py`
5. Protocolo:
   1. `docs/protocol.md`

## Siguiente paso inmediato recomendado

1. Mantener `python/examples/release_gate_multi_phy.py` como no-regresión obligatoria antes y después de cambios RF/TX.
2. Endurecer recuperación post-switch de PHY/TX (timeouts/reintentos y limpieza de estado RF) para reducir bloqueos intermitentes.
3. Consolidar contrato final de `TX_FRAME` por PHY en documentación/protocolo y mantener validación OTA como criterio de release.
4. Ejecutar el workflow manual HW (`ble_release_gate_hw.yml`) al menos una vez en GitHub Actions para validar runner/artefactos.
