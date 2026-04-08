# FeralRF Python API

Fecha: 2026-04-07

Este documento resume la API publica recomendada del paquete `feralrf` y su estado actual.

Relacion con otros documentos:

- Contrato wire-format: `docs/protocol.md`
- Baseline y matriz de validacion: `docs/VALIDATION_MATRIX.md`

## 1. API publica estable

Objetos exportados:

- `Radio`
- `Packet`
- `DeviceInfo`
- `DeviceStats`
- `PHY`
- `Command`
- `Response`
- `PROP_PRESETS`

Constantes de estado exportadas:

- `STABLE_COMMANDS`
- `EXPERIMENTAL_COMMANDS`
- `PENDING_COMMAND_IDS`

Metodos estables de `Radio`:

- `init()`
- `set_phy()`
- `set_channel()`
- `set_power()`
- `start_rx()`
- `read_packets()`
- `stop_rx()`
- `transmit()`
- `transmit_frame()`
- `transmit_burst()`
- `transmit_continuous()`
- `stop_transmit()`
- `get_stats()`
- `configure_prop()`
- `set_ble_addr()`
- `set_ble_addr_str()`
- `set_ble_scan_mode()`
- `set_adv_hop()`
- `reset_device()`

## 2. API experimental

Metodos experimentales de `Radio`:

- `start_jam()`
- `stop_jam()`

Razon:

- El firmware responde a estos comandos y existen smokes de control.
- Aun no se consideran parte del baseline estable porque falta validacion funcional de interferencia real.

## 3. API pendiente

No se consideran parte de la API publica final todavia:

- spectrum / RSSI scan
- GATT discovery
- BLE initiator
- helpers ofensivos IEEE 802.15.4 y Sub-1GHz aun no implementados

Comandos reservados o pendientes hoy:

- `JAM_REACTIVE = 0x31`
- `JAM_PATTERN = 0x32`
- `SPECTRUM_SCAN = 0x40`
- `SPECTRUM_MONITOR = 0x41`
- `SPECTRUM_STOP = 0x42`

## 4. Reglas de uso recomendadas

- Para sesiones normales:
- `init() -> set_phy() -> set_channel()/configure_prop() -> start_rx()/transmit_*()`

- Para OOK:
- usar `configure_prop(mod_type=2, ...)`
- tratar OOK como modo especial
- llamar `reset_device()` antes de volver a BLE/IEEE/u otro modo

- Para BLE active scan:
- llamar `set_ble_scan_mode(active=True)` antes de `start_rx()`

## 5. Compatibilidad

El objetivo de compatibilidad del paquete es:

- mantener estables firmas y semantica de la API listada arriba
- no promover comandos pendientes hasta que firmware, docs y validacion esten alineados
- mantener helpers experimentales disponibles pero etiquetados claramente
