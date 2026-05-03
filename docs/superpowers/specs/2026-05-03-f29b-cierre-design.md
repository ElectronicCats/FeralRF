# F29.b — Cierre completo §F29 (MIOTY + Wi-SUN FAN restantes + demos + docs)

**Date:** 2026-05-03
**Branch (target):** `feature/f29b-cierre` cut from `main` HEAD=`04caba9`
**Tag (target):** `v2.0-f29` (full closure de §F29; supersede semántico de `v2.0-f29-partial`).
**Tag escape (si MIOTY falla):** `v2.0-f29-near-final` (4/5 presets pasan, MIOTY deferred a v2.1).
**Source:** Master plan v2.0 §F29 (`docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`).
**Predecessor:** F29 vuelta 1 (`docs/superpowers/specs/2026-05-03-f29-subg-915-presets-design.md`, tag `v2.0-f29-partial`, commit `04caba9`).

## Goal

Cerrar todo el alcance original de §F29: agregar **MIOTY TS-UNB** + **Wi-SUN FAN restantes (100/150/200/300 kbps)** + **3 demo scripts stub** + nota documental sobre Sidewalk LR no soportado por CC1352. Cero cambios firmware. Smoke marker test 50/50 OTA si MIOTY responde; escape M3 a `v2.0-f29-near-final` si no.

## Bundle layout

| Bundle | Cambios | Tag | Pre-req |
|--------|---------|-----|---------|
| 1 — MIOTY preset + smoke aislado | `presets.py`, `test_props.py`, smoke ajuste | (sin tag intermedio) | — |
| 2 — Wi-SUN 4 rates | `presets.py`, `test_props.py`, smoke extend | (sin tag intermedio) | Bundle 1 (para no invertir tiempo si MIOTY bloquea) |
| 3 — Demos stub × 3 | examples/lab/demo_*.py | (sin tag intermedio) | independiente |
| 4 — Sidewalk LR doc note | `presets.py` comment | (sin tag intermedio) | independiente |
| Final | tag + memory + FF | `v2.0-f29` o `v2.0-f29-near-final` | todos |

Estrategia A confirmada: MIOTY primero para descubrir blockers temprano. Si Bundle 1 falla con escape M3, decidimos antes de gastar tiempo en Bundle 2-4.

## Scope decisions

**Order:** A (MIOTY → Wi-SUN → demos → docs).
**MIOTY:** M1 (OTA marker test 10/10) con escape M3 si CC1352 no soporta nativamente.
**Wi-SUN:** W1 (4 presets simples, mod index implícito ~0.5).
**Demos:** D1 (stubs minimalistas, "parse mínimo" textual del master plan).
**Sidewalk LR:** comment doc en `presets.py` + nota en memoria (no archivo dedicado).

## Preset definitions

Agregar a `python/feralrf/presets.py` después del bloque 902-928 MHz F29 vuelta 1, antes del cierre del dict:

```python
# === MIOTY TS-UNB (ETSI TS 103 357) — 868 MHz EU SRD ===
# WARNING: 396 baud es ultra-low-rate. CC1352 viability via rx_bw mínimo
# pendiente validación OTA. Si rx_bw mínimo del CC1352 no llega a ~5 kHz,
# preset es non-functional y queda como "pending native support".
"mioty_868_tsunb": dict(
    frequency_hz=868000000,
    mod_type=0,            # FSK (TS-UNB también acepta GFSK; FSK como primer intento)
    symbol_rate=396,       # ETSI TS 103 357
    deviation=1,           # ~250 Hz unidad CC1352 → ~250 Hz dev (mod index ~1.3 a 396 baud)
    rx_bw=0x4A,            # mínimo BW soportado por CC1352 ~5 kHz (ajustar si rate_word inválido)
    sync_word=0x930B51DE,
),

# === Wi-SUN FAN 1.0 NA-1 channel 0 — data rates restantes ===
# Mod index implícito ~0.5 (deviation pragmática heredada de templates ya validados).
# Para FSK puro a 200k+, lección F29 vuelta 1: rx_bw=0x5A; para 300k posiblemente 0x5E.
"wisun_915_fsk_100k": dict(
    frequency_hz=902200000,
    mod_type=0,
    symbol_rate=100000,
    deviation=150,
    rx_bw=0x56,            # match gfsk_868_100k que ya pasa
    sync_word=0x930B51DE,
),
"wisun_915_fsk_150k": dict(
    frequency_hz=902200000,
    mod_type=0,
    symbol_rate=150000,
    deviation=200,
    rx_bw=0x5A,            # FSK puro >= 150k → más BW
    sync_word=0x930B51DE,
),
"wisun_915_fsk_200k": dict(
    frequency_hz=902200000,
    mod_type=0,
    symbol_rate=200000,
    deviation=200,
    rx_bw=0x5A,            # F29 vuelta 1 lección
    sync_word=0x930B51DE,
),
"wisun_915_fsk_300k": dict(
    frequency_hz=902200000,
    mod_type=0,
    symbol_rate=300000,
    deviation=200,
    rx_bw=0x5E,            # rate más alto requiere BW más amplio
    sync_word=0x930B51DE,
),
```

Plus comment al final del dict, antes del `}`:

```python
# Sidewalk LR (LoRa-like) NO soportado por CC1352 — usa SX1262 vía Cat-LoRa
# port. CC1352 cubre solo el FSK layer de Sidewalk Sub-G (presets sidewalk_*
# arriba). Para LR ver hardware Cat-LoRa.
```

## Tests

Extender `python/tests/test_props.py` agregando bloque `F29B_PRESET_NAMES`:

```python
F29B_PRESET_NAMES = (
    "mioty_868_tsunb",
    "wisun_915_fsk_100k",
    "wisun_915_fsk_150k",
    "wisun_915_fsk_200k",
    "wisun_915_fsk_300k",
)


@pytest.mark.parametrize("name", F29B_PRESET_NAMES)
def test_f29b_preset_present(name):
    """F29.b — los 5 presets nuevos están en PROP_PRESETS."""
    assert name in PROP_PRESETS, f"Preset {name} ausente"


@pytest.mark.parametrize("name", F29B_PRESET_NAMES)
def test_f29b_preset_uses_fsk(name):
    """F29.b — todos usan FSK puro (mod_type=0). GFSK fallback si OTA falla."""
    assert PROP_PRESETS[name]["mod_type"] == 0


@pytest.mark.parametrize("name", ("wisun_915_fsk_100k", "wisun_915_fsk_150k",
                                  "wisun_915_fsk_200k", "wisun_915_fsk_300k"))
def test_f29b_wisun_in_915_band(name):
    """F29.b — Wi-SUN nuevos en banda 902-928 MHz."""
    f = PROP_PRESETS[name]["frequency_hz"]
    assert 902_000_000 <= f <= 928_000_000


def test_f29b_mioty_in_868_band():
    """F29.b — MIOTY TS-UNB en banda 868 EU SRD."""
    f = PROP_PRESETS["mioty_868_tsunb"]["frequency_hz"]
    assert 863_000_000 <= f <= 870_000_000


def test_f29b_mioty_symbol_rate_396():
    """F29.b — MIOTY symbol rate per ETSI TS 103 357."""
    assert PROP_PRESETS["mioty_868_tsunb"]["symbol_rate"] == 396


def test_f29b_preset_count():
    """F29.b — exactamente 5 presets nuevos."""
    f29b = [n for n in PROP_PRESETS if n in F29B_PRESET_NAMES]
    assert len(f29b) == 5
```

Total: 6 funciones × parametrize → ≈ 24 nuevos test hits + 5 parametrize hits del schema test existente.

## Smoke

Extender `python/examples/smoke_f29_subg_915.py` modificando la tuple `F29_PRESETS` para incluir los 5 nuevos (in-place edit del archivo existente, no rename). Mantener el filename `smoke_f29_subg_915.py` para no romper referencias del README/CI/memoria; agregar comentario al docstring indicando que ahora cubre F29 + F29.b.

```python
F29_PRESETS = (
    # Vuelta 1
    "sidewalk_915_fsk_50k",
    "sidewalk_915_fsk_250k",
    "wisun_915_fsk_50k",
    # Vuelta 2 (F29.b)
    "mioty_868_tsunb",
    "wisun_915_fsk_100k",
    "wisun_915_fsk_150k",
    "wisun_915_fsk_200k",
    "wisun_915_fsk_300k",
)
```

Pass criterio: 80/80 markers (10 × 8 presets) si MIOTY responde. Si MIOTY 0/10, escape M3 = excluir MIOTY del tuple, smoke 70/70, tag `v2.0-f29-near-final`, MIOTY deferred a v2.1.

Hardware: TX board #2 (`/dev/ttyACM1`), RX board #1 (`/dev/ttyACM2`). Ambos boards ya flasheados con firmware F8f follow-up (HEAD `a0e3e07` o más nuevo) per F29 vuelta 1 session.

## Demos (D1 stubs)

3 archivos en `python/examples/lab/`:

`demo_wisun_scan.py` (~50 LOC):
```python
#!/usr/bin/env python3
"""F29 — Wi-SUN FAN 1.0 (915 MHz US ISM) capture stub.

Default: wisun_915_fsk_50k. Otros rates via --preset.
Loads preset, starts RX, prints first N bytes hex of each packet.
NO parse de stack (PHY-only). Ctrl-C para salir.
"""
import argparse
import sys

from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError


def main() -> int:
    parser = argparse.ArgumentParser(description="F29 Wi-SUN scan stub")
    parser.add_argument("--port", required=True)
    parser.add_argument(
        "--preset",
        default="wisun_915_fsk_50k",
        choices=[n for n in PROP_PRESETS if n.startswith("wisun_")],
    )
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--max-hex", type=int, default=32, help="Max bytes to hex-print")
    parser.add_argument("--duration", type=float, default=30.0)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        radio.configure_prop(**PROP_PRESETS[args.preset])
        radio.start_rx()
        print(f"Scanning preset={args.preset}; Ctrl-C to stop")

        for pkt in radio.read_packets(timeout=args.duration):
            if isinstance(pkt, RxStreamError):
                continue
            head = pkt.data[: args.max_hex].hex()
            crc_flag = "OK" if pkt.crc_ok else "ER"
            print(f"[{crc_flag}] len={len(pkt.data)} rssi={pkt.rssi_dbm} {head}")
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

`demo_mioty_listen.py` y `demo_sidewalk_subg.py` siguen el mismo template, cambiando default `--preset` a `mioty_868_tsunb` y `sidewalk_915_fsk_50k` respectivamente, y filtrando los choices a presets `mioty_*` / `sidewalk_*`.

Validación per demo: `--help` exit 0, `python -c "import ast; ast.parse(open(...).read())"` succeeds. NO se requiere hardware run para validar el demo (es harness de captura).

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| MIOTY rx_bw inadecuado (CC1352 mínimo > 5 kHz) | 0/10 markers | Escape M3: probar GFSK (mod_type=1), después escalación rx_bw 0x46/0x42; si todo falla, marcar preset como pending y excluir de smoke. Tag a `v2.0-f29-near-final`. |
| MIOTY rate_word inválido para 396 baud | FW reconfig falla / preset rechazado | Test unitario calcula `rate_word` localmente y verifica rango. Si inválido a priori, M3 directo sin OTA. |
| Wi-SUN 200k/300k rx_bw subestimado | 0/10 en alguno de esos rates | Escalación documentada: 0x5A → 0x5E → 0x62 → fallback a mod_type=1 |
| Vuelta 1 presets regresión | Smoke pre-existente falla | Pre-flight `gfsk_868_50k` baseline + corrida de presets vuelta 1 (3) antes de los nuevos |
| Demos crash en `--help` | Pre-commit fail | ast.parse + argparse `--help` validan sin hardware |
| Pre-commit black auto-format de tuple `F29_PRESETS` extendida | Reformat | aceptar reformat, commit con cambios pre-commit |

**MIOTY M3 escape protocol completo:**
1. Run smoke MIOTY aislado → si 10/10, continuar normal.
2. Si 0/10: switch `mod_type=0` → `mod_type=1` (GFSK) en `mioty_868_tsunb`, re-test.
3. Si sigue 0/10: probar `rx_bw=0x46`, después `0x42`.
4. Si sigue 0/10: registrar `rate_word` calculado vía tracing (print en smoke o telemetry); si fuera de rango FW, preset removido.
5. Final fallback: marcar preset como `# pending: native CC1352 support unknown` (comment solo, sin remover del dict para preservar el ID), excluir del smoke loop, tag a `v2.0-f29-near-final`, F29.c en el roadmap como TBD.

## Acceptance criteria

**Path principal (`v2.0-f29`):**
- 5 presets nuevos en `PROP_PRESETS` con schema correcto
- Comment doc Sidewalk LR en `presets.py`
- Tests F29.b pasan (24+ parametrize hits + schema regression)
- Smoke 80/80 markers (10 × 8 presets total) sobre 2 boards
- 3 demo scripts cargan + `--help` + ast.parse OK
- Pre-commit clean en todos los commits
- `pytest python/tests/` ≥ 512 pass (era 488)
- Tag `v2.0-f29` en HEAD final
- Memory entry `project_f29_done.md` (supersede `project_f29_partial.md` semánticamente)
- FF merge a `main`

**Path escape (`v2.0-f29-near-final` si MIOTY no responde nativamente):**
- 4 presets Wi-SUN nuevos OK
- MIOTY preset present pero excluded del smoke con comment "pending native support"
- Smoke 70/70 markers (10 × 7 presets) — exclude MIOTY
- Resto idéntico al path principal
- Tag `v2.0-f29-near-final` en lugar de `v2.0-f29`
- Memory entry documenta MIOTY como deferred a v2.1

## Out of scope

- Sidewalk LR / LoRa-like (HW limitation CC1352)
- Sync words spec-correct para protocolos (queda como F29.c si se prioriza)
- Channel hopping plans Wi-SUN FAN NA-1 (PHY only, no MAC layer)
- Validación contra hardware real Sidewalk gateway / Wi-SUN node / MIOTY base station (v2.1)
- MIOTY full wire-level spec validation (requiere analizador de espectro)
- Demos interactivos D2 (parsers protocolo) — D1 stubs por master plan textual
- Refactor de presets.py a sub-módulos por banda (sigue siendo flat dict per pattern existente)
