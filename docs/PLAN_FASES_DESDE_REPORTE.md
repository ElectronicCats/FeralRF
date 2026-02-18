# Plan por fases derivado de `REPORTE_SNIFFER_FW_CC1252P_7.md`

## Principios de implementacion (fijos)

1. Mantener protocolo host en `COBS + CRC16` (no migrar a SOF/EOF + FCS8).
2. Copiar arquitectura interna probada del sniffer funcional: tareas, eventos, colas, pipeline RF.
3. Avanzar por incrementos validables en hardware real (CC1352P + RP2040 + Python).

---

## Estado actual (2026-02-18)

1. Fase 1: `COMPLETA` (contrato COBS+CRC16 estable y API Python funcional).
2. Fase 2: `COMPLETA` (UART 921600 + comandos base + smoke test en verde).
3. Fase 3: `COMPLETA (alcance MVP)` (pipeline `control_task + data_task + host_if_task` activo).
4. Fase 4: `COMPLETA (alcance MVP)` (RF BLE real, data queue, manejo de overflow, restart RX).
5. Fase 5: `COMPLETA` (base tabular `phy_manager` + `LL_DEFAULT/LL_BLE` integrada, parser LL BLE ampliado con subtipos advertising/extended y casos reservados, metricas LL por tipo en `GET_STATS`; backend RF real `IEEE_802_15_4` RX validado en HW con captura real y barrido por canales).
6. Fase 6: `COMPLETA (MVP BLE)` (sniffing BLE funcional validado con captura continua de 30 min en hardware).
7. Fase 7: `EN PROGRESO` (metricas base `rx_ok/rx_crc_err/rx_drop/rx_overflow` expuestas por comando host + `TX_RAW` fase 1 con `ACK` estable en PHY4 y BLE).

---

## Fase 1: Contrato de protocolo y API Python

### Objetivo
Cerrar y validar el contrato wire-format host <-> firmware antes de escalar firmware.

### Trabajo
1. Congelar especificacion de frame (`CMD_ID, SEQ, LEN, PAYLOAD, CRC16`, COBS delimitado con `0x00`).
2. Corregir `python/feralrf/commands.py` para que **no** incluya `CMD_ID` dentro del payload.
3. Mantener `CMD_ID` solo en `build_frame(...)` desde `python/feralrf/radio.py`.
4. Agregar pruebas de compatibilidad C/Python para encode/decode y CRC.

### Entregables
1. API Python sin duplicacion de comando.
2. Tests de protocolo en verde y con casos borde (payload con `0x00`, CRC invalido, LEN invalido).

### Criterio de cierre
1. El parser Python acepta frames generados desde firmware C y viceversa.
2. `pytest` de protocolo pasa completo sin flaky tests.

---

## Fase 2: Base de comunicacion en CC1352 (sin blink)

### Objetivo
Reemplazar blink por firmware minimo operativo con UART y command processor.

### Trabajo
1. Implementar UART RX/TX a `921600` baud, modo binario, timeout de lectura.
2. Implementar decoder COBS + validador CRC16 en RX.
3. Implementar dispatcher minimo con:
   1. `CMD_GET_INFO`
   2. `CMD_SET_PHY`
   3. `CMD_RX_START`
   4. `CMD_RX_STOP`
4. Implementar respuestas `ACK/ERROR/INFO`.

### Entregables
1. Firmware CC1352 responde comandos reales desde Python.
2. Loop comando-respuesta estable por UART.

### Criterio de cierre
1. Script de smoke test Python ejecuta secuencia `GET_INFO -> SET_PHY -> RX_START -> RX_STOP` sin errores.

---

## Fase 3: Arquitectura de tareas y colas (patron del sniffer funcional)

### Objetivo
Adoptar arquitectura robusta tipo `control_task + data_task + host_if_task`.

### Trabajo
1. Crear tareas separadas por responsabilidad:
   1. `control_task`: comandos y estado global.
   2. `data_task`: consumo de eventos RF y procesamiento de paquetes.
   3. `host_if_task`: envio a host desde cola.
2. Implementar modulo de eventos entre tareas (`init done`, `rx done`, `buffer full`).
3. Implementar `packet_queue` con pool fijo (sin malloc).

### Entregables
1. Pipeline desacoplado radio -> procesamiento -> host.
2. Eliminacion de bloqueos cruzados entre lectura de comandos y streaming.

### Criterio de cierre
1. Firmware mantiene respuesta a comandos aun durante carga de RX.
2. No hay deadlocks ni starvation en pruebas de 10-15 min.

---

## Fase 4: Pipeline RF robusto

### Objetivo
Implementar recepcion RF confiable con manejo de overflow y timestamps.

### Trabajo
1. Implementar `radio_if`:
   1. `init`, `start_rx`, `stop_rx`, `set_frequency`, `set_phy`.
2. Implementar dataqueue circular RF (entradas fijas, longitud maxima de paquete, bytes appended).
3. Manejar eventos:
   1. `RX_ENTRY_DONE`
   2. `RX_BUFFER_FULL`
4. Implementar estrategia de recuperacion en overflow:
   1. flush queue
   2. restart RX
   3. reporte de error al host.

### Entregables
1. Streaming RX continuo sin cuelgues.
2. Error handling visible al host.

### Criterio de cierre
1. Test de stress de RX (trafico alto) sin reinicios inesperados del MCU.

---

## Fase 5: PHY manager tabular + Link Layer pluggable

### Objetivo
Escalar sin refactor grande al agregar BLE/802.15.4/Sub-1GHz.

### Trabajo
1. Implementar `phy_manager` con tabla de PHYs soportados y validacion de `phy_number`.
2. Implementar comandos por API RF via function pointers.
3. Implementar `ll_manager` con al menos:
   1. `LL_DEFAULT`
   2. `LL_BLE` basico
4. Conectar seleccion de LL automaticamente al cambiar PHY.

### Entregables
1. Cambio de PHY por comando host sin tocar flujo principal.
2. Procesamiento LL encapsulado por tipo de PHY.

### Criterio de cierre
1. Cambio de PHY en caliente (en estado permitido) funciona y responde `ACK`.

---

## Fase 6: MVP BLE Sniffer end-to-end

### Objetivo
Lograr sniffing BLE util y estable de punta a punta.

### Trabajo
1. Inicializar BLE PHY y canal.
2. Recibir PDUs BLE y formatear `RSP_RX_PACKET`.
3. Parsear paquetes en Python (`radio.read_packets()`).
4. Validar contra trafico real (advertising packets).

### Entregables
1. Demo funcional de captura BLE desde Python.
2. Script de ejemplo actualizado y reproducible.

### Criterio de cierre
1. Captura continua durante 30 minutos sin caida.
2. Metadata minima correcta por paquete (timestamp, canal, RSSI, CRC).

### Validacion actual
1. `SOAK 30 min`: `OK` en hardware real (usuario reporta corrida completa estable).

---

## Fase 7: Endurecimiento y preparacion para expansion

### Objetivo
Dejar base lista para Zigbee/Sub-1GHz, jamming y spectrum.

### Trabajo
1. Instrumentacion de errores y metricas (overflow, drop rate, CRC fails).
2. Pruebas de regresion HW + Python automatizadas.
3. Ajustes de latencia/throughput (colas, tamanos de buffer, timeouts).
4. Documentar contrato final en `docs/protocol.md` y actualizar `PLAN_MAESTRO.md`.

### Entregables
1. Baseline estable para nuevas features.
2. Checklist de release interna para Fase BLE MVP.

### Criterio de cierre
1. Build reproducible + tests clave en verde + validacion en hardware.

---

## Orden recomendado de ejecucion inmediata (actualizado)

1. Tarea 1 (cierre tecnico Fase 5 - LL BLE):
   1. `OK`: parser `LL_BLE` ampliado (subtipos advertising/extended y casos reservados).
   2. `OK`: compatibilidad backward mantenida en `RSP_RX_PACKET` (metadata capability-gated).
   3. `OK`: clasificacion consistente validada en captura real multi-canal.
2. Tarea 2 (cierre tecnico Fase 5 - multi-PHY real):
   1. `OK`: validar en hardware el backend RF real para `IEEE_802_15_4` (RX) ya integrado (sin timeouts de cierre).
   2. `OK`: captura real con emisor Zigbee/Thread activo (ej.: canal 25 con `packets>0`, `delta_ok>0`).
   3. `OK`: barrido `11..26` operativo (`python/examples/sweep_phy4_ieee154.py`) con deteccion en canales activos.
3. Tarea 3 (avance Fase 7 - regresion automatizada):
   1. `OK`: agregar script canario que ejecute smoke + soak corto + validacion de stats LL (`python/examples/canary_regression.py`).
   2. `OK`: umbrales de aceptacion implementados (sin timeout, `RX_STOP ACK`, contadores monotonos, `min_packets` configurable + perfiles `lab/ci_manual/quiet`).
   3. `OK`: validacion HW base completada (`CANARY PASS`, 60s, `packets_total=8150`).
4. Tarea 4 (documentacion de contrato):
   1. `OK`: crear `docs/protocol.md` con formato actualizado de `RSP_RX_PACKET` y `RSP_STATS`.
   2. `OK`: documentar capabilities (`0x01`, `0x02`, `0x04`) y reglas de compatibilidad.
   3. `OK`: host y firmware referencian la misma especificacion.
5. Tarea 5 (gate de release interna BLE):
   1. `OK`: checklist de validacion HW ejecutado (smoke, soak, clasificacion LL, cierre limpio).
   2. `OK`: gate operativo formalizado en comando unico (`python/examples/release_gate_ble.py`).
   3. `OK`: corridas 1/2 y 2/2 consecutivas del gate BLE en verde (`BLE RELEASE GATE PASS`).
   4. `OK`: baseline BLE congelado.
   5. `OK`: gate conectado a flujo manual/CI (`.github/workflows/ble_release_gate_hw.yml` + validacion en `build.yml`).
   6. pendiente: abrir Zigbee/Sub-1GHz/TX/jamming manteniendo gate BLE como no-regresion.
6. Tarea 6 (arranque vertical TX):
   1. `OK`: `CMD_TX_RAW` integrado en firmware con ruta no bloqueante (ACK inmediato) y smoke host `python/examples/smoke_tx_phase1.py`.
   2. pendiente: validación RF over-the-air para PHY4 (receptor externo confirma trama emitida).
   3. `OK`: extensión `TX_RAW` a PHY BLE (PHY 0) con smoke equivalente (`python/examples/smoke_tx_ble_phase1.py`, `TX BLE SMOKE PASS`).
   4. pendiente: validación RF over-the-air para BLE (receptor externo confirma advertising emitido en canal 37/38/39).

Este orden prioriza mantener baseline BLE estable mientras se abre Zigbee/Sub-1GHz/TX por incrementos.
