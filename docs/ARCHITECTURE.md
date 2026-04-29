# FeralRF — Architecture

> **Authoritative source for the layered design.** For phase tracking and decisions, see
> [`superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`](superpowers/specs/2026-04-24-feralrf-plan-v2-design.md).

---

## 1. Vista global

```
┌──────────────────────────────────────────────────────────────┐
│  HOST (Python) — feralrf package                             │
│  L5 apps     examples/, demos/, attacks/...                  │
│  L4 features ble.py, _jamming.py, _spectrum.py, emul/...     │
│  L3 core     radio.py (Radio class), presets.py, enums       │
│  L2 cmds     commands.py, _responses.py                      │
│  L1 xport    protocol.py (COBS + CRC16), pyserial            │
└──────────────────┬───────────────────────────────────────────┘
                   │ USB-CDC 921600 baud
┌──────────────────▼───────────────────────────────────────────┐
│  RP2040 bridge (transparente, fuera de scope del plan v2)    │
└──────────────────┬───────────────────────────────────────────┘
                   │ UART 921600 (GPIO0/1 ↔ DIO12/13)
┌──────────────────▼───────────────────────────────────────────┐
│  CC1352P7 firmware — FeralRF v2.0 (TI-RTOS 7)                │
│  L5 app      main_rtos.c, task scheduling                    │
│  L4 services radio/, ble/, protocol/, queues/                │
│  L3 drivers  host_if, RF, GPIO (delgados sobre SDK)          │
│  L2 SDK      TI SimpleLink CC13xx/CC26xx 8.30.01.01          │
│  L1 platform startup, ccfg, linker, RTOS stubs               │
└──────────────────────────────────────────────────────────────┘
```

No existe capa HAL formal. Los services llaman directamente al TI SDK 8.30 (decisión #21 del spec maestro).

---

## 2. Mapeo firmware (`firmware/cc1352/src/`)

| Capa | Propósito | Archivos |
|------|-----------|----------|
| L1 platform | Arranque, linker, glue RTOS | `main.c`, `main_rtos.c`, `rtos_stubs.c`, `startup_cc13x2_cc26x2_gcc.c`, `ccfg.c` |
| L3 drivers | IF físicas (UART, RF, GPIO) | `host_if.c`, `host_if_task.c`, `output_if.c`, `ti_rf_config_min.c` |
| L4 radio | PHY abstraction + SmartRF configs | `radio_if.c`, `phy_manager.c`, `smartrf_ble5_0.c`, `smartrf_ieee_15_4_0.c`, `smartrf_prop_0.c` |
| L4 ble | Link layer, conexión, GATT | `ll_manager.c`, `csa2.c`, `ble_conn.c`, `ble_conn_mgr.c`, `att_client.c` |
| L4 queues | TX + packet queues | `tx_queue.c`, `packet_queue.c` |
| L4 protocol | COBS + commands | `protocol.c`, `command_processor.c` |
| L5 app | Tasks + scheduling | `control_task.c`, `data_task.c`, `task_event.c` |

---

## 3. Mapeo Python (`python/feralrf/`)

| Capa | Propósito | Archivos |
|------|-----------|----------|
| L1 transport | COBS + CRC16 + pyserial | `protocol.py` |
| L2 commands | Command IDs + frame builders | `commands.py`, `_responses.py` |
| L3 core | API pública (`Radio` class, presets, enums) | `radio.py`, `presets.py`, `enums.py`, `exceptions.py` |
| L4 features | Módulos de ataque / spectrum / jamming | `attacks/ble.py`, `_jamming.py`, `_spectrum.py` |
| L5 apps | Ejemplos / demos / tools | `python/examples/` |

---

## 4. Reglas de capas

1. **No saltar capas.** L4 no toca L1 directo. Lo que falta se expone desde L3/L2.
2. **L1 platform no conoce nada de FeralRF.** Startup, ccfg y linker son intercambiables entre boards CC1352P7 si cambias config.
3. **L4 services no conoce transporte.** `att_client` no sabe que hay UART; emite eventos al protocol layer.
4. **Reglas RF validadas son invariante de L4/radio** (skill `ti-rtos-rf-cc1352`): single RF_Object, PHY switch = `RF_flush + RF_yield + RF_close + RF_open`, precompiled libs obligatorias, etc.
5. **Python L3 es la API pública estable.** L4 features construyen sobre L3, nunca bypass a L2/L1.

---

## 5. Reglas RF (resumen — full skill en `ti-rtos-rf-cc1352`)

### Driver lifecycle

| Regla | Por qué |
|-------|---------|
| **Un solo `RF_Object`** | RF driver `N_MAX_CLIENTS=2`. Multiple objects → silent hangs en `RF_open` |
| **`RF_open` UNA vez al boot** | Re-open después de close → deadlocks con TI-RTOS `SemaphoreP` |
| **NUNCA `RF_close`** | `RF_close` triggers `SemaphoreP_pend` que puede no completar |
| **`RF_postCmd` para `CMD_FS`** | `RF_runCmd(FS)` cuelga en TI-RTOS |
| **No `CMD_FS` para BLE** | BLE commands manejan freq vía `.channel` field |
| **`CMD_FS` requerido para IEEE/Prop** | Pero `RF_postCmd`, no `RF_runCmd` |

### RX configuration BLE

| Setting | Valor | Por qué |
|---------|-------|---------|
| `endTrigger.triggerType` | `TRIG_NEVER` | Continuous RX |
| `endTime` | `0` | Sin timeout |
| `bRepeat` | `1` | Multiple packets |
| `accessAddress` | `0x8E89BED6` | BLE adv standard |
| `crcInit` | `0x555555` | BLE adv standard |
| `bAutoFlushIgnored` | `1` | Prevent queue saturation |

### TX configuration BLE ADV

| Setting | Valor | Por qué |
|---------|-------|---------|
| `endTrigger.triggerType` | `TRIG_REL_START` | One-shot — debe terminar |
| `endTime` | `40000` (10 ms @ 4 MHz RAT) | Suficiente para una ADV PDU |
| `condition.rule` | `COND_NEVER` | Don't chain to next command |

⚠️ NUNCA `endTrigger=TRIG_NEVER` en ADV TX — `RF_runCmd` cuelga.

---

## 6. Hardware target

- **MCU radio:** CC1352P7 (decisión #1)
- **CatSniffer:** `frontEndMode=0x0` (differential), `biasMode=0x1` (external bias)
- **Antenna switch:** DIO28 (2.4 GHz), DIO29 (High PA — no configurado), DIO30 (Sub-1GHz)
- **UART CC1352↔RP2040:** DIO12 (RX), DIO13 (TX), 921600, no flow control
- **LEDs:** DIO24 (status, en main loop counter — no Clock module)
- **Flash:** `.hex` via catnip (`.bin` causa boot failures)

---

## 7. Wire protocol

COBS-framed binary con CRC16-CCITT. Detalles completos en [`protocol.md`](protocol.md).

Frame structure:
```
[COBS-encoded]: [LEN(2) | CMD/RSP(1) | SEQ(1) | PAYLOAD(N) | CRC16(2)]
[0x00 delimiter]
```

| ID range | Tipo |
|----------|------|
| `0x01–0x4F` | Commands (host → device) |
| `0x80–0xFF` | Responses (device → host) |

Single CDC, 921600 baud, no composite USB.

---

## 8. Memory model

- **CC1352:** allocación estática únicamente (decisión #7). No `malloc`.
- **RX buffer:** 16 KB circular estático.
- **TX queue (BLE central):** 32 frames (commit `5c8b561` post-F8A growth).
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
| Python L1–L4 unit | `pytest` (test_props, test_attacks_ble, test_protocol, test_commands_contract, ...) | Ninguno |
| Python L4 integration | `pytest -m hardware`, `-m hardware_ble` | 1+ board |
| Firmware functional | `python/examples/smoke_*.py` | 1 board |
| Firmware OTA | `python/examples/run_validation_baseline.sh --rx-port` | 2 boards |
| End-to-end attack | Demos en `python/examples/lab/` (interactivos) | 2 boards + phone/peripheral |

CI corre Python unit + lint + RP2040 build automáticamente. CC1352 build no está en CI (TI SDK no es licencia open).
