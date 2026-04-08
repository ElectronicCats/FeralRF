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

### FASE 0-RTOS: Reinicio Firmware TI-RTOS — 5/6 PHYs FUNCIONANDO

**Objetivo**: Reconstruir el firmware CC1352 con TI-RTOS para habilitar GATT discovery.

**Rama**: `feature/ti-rtos-migration` | **SDK**: 8.30.01.01 (install completo en ~/ti/)

**Arquitectura actual (commit f961d4a)**:
- Single RF_Object, RF_yield+RF_close+RF_open para PHY switch (patron rfDiagnostics)
- Two tasks: UART (prio 3) + RF (prio 3) con Task_yield cooperativo
- Precompiled SDK libs (driverlib.lib, sysbios.a, drivers_cc13x2x7.a)
- SysConfig-generated configs (ti_sysbios_config, ti_drivers_config)
- All RF_Modes usan rf_patch_cpe_multi_protocol
- 72KB firmware

**Reglas RF (actualizadas 2026-04-05)**:
| Regla | Patron |
|-------|--------|
| Single RF_Object + single RF_Handle | rfDiagnostics pattern |
| PHY switch: RF_flush + RF_yield + RF_close + RF_open | Funciona con precompiled libs |
| RF_runCmd para CMD_FS | OK en TI-RTOS con precompiled libs |
| RF_postCmd para RX continuo | Con callback, event_mask = RxEntryDone |
| Precompiled SDK libs OBLIGATORIAS | Source-compiled libs causan RF_postCmd IDLE |
| GPIO_init() requerido | Para antenna switching via SysConfig callback |
| endTrigger=TRIG_NEVER para RX | TRIG_NOW termina inmediatamente |
| TRIG_REL_START+10ms para TX ADV | TRIG_NEVER en ADV cuelga |

**Sub-fases**:
| Fase | Objetivo | Estado |
|------|----------|--------|
| 0.0 | LED + UART (skeleton TI-RTOS) | ✅ PASS |
| 0.1 | BLE RX | ✅ PASS (17+ pkts ambiental) |
| 0.2 | BLE TX/RX OTA | ✅ PASS (20/20 sent, 36 rx) |
| 0.3 | IEEE 802.15.4 TX/RX OTA | ✅ PASS (5/5) |
| 0.4 | Sub-1GHz 868 TX/RX OTA | ✅ PASS (5/5) |
| 0.4b | PHY switching completo | ⚠️ 5/6 (868→BLE falla) |
| 0.5 | Re-integrar attacks + scanner | PENDIENTE |
| 0.6 | GATT discovery | PENDIENTE |

**Issue pendiente**: 868→BLE switch falla (rx=0). Todas las demas transiciones funcionan.
BLE→IEEE→868→IEEE→868 = todo PASS. Solo la vuelta a BLE desde 868 falla.

---

### FASE 3: BLE Security Testing — COMPLETADO ✅ (en rama main, NoRTOS)

#### 3a. BLE Attacks (Python) — COMPLETADO ✅
| Ataque | Estado | Validado |
|--------|--------|----------|
| beacon_flood() | ✅ | Visible en nRF Connect |
| apple_popup_spam() | ✅ | Payload sin Flags (fix aplicado) |
| google_popup_spam() | ✅ | Payload sin Flags (fix aplicado) |
| adv_spoof() | ✅ | MAC configurable via CMD_SET_BLE_ADDR |
| capture_and_replay() | ✅ | Captura + retransmision |
| Fast Pair popup | ✅ | Soundcore Boom 2 Model ID 0x8F95F8 validado en telefono real |
| Device emulation | ✅ | Estable >60s en nRF Connect |

#### 3b. BLE Analysis Tools — COMPLETADO ✅
| Herramienta | Estado |
|-------------|--------|
| python/examples/lab/demo_ble_analyzer.py | ✅ Analisis completo + export JSON |
| python/examples/lab/demo_ble_clone.py | ✅ 6 estrategias interactivas |
| python/examples/lab/demo_emulate_soundcore.py | ✅ Popup Fast Pair funcional |

#### 3c. BLE Scanner (firmware) — PENDIENTE
- CMD_BLE5_SCANNER disponible en SDK pero no implementado
- Habilitaria SCAN_REQ automatico → captura SCAN_RSP
- Obtendria: nombre completo, UUIDs extra, mas manufacturer data
- Esfuerzo: medio (1-2 dias firmware)

#### 3d. BLE GATT Discovery — FUTURO
- CMD_BLE5_INITIATOR → conexion → GATT service discovery
- Obtendria: servicios, characteristics, firmware version, battery
- Esfuerzo: alto (necesita stack BLE LL + ATT + GATT)

---

### FASE 4: IEEE 802.15.4 Security Testing — PENDIENTE

**Objetivo:** Ataques completos Zigbee/Thread a nivel MAC (sin encryption keys).

#### 4a. Attack Module (`attacks/ieee154.py`) — Python puro
| Ataque | Descripcion | Notas |
|--------|------------|-------|
| disassociate() | MAC disassociation notification | Funciona sin keys (MAC layer) |
| beacon_inject() | Beacon con PAN falso | Atrae devices a red rogue |
| replay() | Captura + retransmision | Para comandos sin encryption |
| pan_conflict() | Beacons con mismo PAN ID | Fuerza PAN ID resolution |
| channel_survey() | Barrer ch 11-26 con conteo | Reconocimiento |

#### 4b. IEEE CRC Control — Firmware simple
- Cambiar `txOpt.bIncludeCrc = 1` para enviar frames con CRC custom/invalido
- Esfuerzo: 1 linea de firmware

#### 4c. Tools
- demo_ieee154_survey.py — survey interactivo de canales Zigbee
- demo_ieee154_attack.py — disassociation interactivo

---

### FASE 5: Sub-1GHz / OOK Security Testing — PENDIENTE

**Objetivo:** Ataques para dispositivos 433/868 MHz (garajes, sensores, timbres).

#### 5a. Attack Module (`attacks/sub1ghz.py`) — Python puro
| Ataque | Descripcion | Notas |
|--------|------------|-------|
| ook_capture() | Capturar senal OOK raw | Requiere reset_device() despues |
| ook_replay() | Retransmitir senal capturada | Replay basico |
| encode_ev1527() | Codificar paquete EV1527 | Fixed code encoder |
| decode_ev1527() | Decodificar paquete EV1527 | Analisis de captura |
| encode_pt2262() | Codificar paquete PT2262 | Tri-state encoder |
| debruijn_brute() | Fuerza bruta De Bruijn | 2^12 codigos en ~2s |
| freq_scan() | Barrer frecuencias con RSSI proxy | Reconocimiento |

#### 5b. Tools
- demo_ook_replay.py — captura interactiva → replay
- demo_ook_bruteforce.py — De Bruijn interactivo

---

### FASE 6: Spectrum / Reconocimiento — PENDIENTE

**Objetivo:** Detector de actividad RF por frecuencia.

- Firmware: CMD_GET_RSSI (0x40) — mide RSSI en cualquier frecuencia
- Python: radio.get_rssi(freq) + radio.frequency_scan(start, end, step)
- demo_spectrum.py — visualizacion de actividad
- Cubre: 433, 868, 915 MHz y 2.4 GHz

---

### FASE 7: Emulacion de Targets — PENDIENTE

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
- SDK: TI SimpleLink CC13xx/CC26xx 8.30.01.01 (rama TI-RTOS) / 7.10.01.24 (rama main NoRTOS)
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
