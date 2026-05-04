# F17 — Device Emulation (BLE + IEEE154 + Sub-1GHz + OOK)

**Date:** 2026-05-04
**Branch (target):** `feature/f17-emulation` cut from `main` HEAD=`149a808`
**Tag (target):** `v2.0-f17`
**Source:** Master plan v2.0 §F17 (`docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`).

## Goal

Crear `python/feralrf/emulation/` package con 4 módulos — BLE peripheral
(advertising-only), IEEE 802.15.4 device, Sub-1GHz device, OOK device — con
11 personalidades total. Cada personalidad transmite payload PHY-level
canónico vía burst (count + interval). Validación: 2-board smoke V1 donde
RX board verifica que el TX board (corriendo emulación) emite signature
detectable. Cero cambios firmware.

## Scope decisions (brainstorm 2026-05-04)

- **B1**: BLE peripheral solo advertising (no F20/F21 dependency). Reusa
  `attacks/ble.py adv_spoof` path para TX.
- **E3**: Los 4 módulos en vuelta 1.
- **C2**: 11 personalidades total — BLE (3) + IEEE154 (2) + Sub-1GHz (3) + OOK (3).
- **V1**: Scanner cross-validation entre 2 boards. Sin phone, sin attacks loop.
- **M2**: Burst fijo `count + interval_ms`. Caller envuelve en loop si necesita
  modo continuo.

## Bundle layout

| Bundle | Cambios | Commits |
|--------|---------|---------|
| 1 — BLE peripheral | `emulation/ble_peripheral.py` + tests + smoke segment | 1 |
| 2 — IEEE154 device | `emulation/ieee154_device.py` + tests + smoke segment | 1 |
| 3 — Sub-1GHz device | `emulation/sub1ghz_device.py` + tests + smoke segment | 1 |
| 4 — OOK device | `emulation/ook_device.py` + tests + smoke segment | 1 |
| 5 — Smoke + demos + package wiring | `smoke_f17_emulation.py`, `demo_emulate_subg_sensor.py`, `demo_emulate_ook_garage.py`, `emulation/__init__.py` | 1-2 |
| Final | Tag + memory + FF | — |

Total esperado: 5-7 commits + 1 tag.

## Package structure

```
python/feralrf/emulation/
  __init__.py              # re-export public API: BlePersonality, *_PERSONALITIES, emulate
  ble_peripheral.py        # P1-P3
  ieee154_device.py        # I1-I2
  sub1ghz_device.py        # S1-S3
  ook_device.py            # O1-O3
```

`__init__.py` re-exports:
```python
from feralrf.emulation.ble_peripheral import (
    BlePersonality,
    SOUNDCORE_BOOM_2,
    APPLE_AIRPODS_PRO,
    GOOGLE_FASTPAIR_GENERIC,
    BLE_PERSONALITIES,
    emulate as emulate_ble,
)
# similar for ieee154/sub1ghz/ook
```

Each module exposes:
- 1 `@dataclass(frozen=True)` for the Personality type
- N personalidad constants
- 1 list `<PROTO>_PERSONALITIES` with all
- 1 function `emulate(radio, personality, count: int = 50, interval_ms: int = 100) -> int`

## Personality definitions

### BLE peripheral — 3 personalities

| Sigla | Constant | Identity |
|-------|----------|----------|
| P1 | `SOUNDCORE_BOOM_2` | Anker Mfg (`0x05DA`) + Fast Pair Model ID `0x8F95F8` (already pinned in `test_emulation.py`) |
| P2 | `APPLE_AIRPODS_PRO` | Apple Mfg (`0x004C`) + Proximity (`0x0220` model byte stream) |
| P3 | `GOOGLE_FASTPAIR_GENERIC` | UUID `0xFE2C` + configurable `model_id` (default `0x2C01A2`) |

`BlePersonality`:
```python
@dataclass(frozen=True)
class BlePersonality:
    name: str
    target_mac: bytes  # 6-byte LE
    advertising_payload: bytes  # post-PDU-header advertising data
```

`emulate(radio, p, count, interval_ms)` internamente: para cada iteración,
`radio.set_ble_addr(p.target_mac)` + `attacks.ble.adv_spoof(radio, p.advertising_payload, count=1)`.
Patrón burst con `time.sleep(interval_ms/1000.0)` entre.

### IEEE 802.15.4 device — 2 personalities

| Sigla | Constant | Identity |
|-------|----------|----------|
| I1 | `BEACON_COORDINATOR` | PAN ID `0x1234`, short addr `0x0001`, channel 15, beacon frame |
| I2 | `DATA_POLL_END_DEVICE` | PAN ID `0x1234`, short addr `0x0042`, channel 15, data frame |

`Ieee154Personality`:
```python
@dataclass(frozen=True)
class Ieee154Personality:
    name: str
    pan_id: int
    short_addr: int
    channel: int  # 11/15/20/25
    payload: bytes  # PHY-layer frame WITHOUT CRC (firmware adds)
```

`emulate(radio, p, count, interval_ms)` configura `radio.set_phy(PHY.IEEE154_2_4, channel=p.channel)` + `radio.transmit(p.payload)` × count.

Si `PHY.IEEE154_2_4` no existe en enums.py (verificar), fallback a
`PHY.PROPRIETARY_GFSK` + preset adhoc en banda 2.4 GHz con sync word `0xA7`
(IEEE 802.15.4 SHR).

### Sub-1GHz device — 3 personalities

| Sigla | Constant | Preset | Payload |
|-------|----------|--------|---------|
| S1 | `GFSK_868_SENSOR` | `gfsk_868_50k` | `device_id(2) + temp/humid(5) + checksum(2) = 9 bytes` |
| S2 | `GFSK_433_SENSOR` | `gfsk_433_50k` | similar a S1 con device_id distinto |
| S3 | `WMBUS_T1_METER` | `wmbus_868_t1` si existe, else `msk_868_50k` | preamble + len + ctrl + addr + payload + CRC stub |

`Sub1GhzPersonality`:
```python
@dataclass(frozen=True)
class Sub1GhzPersonality:
    name: str
    preset_name: str  # de PROP_PRESETS
    payload: bytes
```

`emulate(radio, p, count, interval_ms)` llama
`radio.set_phy(PHY.PROPRIETARY_GFSK, 0)` + `radio.configure_prop(**PROP_PRESETS[p.preset_name])` + TX × count.

### OOK device — 3 personalities

| Sigla | Constant | Preset | Payload |
|-------|----------|--------|---------|
| O1 | `PT2262_GARAGE_433` | `ook_433_4k8` | 24-bit fixed code en 12-trit encoding |
| O2 | `EV1527_SENSOR_433` | `ook_433_2k4` | 20-bit ID + 4-bit data |
| O3 | `HORMANN_GARAGE_868` | `ook_868_4k8` | 64-bit fixed Hörmann frame |

`OokPersonality` igual a `Sub1GhzPersonality` (mismo shape).

`emulate(radio, p, count, interval_ms)` mismo patrón que Sub1G + `radio.reset_device()` al final (memoria `feedback_workflow` — OOK locks radio).

## Tests

### Unit tests — extend `python/tests/test_emulation.py`

Para cada protocolo, parametrize sobre `<PROTO>_PERSONALITIES`:
- `test_<proto>_personality_payload_well_formed` — payload no vacío, len > 0
- `test_<proto>_personality_name_set` — `name` string non-empty
- `test_<proto>_personality_specific_field` — field protocol-specific assertions

Plus tests específicos:
- BLE Soundcore: pin manufacturer ID + Fast Pair Model ID (heredado de F13)
- BLE AirPods: Apple Mfg ID `0x004C` + Proximity sub-type
- IEEE154: FCF first 2 bytes per frame type
- Sub1G: payload length matches expected per device type
- OOK: payload bit count matches PT2262 (24-bit) / EV1527 (24-bit) / Hörmann (64-bit)

Total: ≥ 30 nuevos parametrize hits + ~10 specific asserts.

### Integration smoke V1 — `python/examples/smoke_f17_emulation.py`

Patrón derivado de `smoke_f29_subg_915.py` con retry on TimeoutError.

```
Per protocol:
    for personality in <PROTO>_PERSONALITIES:
        Board #2: configure RX path (set_phy, configure_prop, start_rx)
        Board #1: emulate(personality, count=20, interval_ms=200)
        sleep(...)
        Board #2: collect packets via read_packets(timeout=2.0)
        assert >= threshold packets matching personality signature
        Board #2: stop_rx
        if OOK: reset_device on both
```

Thresholds por protocolo:
- BLE: ≥ 1 ScanResult con manufacturer_data matching identity
- IEEE154: ≥ 8/20 packets con CRC OK + FCF byte match
- Sub-1GHz: ≥ 8/20 markers payload match
- OOK: ≥ 5/20 markers (demodulation menos confiable)

Pass total: 11/11 personalidades pasan.

## Demos

`python/examples/lab/` — 2 nuevos demos (Soundcore demo ya existe):

```python
# demo_emulate_subg_sensor.py
# argparse + load S1 default + emulate loop wrapper sobre M2 burst

# demo_emulate_ook_garage.py
# argparse + load O1 default + emulate loop + reset_device cleanup
```

Validación: ast.parse OK + `--help` exit 0.

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| `PHY.IEEE154_2_4` no existe en enums.py | I1/I2 fail al `set_phy` | Verificar al inicio de Bundle 2; fallback `PROPRIETARY_GFSK` con preset 2.4 GHz + sync word `0xA7` |
| `wmbus_868_t1` preset no existe en `PROP_PRESETS` | S3 fail al `configure_prop` | Fallback `msk_868_50k` (memoria menciona MSK validado F2); si tampoco existe, `gfsk_868_50k` |
| OOK locks radio entre personalidades | TX falla en personality post-OOK | `emulate(O*)` cierra con `radio.reset_device()` |
| Smoke 11 personalidades × ~30s = ~5-6 min largo | Timeout transitorio | Retry pattern F29.b (2 intentos on TimeoutError) — copiar el helper a smoke_f17 |
| IEEE154 channel conversion (channel 11 = 2405 MHz, etc) | TX en frecuencia incorrecta | Confirmar que `set_phy(PHY.IEEE154_2_4, channel=N)` ya hace la conversion en firmware (per F12) |
| BLE adv_spoof reusa `set_ble_addr` por iteration | Race entre TX y next iter | adv_spoof actual tiene timing safe; verificar que count=1 por iteración no rompe |
| Pre-commit black auto-format dataclass blocks | Reformat | Aceptar reformat |

## Acceptance criteria

- ✅ Package `python/feralrf/emulation/` con 4 módulos + `__init__.py`
- ✅ 11 personalidades (3 BLE + 2 IEEE154 + 3 Sub-1GHz + 3 OOK)
- ✅ Cada módulo expone `emulate(radio, personality, count, interval_ms)` API
- ✅ Tests unitarios ≥ 30 parametrize hits, ≥ 575 pass total (era 545)
- ✅ Smoke V1 cross-validation: 11/11 personalidades pasan, total runtime ≤ 7 min
- ✅ 2 nuevos demos lab pasan ast.parse + --help
- ✅ Pre-commit clean en todos los commits
- ✅ Tag `v2.0-f17` en HEAD final
- ✅ Memory entry `project_f17_done.md`
- ✅ FF merge a `main`

## Out of scope

- F20 / F21 (BLE peripheral connectable + GATT server) — fases separadas
- Phone validation (V2) — already retroactively validated en F11b
- Real-device interop validation con sniffer comercial — v2.1
- Reverse engineering de devices reales (capturar real Hörmann y replicar rolling code) — payloads son canónicos
- Rolling code / encryption / handshakes — PHY-only per `feedback_protocol_vs_phy`
- Más de 11 personalidades (Tile, Galaxy Buds, generic discoverable, sensores específicos) — F17.b si se prioriza
- Emulation continuous loop mode (M1) — el caller envuelve M2 burst en `while True` si lo necesita
