# FeralRF Python API

Fecha: 2026-04-07

Este documento resume la API publica recomendada del paquete `feralrf` y su estado actual.

Relacion con otros documentos:

- Contrato wire-format: `docs/protocol.md`
- Baseline y matriz de validacion: `docs/VALIDATION_MATRIX.md`

Entry points recomendados:

- Oficiales: `python/examples/`
- Manuales, OTA, demos y caracterizacion: `python/examples/lab/`

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

- Para BLE active scan:
- llamar `set_ble_scan_mode(active=True)` antes de `start_rx()`

- Para modo propietario (GFSK/FSK/OOK/MSK):
- `set_phy(PHY.PROPRIETARY_GFSK)` seguido de `configure_prop(frequency_hz, mod_type, ...)`
- `mod_type`: 0=FSK, 1=GFSK, 2=OOK/ASK, 4=MSK
- O usar presets: `radio.configure_prop(**PROP_PRESETS['gfsk_868_50k'])`

### Presets disponibles (`PROP_PRESETS`)

| Preset | Freq | Mod | Rate | OTA |
|--------|------|-----|------|-----|
| `gfsk_433_50k` | 433.92 MHz | GFSK | 50k | 6-10/10 |
| `gfsk_433_10k` | 433.92 MHz | GFSK | 10k | — |
| `fsk_433_50k` | 433.92 MHz | FSK | 50k | 1/10 |
| `msk_433_50k` | 433.92 MHz | MSK | 50k | 1/10 |
| `ook_433_4k8` | 433.92 MHz | OOK | 4.8k | 0/10 (hw) |
| `ook_433_2k4` | 433.92 MHz | OOK | 2.4k | — |
| `gfsk_868_50k` | 868 MHz | GFSK | 50k | 10/10 |
| `gfsk_868_100k` | 868 MHz | GFSK | 100k | — |
| `msk_868_50k` | 868 MHz | MSK | 50k | 10/10 |
| `ook_868_4k8` | 868 MHz | OOK | 4.8k | 10/10 |
| `wireless_mbus_s_868` | 868.3 MHz | GFSK | 32.7k | 10/10 |
| `wireless_mbus_t_868` | 868.95 MHz | GFSK | 100k | 10/10 |
| `wireless_mbus_c_868` | 868.95 MHz | GFSK | 100k | 10/10 |
| `wireless_mbus_n_169_2k4` | 169.45 MHz | GFSK | 2.4k | — |
| `wireless_mbus_n_169_4k8` | 169.45 MHz | GFSK | 4.8k | — |
| `gfsk_915_50k` | 915 MHz | GFSK | 50k | 10/10 |
| `gfsk_902_50k` | 902.2 MHz | GFSK | 50k | — |
| `gfsk_2440_250k` | 2440 MHz | GFSK | 250k | — |
| `gfsk_2440_50k` | 2440 MHz | GFSK | 50k | 10/10 |

### `reset_device()`

`reset_device()` envia `boot` + `exit` al RP2040 shell port, que resetea el CC1352 por hardware y reinicializa la sesion. Se recomienda usar en los siguientes casos:

| Caso | Razon |
|------|-------|
| Despues de OOK | OOK carga patches MCE+RFE (genook) que no se pueden descargar. El radio queda bloqueado en OOK hasta power cycle o reset. |
| Entre cambios de banda de frecuencia | Cambiar de 433 a 868 MHz (o viceversa) puede dejar el sintetizador en estado inconsistente si no se hace reset. |
| Despues de timeout o error de comunicacion | Si el firmware deja de responder (timeout en `init()` o cualquier comando), un reset recupera el dispositivo. |
| Entre PHYs diferentes en tests automatizados | El validation script (`run_validation_baseline.sh`) resetea entre cada step para garantizar estado limpio. |
| Despues de `start_jam()` / `stop_jam()` | Jamming puede dejar el radio en modo TX continuo. Reset garantiza regreso a idle. |

Nota: `reset_device()` funciona incluso cuando el firmware esta colgado porque opera directamente sobre el RP2040 shell port (bridge_port + 2), sin depender de respuesta del CC1352.

Para scripts automatizados donde `reset_device()` puede fallar (radio completamente muerto), el validation script usa reset directo via serial al shell port como fallback.

## 5. Compatibilidad

El objetivo de compatibilidad del paquete es:

- mantener estables firmas y semantica de la API listada arriba
- no promover comandos pendientes hasta que firmware, docs y validacion esten alineados
- mantener helpers experimentales disponibles pero etiquetados claramente
