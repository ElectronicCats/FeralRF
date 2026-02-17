# Plan por fases derivado de `REPORTE_SNIFFER_FW_CC1252P_7.md`

## Principios de implementacion (fijos)

1. Mantener protocolo host en `COBS + CRC16` (no migrar a SOF/EOF + FCS8).
2. Copiar arquitectura interna probada del sniffer funcional: tareas, eventos, colas, pipeline RF.
3. Avanzar por incrementos validables en hardware real (CC1352P + RP2040 + Python).

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

## Orden recomendado de ejecucion inmediata

1. Fase 1 completa.
2. Fase 2 minima (GET_INFO + ACK/ERROR).
3. Fase 3 parcial (control + host_if_task).
4. Fase 4 minima (RX pipeline).
5. Fase 5 y 6 en iteraciones cortas.

Este orden minimiza retrabajo y evita acoplar el firmware a un contrato de protocolo incorrecto.
