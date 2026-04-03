# FeralRF - Plan Maestro

**Version:** 5.0 | **Fecha:** 2026-04-03

Firmware universal para CatSniffer (CC1352P + RP2040). Objetivo: API Python facil de usar para pentesting RF con todos los protocolos del CC1352.

---

## Estado Actual

### Funcionando (validado OTA con marcadores entre 2 boards)

| Componente | Estado |
|-----------|--------|
| COBS + CRC16 (921600 baud) | OK |
| 8/8 PHYs (BLE 1M/2M/Coded S8/S2, IEEE, Sub-1GHz 868/915, GFSK) | OK |
| TX_RAW, TX_FRAME, TX_BURST, TX_CONTINUOUS + TX_STOP | OK |
| RX con metricas (rx_ok, crc_err, drop, overflow) | OK |
| configure_prop() — freq/mod/rate/deviation/sync configurable en runtime | OK |
| OOK RX+TX con patches genook (433/868 MHz, 10/10 markers) | OK |
| 15 presets validados OTA (GFSK/FSK/OOK en 433/868/915/2440 MHz) | OK |
| Band-specific overrides (433 MHz, 868 MHz, 169 MHz) | OK |
| reset_device() — power cycle CC1352 via RP2040 shell | OK |
| Estabilidad: soak 5min 213 ciclos, 0 errors, 0 timeouts | OK |
| Python API completa + PROP_PRESETS | OK |
| RP2040 USB-CDC bridge | OK |
| Firmware 55KB, todos los modos coexisten | OK |

### Limitaciones conocidas
| Limitacion | Solucion |
|------------|----------|
| OOK bloquea radio (TI SDK bug) | `reset_device()` — power cycle automatico ~2s |
| Jamming no interfiere senales | Pendiente Fase 6 |

### Bandas no funcionales (investigadas, no viables sin SmartRF Studio)
| Banda | Razon |
|-------|-------|
| 169 MHz | Config SDK existe pero falla — antena no optimizada |
| 315 MHz | Sin config ni ejemplos en SDK, solo spec en datasheet |
| 390 MHz | Sin config ni ejemplos en SDK, solo spec en datasheet |
| 470 MHz | En rango SDK pero sin ejemplo/config validado |

---

## Arquitectura

```
HOST (Python API) <-> RP2040 (USB Bridge) <-> CC1352P (Radio Engine)
```

- **CC1352P**: Radio operations, COBS protocol, command processing, TI-RTOS 7
- **RP2040**: USB-CDC bridge, timestamping, CC1352 reset monitoring
- **Python API**: `feralrf` package, sync interface, pyserial

### Radio IF internals
- `radio_if.c`: RF abstraction, enum RadioIF_RfMode (NONE=0, BLE=1, IEEE=2, SUB_1GHZ=3)
- `phy_manager.c`: Tabla de 8 PHYs, todos con rf_backend_rx_supported=true
- SmartRF configs: `smartrf_ble5_0.c` (BLE5), `smartrf_ieee_15_4_0.c` (IEEE), `smartrf_prop_0.c` (Sub-1GHz/OOK)
- OOK: RF_Mode dedicado con patches mce_genook + rfe_genook
- Band overrides: 433 MHz (AGC=0x20, RSSI=-8dB), 169 MHz (IIR/PLL), 868+ MHz (default)

---

## Protocolo

COBS-framed binary protocol con CRC16-CCITT.

```
Frame: [CMD_ID(1B)][SEQ(1B)][LEN(2B LE)][PAYLOAD(0-255B)][CRC16(2B LE)]
       └──────────── COBS encoded, 0x00 delimited ──────────┘
```

### Command IDs
```
Config:    RADIO_INIT(0x01) SET_CHANNEL(0x02) SET_POWER(0x03) SET_PHY(0x04)
           GET_INFO(0x05) GET_STATS(0x06) SET_ADV_HOP(0x07) SET_PROP_CONFIG(0x08)
RX:        RX_START(0x10) RX_STOP(0x11)
TX:        TX_RAW(0x20) TX_CONTINUOUS(0x21) TX_BURST(0x22) TX_FRAME(0x23) TX_STOP(0x24)
Jam:       JAM_CONTINUOUS(0x30) JAM_REACTIVE(0x31) JAM_PATTERN(0x32) JAM_STOP(0x33)
Spectrum:  SPECTRUM_SCAN(0x40) SPECTRUM_MONITOR(0x41) SPECTRUM_STOP(0x42)
Response:  ACK(0x80) ERROR(0x81) RX_PACKET(0x90) SPECTRUM_DATA(0x91) JAM_EVENT(0x95)
```

### SET_PROP_CONFIG payload (16 bytes)
```
freq_hz(4B LE) | mod_type(1B) | symbol_rate(4B LE) | deviation(2B LE) | rx_bw(1B) | sync_word(4B LE)
```

---

## Fases de Desarrollo

### FASE 1: Habilitar todos los PHYs (RX + TX) — COMPLETADA ✅

8/8 PHYs validados OTA con marcadores entre 2 boards.

- BLE 2M/Coded: `RadioIF_applyBlePhyMode()`, TX usa CMD_BLE5_ADV_NC para 2M/Coded
- Sub-1GHz: `smartrf_prop_0.c/h`, CMD_PROP_RADIO_DIV_SETUP_PA, power table Sub-1GHz
- GFSK: Reutiliza prop backend con loDivider dinamico

---

### FASE 2: Radio Propietaria Configurable — COMPLETADA ✅

15 presets validados OTA (13 GFSK/FSK + 2 OOK).

#### Implementado
- CMD_SET_PROP_CONFIG (0x08): configura freq, mod, rate, deviation, rx_bw, sync_word en runtime
- OOK RX+TX con patches genook (10/10 markers en 433 y 868 MHz)
- Band-specific overrides auto-seleccionados por frecuencia
- Python: `radio.configure_prop()` + `PROP_PRESETS` dictionary
- `radio.reset_device()` — power cycle CC1352 via RP2040 para recovery post-OOK

#### Uso
```python
from feralrf import Radio, PHY, PROP_PRESETS

# GFSK/FSK — cambio libre entre modos
radio.set_phy(PHY.PROPRIETARY_GFSK)
radio.configure_prop(**PROP_PRESETS['gfsk_868_50k'])
radio.start_rx()

# OOK — bloquea radio, reset_device() para cambiar
radio.configure_prop(**PROP_PRESETS['ook_433_4k8'])
radio.start_rx()  # captura OOK
radio.stop_rx()
radio.reset_device()  # power cycle, listo para otro modo
radio.set_phy(PHY.BLE_1M)  # funciona
```

#### Bandas funcionales
| Banda | Frecuencias | Modulaciones |
|-------|------------|-------------|
| 433 MHz ISM | 433.92 MHz | GFSK, FSK, OOK |
| 868 MHz ISM (EU) | 868.0, 868.3 MHz | GFSK, OOK |
| 915 MHz ISM (US) | 902.2, 915.0 MHz | GFSK |
| 2.4 GHz Prop | 2440 MHz | GFSK |

---

### FASE 2.5: Estabilidad de Comunicacion — COMPLETADA ✅

#### Validado
- PHY switching: todas las combinaciones PASS (BLE↔IEEE↔Sub-1GHz↔GFSK)
- 15 presets: todos PASS sin timeouts
- OOK→cualquier modo: PASS via reset_device()
- State transitions: 9/9 PASS (RX→TX, TX→RX, rapid cycling)
- Soak 5min: 213 ciclos random PHY, 0 errors, 0 timeouts, 89.7% delivery

#### Fixes aplicados
- RF session cleanup en init()/setPhy() — stopRfBackend + closeTxSession
- OOK session lock — skip RF_close cuando genook patches activos
- reset_device() — boot→exit via RP2040 shell para power cycle CC1352
- RF_flushCmd antes de RF_close en closeTxSession

---

### FASE 3: Attack Commands — EN PROGRESO

**Objetivo:** Metodos Python de alto nivel para ataques RF. Puro Python sobre TX/RX existente, cero firmware.

```
python/feralrf/attacks/
    __init__.py
    ble.py          # beacon_flood(), apple_popup_spam(), adv_spoof(), replay()
    ieee154.py      # beacon_inject(), disassociate(), replay(), pan_conflict()
    sub1ghz.py      # ook_replay(), ook_capture(), debruijn_brute(), encode_ev1527()
    recon.py        # channel_survey(), ble_scan(), generic_replay()
```

#### 3a. BLE Attacks
| Ataque | Descripcion | Dificultad | FW change |
|--------|------------|------------|-----------|
| beacon_flood | ADV_NONCONN_IND spam con nombres falsos | Facil | No |
| apple_popup_spam | Manufacturer data Apple/Google Fast Pair | Facil | No |
| adv_spoof | Clonar advertising de un dispositivo | Medio | MAC configurable (futuro) |
| adv_channel_jam | Saturar ch 37/38/39 | Facil | No |

#### 3b. IEEE 802.15.4 / Zigbee Attacks
| Ataque | Descripcion | Dificultad | FW change |
|--------|------------|------------|-----------|
| disassociate | MAC disassociation notification (sin keys) | Facil | No |
| beacon_inject | Beacon con PAN falso | Medio | No |
| replay | Captura + retransmision | Facil | No |
| pan_conflict | Beacons con mismo PAN ID del target | Medio | No |

#### 3c. Sub-1GHz / OOK Attacks
| Ataque | Descripcion | Dificultad | FW change |
|--------|------------|------------|-----------|
| ook_capture | Capturar senal OOK raw | Facil | No |
| ook_replay | Retransmitir senal capturada | Facil | No |
| debruijn_brute | Fuerza bruta De Bruijn para codigos fijos | Medio | No |
| encode_ev1527 | Codificar paquetes EV1527/PT2262 | Medio | No |

#### 3d. Reconocimiento
| Ataque | Descripcion | Dificultad | FW change |
|--------|------------|------------|-----------|
| zigbee_channel_survey | Barrer ch 11-26 contando paquetes | Facil | No |
| generic_replay | Captura+replay universal para cualquier PHY | Facil | No |

#### Demos planeados
| Demo | Target | Costo | Impacto |
|------|--------|-------|---------|
| Apple popup spam | iPhone cerca | $0 | MUY ALTO |
| BLE beacon flood | Telefono con nRF Connect | $0 | ALTO |
| OOK replay timbre | Timbre 433 MHz (~$5) | $5 | MUY ALTO |
| Zigbee disassociation | SONOFF plug (~$10) | $10 | MEDIO |

#### Orden de implementacion
1. BLE beacon flood + Apple popup (validar API BLE TX)
2. OOK capture + replay (validar OOK workflow)
3. IEEE disassociation + replay (validar IEEE TX)
4. Recon (channel survey)
5. De Bruijn brute force

#### Mejora firmware futura
- `CMD_SET_BLE_ADDR` — MAC address configurable para randomizar en flood
- PDU type seleccionable (ADV_IND, ADV_SCAN_IND)

---

### FASE 4: Spectrum Analyzer — PENDIENTE

**Objetivo:** Detector de actividad RF por frecuencia. No es SA real, es RSSI scan.

- CMD_GET_RSSI simple en firmware + Python sweep loop
- Cubre: 433, 868, 915 MHz y 2.4 GHz
- Complementa SX1262 spectrum (que solo hace LoRa)

---

### FASE 5: Emulacion de Targets — PENDIENTE

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

### FASE 6: Jamming — PENDIENTE

**Objetivo:** Interferencia RF funcional en todas las bandas.

- Jamming en cualquier frecuencia/banda de Fase 2
- Debuggear CMD_TX_TEST (modo propietario 2.4 GHz que fallo anteriormente)
- Alternativa: CMD_PROP_TX con payload largo y bFsOff=0
- Reactive jamming (<500us): ISR en sync word detection
- Pattern jamming: timer-based on/off

---

## Hardware

### Boards disponibles
- Board ...82:2E: Funcional (TX y RX)
- Board ...C1:82: Funcional (anteriormente pensada muerta, solo necesitaba firmware nuevo)
- Board ...82:3C: Funcional
- Board ...6B:F6: Degradada (~5 dB menos TX power)

### Conexiones RP2040 <-> CC1352 (UART 921600, sin flow control)
| Signal | RP2040 | CC1352 |
|--------|--------|--------|
| TX | GPIO0 | DIO12 |
| RX | GPIO1 | DIO13 |

### Restricciones
- Memoria: Solo allocacion estatica (no malloc) en CC1352
- RX Buffer: 16KB circular
- TX Power: -20 a +14 dBm (High PA +15-20 dBm necesita DIO29, no configurado)
- SDK: TI SimpleLink CC13xx/CC26xx 7.10.01.24 (fijo)
- Antena CatSniffer: Optimizada para 868 MHz y 2.4 GHz. 433 MHz funciona con perdidas. <430 MHz no funcional.

---

## PHYs del CC1352

| ID | PHY | RF Backend | Estado |
|----|-----|-----------|--------|
| 0 | BLE 1M | BLE5 SmartRF | ✅ OK |
| 1 | BLE 2M | BLE5 SmartRF | ✅ OK |
| 2 | BLE Coded S8 | BLE5 SmartRF | ✅ OK |
| 3 | BLE Coded S2 | BLE5 SmartRF | ✅ OK |
| 4 | IEEE 802.15.4 | IEEE SmartRF | ✅ OK |
| 5 | Sub-1GHz 868 | CMD_PROP SmartRF | ✅ OK |
| 6 | Sub-1GHz 915 | CMD_PROP SmartRF | ✅ OK |
| 7 | Proprietary GFSK | CMD_PROP configurable | ✅ OK |
