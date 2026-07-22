# FeralRF Validation Matrix

Baseline de validacion del firmware FeralRF sobre CC1352P7 (CatSniffer). Consolida
la corrida OTA de hardware del 2026-04-08 (PHYs base + presets Sub-1GHz) mas las
validaciones posteriores por fase (modos de test TX, crypto, emulacion, presets
Wi-SUN/Sidewalk).

Scope:

- El bridge USB del RP2040 corre el firmware stock del CatSniffer (fuera de este repo).
- El protocolo/GATT BLE fue removido 2026-07-20 (Sniffle lo maneja). Queda
  **captura BLE-PHY cruda** (1M / 2M / Coded S8 / Coded S2): `set_phy` + RX/TX raw,
  sin scan mode, sin conexion, sin GATT.

Leyenda de estado:

- **PASS** - validado (control path y/o OTA) en hardware.
- **marginal** - pasa pero con margen bajo (antena / link budget).
- **FAIL** - no funciona en este hardware.
- **experimental** - presente en firmware/API pero sin validacion de uso real.

## 1. Baseline de comandos

Cubierto por el baseline oficial:

- Sesion / transporte: `RADIO_INIT`, `GET_INFO`, `GET_STATS`.
- Config: `SET_PHY`, `SET_CHANNEL`, `SET_POWER`, `SET_ADV_HOP` (RX BLE-PHY raw).
- RX: `RX_START`, `read_packets()`, `RX_STOP`.
- TX: `TX_RAW`, `TX_FRAME`, `TX_BURST`, `TX_CONTINUOUS`, `TX_STOP`.
- Modos de test TX: `TX_CW`, `TX_PRBS`, `TX_TEST_STOP`.
- Config propietaria: `SET_PROP_CONFIG`, `PROP_PRESETS`.
- Crypto: `CMD_RANDOM`, `CMD_AES_ECB/CTR/CBC/CCM/GCM`, `CMD_SHA256`, `CMD_ECDH`,
  `CMD_ECDSA_SIGN`, `CMD_ECDSA_VERIFY`.
- Recovery: `reset_device()`.

Experimental (presente, sin validacion de uso real): `JAM_CONTINUOUS`, `JAM_STOP`.

Fuera del baseline (no implementado o no validado):

- Spectrum / RSSI scan (dataclasses sin data path).
- Jamming con criterio real de interferencia; jamming reactivo / por patron.
- Protocolo / GATT / scan mode BLE (removido; Sniffle).
- IEEE 802.15.4g Sub-GHz MAC, AIS 162 MHz, High-PA >+14 dBm, RSA.

## 2. Baseline OTA de hardware (ultima corrida completa 2026-04-08)

18/18 control path PASS con `run_validation_baseline.sh`. Se resetea CC1352 via
RP2040 shell entre cada step. Los paths de PHY/prop no cambiaron con la remocion
del stack BLE, por lo que estos resultados siguen vigentes.

### OTA (2-board TX/RX con DEADBEEF markers, `smoke_ota_txrx.py`)

| PHY / preset | Markers | Estado | Notas |
|--------------|---------|--------|-------|
| BLE 1M (raw) | 10/10 | **PASS** | Reset wait 3.5s |
| BLE 2M (raw) | 8/10 | **PASS** | Extended ADV: ADV_EXT(1M,ch37)->ADV_AUX(2M,ch9). Requiere bt5 patch |
| BLE Coded S8 (raw) | 10/10 | **PASS** | |
| BLE Coded S2 (raw) | 10/10 | **PASS** | |
| IEEE 802.15.4 | 10/10 | **PASS** | |
| GFSK 868 | 10/10 | **PASS** | |
| GFSK 915 | 10/10 | **PASS** | |
| GFSK 2440 | 10/10 | **PASS** | |
| GFSK 433 | 6-10/10 | **marginal** | Depende de posicion de antena |
| FSK 433 | 1/10 | **marginal** | |
| MSK 868 | 10/10 | **PASS** | |
| MSK 433 | 1/10 | **marginal** | |
| W-MBus S 868 | 10/10 | **PASS** | |
| W-MBus T 868 | 10/10 | **PASS** | |
| W-MBus C 868 | 10/10 | **PASS** | |
| OOK 868 | 10/10 | **PASS** | Con auto-reset recovery |
| OOK 433 | 0/10 | **FAIL** | Hardware: antena CatSniffer + OOK baja sensibilidad |

## 3. API publica (superficie actual de `Radio`)

### Estable

- `init()`, `connect()`, `disconnect()` (ciclo de vida del puerto serial)
- `set_phy()`, `set_channel()`, `set_power()`, `set_adv_hop()`
- `start_rx()`, `read_packets()`, `read_one_packet()`, `stop_rx()`, `get_stats()`
- `transmit()`, `transmit_frame()`, `transmit_burst()`, `transmit_continuous()`, `stop_transmit()`
- `configure_prop()`
- `tx_cw()`, `tx_prbs()`, `tx_test_stop()`
- `random_bytes()`, `aes_encrypt/decrypt()`, `aes_ccm_*()`, `aes_gcm_*()`, `sha256()`,
  `ecdh()`, `ecdsa_sign()`, `ecdsa_verify()`
- `reset_device()` (requerido despues de OOK y entre cambios de banda)

### Experimental

- `start_jam()`, `stop_jam()` (ACK pero sin interferencia validada)

### Pendiente

- Spectrum scan / RSSI, jamming reactivo, RSA, AIS 162 MHz, IEEE 802.15.4g Sub-GHz.

## 4. Matriz por PHY (control + RX/TX)

| ID | PHY | Test | Estado | Script |
|----|-----|------|--------|--------|
| 0 | BLE 1M (raw) | Control + RX + TX raw/frame | **PASS** | `smoke_phase2.py --phy 0`, `smoke_tx_frame_phase1.py --phy 0` |
| 1 | BLE 2M (raw) | Control + RX | **PASS** | `smoke_phase2.py --phy 1` |
| 2 | BLE Coded S8 (raw) | Control + RX | **PASS** | `smoke_phase2.py --phy 2` |
| 3 | BLE Coded S2 (raw) | Control + RX | **PASS** | `smoke_phase2.py --phy 3` |
| 4 | IEEE 802.15.4 | RX + TX raw/frame/burst/continuous | **PASS** | `smoke_phy4_ieee154.py`, `smoke_tx_*.py --phy 4` |
| 5 | Sub-1GHz 868 | Config + RX/TX | **PASS** | `smoke_prop_phase1.py --preset gfsk_868_50k` |
| 6 | Sub-1GHz 915 | Config + RX/TX | **PASS** | `smoke_prop_phase1.py --preset gfsk_915_50k` |
| 7 | Prop GFSK 2.4G | Config + RX/TX | **PASS** | `smoke_prop_phase1.py --preset gfsk_2440_50k` |

## 5. Matriz por modulacion propietaria

| Modulacion | Banda | Control | OTA | Preset |
|-----------|-------|---------|-----|--------|
| GFSK | 433 MHz | **PASS** | 6-10/10 (marginal) | `gfsk_433_50k` |
| GFSK | 868 MHz | **PASS** | 10/10 | `gfsk_868_50k` |
| GFSK | 915 MHz | **PASS** | 10/10 | `gfsk_915_50k` |
| GFSK | 2440 MHz | **PASS** | 10/10 | `gfsk_2440_50k` |
| FSK | 433 MHz | **PASS** | 1/10 (marginal) | `fsk_433_50k` |
| MSK | 868 MHz | **PASS** | 10/10 | `msk_868_50k` |
| MSK | 433 MHz | **PASS** | 1/10 (marginal) | `msk_433_50k` |
| 4-FSK / 4-GFSK | 868 MHz | **PASS** | 10/10 | `4fsk_868_50k`, `4gfsk_868_50k` |
| OOK | 868 MHz | **PASS** | 10/10 | `ook_868_4k8` (con `--auto-reset`) |
| OOK | 433 MHz | **PASS** (ctrl) | 0/10 (hw) | `ook_433_4k8` |
| W-MBus S/T/C | 868 MHz | **PASS** | 10/10 | `wireless_mbus_{s,t,c}_868` |
| W-MBus N | 169 MHz | preset | no probado | `wireless_mbus_n_169_2k4` |

Notas:

- OOK bloquea el radio; requiere `reset_device()` o power cycle despues.
- 433 MHz OTA es marginal en CatSniffer (antena optimizada para 868/915).
- OOK 433 OTA no funciona por link budget insuficiente (OOK -10 dB sensibilidad +
  antena -7 dB).

## 6. Modos de test TX (F22, smoke 2026-04-29)

| Modo | Estado | Notas |
|------|--------|-------|
| CW (portadora) | **PASS** (wire-level) | `tx_cw(power_dbm)` |
| PRBS-15 / PRBS-32 | **PASS** (wire-level) | `tx_prbs(pattern="prbs15"|"prbs32")`. No es BLE DTM PRBS-9 |
| Stop | **PASS** | `tx_test_stop()`, idempotente |

## 7. Crypto (F25, 9/9 hardware smoke 2026-04-30)

| Primitiva | Estado | Notas |
|-----------|--------|-------|
| TRNG | **PASS** | `random_bytes(n)`, 1-240 B |
| AES ECB/CTR/CBC/CCM/GCM | **PASS** | `aes_*`, GCM devuelve (ciphertext, tag) |
| SHA-256 | **PASS** | `sha256(data)`, <=240 B one-shot |
| ECDH | **PASS** | P-256 y Curve25519 |
| ECDSA sign/verify | **PASS** (P-256) | Curve25519 lo rechaza el firmware (err 0x05) |

## 8. Emulacion de dispositivos (F17, smoke V1 7/7 wire-level 2026-05-04)

Firmas PHY-level por burst; no es un emulador de stack (sin auth/framing/encryption).

| Grupo | Personalidades | Estado |
|-------|----------------|--------|
| IEEE154 | BEACON_COORDINATOR, DATA_POLL_END_DEVICE | **PASS** (20/20) |
| Sub-1GHz 868 | GFSK_868_SENSOR, WMBUS_T1_METER | **PASS** (20/20) |
| 433 / OOK | GFSK_433, PT2262/EV1527/Hormann | **experimental** (diferido: antena 433 / lock-up OOK) |

## 9. Presets Sub-1GHz de protocolo (F29, OTA 70/70 sobre 7 presets 2026-05-03)

| Preset | Estado | Notas |
|--------|--------|-------|
| `sidewalk_915_fsk_50k` / `_250k` | **PASS** (OTA F29) | Capa FSK de Sidewalk; LR (LoRa) fuera del chip (SX1262) |
| `wisun_915_fsk_50k/100k/150k/200k/300k` | **PASS** (OTA F29) | Wi-SUN FAN 1.0 NA-1, capa FSK |
| `mioty_868_tsunb` | **FAIL** | TS-UNB 396 baud no soportado nativamente sin CPE patch custom |

## 10. Matriz de switching

| Secuencia | Estado | Notas |
|-----------|--------|-------|
| BLE 1M -> IEEE -> BLE 1M | **PASS** | Con reset entre cambios |
| BLE 1M -> Sub-1GHz 868 -> BLE 1M | **PASS** | Con reset |
| BLE 1M -> GFSK 433 -> BLE 1M | **PASS** | Con reset |
| OOK -> reset_device() -> BLE 1M | **PASS** | |
| PHY switch sin reset | FAIL | RF_close deadlock en 2do ciclo (por eso se resetea entre cambios) |

## 11. Protocolo x modulacion (superficie del chip)

| | 2-GFSK | 2-FSK | OOK/ASK | MSK | 4-FSK | 4-GFSK |
|---|---|---|---|---|---|---|
| **BLE (PHY raw)** | si (nativo) | n/a | n/a | n/a | n/a | n/a |
| **IEEE 802.15.4** | si (O-QPSK nativo) | n/a | n/a | n/a | n/a | n/a |
| **Zigbee / Thread / 6LoWPAN / Matter** | parcial (PHY 15.4; stack en Python) | n/a | n/a | n/a | n/a | n/a |
| **Prop Sub-1G 868** | si | si | si | si | si (10/10) | si (10/10) |
| **Prop Sub-1G 915** | si | parcial | parcial | parcial | falta | falta |
| **Prop Sub-1G 433** | si (marginal) | si | ctrl (hw lim) | si | falta | falta |
| **Prop 2.4 GHz** | parcial (CW ok, GFSK pendiente) | parcial | n/a | parcial | falta | falta |
| **W-MBus S/T/C** | si (868) | n/a | n/a | n/a | n/a | n/a |
| **W-MBus N** | preset (169) | n/a | n/a | n/a | n/a | n/a |
| **Wi-SUN** | preset FSK (OTA F29) | si | n/a | n/a | falta | n/a |
| **Amazon Sidewalk** | preset FSK (OTA F29) | si | n/a | n/a | n/a | n/a |
| **MIOTY** | falla (TS-UNB) | n/a | n/a | n/a | n/a | n/a |

Leyenda:

- **si**: validado OTA o control path PASS.
- **parcial**: PHY funciona pero sin stack/parser, o sin test dedicado en esa banda.
- **ctrl**: control path PASS pero OTA falla por limitacion hardware.
- **preset**: preset Python existe y validado a nivel FSK (sin stack de protocolo).
- **falta**: el chip lo soporta pero no esta implementado/validado.
- **falla**: intentado, no funciona en este chip/hardware.
- **n/a**: combinacion no valida fisicamente.

## 12. Checklist ejecutable

```bash
# Control path (1 board, reset entre cada step)
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3

# Control path + OTA markers (2 boards)
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --rx-port /dev/ttyACM0

# Subsets
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only IEEE
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only 433
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only OOK
bash python/examples/run_validation_baseline.sh --port /dev/ttyACM3 --only W-MBus
```

## 13. Notas tecnicas

### BLE 2M Extended Advertising

BLE spec prohibe 2M PHY en canales primarios de advertising (37/38/39). El TX raw de
BLE 2M usa cadena extendida: `CMD_BLE5_ADV_EXT` (1M, ch37) -> `CMD_BLE5_ADV_AUX`
(2M, ch9). Requiere `rf_patch_cpe_bt5 + rf_patch_mce_bt5` (multi_protocol no soporta
`CMD_BLE5_ADV_AUX`), `RF_runCmd` para la cadena, `COND_ALWAYS` en ADV_EXT.

### Hardware

- Device #2 tiene TX debil a 868 MHz (issue de antena/SMA, no firmware): usar
  device #1 como TX en OTA a 868/Sub-1GHz.
- High-PA (+15..+20 dBm) no enruta por DIO29; el rango util es -20..+14 dBm (std PA).

## 14. Pendientes

- ASK como modo separado de OOK (mismo `mod_type=2`, sin test dedicado).
- 4-FSK / 4-GFSK 433 MHz OTA (hardware-limited).
- W-MBus N-mode OTA a 169 MHz.
- Jamming con criterio real de interferencia; jamming reactivo; spectrum / RSSI scan.
- RSA; AIS 162 MHz; IEEE 802.15.4g Sub-GHz; High-PA (fix DIO29).
- MIOTY TS-UNB (requiere CPE patch custom).
- Re-correr el baseline OTA completo contra el firmware actual (la ultima corrida
  full fue 2026-04-08; los paths PHY/prop no cambiaron pero conviene re-confirmar).
