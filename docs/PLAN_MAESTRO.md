# FeralRF - Plan Maestro

**Version:** 3.0 | **Fecha:** 2026-04-02

Firmware universal para CatSniffer (CC1352P + RP2040). Objetivo: API Python facil de usar para pentesting RF con todos los protocolos del CC1352.

---

## Estado Actual

### Funcionando (validado OTA)
| Componente | Estado |
|-----------|--------|
| COBS + CRC16 (921600 baud) | OK |
| BLE 1M RX (ch37-39, hopping, LL metadata) | OK |
| IEEE 802.15.4 RX (ch11-26) | OK |
| TX_RAW (BLE + IEEE) | OK |
| TX_FRAME (BLE + IEEE) | OK |
| TX_BURST | OK |
| TX_CONTINUOUS + TX_STOP | OK |
| Python API completa | OK |
| RP2040 USB-CDC bridge | OK |
| Metricas RX (rx_ok, crc_err, drop, overflow) | OK |

### NO funcionando
| Componente | Nota |
|-----------|------|
| BLE 2M / Coded S8 / Coded S2 | Overrides existen, no wired en radio_if.c |
| Sub-1GHz 868/915 MHz | Sin SmartRF config ni backend |
| Proprietary GFSK | Sin backend |
| Jamming | Modo propietario 2.4 GHz fallo, no interfiere senales |
| Spectrum Analyzer | Solo skeleton Python |
| Attack commands | No implementados |
| Bootloader/OTA | Solo stub |

---

## Arquitectura

```
HOST (Python API) <-> RP2040 (USB Bridge) <-> CC1352P (Radio Engine)
```

- **CC1352P**: Radio operations, COBS protocol, command processing, TI-RTOS 7
- **RP2040**: USB-CDC bridge, timestamping, CC1352 reset monitoring
- **Python API**: `feralrf` package, sync interface, pyserial

### Radio IF internals
- `radio_if.c`: RF abstraction, enum RadioIF_RfMode (NONE=0, BLE=1, IEEE=2)
- `phy_manager.c`: Tabla de 8 PHYs, solo 2 con rf_backend_rx_supported=true
- SmartRF configs: `smartrf_ble5_0.c` (BLE5) y `smartrf_ieee_15_4_0.c` (IEEE)
- Patron: RF_open(mode, setup) -> RF_runCmd(fs) -> RF_postCmd(rx) para RX

---

## Protocolo

COBS-framed binary protocol con CRC16-CCITT.

```
Frame: [CMD_ID(1B)][SEQ(1B)][LEN(2B LE)][PAYLOAD(0-255B)][CRC16(2B LE)]
       └──────────── COBS encoded, 0x00 delimited ──────────┘
```

### Command IDs
```
Config:    RADIO_INIT(0x01) SET_CHANNEL(0x02) SET_POWER(0x03) SET_PHY(0x04) GET_INFO(0x05) GET_STATS(0x06) SET_ADV_HOP(0x07)
RX:        RX_START(0x10) RX_STOP(0x11)
TX:        TX_RAW(0x20) TX_CONTINUOUS(0x21) TX_BURST(0x22) TX_FRAME(0x23) TX_STOP(0x24)
Jam:       JAM_CONTINUOUS(0x30) JAM_REACTIVE(0x31) JAM_PATTERN(0x32) JAM_STOP(0x33)
Spectrum:  SPECTRUM_SCAN(0x40) SPECTRUM_MONITOR(0x41) SPECTRUM_STOP(0x42)
Response:  ACK(0x80) ERROR(0x81) RX_PACKET(0x90) SPECTRUM_DATA(0x91) JAM_EVENT(0x95)
```

---

## Fases de Desarrollo

### FASE 1: Habilitar todos los PHYs (RX + TX)

**Objetivo:** Todos los protocolos del CC1352 operativos con la misma API.

#### 1a. BLE 2M / Coded S8 / Coded S2
- Riesgo: Bajo (overrides ya existen en smartrf_ble5_0.c)
- Agregar `RadioIF_applyBlePhyMode()` en radio_if.c
- Configurar `defaultPhy.mainMode` y `coding` en cmdBle5RadioSetup
- Habilitar rf_backend_rx_supported para PHY 1,2,3 en phy_manager.c

#### 1b. Sub-1GHz 868 MHz
- Riesgo: Medio (nuevo RF mode completo)
- Crear smartrf_prop_0.c/h (CMD_PROP_RADIO_DIV_SETUP_PA, 50kBaud GFSK, loDivider=0x05)
- Agregar RADIO_IF_RF_MODE_SUB_1GHZ=3 en radio_if.c
- Implementar: startSub1ghzRfBackend(), processSub1ghzPackets(), transmitSub1ghzRaw()
- Power table Sub-1GHz separada
- Actualizar todos los dispatch points

#### 1c. Sub-1GHz 915 MHz
- Riesgo: Bajo (reutiliza backend de 1b, diferente frecuencia)

#### 1d. Proprietary GFSK
- Riesgo: Bajo-Medio (reutiliza CMD_PROP con loDivider dinamico)

**Orden:** 1a -> 1b -> 1c -> 1d

**Estado:** COMPLETADA (2026-04-02). 8/8 PHYs validados OTA con marcadores.

---

### FASE 2: Radio Propietaria Configurable

**Objetivo:** Exponer TODAS las capacidades RF del CC1352 al usuario: cualquier frecuencia, modulacion y data rate.

El CC1352 soporta bandas 143-1315 MHz + 2.4 GHz, modulaciones 2(G)FSK/4(G)FSK/MSK/OOK/ASK. Hoy solo usamos GFSK 50kBaud. Esta fase hace configurable todo via `CMD_PROP_RADIO_DIV_SETUP_PA`.

#### 2a. Firmware: CMD_SET_PROP_CONFIG (nuevo comando)
- Nuevo comando 0x08 que configura los campos del radio setup propietario:
  - `frequency_hz` (uint32): Frecuencia exacta
  - `modulation` (uint8): FSK=1, GFSK=1, 4FSK=2, 4GFSK=2, OOK=3, MSK=4
  - `symbol_rate` (uint32): Baud rate
  - `deviation` (uint16): Desviacion en Hz (para FSK/GFSK)
  - `rx_bandwidth` (uint8): Ancho de banda RX
  - `sync_word` (uint32): Palabra de sincronizacion
- Modifica campos de `Prop0_cmdPropRadioDivSetup` en runtime
- Calcula loDivider automaticamente segun frecuencia
- Calcula rateWord/preScale desde symbol_rate

#### 2b. Python API: radio.configure_prop()
```python
radio.configure_prop(
    frequency_hz=433920000,  # 433.92 MHz
    modulation='OOK',        # FSK, GFSK, 4GFSK, OOK, MSK
    data_rate=4800,          # baud
    deviation=5000,          # Hz
    sync_word=0x930B51DE,    # optional
)
# Despues usar normalmente:
radio.start_rx()
radio.transmit(payload)
```

#### 2c. Presets por protocolo comun
```python
radio.configure_prop(**PRESETS['wireless_mbus'])   # 868 MHz, FSK
radio.configure_prop(**PRESETS['sidewalk'])         # 900 MHz, FSK  
radio.configure_prop(**PRESETS['433_ook'])          # 433.92 MHz, OOK (garages, sensores)
radio.configure_prop(**PRESETS['315_ask'])          # 315 MHz, ASK (controles remotos)
```

#### Bandas soportadas por el CC1352
| Rango MHz | loDivider | Uso tipico |
|-----------|-----------|------------|
| 2360-2500 | 0x00 | BLE, IEEE 802.15.4, Prop 2.4 GHz |
| 1076-1315 | 0x04 | - |
| 861-1054 | 0x05 | 868/915 ISM, Wi-SUN, W-MBus, Sidewalk |
| 431-527 | 0x0A | 433 MHz ISM (sensores, garages) |
| 359-439 | 0x0C | 390-433 MHz (controles remotos) |
| 287-351 | 0x0F | 315 MHz (controles remotos NA) |
| 143-176 | 0x1E | 169 MHz (smart metering EU) |

#### Modulaciones
| Tipo | modType | Uso tipico |
|------|---------|------------|
| 2-GFSK | 0x1 | BLE, la mayoria IoT |
| 2-FSK | 0x1 | Wi-SUN, W-MBus |
| 4-GFSK | 0x2 | Mayor throughput |
| OOK | 0x3 | Garages, sensores baratos, 315/433 MHz |
| ASK | 0x3 | Controles remotos |
| MSK | 0x4 | Telemetria |

---

### FASE 3: Spectrum Analyzer

**Objetivo:** Escaneo RSSI en todas las bandas para reconocimiento pre-ataque.

- Firmware: SPECTRUM_SCAN(0x40), SPECTRUM_MONITOR(0x41), SPECTRUM_STOP(0x42)
- 2.4 GHz: CMD_IEEE_ED_SCAN o RF_getRssi()
- Sub-1GHz: CMD_PROP_RX con dwell corto + RF_getRssi()
- Python: `Radio.spectrum_scan(start_hz, end_hz, step_khz, dwell_ms)`
- Cubre TODAS las bandas de Fase 2 (143 MHz a 2.5 GHz)

---

### FASE 4: Attack Commands

**Objetivo:** Metodos Python de alto nivel para ataques RF.

Ataques en Python sobre TX existente (no en firmware). Mas flexible.

```
python/feralrf/attacks/
    ble.py          # beacon_flood(), adv_spoof(), replay()
    ieee154.py      # beacon_inject(), disassociate(), replay()
    sub1ghz.py      # replay(), brute_force(), ook_brute()
    prop.py         # generic replay, frequency hopping attacks
```

---

### FASE 5: Emulacion de Targets

**Objetivo:** CatSniffer como dispositivo victima para validar ataques.

Setup: 2 CatSniffers (atacante + target)

```
python/feralrf/emulation/
    ble_peripheral.py    # BLE advertising + scan response
    ieee154_device.py    # 802.15.4 beacon + data
    sub1ghz_device.py    # Sub-1GHz device emulation
    ook_device.py        # OOK/ASK device (garage, sensor)
```

---

### FASE 6: Jamming

**Objetivo:** Interferencia RF funcional en todas las bandas (143 MHz - 2.5 GHz).

- Jamming en cualquier frecuencia/banda configurada en Fase 2
- Debuggear CMD_TX_TEST (modo propietario 2.4 GHz que fallo)
- Alternativa: CMD_PROP_TX con payload largo y bFsOff=0
- Reactive jamming (<500us): ISR en sync word detection
- Pattern jamming: timer-based on/off

---

## Hardware

### Boards disponibles
- Board ...82:2E: Funcional (TX y RX)
- Board ...C1:82: RF muerta (danada durante PA testing)
- Board ...6B:F6: Funcional (reemplazo de C1:82)

### Conexiones RP2040 <-> CC1352 (UART 921600, sin flow control)
| Signal | RP2040 | CC1352 |
|--------|--------|--------|
| TX | GPIO0 | DIO12 |
| RX | GPIO1 | DIO13 |

### Restricciones
- Memoria: Solo allocacion estatica (no malloc) en CC1352
- RX Buffer: 16KB circular
- TX Power: -20 a +20 dBm (High PA necesita DIO29, no configurado)
- SDK: TI SimpleLink CC13xx/CC26xx 7.10.01.24 (fijo)

---

## PHYs del CC1352

| ID | PHY | RF Backend | Estado |
|----|-----|-----------|--------|
| 0 | BLE 1M | BLE5 SmartRF | OK |
| 1 | BLE 2M | BLE5 SmartRF (override existe) | Fase 1a |
| 2 | BLE Coded S8 | BLE5 SmartRF (override existe) | Fase 1a |
| 3 | BLE Coded S2 | BLE5 SmartRF (override existe) | Fase 1a |
| 4 | IEEE 802.15.4 | IEEE SmartRF | OK |
| 5 | Sub-1GHz 868 | CMD_PROP (nuevo) | Fase 1b |
| 6 | Sub-1GHz 915 | CMD_PROP (nuevo) | Fase 1c |
| 7 | Proprietary GFSK | CMD_PROP (nuevo) | Fase 1d |
