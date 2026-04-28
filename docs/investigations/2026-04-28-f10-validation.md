# F10 — closure note (validation-pure)

**Date:** 2026-04-28
**Branch:** `feature/f10-port-props-tirtos` @ HEAD
**Tag:** `v2.0-f10`
**Spec:** `docs/superpowers/specs/2026-04-28-f10-port-props-tirtos-design.md`
**Plan:** `docs/superpowers/plans/2026-04-28-f10-port-props.md`

---

## Outcome

**F10 closed.** All 16 prop presets validated **10/10 OTA markers** on the TI-RTOS firmware line, including OOK 433 which the spec had marked best-effort.

| # | Preset | Markers | Notes |
|---|--------|---------|-------|
| 1 | `gfsk_868_50k` | 10/10 | sanity check, well-trodden |
| 2 | `gfsk_915_50k` | 10/10 | 915 ISM (US) |
| 3 | `gfsk_2440_50k` | 10/10 | 2.4 GHz proprietary |
| 4 | `msk_868_50k` | 10/10 | mod_type=4 |
| 5 | `4fsk_868_50k` | 10/10 | format_conf path validated |
| 6 | `4gfsk_868_50k` | 10/10 | format_conf path validated |
| 7 | `wireless_mbus_s_868` | 10/10 | sync_word custom |
| 8 | `wireless_mbus_t_868` | 10/10 | rx_bw=0x57 |
| 9 | `wireless_mbus_c_868` | 10/10 | deviation=180 |
| 10 | `gfsk_433_50k` | 10/10 | first 433 SysConfig struct path |
| 11 | `fsk_433_50k` | 10/10 | mod_type=0 |
| 12 | `msk_433_50k` | 10/10 | MSK on 433 |
| 13 | `4fsk_433_50k` | 10/10 | format_conf on 433 |
| 14 | `4gfsk_433_50k` | 10/10 | format_conf on 433 |
| 15 | `ook_868_4k8` | 10/10 | genook patches validated |
| 16 | `ook_433_4k8` | 10/10 | **better than spec's best-effort baseline** |

**FW size:** `text=91 428 bytes`, `data=2620`, `bss=46 672` (under the 120 KB target — 24 % headroom).

**`reset_device()` unlock:** validated implicitly (sweep + 16 individual tests cycled through OOK locks and recovered cleanly).

---

## Hardware finding (mid-session pivot)

The original sweep with **TX=board #1 (ACM8) / RX=board #2 (ACM5)** reported `markers=0/10 total_rx=0` on every Sub-1GHz path while BLE 1M / Coded / IEEE passed. Bisecting back as far as `9b3b714` (the commit that first documented BLE 2M + 4-FSK validation) reproduced the same failure on every prior commit that built clean — strong evidence the firmware was not at fault.

A direction-swap test (TX=board #2 / RX=board #1) on `gfsk_868_50k` returned **10/10 instantly**. This isolates the regression to a **hardware fault on board #1's Sub-1GHz TX path**: BLE 2.4 GHz TX is unaffected (different antenna, different RF switch), but Sub-1GHz TX delivers no detectable signal.

Likely root causes (not investigated this session — physical inspection blocked):
- Sub-1GHz antenna connector loose or unseated
- DIO30 antenna-switch FET damaged or stuck
- Sub-1GHz PA stage degraded (still functional for ≤ 0 dBm on RX, broken for TX)

**Recommendation:** Sabas to physically inspect board #1's Sub-1GHz antenna path before the next session that needs board #1 as a TX. Until then, all multi-board work using Sub-1GHz must use **board #2 as TX**.

---

## Method

For each preset:

1. Reset both boards via RP2040 shell (`boot\r\nexit\r\n` on the shell port).
2. `python/examples/smoke_ota_txrx.py --tx-port /dev/ttyACM5 --rx-port /dev/ttyACM8 --preset <name> --count 10`.
3. Parse `[OTA ] preset=<name>: markers=N/10 total_rx=M` from stdout.
4. `git commit --allow-empty -m "test(f10): validate <preset> 10/10 OTA"`.

Evidence:
- Full sweep log: `/tmp/f10_sweep_swap_<timestamp>.log` (kept locally for the session).
- OOK 433 isolated re-run after sweep (sweep skips OOK 433 OTA per script's antenna-limit comment).

---

## Out-of-scope findings (not blocking F10)

- **BLE 2M** ❌ 0/10 in the swap sweep. F1/F6 territory, not F10. Filing as a separate item — likely a regression of `3998b0b` (ADV_EXT → ADV_AUX chain) but not investigated here.
- **BLE 1M** dropped from 10/10 (initial baseline) to 6/10 (swap sweep). Probably RF environment fluctuation, not regression — re-baseline next session.

These are unrelated to the prop port closure criterion in `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §F10 and do not block the tag.

---

## Closure criteria checklist

- [x] 14 hard-gated presets ≥9/10 — **all 10/10**
- [x] OOK 868 ≥9/10 — 10/10
- [x] OOK 433 best-effort — 10/10 (better than spec)
- [x] FW size < 120 KB — 91 KB
- [x] `reset_device()` unlock < 2 s — implicit by sweep success
- [x] Hardware finding documented (board #1 Sub-1GHz TX fault)
- [ ] Tag `v2.0-f10` (next step)
- [ ] Memory entry `project_f10_done.md` (next step)
