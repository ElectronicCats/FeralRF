# FeralRF — Architecture

> **Authoritative source for the layered design.**

---

## 1. Vista global

```
┌──────────────────────────────────────────────────────────────┐
│  HOST (Python) — feralrf package                             │
│  L5 apps     examples/, examples/lab/                        │
│  L4 features _jamming.py, _spectrum.py, emul/...             │
│  L3 core     radio.py (Radio class), presets.py, enums       │
│  L2 cmds     commands.py, _responses.py                      │
│  L1 xport    protocol.py (COBS + CRC16), pyserial            │
└──────────────────┬───────────────────────────────────────────┘
                   │ USB-CDC 921600 baud
┌──────────────────▼───────────────────────────────────────────┐
│  RP2040 bridge (transparente, firmware stock del CatSniffer) │
└──────────────────┬───────────────────────────────────────────┘
                   │ UART 921600 (GPIO0/1 ↔ DIO12/13)
┌──────────────────▼───────────────────────────────────────────┐
│  CC1352P7 firmware — FeralRF v2.0 (TI-RTOS 7)                │
│  L5 app      main_rtos.c, task scheduling                    │
│  L4 services radio/, rx/data (ll_manager), protocol/, queues │
│  L3 drivers  host_if, RF, GPIO (delgados sobre SDK)          │
│  L2 SDK      TI SimpleLink CC13xx/CC26xx 8.30.01.01          │
│  L1 platform startup, ccfg, linker, RTOS stubs               │
└──────────────────────────────────────────────────────────────┘
```

No existe capa HAL formal. Los services llaman directamente al TI SDK 8.30.

---

## 2. Mapeo firmware (`firmware/cc1352/src/`)

| Capa | Propósito | Archivos |
|------|-----------|----------|
| L1 platform | Arranque, linker, glue RTOS | `main.c`, `main_rtos.c`, `rtos_stubs.c`, `startup_cc13x2_cc26x2_gcc.c`, `ccfg.c` |
| L3 drivers | IF físicas (UART, RF, GPIO) | `host_if.c`, `host_if_task.c`, `output_if.c`, `ti_rf_config_min.c` |
| L4 radio | PHY abstraction + SmartRF configs | `radio_if.c`, `phy_manager.c`, `smartrf_ble5_0.c`, `smartrf_ieee_15_4_0.c`, `smartrf_prop_0.c` |
| L4 rx/data | TX + RX packet queues; clasificador de PDU RX | `tx_queue.c`, `packet_queue.c`, `ll_manager.c` |
| L4 protocol | COBS + commands | `protocol.c`, `command_processor.c` |
| L5 app | Tasks + scheduling | `control_task.c`, `data_task.c`, `task_event.c` |

BLE PHY retained for raw capture; BLE protocol stack removed 2026-07-20 (Sniffle handles BLE).
`ll_manager.c` is the shared RX PDU classifier (ADV/SCAN/CONNECT/DATA) and is kept; the BLE
connection/GATT stack (`ble_conn.c`, `ble_conn_mgr.c`, `ble_conn_pdu.c`, `att_client.c`, `csa2.c`,
`ll_follower.c`) was deleted.

---

## 3. Mapeo Python (`python/feralrf/`)

| Capa | Propósito | Archivos |
|------|-----------|----------|
| L1 transport | COBS + CRC16 + pyserial | `protocol.py` |
| L2 commands | Command IDs + frame builders | `commands.py`, `_responses.py` |
| L3 core | API pública (`Radio` class, presets, enums) | `radio.py`, `presets.py`, `enums.py`, `exceptions.py` |
| L4 features | Spectrum / jamming (experimental, stub) | `_jamming.py`, `_spectrum.py` |
| L5 apps | Ejemplos / demos / tools | `python/examples/` |

---

## 4. Reglas de capas

1. **No saltar capas.** L4 no toca L1 directo. Lo que falta se expone desde L3/L2.
2. **L1 platform no conoce nada de FeralRF.** Startup, ccfg y linker son intercambiables entre boards CC1352P7 si cambias config.
3. **L4 services no conoce transporte.** `ll_manager` no sabe que hay UART; emite eventos al protocol layer.
4. **Reglas RF de L4/radio** (skill `ti-rtos-rf-cc1352`, detalle en seccion 5): `CMD_FS` siempre via `RF_postCmd` (nunca `RF_runCmd`, todo PHY incl. BLE), un cliente RF abierto a la vez, `RF_open` lazy en primer uso, `RF_close` evitado salvo excepciones guardadas, PHY switch = `RF_flush + RF_yield` + reconfigurar. Ante la duda, gana lo que hace `radio_if.c`.
5. **Python L3 es la API pública estable.** L4 features construyen sobre L3, nunca bypass a L2/L1.

---

## 5. Reglas RF (resumen — full skill en `ti-rtos-rf-cc1352`)

Estas reglas estan verificadas contra `radio_if.c`. La primera es un hazard duro
(cuelga el firmware); las demas son patrones con excepciones documentadas. Ante la
duda, lo que hace `radio_if.c` gana sobre cualquier regla en prosa.

### Driver lifecycle

| Regla | Realidad / por que |
|-------|--------------------|
| **`CMD_FS` SIEMPRE via `RF_postCmd`, nunca `RF_runCmd(FS)`** | `RF_runCmd(FS)` cuelga en TI-RTOS (`loDivider=0x0A` lo agrava). Aplica a TODO PHY, BLE incluido: el firmware SI emite `CMD_FS` para BLE (`radio_if.c:453,1115`) via `RF_postCmd`. La vieja regla "BLE no emite CMD_FS" es falsa aqui |
| **Un cliente RF abierto a la vez** | Hay `RF_Object` por modo (`s_rf_object`, `s_433_rf_object`, `s_rf_tx_session_object`) pero solo uno abierto/registrado a la vez; `N_MAX_CLIENTS=2` hace que un 2do `RF_open` concurrente devuelva NULL (`radio_if.c:2867`). Comparte el handle activo |
| **`RF_open` es lazy, no en boot** | `radio_if.c:1889` "No RF_open at init"; abre en el primer `set_phy`/TX. Un PHY switch normal es `RF_flush + RF_yield` + reconfigurar |
| **`RF_close` se evita, con excepciones guardadas** | Puede deadlock en `SemaphoreP_pend`, por eso el PHY switch no cierra. Pero hay `RF_close` deliberados y guardados: re-init de `RADIO_INIT` a media sesion (`radio_if.c:1853`) y teardown de OOK. "NUNCA RF_close" es guia, no absoluto |

### RX configuration BLE

| Setting | Valor | Por qué |
|---------|-------|---------|
| `endTrigger.triggerType` | `TRIG_NEVER` | Continuous RX |
| `endTime` | `1` | Sin timeout efectivo (`smartrf_ble5_0.c:193`) |
| `bRepeat` | `1` | Multiple packets |
| `accessAddress` | `0x8E89BED6` | BLE adv standard |
| `crcInit` | `0x555555` | BLE adv standard |
| `bAutoFlushIgnored` | `1` | Prevent queue saturation |

### TX configuration BLE ADV

El path ADV TX validado (`radio_if.c:742-751`) emite una PDU por `RF_runCmd`:

| Setting | Valor | Por qué |
|---------|-------|---------|
| `startTrigger.triggerType` | `TRIG_NOW` | Dispara al postear |
| `condition.rule` | `COND_NEVER` | No encadena al siguiente comando; termina tras la PDU |

⚠️ Principio: una operacion TX one-shot debe terminar sola. No configures un comando
TX para correr indefinidamente (eso cuelga `RF_runCmd`).

---

## 6. Hardware target

- **MCU radio:** CC1352P7
- **CatSniffer:** `frontEndMode=0x0` (differential), `biasMode=0x1` (external; `0x0` en IEEE 802.15.4)
- **Antenna switch:** DIO28 (2.4 GHz), DIO29 (High PA — no configurado), DIO30 (Sub-1GHz)
- **UART CC1352↔RP2040:** DIO12 (RX), DIO13 (TX), 921600, no flow control
- **LEDs:** DIO24 (status, en main loop counter — no Clock module)
- **Flash:** `.hex` via catnip (`.bin` causa boot failures)

---

## 7. Wire protocol

COBS-framed binary con CRC16-CCITT. Detalles completos en [`protocol.md`](protocol.md).

Frame structure:
```
[COBS-encoded]: [CMD/RSP(1) | SEQ(1) | LEN(2, LE) | PAYLOAD(N) | CRC16(2, LE)]
[0x00 delimiter]

Campos multibyte en little-endian. CRC16-CCITT (poly 0x1021, init 0xFFFF)
calculado sobre CMD/RSP + SEQ + LEN + PAYLOAD. Fuente de verdad:
`python/feralrf/protocol.py` y `firmware/cc1352/src/protocol.c` (identicos).
```

| ID range | Tipo |
|----------|------|
| `0x01–0x62` | Commands (host → device) |
| `0x80–0xFF` | Responses (device → host) |

El RP2040 expone 3 puertos USB CDC (Cat-Bridge / Cat-LoRa / Cat-Shell); el enlace de radio es Cat-Bridge @ 921600.

---

## 8. Memory model

- **CC1352:** allocación estática únicamente. No `malloc`.
- **RX buffer:** 16 KB circular estático.
- **Host output queue (`packet_queue.c`, `PACKET_QUEUE_DEPTH`):** 32 entries (commit `5c8b561`, F8A growth). Still live.
- **RF data-channel queue (`tx_queue.c`, `TX_QUEUE_SIZE`):** 8, unchanged. Originally sized for the BLE central/follow role; that role was removed 2026-07-20, no current caller remains.
- **FW size budget:** <120 KB (actual: ~91 KB post-F10).

---

## 9. Branching

- Una rama por fase: `feature/fN-<slug>`
- Merge a `main` solo tras checkpoint humano
- Tag anotado al cerrar: `v2.0-fN`
- Post-F11 hito: `feature/ti-rtos-migration` se merge a `main`. NoRTOS retirado como `v1.5-legacy-final`.

---

## 10. Testing strategy

| Layer | Test type | Hardware |
|-------|-----------|----------|
| Python L1–L4 unit | `pytest` (test_props, test_protocol, test_commands_contract, ...) | Ninguno |
| Python L4 integration | `pytest -m hardware` | 1+ board |
| Firmware functional | `python/examples/smoke_*.py` | 1 board |
| Firmware OTA | `python/examples/run_validation_baseline.sh --rx-port` | 2 boards |
| End-to-end attack | Demos en `python/examples/lab/` (interactivos) | 2 boards + phone/peripheral |

CI corre Python unit + lint automáticamente; el build de CC1352 es best-effort (requiere las libs del installer completo de TI, no disponibles en CI).
