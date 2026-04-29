# FeralRF — Plan Maestro v2.0 (design spec)

> **Fecha:** 2026-04-24
> **Autor:** Sabas + Claude (brainstorming session)
> **Base del formato:** `FAULTYCAT_REFACTOR_PLAN.md` (enfoque 3 — FeralRF-native, disciplina FaultyCat)
> **Decisión raíz:** re-plan del proyecto, **no rewrite**. El código actual se conserva y evoluciona por fases. `feature/ti-rtos-migration` es la línea de desarrollo. `main` NoRTOS se retira post-F11.

---

## Tabla de contenidos

1. Decisiones congeladas
2. Estado actual
3. Arquitectura objetivo
4. Dependencias
5. Plan por fases (F0 → F29)
6. Governance
7. Riesgos
8. Migración NoRTOS → TI-RTOS
9. Decisiones pendientes
10. Prompt inicial para Claude Code
11. Template de skill por fase
12. Resumen de entregables de esta spec

---

## 1. Decisiones congeladas

Irrevocables. No re-litigar sin invalidar este plan.

| # | Decisión | Valor |
|---|----------|-------|
| 1 | MCU radio | **CC1352P7** (primario y único nuevo; CC1352P antiguo solo "best effort" si no rompe nada) |
| 2 | SDK | **TI SimpleLink CC13xx/CC26xx 8.30.01.01** pineado |
| 3 | RTOS | **TI-RTOS 7 (SysBIOS)** como única línea. NoRTOS retirado. |
| 4 | Bridge MCU | **RP2040** con Pico SDK 2.0.0. **Fuera de alcance del plan** (se asume estable) |
| 5 | Host API | **Python** sync primario, async opcional (pyserial / pyserial-asyncio) |
| 6 | Protocolo device↔host | **COBS + CRC16-CCITT**, single CDC, 921600 baud, sin composite USB |
| 7 | Memoria CC1352 | **Allocación estática únicamente** — no malloc |
| 8 | PHYs obligatorios | **8** — BLE 1M / 2M / Coded S8 / Coded S2 / IEEE 802.15.4 / Sub-1GHz 868 / Sub-1GHz 915 / Proprietary GFSK configurable |
| 9 | RX buffer | **16 KB circular estático** |
| 10 | TX power | **−20 a +14 dBm**. High PA (+15–20 dBm) diferido hasta resolver DIO29 antenna switch |
| 11 | Licencia | **GPL-3.0** (mantiene la actual) |
| 12 | Branching | Rama por fase, merge a `main` **solo tras checkpoint humano firmado** |
| 13 | Backward compat | **Romper si hace falta.** Firmware nuevo = **v2.0**. Firmwares viejos no soportados. |
| 14 | Hardware target | **CatSniffer v3.x** con CC1352P7. 4 boards disponibles, 1 degradada |
| 15 | Reactive jamming | Latencia target **<500 µs** |
| 16 | Antena | **CatSniffer stock** — optimizada 868 / 2.4 GHz, 433 con pérdidas, <430 MHz no viable |
| 17 | Flash tooling | **catnip con `.hex`** — nunca `.bin` (causó boot failures) |
| 18 | GATT strategy | **Raw RF Sniffle-style** — NO ICall / BLE5-Stack |
| 19 | RF driver rules | **10 reglas validadas** (single RF_Object, precompiled libs, `RF_yield+close+open` para PHY switch, etc. — skill `ti-rtos-rf-cc1352`) |
| 20 | Versionado | Firmware = **v2.0**. Python package = decisión D5 (Sección 9). |
| 21 | HAL | **No hay capa HAL formal.** Los services llaman directamente al TI SDK 8.30. Testing con fakes se hace a nivel de service, no de SDK. |

---

## 2. Estado actual

### 2.1 Capacidades validadas y en progreso (tabla unificada)

| Área | Capacidad | Branch | Estado | Evidencia / Métrica |
|------|-----------|--------|--------|---------------------|
| PHYs | 8/8 TX/RX OTA | main | ✅ | validation matrix 18/18, markers 10/10 |
| Props | 15 presets GFSK/FSK/OOK | main | ✅ | OTA markers 10/10 por preset, 4-FSK/4-GFSK incluidos |
| Props | W-MBus T/C/N mode | main | ✅ | commit `4676f6d` |
| Props | MSK 868 / 433 MHz | main | ✅ | commit `04881c7` |
| Props | 4-FSK / 4-GFSK | main | ✅ | commit `b089f71` |
| OOK | 433 / 868 MHz genook RX+TX | main | ✅ | markers 10/10 |
| Bridge | RP2040 USB-CDC | main | ✅ | 921600 baud estable |
| Estabilidad | Soak 5 min | main | ✅ | 213 ciclos, 0 errs, 89.7% delivery |
| FW size | CC1352 (NoRTOS) | main | ✅ | 55 KB / 352 KB flash |
| Protocolo | COBS + CRC16 @ 921600 | main | ✅ | 0 CRC errs en soak |
| Python | API sync + PROP_PRESETS | main | ✅ | 13/13 unit tests |
| BLE attacks | beacon_flood, apple / google popup, adv_spoof, replay, clone, Fast Pair | main | ✅ | Soundcore 0x8F95F8 >60 s estable |
| Recovery | `reset_device()` via RP2040 | main | ✅ | OOK unlock <2 s |
| RTOS skeleton | LED + UART TI-RTOS7 | ti-rtos-migration | ✅ | `f961d4a` |
| RTOS PHYs | BLE / IEEE / Sub-1GHz TX/RX | ti-rtos-migration | ⚠️ 5/6 | 868→BLE falla (rx=0) |
| FW size | CC1352 (TI-RTOS) | ti-rtos-migration | ✅ | 72 KB |
| BLE 2M ext adv | ADV_EXT → ADV_AUX chain | ti-rtos-migration | ✅ | commit `3998b0b` |
| BLE central | CMD_CONNECT, CSA#2, TRNG workaround | ti-rtos-migration | ✅ | commits `1756676`, `1b479db`, `0d81a7c` |
| Anchor timing | NOSYNC fix | ti-rtos-migration | ✅ | commit `b4fac25` |
| GATT client | ATT state machine + L2CAP | ti-rtos-migration | 🟡 construido, no validado | commit `41b81fe` |
| Test script | `test_connect.py` | ti-rtos-migration | 🟡 sin commitear | WIP, falta probar contra peripheral real |

### 2.2 Known issues

- **868→BLE PHY switch falla** (rx=0 tras cambio). Resto de transiciones (BLE→IEEE→868→IEEE→868) PASS.
- **GATT discovery no validado end-to-end** con peripheral real (nRF Connect advertiser no implementa GATT server).
- **High PA +15–20 dBm** requiere fix DIO29 antenna switch (fuera de v2.0).
- **169 / 315 / 390 / 470 MHz no viables** sin SmartRF Studio + antena adecuada.
- **CMD_TX_TEST (jamming 2.4 GHz proprietary) no funciona** — blocker para F18.
- **Antena CatSniffer limita 433 MHz** (opera con pérdidas aceptables).

### 2.3 Próxima acción inmediata (fuera del plan)

Validar GATT con peripheral real antes de formalizar F8. Sugeridos:
- Móvil Android con app "BLE Peripheral Simulator" o nRF Connect **modo peripheral**.
- ESP32 / nRF52840 con firmware GATT server (p. ej. bluefruit nRF52 Feather).
- Raspberry Pi con `bleno` GATT server.

---

## 3. Arquitectura objetivo

### 3.1 Vista global

```
┌──────────────────────────────────────────────────────────────┐
│  HOST (Python) — feralrf package                             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ L5 apps     examples/, demos/, attacks/...              │ │
│  │ L4 features ble.py, _jamming.py, _spectrum.py, emul/... │ │
│  │ L3 core     radio.py (Radio class), presets.py, enums   │ │
│  │ L2 cmds     commands.py, _responses.py                  │ │
│  │ L1 xport    protocol.py (COBS + CRC16), pyserial        │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────┬───────────────────────────────────────────┘
                   │ USB-CDC 921600 baud
┌──────────────────▼───────────────────────────────────────────┐
│  RP2040 bridge (transparente, fuera de scope del plan)       │
└──────────────────┬───────────────────────────────────────────┘
                   │ UART 921600 (GPIO0/1 ↔ DIO12/13)
┌──────────────────▼───────────────────────────────────────────┐
│  CC1352P7 firmware — FeralRF v2.0 (TI-RTOS 7)                │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ L5 app      main_rtos.c, task scheduling                │ │
│  │ L4 services radio/, ble/, protocol/, queues/            │ │
│  │ L3 drivers  host_if, RF, GPIO (delgados sobre SDK)      │ │
│  │ L2 SDK      TI SimpleLink CC13xx/CC26xx 8.30.01.01      │ │
│  │ L1 platform startup, ccfg, linker, RTOS stubs           │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

No existe capa HAL formal (decisión #21). Services llaman al SDK directamente.

### 3.2 Mapeo firmware CC1352 — archivos actuales a capas

| Capa | Propósito | Archivos actuales (`firmware/cc1352/src/`) |
|------|-----------|---------------------------------------------|
| L1 platform | Arranque, linker, glue RTOS | `main.c`, `main_rtos.c`, `rtos_stubs.c`, `startup_cc13x2_cc26x2_gcc.c`, `ccfg.c` |
| L3 drivers | IF físicas (UART, RF, GPIO) | `host_if.c`, `host_if_task.c`, `output_if.c`, `ti_rf_config_min.c` |
| L4 services: **radio** | PHY abstraction + SmartRF configs | `radio_if.c`, `phy_manager.c`, `smartrf_ble5_0.c`, `smartrf_ieee_15_4_0.c`, `smartrf_prop_0.c` |
| L4 services: **ble** | Link layer, connection, GATT | `ll_manager.c`, `csa2.c`, `ble_conn.c`, `ble_conn_mgr.c`, `att_client.c` |
| L4 services: **queues** | TX + packet queues | `tx_queue.c`, `packet_queue.c` |
| L4 services: **protocol** | COBS + commands | `protocol.c`, `command_processor.c` |
| L5 app | Tasks + scheduling | `control_task.c`, `data_task.c`, `task_event.c` |

### 3.3 Mapeo Python

| Capa | Propósito | Archivos (`python/feralrf/`) |
|------|-----------|------------------------------|
| L1 transport | COBS + CRC16 + pyserial | `protocol.py` |
| L2 commands | Command IDs + frame builders | `commands.py`, `_responses.py` |
| L3 core | API pública (Radio class, presets, enums) | `radio.py`, `presets.py`, `enums.py`, `exceptions.py` |
| L4 features | Módulos de ataque / spectrum / jamming | `attacks/ble.py`, `_jamming.py`, `_spectrum.py` |
| L5 apps | Ejemplos / demos / tools | `python/examples/` |

### 3.4 Reglas de capas

1. **No saltar capas**: L4 no toca L1 directo. Si falta algo, se expone desde L3/L2.
2. **L1 platform no conoce nada de FeralRF**: startup, ccfg y linker son intercambiables entre boards CC1352P7 si cambias config.
3. **L4 services no conoce transporte**: `att_client` no sabe que hay UART; emite eventos al protocol layer.
4. **Reglas RF validadas (skill `ti-rtos-rf-cc1352`) son invariante de L4/radio**: single RF_Object, PHY switch = `RF_flush + RF_yield + RF_close + RF_open`, precompiled libs obligatorias, etc.
5. **Python L3 es la API pública estable.** L4 features construyen sobre L3, nunca bypass a L2/L1.

### 3.5 Movimiento de archivos: progresivo

El código actual **no se mueve** en F0. Cada fase que toca un archivo lo mueve a su sub-carpeta si la capa la crea.

**Sub-carpetas sugeridas (aplicar progresivamente):**
```
firmware/cc1352/src/
├── platform/    # main*.c, startup, ccfg, rtos_stubs
├── drivers/     # host_if*, output_if, ti_rf_config_min
├── radio/       # radio_if, phy_manager, smartrf_*
├── ble/         # ll_manager, csa2, ble_conn*, att_client
├── protocol/    # protocol, command_processor
├── queues/      # tx_queue, packet_queue
└── app/         # control_task, data_task, task_event
```

---

## 4. Dependencias

### 4.1 Firmware CC1352

| Dep | Versión pineada | Ubicación | Estrategia |
|-----|-----------------|-----------|-----------|
| TI SimpleLink CC13xx/CC26xx SDK | **8.30.01.01** | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/` | Instalado separado (installer TI, no git). `CMakeLists.txt` valida path vía `TI_SDK_PATH`. No vendorizado. |
| arm-none-eabi-gcc | ≥11.3 | Sistema (PATH) | Documentado en `CLAUDE.md`. Verificado en cada build. |
| CMake | ≥3.20 | Sistema | `cmake_minimum_required(VERSION 3.20)` |
| Precompiled SDK libs (driverlib, sysbios, drivers) | Del SDK 8.30 | `$TI_SDK_PATH/source/ti/.../lib/` | **OBLIGATORIAS** (regla RF #19). Source-compiled libs causan `RF_postCmd IDLE`. |

### 4.2 Python (`feralrf` package)

| Dep | Versión mínima | Propósito |
|-----|----------------|-----------|
| `pyserial` | ≥3.5 | Transporte UART/USB-CDC sync |
| `pyserial-asyncio` | ≥0.6 | Async opcional (L1 xport) |
| `cobs` | ≥1.2 | COBS encode/decode |

**Dev deps:** `pytest`, `pytest-asyncio`, `pytest-cov`, `black` (line-length 100), `isort`, `mypy`.
**Target Python:** 3.9 – 3.12.

### 4.3 Repos de referencia (NO dependencias, solo consulta)

| Repo | Uso |
|------|-----|
| **Sniffle** (nccgroup) | Arquitectura raw RF para BLE central + GATT (decisión #18) |
| **rfDiagnostics** (TI example) | Patrón single RF_Object + PHY switch (regla RF #19) |

### 4.4 Patches propios (MCE / RFE)

Viven en `firmware/cc1352/smartrf_settings/` mezclados con configs generadas. En F13 (retro-fill) se mueven a una sub-carpeta dedicada `firmware/cc1352/patches/` con README por patch.

| Patch | Origen | Propósito | Estado |
|-------|--------|-----------|--------|
| `mce_genook` | TI app note + customización CatSniffer | OOK RX/TX en 433/868 MHz | ✅ validado 10/10 markers |
| `rfe_genook` | TI app note | RFE para OOK (complementa `mce_genook`) | ✅ validado |
| `multi-protocol` | TI SDK (`rf_patch_cpe_multi_protocol`) | CPE patch común a todos los RF_Modes | ✅ obligatorio (regla RF) |

### 4.5 Reglas de dependencias

1. **SDK TI no se cambia sin re-validación completa.** Cambio de versión = fase dedicada con checkpoint humano.
2. **Python deps libres dentro del major pineado**, versiones mínimas testeadas en `pyproject.toml`.
3. **Binarios precompilados del SDK son inmutables** — si aparece bug, se abre issue upstream TI, no se recompila local.
4. **Referencias (Sniffle, rfDiagnostics) NO se vendorizan.** Se consultan, no se copian. Si se porta código literal, se anota en el commit y se respeta licencia.

---

## 5. Plan por fases (F0 → F29, + F8A insertada 2026-04-24, + Bloque D agregado 2026-04-29)

**Leyenda estados:** ✅ completa · 🟡 construida sin validar / bloqueada por prereq · ⚠️ parcial (issues abiertos) · 🔜 pendiente

**Plantilla por fase:**
```
### FN — <título>

**Prereq:** F<M>, F<K>
**Branch:** feature/<slug>
**Tag al cerrar:** v2.0-fN

**Entregables firmware:**    (si aplica)
**Entregables Python:**       (si aplica)
**Criterio de cierre:**       qué debe funcionar medible
**Checkpoint humano:**        acciones físicas del usuario + HW requerido
**Riesgos específicos:**      (si hay)
```

---

### BLOQUE A — Histórico

### F0 — Setup inicial ✅

**Estado:** ✅ completado (rama `main`).
**Entregables (ejecutados):** Docker build container, CMake raíz para RP2040 + CC1352, skeleton firmware, GitHub Actions (`build.yml`, `release.yml`), SDK TI 7.10.01.24 + Pico SDK 2.0.0, Python package esqueleto con 13/13 tests.
**Evidencia:** `feralrf_rp2040.uf2` (46 KB), `feralrf_cc1352.elf` (392 B code), 13/13 Python tests passing.

### F1 — 8 PHYs TX/RX OTA (NoRTOS) ✅

**Estado:** ✅ completado (rama `main`).
**Entregables ejecutados:**
- BLE 1M / 2M / Coded S8 / Coded S2 (`smartrf_ble5_0.c`, `CMD_BLE5_ADV_NC` para 2M/Coded).
- IEEE 802.15.4 (`smartrf_ieee_15_4_0.c`).
- Sub-1GHz 868 / 915 (`smartrf_prop_0.c`, `CMD_PROP_RADIO_DIV_SETUP_PA`, power table).
- Proprietary GFSK configurable (reutiliza prop backend con `loDivider` dinámico).

**Evidencia:** validation matrix 18/18, markers OTA 10/10 por PHY.

### F2 — Radio propietaria + 15 presets + MSK + W-MBus + 4-FSK ✅

**Estado:** ✅ completado (rama `main`).
**Entregables ejecutados:**
- `CMD_SET_PROP_CONFIG (0x08)` — freq / mod / rate / deviation / rx_bw / sync_word en runtime (16 B payload).
- Band-specific overrides auto-seleccionados por frecuencia (433 / 868 / 169 MHz).
- 15 presets (13 GFSK/FSK + 2 OOK).
- MSK 868 / 433 MHz (commit `04881c7`).
- W-MBus T / C / N modes (commit `4676f6d`).
- 4-FSK / 4-GFSK (commit `b089f71`).
- Python `configure_prop()` + `PROP_PRESETS` dictionary.

**Evidencia:** OTA markers 10/10 por preset.

### F3 — OOK 433/868 + genook + recovery ✅

**Estado:** ✅ completado (rama `main`).
**Entregables ejecutados:**
- RF_Mode dedicado OOK con patches `mce_genook` + `rfe_genook`.
- `reset_device()` via RP2040 shell (boot→exit) para unlock tras OOK.
- Band overrides para 433 (AGC=0x20, RSSI=−8 dB), 868+ (default).

**Evidencia:** 10/10 markers por banda OOK.

### F4 — Estabilidad COBS + PHY switching + soak ✅

**Estado:** ✅ completado (rama `main`).
**Entregables ejecutados:**
- RF session cleanup en `init()`/`setPhy()` (`stopRfBackend + closeTxSession`).
- OOK session lock — skip `RF_close` cuando genook patches activos.
- `RF_flushCmd` antes de `RF_close` en `closeTxSession`.
- State transitions 9/9 PASS (RX↔TX, rapid cycling).

**Evidencia:** soak 5 min, 213 ciclos random PHY, 0 errors, 0 timeouts, 89.7% delivery.

### F5 — BLE Security Testing (NoRTOS) ✅

**Estado:** ✅ completado (rama `main`).
**Entregables ejecutados:**
- `attacks/ble.py`: `beacon_flood()`, `apple_popup_spam()`, `google_popup_spam()`, `adv_spoof()`, `capture_and_replay()`.
- Fast Pair popup Soundcore Boom 2 Model ID `0x8F95F8`.
- Device emulation estable >60 s.
- Demos: `demo_ble_analyzer.py`, `demo_ble_clone.py` (6 estrategias), `demo_emulate_soundcore.py`.

**Evidencia:** validados en móvil real + nRF Connect.

### F6 — TI-RTOS baseline (5/6 PHYs) ⚠️

**Estado:** ⚠️ parcial (rama `feature/ti-rtos-migration`).
**Entregables ejecutados:**
- Skeleton TI-RTOS7: LED + UART (commit `f961d4a`).
- Single RF_Object + RF_yield/close/open para PHY switch (patrón rfDiagnostics).
- Precompiled SDK libs (driverlib.lib, sysbios.a, drivers_cc13x2x7.a).
- SysConfig-generated configs (ti_sysbios_config, ti_drivers_config).
- BLE / IEEE / Sub-1GHz TX/RX validados 5/6.

**Issue abierto:** 868→BLE switch falla (rx=0). Se resuelve en **F9**.

### F7 — BLE central + ATT/GATT client 🟡

**Estado:** 🟡 construido, no validado end-to-end (rama `feature/ti-rtos-migration`).
**Entregables ejecutados:**
- `CMD_BLE5_INITIATOR` + `CMD_BLE5_MASTER` SmartRF structs (`84dc0ea`).
- `BleConn_init` + `ble_conn_mgr` (`9404f08`, `0d81a7c`).
- `CMD_CONNECT`, `CMD_DISCONNECT`, `CMD_CONN_STATUS` (`4a78c57`).
- CSA#2 channel selection (`1b479db`).
- TRNG workaround xorshift32 (`1756676`).
- Anchor timing fix — NOSYNC resolved (`b4fac25`).
- `att_client.c/h` — ATT state machine + L2CAP CID 0x0004.
- `CMD_GATT_DISCOVER (0x43)`, `CMD_GATT_READ (0x45)`, `CMD_GATT_WRITE (0x46)`.
- Responses `RSP_GATT_SERVICE`, `RSP_GATT_CHAR`, `RSP_GATT_READ`, `RSP_GATT_DONE` (`41b81fe`).

**Issue abierto:** GATT round-trip no validado con peripheral real (nRF Connect advertiser no implementa GATT server). Se resuelve en **F8**.

---

### BLOQUE B — Consolidación TI-RTOS

### F8A — BLE Central rewrite Sniffle-style ✅ (tag `v2.0-f8a`)

**Agregada 2026-04-24** tras descubrir que el `CMD_BLE5_INITIATOR` de TI no entrega un `connTime` compatible con el anchor que el peripheral espera — la conexión se cae al primer master event (`BLE_DONE_NOSYNC = 0x1402`). Validado contra CH573 `DC:32:62:8D:E1:09`: Sniffle firmware conecta limpio en el mismo board; FeralRF falla incluso con sweep completo de WinOffset en 1.25 ms.

**Prereq:** F7
**Branch:** `feature/f8a-ble-central-sniffle`
**Tag al cerrar:** `v2.0-f8a`

**Entregables firmware:**
- Reemplazar `RadioIF_bleInitiate` (CMD_BLE5_INITIATOR) con CONNECT_IND manual (CMD_BLE5_GENERIC_TX / ADV_NC con T_IFS 150 µs tras ADV_IND).
- `connTime` = timestamp RAT de nuestra propia TX de CONNECT_IND.
- Primer master event a `connTime + transmitWindowOffset + 1.25 ms` (valores que nosotros pusimos en CONNECT_IND → sin incertidumbre).
- Mover `BleConnMgr_poll()` a `RfTask` (fix UART starvation — ya implementado como commit `f125473` en rama `fix/uart-starvation-during-conn`, re-aplicar).
- Retirar ICall/BLE5-Stack residuos: `startup/osal_icall_ble.c`, `syscfg/ti_ble_config.c/h`.

**Entregables Python:** ninguno (Python F8 ya está listo, se reutiliza tal cual).

**Criterio de cierre:**
- `demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 --read` conecta, discovery completa, read OK, disconnect limpio.
- `conn_status` post-connect: `connected=True events>0 tx>0 rx>0 last_status=0x1400`.
- Regression 8/8 PHYs OTA markers 10/10.

**Plan de implementación:** `docs/superpowers/plans/2026-04-24-f8a-ble-central-sniffle-rewrite.md` (spec sólo — implementación en 2-3 sesiones futuras).

**Referencia:** branch `fix/uart-starvation-during-conn` (commits `f125473` move + `5b7325a` sweep intento incompleto) — conservar como referencia histórica, no mergear.

### F8 — Validar GATT end-to-end ✅ (tag `v2.0-f8`)

**Prereq:** F7, **F8A** ✅
**Validation note:** `docs/investigations/2026-04-28-f8-validation.md`
**Branch:** `feature/f8-gatt-validation`
**Tag al cerrar:** `v2.0-f8`

**Estado 2026-04-24:** código Python terminado (enum IDs, CommandBuilder, dataclasses, 6 métodos Radio, demo, integration test, 26 unit tests PASS, marker `hardware_ble`, fix VID 0x1209). **Pendiente:** checkpoint humano T12-T13, cierre docs T14, tag T15. **Bloqueado** porque el checkpoint requiere conexión GATT sostenida — fallaba con `BLE_DONE_NOSYNC` en el primer master event. F8A desbloquea.

**Entregables firmware:** ninguno nuevo — solo debug / telemetría si hace falta.
**Entregables Python:**
- `test_connect.py` refactorizado a `python/examples/lab/demo_ble_connect_gatt.py` (D2). ✅
- Ajustes en `python/feralrf/radio.py` para exponer `ble_connect(addr, addr_type)`, `conn_status()`, `gatt_discover()`, `gatt_read(handle)`, `gatt_write(handle, data)`, `ble_disconnect()`. ✅

**Criterio de cierre:**
- Discovery completa de peripheral real devuelve ≥1 servicio y ≥1 characteristic.
- Lectura de Device Name (UUID 0x2A00) devuelve el nombre publicitado.
- Disconnect limpio: tras `CMD_DISCONNECT` se puede volver a `CMD_CONNECT` al mismo device sin reset.
- No hay leaks: `att_state` vuelve a IDLE tras `RSP_GATT_DONE`.

**Checkpoint humano:**
- Peripheral real (D1 resuelto): smartphone primario (T12), ESP32/CH573 secundario (T13).
- `demo_ble_connect_gatt.py` corrido al menos 2 veces sobre el mismo target sin reset intermedio.
- Validar en 2+ peripherals distintos si es posible.

**Riesgos específicos:**
- ATT MTU default (23 B) puede cortar characteristics largas — si falla, agregar MTU exchange.
- L2CAP SDU length mismatch puede causar `RSP_ERROR` → revisar `tx_queue.c` LLID routing.

### F9 — Fix 868→BLE PHY switch (6/6 PHYs) 🔜

**Prereq:** F6
**Branch:** `feature/f9-868-to-ble-switch`
**Tag al cerrar:** `v2.0-f9`

**Entregables firmware:**
- Root cause del rx=0 documentado en commit.
- Fix en `radio_if.c` / `phy_manager.c` (esperado: cleanup de RF state insuficiente, posible calibration stuck).
- Validation matrix TI-RTOS 6/6 (BLE→IEEE→868→IEEE→868→BLE ciclo completo).

**Entregables Python:** actualización de `tests/test_validation_matrix.py` para incluir el ciclo que fallaba.

**Criterio de cierre:** matriz 6/6 PASS en TI-RTOS, idéntica a la de `main` NoRTOS.

**Checkpoint humano:** correr matriz 3 ciclos consecutivos sobre 2 boards OTA.

**Riesgos específicos:**
- Puede ser un bug latente en precompiled libs — si es así, workaround documentado y decisión ir/no ir.

### F10 — Port props NoRTOS → TI-RTOS ✅ (tag `v2.0-f10`)

**Closed 2026-04-28.** All 16 prop presets validated 10/10 OTA on TI-RTOS, including OOK 433 (better than spec's best-effort baseline). FW size 91 KB. Closure note: `docs/investigations/2026-04-28-f10-validation.md`. Mid-session pivot: discovered hardware fault on board #1's Sub-1GHz TX path — validation completed with TX=board #2.

**Prereq:** F9
**Branch:** `feature/f10-port-props-tirtos`
**Tag al cerrar:** `v2.0-f10`

**Entregables firmware:**
- `CMD_SET_PROP_CONFIG (0x08)` funcional sobre TI-RTOS.
- Band overrides 433 / 868 / 169 MHz.
- OOK 433 / 868 con `mce_genook` + `rfe_genook` sobre TI-RTOS.
- `reset_device()` sobre TI-RTOS (via RP2040 shell).
- MSK, W-MBus T/C/N, 4-FSK / 4-GFSK presets.

**Entregables Python:** validar que `configure_prop()` + `PROP_PRESETS` funcionan sin cambios (el wire protocol es idéntico).

**Criterio de cierre:**
- 15 presets: OTA markers 10/10 por preset.
- OOK: 10/10 markers en 433 y 868 MHz.
- `reset_device()`: unlock en <2 s.
- FW size <120 KB (objetivo).

**Checkpoint humano:** validation OTA completa sobre 2 boards para cada preset.

**Riesgos específicos:**
- OOK sobre TI-RTOS puede tener timing más estricto — si falla, bypass RTOS scheduler durante OOK.
- FW size puede superar 120 KB con TI-RTOS + props + BLE central — revisar antes de F11.

### F11 — Port BLE attacks NoRTOS → TI-RTOS 🔜

**Prereq:** F8, F10
**Branch:** `feature/f11-port-ble-attacks-tirtos`
**Tag al cerrar:** `v2.0-f11`

**Entregables firmware:** ajustes si hace falta en `ll_manager.c` / `ble_conn.c` para soportar adv MAC spoofing, captura/replay, Fast Pair payload.

**Entregables Python:**
- `attacks/ble.py` portado: `beacon_flood()`, `apple_popup_spam()`, `google_popup_spam()`, `adv_spoof()`, `capture_and_replay()`.
- `demo_ble_analyzer.py`, `demo_ble_clone.py`, `demo_emulate_soundcore.py` re-validados.

**Criterio de cierre:**
- Todos los demos BLE funcionan en móvil real con mismos resultados que en `main` NoRTOS.
- Fast Pair Soundcore popup estable >60 s.
- Device clone con las 6 estrategias.

**Checkpoint humano:** correr 3 demos BLE sobre 2 boards TI-RTOS + móvil real + nRF Connect.

**Hito de migración:** al cerrar F11, `feature/ti-rtos-migration` se merge a `main`. `main` NoRTOS se tag `v1.5-legacy-final` y se retira.

---

### BLOQUE C — Features nuevas (forward)

### F12 — BLE Scanner activo 🔜

**Prereq:** F8
**Branch:** `feature/f12-ble-scanner`
**Tag al cerrar:** `v2.0-f12`

**Entregables firmware:**
- Scanner mode con `CMD_BLE5_SCANNER` (SDK), envía `SCAN_REQ` automático y captura `SCAN_RSP`.
- `RSP_RX_PACKET` extendido con flag `is_scan_rsp`.

**Entregables Python:**
- `radio.scan_ble_active(duration, channel)` — devuelve advertising + scan response merged por MAC.
- Captura nombre completo, UUIDs extra, manufacturer data completo.
- `demo_ble_scan_active.py` interactivo.

**Criterio de cierre:** scan activo sobre peripheral conocido retorna nombre completo + ≥1 UUID que no aparecía en ADV_IND solo.

**Checkpoint humano:** scan sobre 3 peripherals distintos (móvil, ESP32, dispositivo comercial).

### F13 — Retro-fill Bloque A (tests + docs) ✅ (tag `v2.0-f13`)

**Closed 2026-04-29.** 234 new unit tests added (test_props.py, test_attacks_ble.py, test_emulation.py) — all hardware-free, run on every push via existing `build.yml` CI. ARCHITECTURE.md created. Reorder of `smartrf_settings/` → `patches/` skipped: the directory is empty (TI patches live in `syscfg/` already). PYTHON_API.md and protocol.md exist and pass current API surface — light refresh deferred to F19. Prereq inversion (closed before F12) is fine — F13 work is decoupled from BLE Scanner.

**Prereq:** F12
**Branch:** `feature/f11-port-ble-attacks-tirtos` (rolled into F11 work this session)
**Tag al cerrar:** `v2.0-f13`

**Entregables:**
- **Tests faltantes:**
  - `tests/test_props.py` — presets, configure_prop, band overrides.
  - `tests/test_attacks_ble.py` — attack payload building (sin hardware).
  - `tests/test_emulation.py` — stubs y payloads.
- **Docs actualizadas:**
  - `docs/PYTHON_API.md` — API pública completa L3 + L4.
  - `docs/protocol.md` — tabla final de CMD / RSP IDs con payload schemas.
  - `docs/ARCHITECTURE.md` — diagrama de capas + reglas RF.
- **Patches propios** (`firmware/cc1352/smartrf_settings/` → `firmware/cc1352/patches/`).
- **CI:** `.github/workflows/ci.yml` que corre `pytest` + lint + build firmware por push.

**Criterio de cierre:** CI verde en PR draft.

### F14 — IEEE 802.15.4 security 🔜

**Prereq:** F10
**Branch:** `feature/f14-ieee154-attacks`
**Tag al cerrar:** `v2.0-f14`

**Entregables firmware:**
- `CMD_IEEE_CRC_CONTROL` — `txOpt.bIncludeCrc` configurable (frames con CRC custom / inválido).

**Entregables Python:**
- `attacks/ieee154.py`:
  - `disassociate(target_addr, pan_id)` — MAC disassociation notification.
  - `beacon_inject(pan_id, payload)` — beacon con PAN falso.
  - `replay(captured)` — retransmisión.
  - `pan_conflict(pan_id)` — beacons con mismo PAN ID.
  - `channel_survey(start=11, end=26)` — reconocimiento.
- `demo_ieee154_survey.py`, `demo_ieee154_attack.py`.

**Criterio de cierre:** disassociate real a un Zigbee device de test + channel survey 11–26.
**Checkpoint humano:** Zigbee device de test (CC2531 + coordinador Zigbee2MQTT o equivalente).

### F15 — Sub-1GHz security 🔜

**Prereq:** F10
**Branch:** `feature/f15-sub1ghz-attacks`
**Tag al cerrar:** `v2.0-f15`

**Entregables Python:**
- `attacks/sub1ghz.py`:
  - `ook_capture(freq, duration)` — raw OOK (requiere `reset_device()` después).
  - `ook_replay(captured)` — retransmisión.
  - `encode_ev1527(code, button)` / `decode_ev1527(capture)`.
  - `encode_pt2262(code)`.
  - `debruijn_brute(bits=12)` — fuerza bruta De Bruijn.
  - `freq_scan(start, end, step)` — RSSI proxy.
- `demo_ook_replay.py`, `demo_ook_bruteforce.py`.

**Criterio de cierre:** replay funcional de un garage/timbre de test + decode de al menos 1 captura EV1527.
**Checkpoint humano:** mando OOK de test (garage/timbre 433 MHz) disponible.

### F16 — Spectrum / RSSI 🔜

**Prereq:** F10
**Branch:** `feature/f16-spectrum`
**Tag al cerrar:** `v2.0-f16`

**Entregables firmware:**
- `CMD_GET_RSSI (0x40)` — medir RSSI en freq arbitraria (setup fast-switch CMD_FS + CMD_GET_RSSI del SDK).

**Entregables Python:**
- `radio.get_rssi(freq_hz)` — sync, devuelve dBm.
- `radio.frequency_scan(start_hz, end_hz, step_hz)` — iterador.
- `demo_spectrum.py` con visualización (matplotlib).

**Criterio de cierre:** scan 433 MHz ISM detecta señal de un mando activo con SNR >10 dB.

### F17 — Device emulation 🔜

**Prereq:** F11
**Branch:** `feature/f17-emulation`
**Tag al cerrar:** `v2.0-f17`

**Entregables Python:**
```
python/feralrf/emulation/
  ble_peripheral.py    # advertising + scan response + connection acceptor
  ieee154_device.py    # 802.15.4 beacon + data
  sub1ghz_device.py    # Sub-1GHz device emulation
  ook_device.py        # OOK/ASK device (garage, sensor)
```

**Entregables firmware:** si se requiere modo slave / peripheral BLE completo, agregar `CMD_BLE5_SLAVE` (evaluar).

**Criterio de cierre:** setup 2-board (atacante + target emulado) donde los attacks F5/F11 funcionan contra el target emulado con misma latencia.

### F18 — Jamming funcional 🔜

**Prereq:** F15
**Branch:** `feature/f18-jamming`
**Tag al cerrar:** `v2.0-f18`

**Entregables firmware:**
- Debug `CMD_TX_TEST` (modo 2.4 GHz proprietary que actualmente falla). Alternativa documentada: `CMD_PROP_TX` payload largo con `bFsOff=0`.
- `CMD_JAM_CONTINUOUS (0x30)` — jamming en cualquier freq/banda de F10.
- `CMD_JAM_REACTIVE (0x31)` — ISR en sync-word detection, target latencia <500 µs (regla #15).
- `CMD_JAM_PATTERN (0x32)` — timer-based on/off.
- `RSP_JAM_EVENT (0x95)`.

**Entregables Python:**
- `radio.jam_continuous(freq)`, `radio.jam_reactive(freq, sync_word)`, `radio.jam_pattern(freq, on_ms, off_ms)`.
- `demo_jamming.py`.

**Criterio de cierre:**
- Jamming continuo medible con otro CC1352 como víctima (delivery drop >80%).
- Reactive jamming <500 µs entre sync detect y TX start (medido con osciloscopio o 2-board timestamping).

**Checkpoint humano:** osciloscopio o 2 boards con timestamping RP2040 para medir latencia.

**Riesgos específicos:**
- R11 (ISR latency en TI-RTOS): si <500 µs no se logra, evaluar bypass del scheduler en ISR directo.

### BLOQUE D — Cobertura completa CC1352 (API gaps)

> **Razón:** el goal de FeralRF es exponer **toda** la superficie RF del CC1352P7 vía Python, no solo capacidades para attacks. Bloques A–C cubren ~75% del chip; este bloque cierra el 25% restante. Agregado 2026-04-29 tras alineación con Sabas.
>
> **Regla:** protocolos de capa alta (Zigbee, Thread, Matter, 6LoWPAN, Wireless M-Bus, Wi-SUN, MIOTY, Sidewalk) son frame-crafting en Python sobre PHY raw — NO son fases nuevas, ya son accesibles vía F1/F10 + custom config. Lo que entra a Bloque D son capacidades **del chip** que el firmware aún no expone.

### F20 — BLE Peripheral + GATT server (rol completo) 🔜

**Prereq:** F8 (GATT client validado), F21 (connectable advertiser)
**Branch:** `feature/f20-ble-peripheral`
**Tag al cerrar:** `v2.0-f20`

**Entregables firmware:**
- `CMD_BLE_PERIPHERAL_START (0x50)` — entra a rol peripheral con tabla GATT.
- `CMD_GATT_SERVE_TABLE (0x51)` — define services/chars/descriptors estáticos.
- ATT server: handle MTU exchange, Read Req, Write Req, Read Blob, Read Multiple, Write Cmd, HVN/HVI (notify/indicate).
- L2CAP fixed channels reuse del F8A.
- Connection management: aceptar `CONNECT_IND`, transitar a estado conectado, manejar terminate.

**Entregables Python:**
- `radio.serve_gatt(table)` — `table` es lista de service/char/descriptor con permisos.
- `radio.notify(handle, data)` / `radio.indicate(handle, data)`.
- `demo_gatt_server.py` con perfil de ejemplo.

**Criterio de cierre:**
- nRF Connect (Android/iPhone) descubre todos los services/chars expuestos.
- Read/Write/Notify funcionan end-to-end con valores correctos.
- Conexión sostiene 60 s sin terminate por timeout.

**Checkpoint humano:** móvil con nRF Connect.

**Riesgos específicos:**
- ATT server stack es trabajo grande (~1500 LOC) — considerar reuso de TI BLE5-Stack si OneLib.a se valida en el contexto Sniffle-style.

### F21 — BLE Connectable advertiser 🔜

**Prereq:** F11 (BLE attacks port done)
**Branch:** `feature/f21-ble-conn-adv`
**Tag al cerrar:** `v2.0-f21`

**Entregables firmware:**
- `CMD_BLE_ADV_IND (0x52)` — ADV_IND (general connectable + scannable).
- `CMD_BLE_ADV_DIRECT (0x53)` — ADV_DIRECT_IND (directed connectable, low-duty + high-duty modes).
- `CMD_BLE_ADV_SCAN_IND (0x54)` — ADV_SCAN_IND (scannable no-conn).
- Acepta `CONNECT_IND` y entrega control a F20 (peripheral) o termina si F20 no activo.

**Entregables Python:**
- `radio.advertise_ind(payload, scan_resp_data, ...)`.
- `radio.advertise_direct(target_addr, mode='low'|'high')`.
- `radio.advertise_scan_ind(payload, scan_resp_data)`.

**Criterio de cierre:**
- nRF Connect ve cada PDU type correctamente clasificado (ADV_IND vs ADV_DIRECT_IND vs ADV_SCAN_IND).
- Para ADV_IND: scanner activo recibe `SCAN_REQ` y este FW emite `SCAN_RSP` con scan_resp_data.

**Checkpoint humano:** nRF Connect en móvil.

### F22 — Test mode CW + PRBS 🔜

**Prereq:** F10 (props validados)
**Branch:** `feature/f22-test-modes`
**Tag al cerrar:** `v2.0-f22`

**Entregables firmware:**
- `CMD_TX_CW (0x55)` — `rfc_CMD_TX_TEST` con `whitening=0`, modo unmodulated carrier en freq arbitraria.
- `CMD_TX_PRBS (0x56)` — `rfc_CMD_TX_TEST` con whitening PRBS-9/PRBS-15 modulado.
- `CMD_TX_TEST_STOP (0x57)` — termina cualquier modo test.

**Entregables Python:**
- `radio.tx_cw(frequency_hz, power_dbm)`.
- `radio.tx_prbs(frequency_hz, power_dbm, pattern='prbs9'|'prbs15')`.
- `radio.tx_test_stop()`.

**Criterio de cierre:**
- Otro CC1352 en RX modo RSSI ve carrier estable (CW) o señal modulada continua (PRBS) en la freq elegida.
- Spectrum analyzer (si disponible) confirma single-tone (CW) o spread sin huecos (PRBS).

**Checkpoint humano:** opcional — spectrum analyzer ideal, fallback con 2 boards y `radio.start_rx + read_packets`.

### F23 — High PA +15 a +20 dBm 🔜

**Prereq:** F10
**Branch:** `feature/f23-high-pa`
**Tag al cerrar:** `v2.0-f23`

**Entregables firmware:**
- Fix DIO29 antenna switch control (PIN config + GPIO drive durante TX high-PA).
- Path TX overrides `pRegOverrideTx20` activado cuando `power_dbm >= 15` (aplica a BLE/IEEE/Prop con `frontEndMode=0x0`, `biasMode=0x1`).
- Verificar en runtime que `RFC_PA_TYPE_HIGH` está seleccionado.

**Entregables Python:**
- `set_power(dbm)` acepta hasta +20 dBm; selecciona high-PA path automáticamente.
- `radio.get_pa_type()` retorna `'std'` o `'high'` según power último.

**Criterio de cierre:**
- TX +20 dBm medible: 2 boards, RX board reporta RSSI con delta ≥+18 dB respecto a TX 0 dBm en misma freq y distancia.
- Sin hangs / spurious / cambio de freq.

**Checkpoint humano:** 2 boards CatSniffer + ideal spectrum analyzer.

**Riesgos específicos:**
- Resuelve R4 (planeado fuera de scope v2.0 originalmente — promovido a Bloque D 2026-04-29).
- DIO29 puede tener conflicto de uso (verificar en `hardware/PINOUT.md`).

### F24 — DMM concurrent BLE + IEEE 🔜

**Prereq:** F11, F1 (IEEE validado)
**Branch:** `feature/f24-dmm`
**Tag al cerrar:** `v2.0-f24`

**Entregables firmware:**
- Integrar TI DMM (Dual Mode Manager) o implementación reducida tipo time-slicing.
- `CMD_START_CONCURRENT (0x58)` — argumentos: PHY1, PHY2, slice_ms.
- Coordinar `RF_RadioSetup` entre los dos modos sin re-RF_open.

**Entregables Python:**
- `radio.start_concurrent(phy1=PHY.BLE_1M, phy2=PHY.IEEE_802_15_4, slice_ms=20)`.
- `read_packets()` retorna paquetes etiquetados con su PHY de origen.

**Criterio de cierre:**
- Concurrent RX BLE adv ch37 + IEEE ch20: en 30 s captura ≥80% de los paquetes que cada PHY captura por separado.
- Sin pérdida total en una de las PHYs (no starvation).

**Checkpoint humano:** beacon BLE conocido + emisor IEEE conocido.

**Riesgos específicos:**
- DMM SDK puede requerir RF_MODE específico; validar contra `multi_protocol` patch.

### F25 — Crypto HW expuesto (TRNG + AES + PKA) 🔜

**Prereq:** independiente
**Branch:** `feature/f25-crypto-hw`
**Tag al cerrar:** `v2.0-f25`

**Entregables firmware:**
- TRNG: fix PERIPH power domain (resuelve R6 / `feedback_trng_hang.md` permanentemente).
- `CMD_RANDOM (0x59)` — retorna N bytes aleatorios (1..256).
- `CMD_AES_ECB (0x5A)`, `CMD_AES_CCM (0x5B)` — AES-128 vía driverlib.
- `CMD_PKA_ECDH (0x5C)` — ECC P-256 ECDH para casos curve25519/p256.

**Entregables Python:**
- `radio.random_bytes(n)`.
- `radio.aes_encrypt(key, data, mode='ecb'|'ccm', nonce=...)`.
- `radio.ecdh_p256(my_priv, peer_pub)`.

**Criterio de cierre:**
- TRNG pasa monobit + runs tests sobre 1 MB de output.
- AES vectors NIST validados (encrypt + decrypt round-trip).
- ECDH genera shared secret consistente con biblioteca host (cryptography Python).

**Checkpoint humano:** ninguno — todo unit-test desde host.

### F26 — Proprietary 2.4 GHz como PHY normal 🔜

**Prereq:** F10
**Branch:** `feature/f26-prop-24ghz`
**Tag al cerrar:** `v2.0-f26`

**Entregables firmware:**
- Nuevo `RadioIF_RfMode` value `RADIO_IF_RF_MODE_PROP_2_4GHZ`.
- `RF_RadioSetup` para 2.4 GHz prop (`rfc_CMD_PROP_RADIO_DIV_SETUP_PA` con `centerFreq=2440`, `loDivider=0x05`).
- Path en `radio_if.c` para TX/RX con FSK/GFSK/MSK/4-FSK 2400-2483.5 MHz.
- `PHY` enum agrega `PHY_MANAGER_PHY_PROP_2_4GHZ`.
- `CMD_SET_PHY` acepta el nuevo PHY + frequency_hz en banda 2.4 GHz.

**Entregables Python:**
- `PHY.PROP_2_4GHZ`.
- `set_phy(PHY.PROP_2_4GHZ, frequency_hz=2440000000)`.
- `configure_prop()` también soporta 2.4 GHz (sym rate + deviation + sync word).
- Documentación: protocolos típicos en banda (Nordic ESB, Bluetooth Mesh PB-ADV custom, drones RC, etc).

**Criterio de cierre:**
- TX/RX entre 2 boards en 2440 MHz GFSK 250 kbps con marker test: 10/10 markers.
- TX/RX con sym rate 1 Mbps custom: 10/10 markers.
- Refactor coordinado: F18 jamming usa esta PHY en vez de configurar desde cero (out of scope para F26 — solo deja la PHY lista).

**Checkpoint humano:** 2 boards CatSniffer.

### F27 — IEEE 802.15.4 Sub-GHz (15.4g) 🔜

**Prereq:** F10
**Branch:** `feature/f27-ieee-subghz`
**Tag al cerrar:** `v2.0-f27`

**Entregables firmware:**
- `RadioSetup` IEEE 802.15.4g (GFSK Sub-G) en `smartrf_ieee_subghz.c`.
- Path en `radio_if.c` para selectivar entre 2.4 GHz y Sub-G según frequency_hz.
- Channel pages 2/9 (902-928 MHz GFSK 50/100/200 kbps).

**Entregables Python:**
- `set_phy(PHY.IEEE_802_15_4, frequency_hz=915000000)` — selecciona Sub-G mode.
- `radio.get_ieee_mode()` retorna `'2.4ghz'` o `'sub_g_915'`.

**Criterio de cierre:**
- TX/RX entre 2 boards en 915 MHz IEEE 15.4g: 10/10 markers con frame válido (PHY header + MHR + payload + FCS).

**Checkpoint humano:** 2 boards.

### F28 — AIS RX 162 MHz 🔜

**Prereq:** F10
**Branch:** `feature/f28-ais-rx`
**Tag al cerrar:** `v2.0-f28`

**Entregables firmware:**
- Preset prop 162 MHz GFSK 9600 baud, h=0.5 (≈ GMSK), `loDivider=0x1E`.
- Adaptación de overrides 169 MHz a 162 MHz.
- Demodulador HDLC-like para frame AIS (start flag 0x7E, NRZI, scrambling, CRC-16).

**Entregables Python:**
- `radio.start_rx_ais()` — wraps set_phy con preset AIS y RX continuo.
- Parser AIS en Python: extrae MMSI, nav status, lat/lon, speed/course de mensajes 1/2/3.
- `demo_ais_receiver.py` con plot de tracks.

**Criterio de cierre:**
- RX-only (TX requiere licencia marítima — documentado en `docs/SAFETY.md`).
- Captura ≥1 mensaje AIS válido (CRC OK) con MMSI + posición decoded.
- Test en condición controlada con simulador de señal AIS si no hay tráfico marítimo cerca.

**Checkpoint humano:** AIS RF generator (e.g., GNU Radio + USRP) o ubicación con tráfico marítimo.

**Riesgos específicos:**
- Antena CatSniffer no optimizada para 162 MHz; sensitivity puede ser pobre.
- Si sensitivity insuficiente, marcar feature como "experimental — antenna external recomendada" sin bloquear release.

### F29 — Presets Wi-SUN / MIOTY / Sidewalk Sub-G 🔜

**Prereq:** F10
**Branch:** `feature/f29-stack-presets`
**Tag al cerrar:** `v2.0-f29`

**Entregables firmware:**
- Presets en `radio_if_props.c`:
  - `WI_SUN_FAN_1_0` — FSK 2-FSK 50/100/150/200/300 kbps en 902-928 MHz, channel plan FAN 1.0.
  - `MIOTY_TS_UNB` — TS-UNB modulation (sym rate 396 baud, deviation TBD, banda 868 MHz EU / 915 MHz US).
  - `SIDEWALK_FSK_50K` — FSK 50 kbps 902-928 MHz (Amazon Sidewalk Sub-G FSK layer).
  - `SIDEWALK_FSK_250K` — FSK 250 kbps variant.
- Documentar que **Sidewalk LR (LoRa-like) NO es soportado por CC1352** — usa SX1262 en Cat-LoRa port.

**Entregables Python:**
- `PROP_PRESETS` agrega 4 entradas con metadata.
- `demo_wisun_scan.py`, `demo_mioty_listen.py`, `demo_sidewalk_subg.py` (cada uno set_phy con preset + start_rx + parse mínimo).

**Criterio de cierre:**
- Cada preset: TX/RX entre 2 boards con marker test, 10/10 markers.
- Para MIOTY: validar al menos que sym rate + deviation están dentro de spec TS-UNB.

**Checkpoint humano:** 2 boards (suficiente para markers); validación contra dispositivos reales (Wi-SUN node, MIOTY base station, Sidewalk gateway) opcional para v2.1.

**Riesgos específicos:**
- MIOTY: TS-UNB es modulation no estándar GFSK pura — investigar si CC1352 soporta nativamente o requiere truco.

---

### F19 — Release v2.0 🔜

**Prereq:** F29
**Branch:** `release/v2.0`
**Tag al cerrar:** `v2.0.0`

**Entregables:**
- Docs completa (ARCHITECTURE, PROTOCOL, PYTHON_API, MIGRATION_GUIDE_NOROTOS, SAFETY).
- CHANGELOG completo desde `v1.5-legacy-final`.
- Benchmarks:
  - Latencia trigger → reactive jam TX start.
  - Throughput RSSI scan (samples/s).
  - GATT discovery time típico.
  - FW size final.
- GitHub Release `v2.0.0` con UF2 RP2040 + `.hex` CC1352 + wheel Python.
- PyPI publish `feralrf` versión decidida en D5.

---

## 6. Governance

### 6.1 Reglas por fase

1. Cada fase termina en **commit + tag anotado** `v2.0-fN`.
2. **No se toca F(N+1) hasta que F(N) esté verde** (criterio cumplido + checkpoint humano firmado).
3. **Claude Code para y reporta** al cerrar cada fase: entregables, checkpoints pendientes, dudas abiertas.
4. **Pre-commit obligatorio** antes de cada commit. Nunca `--no-verify`.
5. **Flash con `.hex` vía catnip.** Nunca `.bin` (decisión #17).
6. **Retry flash 2× antes de pedir reset manual** (feedback memory).
7. **Validación OTA con markers** es el gate de features RF (10/10 markers = pasa).

### 6.2 Branching

- Trabajo por fase en rama `feature/fN-<slug>` a partir de `feature/ti-rtos-migration`.
- Merge a `main` solo tras checkpoint humano.
- **Post-F11:** `feature/ti-rtos-migration` se merge a `main`. `main` NoRTOS se tag `v1.5-legacy-final` y se retira como línea activa.

### 6.3 Skill por fase

Mantener `.claude/skills/feralrf-fase-actual/SKILL.md` actualizado al arrancar cada fase (estado / qué no tocar / criterios de salida). Ver template en Sección 11.

### 6.4 Skill invariante

`ti-rtos-rf-cc1352` (ya existe) con las 10 reglas RF validadas — no tocar sin validación física.

### 6.5 Convenciones de commit

- Mensaje corto primera línea (<72 chars), prefijo `feat:` / `fix:` / `docs:` / `chore:` / `refactor:`.
- Cuerpo explica *por qué* (no *qué*).
- Commits de fase referencian la fase: `feat(f10): port MSK preset to TI-RTOS`.
- Trailer `Co-Authored-By: Claude <noreply@anthropic.com>` si Claude generó el commit.

---

## 7. Riesgos

| # | Riesgo | Nivel | Mitigación | Se resuelve en |
|---|--------|-------|-----------|----------------|
| R1 | `868→BLE` PHY switch falla | Alto | Fase dedicada a root cause + fix | F9 |
| R2 | GATT discovery no validado con peripheral real | Alto | Bloquea BLE Scanner y port attacks. **Actualizado 2026-04-24:** causa raíz = CMD_BLE5_INITIATOR timing incompatible con peer. Requiere rewrite Sniffle-style. | F8A → F8 |
| R3 | Port NoRTOS→TI-RTOS rompe features maduros | Medio | Validation matrix OTA en cada port, 10/10 markers como gate | F10/F11 |
| R4 | High PA (+15–20 dBm) requiere fix DIO29 antenna switch | Bajo | Fuera de scope v2.0; abierto para v2.1. TX cap +14 dBm | — |
| R5 | OOK bloquea radio (TI SDK bug) | Bajo | `reset_device()` via RP2040 ya validado en main; portar igual a TI-RTOS | F10 |
| R6 | TRNG hang (PERIPH power domain) | Resuelto | Workaround xorshift32 PRNG | cerrado |
| R7 | `CMD_TX_TEST` (2.4 GHz proprietary jamming) no funciona | Medio | Debug + alternativa `CMD_PROP_TX` payload largo | F18 |
| R8 | CC1352 memoria (static only) — growth con ATT/GATT + scanner + emulation | Medio | Monitoreo tamaño por fase (objetivo <120 KB); feature flags si hace falta | cada fase |
| R9 | Antena CatSniffer limita 433 MHz y <430 MHz | Documentado | No se intentan 169/315/390/470 MHz. 433 con pérdidas aceptado. | — |
| R10 | SDK 8.30 pineado — si aparece bug upstream, bloqueamos | Bajo | No upgrade sin fase dedicada (regla decisión #2) | — |
| R11 | Reactive jamming <500 µs — ISR latency puede no ser suficiente en TI-RTOS | Medio | Medir en F18; plan B: bypass RTOS scheduler con ISR directa | F18 |
| R12 | Test hardware = múltiples boards + móvil + peripheral GATT + target para attacks | Bajo | Lista de HW requerido por fase en Sección 5 | cada fase |

---

## 8. Migración NoRTOS → TI-RTOS

### 8.1 Política

- **`main` NoRTOS queda como `legacy/nortos`** en tag `v1.5-legacy-final` post-F11.
- **`feature/ti-rtos-migration` es el nuevo `main`** después de F11.
- **No se hacen cherry-picks hacia atrás** — bugfixes post-F11 van solo a TI-RTOS.

### 8.2 Tabla de port

| Feature en `main` NoRTOS | Destino TI-RTOS | Fase |
|--------------------------|------------------|------|
| 8 PHYs TX/RX base | ti-rtos-migration cubre 5/6 | F6 / F9 |
| `configure_prop()` + 15 presets | port | F10 |
| MSK, W-MBus T/C/N, 4-FSK / 4-GFSK | port | F10 |
| OOK 433 / 868 + genook patches | port | F10 |
| `reset_device()` recovery | port | F10 |
| BLE attacks (beacon / apple / google / spoof / replay) | port | F11 |
| Device clone + Fast Pair Soundcore | port | F11 |
| Python demos (demo_ble_*, demo_ook_*) | validar sobre TI-RTOS | F10 / F11 |
| Python API `feralrf` package | mismo wire protocol → cambio transparente | — |

### 8.3 Criterio de "migración completa"

Post-F11: todos los tests OTA pasan sobre TI-RTOS con la misma matriz que en `main` NoRTOS (validation matrix 18/18, soak 5 min 0 errs, 10/10 markers por preset, demos BLE validados en móvil). Solo entonces se retira NoRTOS como línea activa.

---

## 9. Decisiones pendientes

Preguntas que este plan deja abiertas. Se resuelven en la fase indicada.

| # | Decisión | Resuelto en | Responsable |
|---|----------|-------------|-------------|
| D1 | ~~Qué peripheral real usar para validar GATT~~ **Resuelto 2026-04-24:** smartphone como primario (T12), ESP32/nRF52840 como segundo peripheral (T13). | F8 ✅ | Sabas |
| D2 | ~~Ubicación final de `test_connect.py`~~ **Resuelto 2026-04-24:** `python/examples/lab/demo_ble_connect_gatt.py` (consistente con `demo_ble_analyzer.py`, `demo_ble_clone.py`). | F8 ✅ | Sabas |
| D3 | Si BLE Scanner (F12) va antes o después de los ports (F10 / F11) | F8 (al arrancar F11 o F12) | Sabas |
| D4 | VID / PID del dispositivo USB (cambiar para distinguir v2.0 o mantener) | F19 | Sabas |
| D5 | Versionado Python package (`feralrf` 0.2.0 → 1.0.0 con firmware v2.0?) | F19 | Sabas |
| D6 | Formato de evidencia OTA (log texto vs JSON vs test runner) | F10 | Claude propone |
| D7 | Monitoreo tamaño firmware — gate automático por fase o revisión manual | F9 (durante consolidación) | Claude propone |

---

## 10. Prompt inicial para Claude Code

Pega esto como **primer mensaje** al arrancar F8 (primera fase activa del plan):

```
Vamos a ejecutar el plan maestro v2.0 de FeralRF. El plan completo
está en docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md
— léelo ENTERO antes de escribir una línea de código.

Contexto irrevocable (decisiones congeladas — no re-litigar, Sección 1):
  - MCU: CC1352P7 único. SDK TI 8.30.01.01 pineado. TI-RTOS 7 única
    línea; NoRTOS se retira post-F11.
  - Protocolo: COBS + CRC16 single CDC. Python API sync primario.
  - Alloc estático. 8 PHYs obligatorios. RX buffer 16 KB.
  - TX power −20 a +14 dBm (High PA diferido).
  - Branching: fase por rama, merge a main solo con checkpoint humano.
  - Flash con .hex vía catnip, nunca .bin.
  - GATT raw RF Sniffle-style, NO ICall/BLE5-Stack.
  - 10 reglas RF validadas (skill ti-rtos-rf-cc1352) — invariante.

Metodología:
  1. Fase por fase F0→F19. Fases F0-F7 son históricas (ya hechas);
     empezamos en F8.
  2. Cada fase termina con commit + tag v2.0-fN.
  3. Al cerrar cada fase paras y reportas:
     (a) entregables,
     (b) checkpoints físicos que debo correr yo,
     (c) dudas abiertas / decisiones pendientes tocadas.
     Esperas mi confirmación antes de F(N+1).
  4. No saltas fases. Si F(N) no está verde, no tocas F(N+1).
  5. Pre-commit obligatorio. Nunca --no-verify.
  6. Retry flash 2× antes de pedir reset manual.

Empieza con F8 — Validar GATT end-to-end.
Primero:
  1. Leeme el plan resumido (máx 10 líneas).
  2. Confirma que entendiste las 21 decisiones congeladas.
  3. Revisa el estado actual (git status, últimos commits de
     feature/ti-rtos-migration, test_connect.py).
  4. Propón el plan DETALLADO de F8 sin escribir código:
     - dónde vive test_connect.py tras el commit (D2).
     - qué peripheral real vas a pedirme para validar (D1).
     - exposición Python (signatures de connect/gatt_discover/
       gatt_read/gatt_write/disconnect en radio.py).
     - criterios de aceptación físicos.
  5. Yo apruebo o ajusto, y recién ahí ejecutas.
```

---

## 11. Template de skill por fase

Crear/actualizar `.claude/skills/feralrf-fase-actual/SKILL.md` al arrancar cada fase. Ejemplo para F8:

```markdown
---
name: feralrf-fase-actual
description: Contexto activo del plan FeralRF v2.0. Consultar antes de cualquier commit, creación de archivo, o cambio de dirección.
---

# Fase actual: F8 — Validar GATT end-to-end

## Qué está hecho (histórico)
- F0–F5: setup + 8 PHYs + props + OOK + estabilidad + BLE attacks (main NoRTOS).
- F6: TI-RTOS baseline 5/6 PHYs (blocker 868→BLE, se resuelve en F9).
- F7: BLE central + ATT/GATT client construido (no validado).

## En qué estamos
Validar discovery y read/write GATT contra peripheral real.
test_connect.py está sin commitear — hay que decidir ubicación final (D2).

## Qué NO tocar
- att_client.c / ble_conn*.c (construidos en F7, no modificar sin datos).
- radio_if.c / phy_manager.c (868→BLE blocker es F9, no F8).
- BLE attacks Python (F11).

## Salida de F8
- [ ] test_connect.py commiteado en ubicación decidida (D2).
- [ ] Peripheral real usado documentado (D1).
- [ ] Discovery + read Device Name funcional.
- [ ] Disconnect limpio (CMD_CONNECT tras CMD_DISCONNECT sin reset).
- [ ] Commit + tag v2.0-f8.

## Reglas extra de esta fase
- No agregar Scanner (F12) ni port attacks (F11) en esta rama.
- Debug counters en CONN_STATUS se pueden quitar tras validación.
```

---

## 12. Resumen de entregables de esta spec

1. **Este documento** — plan maestro v2.0 del proyecto FeralRF.
2. **Sección 10** — prompt inicial a copiar al arrancar F8.
3. **Sección 11** — template de skill por fase.
4. **Actualización de `docs/PLAN_MAESTRO.md`** — deprecar y apuntar a este spec (pendiente de F8).
5. **Actualización de `CLAUDE.md`** — nuevos phases y decisiones (pendiente de F8).
