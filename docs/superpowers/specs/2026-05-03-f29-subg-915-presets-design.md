# F29 (vuelta 1) — Presets Sub-G 915 MHz para Sidewalk + Wi-SUN

**Date:** 2026-05-03
**Branch (target):** `feature/f29-stack-presets` cut from `main` HEAD=`3451720`
**Tag (target):** `v2.0-f29-partial` (la "b" cubrirá MIOTY + Wi-SUN FAN completo + Sidewalk fidelidad alta)
**Source:** Master plan v2.0 §F29 (`docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`).
**Sibling sub-projects (deferred):**
- F29.b — Wi-SUN FAN 1.0 data rates 100/150/200/300 kbps + MIOTY TS-UNB + spec-correct sync words

## Goal

Agregar **3 presets PROP_PRESETS** a la API Python para cubrir la identidad PHY mínima de Sidewalk Sub-G y Wi-SUN FAN 1.0 baseline. Cero cambios firmware: el path `CMD_SET_PROP_CONFIG` ya soporta cualquier combinación de parámetros y está validado con los 16 presets existentes (433/868 MHz). Smoke marker test 30/30 sobre 2 CatSniffer boards.

## Bundle layout

| Bundle | Cambios | Tag |
|--------|---------|-----|
| 1 — Python presets + tests + smoke | `presets.py`, `test_props.py`, `smoke_f29_subg_915.py` | `v2.0-f29-partial` |

Todo en una rama, commits separados por área (presets / tests / smoke), pre-commit clean por commit.

## Scope decisions

**Fidelity nivel: MEDIA.**
- Banda y symbol rate del estándar real (915 MHz para Sidewalk, 902.2 MHz para Wi-SUN canal 0).
- Deviation y rx_bw heredados del template GFSK 868 MHz ya validado.
- Sync word genérico `0x930B51DE` (mismo que los otros 16 presets) — sync correcto del protocolo queda para F29.b.
- Justificación: spec v2.0 dice "validación contra dispositivos reales opcional para v2.1". Esta vuelta valida que la banda 915 MHz funciona OTA, no interop con gateway real.

**Selección: A.1 (3 presets).**
- `sidewalk_915_fsk_50k` — Amazon Sidewalk Sub-G FSK base.
- `sidewalk_915_fsk_250k` — Sidewalk variante alta.
- `wisun_915_fsk_50k` — Wi-SUN FAN 1.0 mode 1a baseline.

## Preset definitions

Agregar a `python/feralrf/presets.py` después del bloque 868 MHz, antes de cualquier nuevo bloque de comentarios:

```python
# === 902-928 MHz ISM Band (US / Sidewalk / Wi-SUN FAN 1.0) ===
"sidewalk_915_fsk_50k": dict(
    frequency_hz=915000000,
    mod_type=0,        # FSK
    symbol_rate=50000,
    deviation=100,     # template GFSK 868 ya validado
    rx_bw=0x52,
    sync_word=0x930B51DE,
),
"sidewalk_915_fsk_250k": dict(
    frequency_hz=915000000,
    mod_type=0,        # FSK
    symbol_rate=250000,
    deviation=200,     # escalada vs 50k variant
    rx_bw=0x56,        # bandwidth más ancho para 250k
    sync_word=0x930B51DE,
),
"wisun_915_fsk_50k": dict(
    frequency_hz=902200000,  # Wi-SUN FAN 1.0 NA-1 plan canal 0
    mod_type=0,        # FSK
    symbol_rate=50000,
    deviation=100,
    rx_bw=0x52,
    sync_word=0x930B51DE,
),
```

`mod_type=0` (FSK puro) en lugar de `1` (GFSK) para reflejar el modo "real" de los protocolos. Si la validación OTA falla con FSK puro pero pasa con GFSK, cambiamos a `mod_type=1` y documentamos como deviación pragmática en F29.b.

## Tests

Extender `python/tests/test_props.py`:

1. **Schema test** del existente cubre keys obligatorias — debe seguir pasando con 19 presets en lugar de 16.

2. **Nuevos tests específicos F29:**

```python
def test_f29_presets_present():
    """F29 vuelta 1 — 3 nuevos presets Sub-G 915 MHz."""
    expected = {"sidewalk_915_fsk_50k", "sidewalk_915_fsk_250k", "wisun_915_fsk_50k"}
    assert expected.issubset(PROP_PRESETS.keys())


def test_f29_presets_in_915_band():
    """Cada preset F29 debe estar en banda 902-928 MHz US ISM."""
    for name in ("sidewalk_915_fsk_50k", "sidewalk_915_fsk_250k", "wisun_915_fsk_50k"):
        f = PROP_PRESETS[name]["frequency_hz"]
        assert 902_000_000 <= f <= 928_000_000, f"{name}: freq {f} out of band"


def test_f29_presets_use_fsk():
    """F29 vuelta 1 usa FSK puro (mod_type=0). GFSK fallback queda para F29.b."""
    for name in ("sidewalk_915_fsk_50k", "sidewalk_915_fsk_250k", "wisun_915_fsk_50k"):
        assert PROP_PRESETS[name]["mod_type"] == 0
```

## Smoke

`python/examples/smoke_f29_subg_915.py` (nuevo) — patrón equivalente a `smoke_prop_phase1.py`:

```
para cada preset en [sidewalk_915_fsk_50k, sidewalk_915_fsk_250k, wisun_915_fsk_50k]:
    Board #2: connect + init + configure_prop(**PROP_PRESETS[preset]) + tx_burst(b"MARKER" + i, count=10)
    Board #1: connect + init + configure_prop(**PROP_PRESETS[preset]) + start_rx + read_packets(timeout=15s)
    assert >= 10 packets with substring b"MARKER" and crc_ok=True
```

Hardware: ambos boards CatSniffer #1 (`/dev/ttyACM2`) + #2 — coordinar TX/RX por timestamp manual (script no orquestra ambos boards). Per memoria `project_hardware`: TX desde board #2 (board #1 tiene Sub-1GHz TX hardware fault); RX desde board #1 (Sub-1GHz RX OK). Antena CatSniffer cubre 868 y 915 MHz sin issue (solo 433 MHz es marginal).

Runtime esperado: ~30-45 s por preset, total ~90-135 s.

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| `mod_type=0` (FSK puro) menos validado que GFSK | Posible fallo de modulación/demod en banda 915 | Plan B: cambiar a `mod_type=1` (GFSK) con misma deviation. Documentar en F29.b |
| Board #1 Sub-1GHz TX hardware fault | TX limitado a board #2 | TX siempre desde #2; RX desde #1 (Sub-1GHz RX OK en #1 confirmado) |
| Symbol rate 250k / rx_bw 0x56 no probado en banda 915 | Posible saturación o bandwidth mismatch | Si solo `sidewalk_915_fsk_250k` falla, ajustar rx_bw — bisectar contra `gfsk_868_100k` (sym 100k, rx_bw 0x56 ya validado) para descartar el bw vs frecuencia |
| Pre-commit format/lint en presets.py | Reformat de dict | Usar pre-commit run --files (no --all-files) |
| Regulatory band (US ISM en EU/MX) | Compliance | Solo smoke marker test, segundos de TX a baja potencia — sin issue |

**Antena:** confirmado por usuario — CatSniffer funciona bien en 915 MHz y 868 MHz, solo 433 está marginal. NO hay riesgo de antena para esta vuelta.

## Acceptance criteria

- 3 presets en `PROP_PRESETS` con schema correcto
- Tests unitarios `test_props.py` pasan, incluyendo nuevos
- Smoke marker test **30/30** (10/10 × 3 presets) sobre 2 boards
- Pre-commit clean en todos los commits
- `cmake --build firmware/cc1352/build -j2` clean (no firmware changes pero build sigue verde)
- `pytest python/tests/` sin regresiones (>= 454 pass)
- Tag `v2.0-f29-partial` en HEAD final
- Memory entry `project_f29_partial.md`
- FF merge a `main` — directo, sin PR (workflow del repo)

## Out of scope

- Wi-SUN FAN 1.0 data rates 100/150/200/300 kbps (F29.b)
- MIOTY TS-UNB 396 baud (F29.b — investigar si CC1352 soporta nativamente)
- Sidewalk LR / LoRa-like (NO soportado por CC1352, requiere SX1262)
- demo_sidewalk_subg.py / demo_wisun_scan.py interactivos (deferred — no aportan al criterio "10/10 markers")
- Sync words spec-correct (F29.b)
- Validación contra hardware real Sidewalk gateway / Wi-SUN node (v2.1 per master spec)
- Channel hopping plans (Wi-SUN FAN 1.0 hop sequence) — preset es single-channel
- Encryption / framing (capa stack, no PHY)
