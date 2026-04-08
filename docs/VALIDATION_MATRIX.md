# FeralRF Validation Matrix

Ultima actualizacion: 2026-04-07
Branch: `feature/ti-rtos-migration` (commit b80b38f)

Este documento define el baseline de validacion del firmware FeralRF sobre CC1352P7 + RP2040 (CatSniffer).

## 1. Alcance del baseline oficial

El baseline oficial cubre:

- Sesion y transporte: `RADIO_INIT`, `GET_INFO`, `GET_STATS`.
- Configuracion: `SET_PHY`, `SET_CHANNEL`, `SET_POWER`.
- RX: `RX_START`, `read_packets()`, `RX_STOP`.
- TX: `TX_RAW`, `TX_FRAME`, `TX_BURST`, `TX_CONTINUOUS`, `TX_STOP`.
- Configuracion propietaria: `SET_PROP_CONFIG`, `PROP_PRESETS`.
- BLE extras: `SET_BLE_ADDR`, `SET_BLE_SCAN_MODE`, `SET_ADV_HOP`.
- Recovery OOK: `reset_device()`.

Fuera del baseline:

- Spectrum / RSSI scan.
- Jamming reactivo o por patron.
- GATT discovery / initiator.
- Tooling ofensivo IEEE 802.15.4 / Sub-1GHz no implementado.

## 2. Resultado del baseline (2026-04-07)

**18/18 PASS** con `run_validation_baseline.sh --port /dev/ttyACM3`

Se ejecuta CC1352 reset via RP2040 shell entre cada step.

### Control path (single board)

| # | Test | Resultado | Notas |
|---|------|-----------|-------|
| 1 | BLE 1M control path | **PASS** | init/set_phy/rx_start/rx_stop |
| 2 | BLE passive scan | **PASS** | 415 pkts en 5s (cuando hay BLE cerca) |
| 3 | BLE active scan | **PASS** | 448 pkts, SCAN_RSP capturado |
| 4 | BLE 1M TX raw | **PASS** | TX_RAW ACK |
| 5 | BLE 1M TX frame | **PASS** | TX_FRAME ACK |
| 6 | BLE 2M control path | **PASS** | Requiere reset entre tests |
| 7 | BLE Coded S8 control | **PASS** | |
| 8 | BLE Coded S2 control | **PASS** | |
| 9 | IEEE 802.15.4 RX | **PASS** | 20 pkts capturados |
| 10 | IEEE TX raw | **PASS** | |
| 11 | IEEE TX frame | **PASS** | |
| 12 | IEEE TX burst | **PASS** | |
| 13 | IEEE TX continuous | **PASS** | TX_CONTINUOUS + TX_STOP |
| 14 | GFSK 868 preset | **PASS** | |
| 15 | GFSK 915 preset | **PASS** | |
| 16 | GFSK 2.4 GHz preset | **PASS** | |
| 17 | GFSK 433 preset | **PASS** | |
| 18 | FSK 433 preset | **PASS** | |
| 19 | OOK 868 preset | **PASS** | Con auto-reset recovery |
| 20 | OOK 433 preset | **PASS** | Control path OK, OTA limitado |

### OTA (2-board TX/RX con DEADBEEF markers)

| Test | Resultado | Notas |
|------|-----------|-------|
| 868 GFSK default | **10/10 PASS** | |
| 868 GFSK configure_prop | **10/10 PASS** | |
| 433 GFSK configure_prop | **7-9/10 PASS** | Marginal, depende de posicion de antena |
| OOK 868 | **10/10 PASS** | |
| OOK 433 | **0/10 FAIL** | Hardware: antena CatSniffer no optimizada para 433 + OOK baja sensibilidad |

## 3. API publica

### Estable

- `init()`, `connect()`, `disconnect()`
- `set_phy()`, `set_channel()`, `set_power()`
- `start_rx()`, `read_packets()`, `stop_rx()`
- `transmit()`, `transmit_frame()`, `transmit_burst()`, `transmit_continuous()`, `stop_transmit()`
- `get_stats()`
- `configure_prop()`
- `set_ble_addr()`, `set_ble_addr_str()`, `set_ble_scan_mode()`, `set_adv_hop()`
- `reset_device()` (requerido despues de OOK y entre cambios de banda)

### Experimental

- `start_jam()`, `stop_jam()`

### Pendiente

- Spectrum scan / RSSI
- GATT / initiator / scanner avanzado
- Ataques IEEE 802.15.4 / Sub-1GHz

## 4. Matriz por PHY y protocolo

| ID | PHY | Test | Estado | Script |
|----|-----|------|--------|--------|
| 0 | BLE 1M | Passive scan | **PASS** | `smoke_ble_scan_mode.py --mode passive` |
| 0 | BLE 1M | Active scan | **PASS** | `smoke_ble_scan_mode.py --mode active` |
| 0 | BLE 1M | TX raw/frame | **PASS** | `smoke_tx_ble_phase1.py`, `smoke_tx_frame_phase1.py --phy 0` |
| 1 | BLE 2M | Control + RX | **PASS** | `smoke_phase2.py --phy 1` |
| 2 | BLE Coded S8 | Control + RX | **PASS** | `smoke_phase2.py --phy 2` |
| 3 | BLE Coded S2 | Control + RX | **PASS** | `smoke_phase2.py --phy 3` |
| 4 | IEEE 802.15.4 | RX | **PASS** | `smoke_phy4_ieee154.py --channel 25` |
| 4 | IEEE 802.15.4 | TX raw/frame/burst/continuous | **PASS** | `smoke_tx_*.py --phy 4` |
| 5 | Sub-1GHz 868 | Config + RX/TX | **PASS** | `smoke_prop_phase1.py --preset gfsk_868_50k` |
| 6 | Sub-1GHz 915 | Config + RX/TX | **PASS** | `smoke_prop_phase1.py --preset gfsk_915_50k` |
| 7 | Prop GFSK 2.4G | Config + RX/TX | **PASS** | `smoke_prop_phase1.py --preset gfsk_2440_50k` |

## 5. Matriz por modulacion propietaria

| Modulacion | Banda | Estado | OTA | Script |
|-----------|-------|--------|-----|--------|
| GFSK | 433 MHz | **PASS** | 7-9/10 (marginal) | `smoke_prop_phase1.py --preset gfsk_433_50k` |
| GFSK | 868 MHz | **PASS** | 10/10 | `smoke_prop_phase1.py --preset gfsk_868_50k` |
| GFSK | 915 MHz | **PASS** | 10/10 | `smoke_prop_phase1.py --preset gfsk_915_50k` |
| GFSK | 2440 MHz | **PASS** | 10/10 | `smoke_prop_phase1.py --preset gfsk_2440_50k` |
| FSK | 433 MHz | **PASS** | no probado | `smoke_prop_phase1.py --preset fsk_433_50k` |
| OOK | 868 MHz | **PASS** | 10/10 | `smoke_prop_phase1.py --preset ook_868_4k8 --auto-reset` |
| OOK | 433 MHz | **PASS** (ctrl) | 0/10 (hw) | `smoke_prop_phase1.py --preset ook_433_4k8 --auto-reset` |
| MSK | cualquier | Pendiente | - | sin script |

Notas:
- OOK bloquea el radio. Requiere `reset_device()` o power cycle despues.
- 433 MHz OTA es marginal en CatSniffer (antena optimizada para 868/915).
- OOK 433 OTA no funciona por link budget insuficiente (OOK -10dB sensibilidad + antena -7dB).

## 6. Matriz de switching

| Secuencia | Estado | Notas |
|-----------|--------|-------|
| BLE 1M -> IEEE -> BLE 1M | **PASS** | Con reset entre cambios |
| BLE 1M -> Sub-1GHz 868 -> BLE 1M | **PASS** | Con reset |
| BLE 1M -> GFSK 433 -> BLE 1M | **PASS** | Con reset |
| OOK -> reset_device() -> BLE 1M | **PASS** | |
| PHY switch sin reset | FAIL | RF_close deadlock en 2do ciclo |

## 7. Orden de ejecucion

1. BLE 1M control + scan + TX
2. BLE 2M, Coded S8, Coded S2
3. IEEE 802.15.4 RX/TX
4. Proprietary GFSK/FSK 868/915/2.4G
5. Proprietary GFSK/FSK 433 (marginal)
6. OOK 868 + recovery (ultimo, bloquea radio)
7. OOK 433 + recovery (ultimo)

## 8. Checklist ejecutable

```bash
# Todas las pruebas (reset entre cada step)
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3

# Solo un subset
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only BLE
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only IEEE
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only 433
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only OOK
```

El script:
- Resetea CC1352 via RP2040 shell entre cada test (funciona aun con radio trabado)
- 433 MHz y OOK corren al final
- `--only FILTER` para correr solo tests que contengan el filtro (case-insensitive)

## 9. Pendientes

- OTA para BLE 2M / Coded S8 / Coded S2
- Validacion SCAN_RSP con entorno BLE controlado
- TX OTA dedicada por banda con 2 boards
- MSK
- Jamming con criterio real de interferencia
- Spectrum / RSSI scan
- GATT discovery (requiere TI-RTOS + BLE5-Stack)
- PHY switching sin reset (RF_close deadlock)
