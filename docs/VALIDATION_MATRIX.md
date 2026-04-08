# FeralRF Validation Matrix

Fecha: 2026-04-07

Este documento define el baseline recomendado para validar el firmware actual de FeralRF sobre CC1352P + RP2040.

Objetivos:

- Alinear firmware, Python API, ejemplos y documentacion sobre un baseline comun.
- Separar soporte oficial, experimental y pendiente.
- Tener una matriz simple para validar lo basico de cada PHY, protocolo y modulacion.
- Mapear cada caso a un script real cuando ya exista automatizacion.

## 1. Alcance del baseline oficial

El baseline oficial recomendado cubre:

- Sesion y transporte: `RADIO_INIT`, `GET_INFO`, `GET_STATS`.
- Configuracion: `SET_PHY`, `SET_CHANNEL`, `SET_POWER`.
- RX: `RX_START`, `read_packets()`, `RX_STOP`.
- TX: `TX_RAW`, `TX_FRAME`, `TX_BURST`, `TX_CONTINUOUS`, `TX_STOP`.
- Configuracion propietaria: `SET_PROP_CONFIG`, `PROP_PRESETS`.
- BLE extras ya presentes en codigo: `SET_BLE_ADDR`, `SET_BLE_SCAN_MODE`, `SET_ADV_HOP`.
- Recovery especial para OOK: `reset_device()`.

Queda fuera del baseline oficial inicial:

- Spectrum / RSSI scan.
- Jamming reactivo o por patron.
- GATT discovery / initiator.
- Tooling ofensivo de IEEE 802.15.4 y Sub-1GHz aun no implementado.

## 2. API publica recomendada

### Estable

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
- `set_ble_scan_mode()`
- `set_adv_hop()`
- `reset_device()` con nota explicita para OOK

### Experimental

- `start_jam()`
- `stop_jam()`

### Pendiente o no oficial

- Cualquier API de spectrum.
- GATT / initiator / scanner avanzado mas alla de `SET_BLE_SCAN_MODE`.
- Modulos de ataques no-BLE que aun no existan en `python/feralrf/attacks/`.

## 3. Criterio general de PASS

Un caso se considera `PASS` cuando:

- El dispositivo responde `ACK` a los comandos de configuracion.
- `RX_START` y `RX_STOP` no hacen timeout.
- Si el caso es RX, se puede abrir y cerrar la ruta de recepcion sin cuelgues.
- Si el caso es TX, se obtiene `ACK` del comando correspondiente.
- `GET_STATS` responde con payload valido.
- El flujo puede repetirse al menos 3 veces sin dejar al equipo en estado roto.

Opcionalmente, para validacion OTA mas fuerte:

- En RX: se exige `min_packets > 0`.
- En TX: un segundo equipo o sonda externa confirma el marcador/payload.

## 4. Matriz por PHY y protocolo

| ID | PHY | Basico a validar | Estado esperado hoy | Script recomendado |
|----|-----|------------------|---------------------|-------------------|
| 0 | BLE 1M | RX passive scan | Debe pasar | `python/examples/smoke_ble_scan_mode.py --mode passive` |
| 0 | BLE 1M | RX active scan | Debe pasar | `python/examples/smoke_ble_scan_mode.py --mode active` |
| 0 | BLE 1M | TX raw/frame advertising | Debe pasar | `python/examples/smoke_tx_ble_phase1.py`, `python/examples/smoke_tx_frame_phase1.py --phy 0` |
| 1 | BLE 2M | Config + RX start/stop | Debe pasar | `python/examples/smoke_phase2.py --phy 1 --channel 37` |
| 1 | BLE 2M | TX frame basico | Debe pasar | `python/examples/smoke_tx_frame_phase1.py --phy 1 --channel 37` |
| 2 | BLE Coded S8 | Config + RX start/stop | Debe pasar | `python/examples/smoke_phase2.py --phy 2 --channel 37` |
| 2 | BLE Coded S8 | TX frame basico | Debe pasar | `python/examples/smoke_tx_frame_phase1.py --phy 2 --channel 37` |
| 3 | BLE Coded S2 | Config + RX start/stop | Debe pasar | `python/examples/smoke_phase2.py --phy 3 --channel 37` |
| 3 | BLE Coded S2 | TX frame basico | Debe pasar | `python/examples/smoke_tx_frame_phase1.py --phy 3 --channel 37` |
| 4 | IEEE 802.15.4 | RX en canal fijo | Debe pasar | `python/examples/smoke_phy4_ieee154.py --channel 25` |
| 4 | IEEE 802.15.4 | TX raw | Debe pasar | `python/examples/smoke_tx_phase1.py --phy 4 --channel 25` |
| 4 | IEEE 802.15.4 | TX frame | Debe pasar | `python/examples/smoke_tx_frame_phase1.py --phy 4 --channel 25` |
| 4 | IEEE 802.15.4 | TX burst | Debe pasar | `python/examples/smoke_tx_burst_phase1.py --phy 4 --channel 25` |
| 4 | IEEE 802.15.4 | TX continuous + stop | Debe pasar | `python/examples/smoke_tx_continuous_phase1.py --phy 4 --channel 25` |
| 5 | Sub-1GHz 868 | Config prop + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_868_50k` |
| 6 | Sub-1GHz 915 | Config prop + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_915_50k` |
| 7 | Proprietary GFSK | Config prop + RX/TX 2.4 GHz | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_2440_50k` |

## 5. Matriz por modulacion propietaria

La ruta propietaria usa `PHY.PROPRIETARY_GFSK` con `configure_prop()`.

| Modulacion | Banda | Caso minimo | Estado esperado hoy | Script recomendado |
|-----------|-------|-------------|---------------------|-------------------|
| GFSK | 433 MHz | Config + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_433_50k` |
| GFSK | 868 MHz | Config + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_868_50k` |
| GFSK | 915 MHz | Config + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_915_50k` |
| GFSK | 2440 MHz | Config + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset gfsk_2440_50k` |
| FSK | 433 MHz | Config + RX/TX | Debe pasar | `python/examples/smoke_prop_phase1.py --preset fsk_433_50k` |
| OOK | 433 MHz | Config + RX/TX + recovery | Debe pasar con recovery especial | `python/examples/smoke_prop_phase1.py --preset ook_433_4k8 --auto-reset` |
| OOK | 868 MHz | Config + RX/TX + recovery | Debe pasar con recovery especial | `python/examples/smoke_prop_phase1.py --preset ook_868_4k8 --auto-reset` |
| MSK | cualquier banda | No incluir en baseline aun | Pendiente smoke dedicado | sin script oficial aun |

Notas:

- OOK debe validarse como caso especial porque bloquea el radio y requiere `reset_device()`.
- MSK no debe marcarse como soporte oficial hasta tener smoke dedicado y evidencia OTA.

## 6. Matriz de switching

Casos minimos recomendados:

- `BLE 1M -> IEEE 802.15.4 -> BLE 1M`
- `BLE 1M -> Sub-1GHz 868 -> BLE 1M`
- `BLE 1M -> Sub-1GHz 915 -> BLE 1M`
- `BLE 1M -> Proprietary 2.4 GHz -> BLE 1M`
- `OOK -> reset_device() -> BLE 1M`

Estado esperado:

- Todo debe volver a responder al baseline de control.
- Ningun cambio de PHY debe dejar la radio muda sin recovery conocido.
- OOK solo se considera `PASS` si el recovery via `reset_device()` tambien pasa.

## 7. Orden recomendado de ejecucion

1. Validar sesion y control base en BLE 1M.
2. Validar BLE passive y active scan.
3. Validar BLE TX basico.
4. Validar BLE 2M, Coded S8 y Coded S2.
5. Validar IEEE 802.15.4 RX/TX.
6. Validar modulaciones propietarias GFSK/FSK.
7. Validar OOK con recovery.
8. Validar switching entre familias.
9. Ejecutar soak corto o gate consolidado.

## 8. Checklist ejecutable

Baseline automatizado:

- `python/examples/run_validation_baseline.sh`

Este wrapper:

- Ejecuta los smokes existentes por familia.
- Usa nuevos smokes para BLE scan mode y presets propietarios.
- Puede incluir OOK solo cuando se solicita explicitamente.
- Resume al final los casos ejecutados y los que siguen siendo manuales.

Entry points del repo:

- Oficiales: `python/examples/`
- Lab / manuales / OTA / demos / soak: `python/examples/lab/`

## 9. Casos aun manuales o pendientes

Quedan como pendientes de automatizacion o con validacion mas debil:

- OTA fuerte para BLE 2M / Coded S8 / Coded S2.
- Validacion de `SCAN_RSP` con entorno BLE controlado y `min_packets > 0`.
- TX OTA dedicada de presets propietarios por banda.
- MSK.
- Jamming con criterio real de interferencia.
- Spectrum / RSSI scan.
- GATT discovery.
