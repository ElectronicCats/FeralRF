# F10 — Port props NoRTOS → TI-RTOS (validation-pure)

**Date:** 2026-04-28
**Phase:** F10
**Branch:** `feature/f10-port-props-tirtos` (forked from `feature/f8a-ble-central-sniffle` @ `e3c50cb`)
**Tag at close:** `v2.0-f10`
**Prereq:** F8 ✅ (`v2.0-f8`)
**Note on F9:** F9 is `⚠️ partial` on a different branch and is NOT in the F10 lineage. F10 tests use the existing reset-between-tests pattern in `run_validation_baseline.sh`, so the F9 cycle-without-reset bug does not block F10.

---

## 1. Scope and approach (decided 2026-04-28)

**Approach A — validation-pure.** Source review confirmed that the prop port from NoRTOS to TI-RTOS is already complete on `feature/f8a-ble-central-sniffle`:

| Component | Status | Location |
|-----------|--------|----------|
| `CMD_SET_PROP_CONFIG (0x08)` handler, 18-byte payload | present | `command_processor.c:289` |
| `RadioIF_setPropConfig()` — full mod_type + format_conf + band overrides | present | `radio_if.c:1854-2004` |
| Band-overrides auto-select (169 / 433 / 861+ MHz, OOK 433 / OOK 868) | present | `radio_if.c:1969-2000` |
| OOK genook patches (`Prop0_pOverridesOok`, `Prop0_pOverridesOok433`) | present | `smartrf_prop_0.c` |
| OOK lock + persistent 433 handle + RF_yield trick | present | `radio_if.c:1868-1881, 1959-1962` |
| 4-FSK / 4-GFSK via `format_conf` | present | `radio_if.c:1942, 1954` |
| `Prop0_*433` SysConfig structs (independent setup for <861 MHz) | present | `syscfg/ti_radio_config_433.h`, `smartrf_prop_0.c` |
| `reset_device()` via RP2040 shell (boot+exit) | present | `python/examples/run_validation_baseline.sh:_reset_one()` |

**Build (this branch):** clean, `text=91 428 / data=2620 / bss=46 672` → ~94 KB flash, well under the 120 KB target.

F10 is therefore **validation-driven, not implementation-driven**. The discipline is per-preset commits (TDD-style) and incremental validation, not batch.

Plans B (regression audit) and C (full re-port) were considered and rejected:
- Build is clean and the sanity smoke (`BLE 1M` scan, post-reset) returns 227 packets — F8A did not regress the radio path
- F8A only touched BLE central / GATT (master loop, host TX queue, LL_TERMINATE_IND) — none of those code paths overlap with the prop hot path
- Full re-port (C) is busywork for code that is provably present and that built clean

If during the initial sweep more than 3 presets fail, the design escalates to plan B (per-preset audit against `main` NoRTOS).

---

## 2. Hardware & test setup

| Role | Device | Port |
|------|--------|------|
| TX | CatSniffer #1 (CC1352P7) | `/dev/ttyACM8` Cat-Bridge, `/dev/ttyACM10` Shell |
| RX | CatSniffer #2 (CC1352P7) | `/dev/ttyACM5` Cat-Bridge, `/dev/ttyACM7` Shell |

**Reset between tests:** `python/examples/run_validation_baseline.sh:_reset_one()` writes `boot\r\nexit\r\n` to the RP2040 shell, settles 3.5 s. Both boards reset before every step. This makes the F9 cycle-without-reset bug irrelevant for F10.

**Flash tool:** `catnip` with `.hex`, retry 2× before manual reset (per memory).

---

## 3. Preset list (15 from spec §F10 close criterion)

Order chosen to surface issues early (well-trodden 868 first, then 915, 2.4 GHz, 433, OOK last):

| # | Preset | Frequency | mod_type | Notes |
|---|--------|-----------|----------|-------|
| 1 | `gfsk_868_50k` | 868.000 MHz | 1 (GFSK) | Sanity check — most-used path |
| 2 | `gfsk_915_50k` | 915.000 MHz | 1 (GFSK) | 915 ISM (US) |
| 3 | `gfsk_2440_50k` | 2440.000 MHz | 1 (GFSK) | 2.4 GHz proprietary |
| 4 | `msk_868_50k` | 868.000 MHz | 4 (MSK) | mod_type=4 path |
| 5 | `4fsk_868_50k` | 868.000 MHz | 5 (4-FSK) | format_conf path, 868 |
| 6 | `4gfsk_868_50k` | 868.000 MHz | 6 (4-GFSK) | format_conf path, 868 |
| 7 | `wireless_mbus_s_868` | 868.300 MHz | 1 (GFSK) | sync_word custom, 32.768 kbps |
| 8 | `wireless_mbus_t_868` | 868.950 MHz | 1 (GFSK) | rx_bw=0x57, 100 kbps |
| 9 | `wireless_mbus_c_868` | 868.950 MHz | 1 (GFSK) | deviation=180 (vs T's 200) |
| 10 | `gfsk_433_50k` | 433.920 MHz | 1 (GFSK) | First 433-struct test |
| 11 | `fsk_433_50k` | 433.920 MHz | 0 (FSK) | mod_type=0 |
| 12 | `msk_433_50k` | 433.920 MHz | 4 (MSK) | mod_type=4 on 433 |
| 13 | `4fsk_433_50k` | 433.920 MHz | 5 (4-FSK) | format_conf on 433 |
| 14 | `4gfsk_433_50k` | 433.920 MHz | 6 (4-GFSK) | format_conf on 433 |
| 15 | `ook_868_4k8` | 868.000 MHz | 2 (OOK) | genook patches, auto-reset after |
| 16 | `ook_433_4k8` | 433.920 MHz | 2 (OOK) | best-effort (CatSniffer antenna limits 433 OOK) |

**Note on count:** the spec text says "15", the table above lists 16 because OOK 433 is mentioned but explicitly excluded from the 10/10 criterion. The actual 10/10 gate is on 14 presets (1–14 above). OOK 868 is a 15th hard requirement. OOK 433 is best-effort.

---

## 4. Per-preset workflow (TDD-style)

For each preset:

1. **Reset** both boards via RP2040 shell (the same `_reset_one()` from `run_validation_baseline.sh`).
2. **Run** `python/examples/smoke_ota_txrx.py --tx-port /dev/ttyACM8 --rx-port /dev/ttyACM5 --preset <name> --count 10 --min-markers 9` (script default is `--min-markers 1`; we tighten the gate to 9/10).
3. **Parse** the line `[OTA ] <label>: markers=N/10 total_rx=M` to record the actual marker count.
4. **Pass:** N ≥ 9/10 (10/10 nominal, 9/10 acceptable for marginal links).
5. **Commit pass:** `test(f10): validate <preset> N/10 OTA — evidence in plan.md` (with the parsed `N` recorded in the plan's checklist).
6. **Fail:** investigate (RF state, override, FS tuning, queue saturation), fix, re-validate.
7. **Commit fix:** `fix(f10): <preset> <root cause>` followed by a separate test commit re-running validation.

OOK presets must close their session before any non-OOK preset runs next. The reset between tests in step 1 is sufficient — OOK 868 → reset → next preset always succeeds. For OOK 433, expect <9/10 (best-effort per CatSniffer 433 antenna limit) and skip `--min-markers`.

---

## 5. Closure criteria

- [ ] 14/14 OTA markers ≥9/10 on presets 1–14 (numbers 1–9 = 868+, 10–14 = 433)
- [ ] OOK 868 (#15) ≥9/10
- [ ] OOK 433 (#16) reported as best-effort with whatever marker count is observed (per `project_433mhz_root_cause.md` and spec §F10 risk table — antenna is the limit, not firmware)
- [ ] FW size <120 KB (already 94 KB at this point — only flag a regression check at end)
- [ ] `reset_device()` unlock <2 s (validated by completing the OOK steps and successfully running a non-OOK preset right after)
- [ ] Tag `v2.0-f10` annotated, with closure note in commit message and in `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §5 F10 status updated to ✅
- [ ] Memory entry `project_f10_done.md` created summarizing what was validated

---

## 6. Risks

| # | Risk | Likelihood | Mitigation |
|---|------|------------|------------|
| R1 | F8A queue/loop changes silently regressed a prop preset | Low (paths don't overlap) | Initial sweep surfaces it; if >3 fail, escalate to plan B audit |
| R2 | OOK 868 fails on TI-RTOS even though present on `main` NoRTOS | Low (genook patches identical) | Auto-reset after OOK; if hang, revisit `s_prop_ook_active` lock path |
| R3 | OOK 433 marker count <9/10 | Expected | Already accepted per spec — best-effort, documented in `project_433mhz_root_cause.md` |
| R4 | `format_conf` path for 4-FSK / 4-GFSK never tested on TI-RTOS | Medium | These are presets 5, 6, 13, 14 — if all four fail with the same symptom, root-cause `RadioIF_applyFormatConf` |
| R5 | Persistent 433 handle interaction with new presets | Low | Same persistent handle pattern that worked for F6 baseline; if regression, log RF_core state per `project_f9_partial.md` next-steps |
| R6 | `smoke_ota_txrx.py` itself broken for some presets | Low | Script is from F2 era and was used for the 10/10 evidence on `main`; run with `--only` filter early to spot script issues |

---

## 7. Out of scope

- **F9 fix (IEEE→BLE switch).** F9 stays open on its own branch.
- **High PA (+15–20 dBm).** Deferred to v2.1 (DIO29 antenna switch fix).
- **169 / 315 / 390 / 470 MHz.** Antenna not viable on CatSniffer.
- **CMD_TX_TEST jamming 2.4 GHz proprietary.** F18 problem.
- **W-MBus N (169 MHz)** — preset exists in `presets.py` but spec lists it as N/A on CatSniffer antenna; not part of F10 closure.
- **`gfsk_868_100k`, `gfsk_433_10k`, `ook_433_2k4`, `gfsk_902_50k`, `gfsk_2440_250k`** — extra presets in `presets.py` beyond the spec's 15. They will benefit from F10 work indirectly (same code path) but are not gated on F10 closure.

---

## 8. Implementation pointer

The matching plan with the per-preset checklist and execution order lives at:
`docs/superpowers/plans/2026-04-28-f10-port-props.md`
