# F26 — Proprietary 2.4 GHz como PHY normal — Design Spec

> **Date:** 2026-04-29
> **Branch:** `feature/f26-prop-24ghz`
> **Phase:** F26 (Bloque D — chip API completeness) of plan-v2
> **Author:** Sabas + Claude (brainstorming session)

---

## 1. Goal & scope

Expose CC1352P7's **proprietary 2.4 GHz** RF capability as a first-class selectable PHY. Today the chip's 2.4 GHz prop mode (`CMD_PROP_RADIO_SETUP_PA` 0x3806, distinct from the Sub-1G `CMD_PROP_RADIO_DIV_SETUP_PA` 0x3807) is unreachable from the API — only used internally by the (non-functional) `start_jam` path. F26 makes it a regular `set_phy(PHY.PROP_2_4GHZ)` target with TX/RX, custom sym rate / deviation / sync word via the existing `configure_prop()` plumbing.

Primary value: API surface coverage for the chip's full RF capability per Sabas's stated F-project goal. Use cases include Nordic ESB (250 kbps GFSK at 2.4 GHz), drones RC custom protocols, Bluetooth-Mesh PB-ADV custom, and as a clean foundation for the future F18 reactive jamming refactor.

### In scope

- New `PHY.PROP_2_4GHZ` enum entry on Python and firmware sides.
- New SmartRF config file (`smartrf_prop_2_4ghz.c`) with mode struct, `CMD_PROP_RADIO_SETUP_PA`, FS, TX, RX command structs, plus required register overrides.
- New code path in `radio_if.c` mirroring the existing Sub-1G prop path but using the no-divider 2.4 GHz setup command.
- Default config on `set_phy(PHY.PROP_2_4GHZ)` with no other args: GFSK 250 kbps @ 2440 MHz centerFreq.
- `configure_prop()` extended to detect frequencies in 2400-2483.5 MHz and route to the new path.
- Validation: 2-board OTA marker tests at 2440 MHz GFSK 250 kbps and at 1 Mbps custom.

### Out of scope

- 2.4 GHz prop preset pack (FSK/MSK/OOK/4-FSK presets) — deferred. F26 closes only with the default GFSK 250 kbps + custom configurable. Preset pack is a future incremental improvement following the F10 pattern.
- F18 jamming refactor onto the new PHY — F26 only makes the path available. F18 itself is a separate phase.
- TX power calibration / High PA (DIO29) — F23 work.
- DMM concurrent BLE + prop 2.4 GHz — F24 work.

## 2. Brainstorm decisions

| # | Decision | Rationale |
|---|---|---|
| 1 | Modulation scope: GFSK default + custom configurable (not full preset pack from day 1) | Covers spec §F26 closure criterion (250 kbps GFSK + custom 1 Mbps) with minimum scope. Preset pack incrementally later. |
| 2 | Default `set_phy(PHY.PROP_2_4GHZ)` config: GFSK 250 kbps @ 2440 MHz centerFreq | Sweet spot for typical 2.4 GHz prop use cases (Nordic ESB factory default). Spec §F26 explicitly mentions 250 kbps. User overrides via `configure_prop()`. |

## 3. API

### 3.1 Python — `enums.py`

```python
class PHY(IntEnum):
    BLE_1M = 0
    BLE_2M = 1
    BLE_CODED_S8 = 2
    BLE_CODED_S2 = 3
    IEEE_802_15_4 = 4
    SUB_1GHZ_868 = 5
    SUB_1GHZ_915 = 6
    PROPRIETARY_GFSK = 7
    PROP_2_4GHZ = 8           # NEW
```

### 3.2 Python — `Radio` (no new methods)

`set_phy()` and `configure_prop()` already accept arbitrary frequency. F26 only adds the new enum value as a valid target. Usage:

```python
# Default: GFSK 250 kbps @ 2440 MHz
r.set_phy(PHY.PROP_2_4GHZ)

# Tune to specific 2.4 GHz frequency
r.set_phy(PHY.PROP_2_4GHZ, frequency_hz=2410000000)

# Custom sym rate / deviation
r.configure_prop(
    frequency_hz=2410000000,
    mod_type=1,            # GFSK
    symbol_rate=1000000,   # 1 Mbps
    deviation=500000,      # 500 kHz
    rx_bw=0x59,
    sync_word=0x930B51DE,
    format_conf=0,
)

r.transmit(b'hello')
r.start_rx()
```

### 3.3 Firmware — command IDs

No new command IDs. `CMD_SET_PHY` (0x04) and `CMD_SET_PROP_CONFIG` (0x08) handle the new PHY through their existing payloads.

`CMD_SET_PHY` payload byte 0 (PHY enum value) accepts 8 = `PHY_MANAGER_PHY_PROP_2_4GHZ`.

`CMD_SET_PROP_CONFIG` payload `frequency_hz` field detects 2400-2483.5 MHz range and routes the config to the new prop24g path.

## 4. Firmware architecture

### 4.1 New SmartRF config

`firmware/cc1352/src/smartrf_prop_2_4ghz.c` (new) — generated from SmartRF Studio 2.4 GHz GFSK 250 kbps export with manual adaptations:

- `Prop24g_mode` (RF_Mode struct): `rfMode = RF_MODE_MULTIPLE`, `cpePatchFxn = &rf_patch_cpe_multi_protocol` (consistent with existing Sub-1G/BLE/IEEE in F1+).
- `Prop24g_cmdPropRadioSetup` (`rfc_CMD_PROP_RADIO_SETUP_PA_t`, commandNo=0x3806): no `loDivider` field, `centerFreq=2440`, `txPower` set for 2.4 GHz table.
- `Prop24g_cmdFs` (`rfc_CMD_FS_t`): `frequency=2440`.
- `Prop24g_cmdPropTx` (`rfc_CMD_PROP_TX_t`): standard TX command, syncWord shared with prop_0.
- `Prop24g_cmdPropRx` (`rfc_CMD_PROP_RX_t`): standard RX command with bAutoFlushIgnored=1 (skill rule).
- `Prop24g_pOverrides[]`, `Prop24g_pOverridesTxStd[]`, `Prop24g_pOverridesTx20[]`: TI-generated overrides for 2.4 GHz prop.

Header `firmware/cc1352/include/smartrf_prop_2_4ghz.h`: extern declarations only.

### 4.2 Modifications to existing firmware

| File | Change |
|---|---|
| `phy_manager.h` | Add `PHY_MANAGER_PHY_PROP_2_4GHZ = 8u` to PHY enum |
| `phy_manager.c` | Add `PhyManager_isProp24ghzPhy(uint8_t phy)` predicate (returns true for the new value); update `PhyManager_supportsRfBackendRx` to include it |
| `radio_if.h` | Add `RADIO_IF_RF_MODE_PROP_2_4GHZ = 4` to `RadioIF_RfMode` enum |
| `radio_if.c` | • `RadioIF_isProp24ghzPhySelected()` predicate<br>• `RadioIF_applyProp24ghzChannelConfig(channel, freq_hz)` — write freq to `Prop24g_cmdFs` and `Prop24g_cmdPropRadioSetup.centerFreq`<br>• `RadioIF_startProp24ghzRfBackend()` — mirror `startSub1ghzRfBackend`, calls `switchRfMode(&Prop24g_mode, &Prop24g_cmdPropRadioSetup)`, sets up RX queue with `Prop24g_cmdPropRx`<br>• `RadioIF_transmitProp24ghzRaw()` — mirror `transmitPropRaw`, uses `Prop24g_cmdPropTx`<br>• Wire up in `RadioIF_setPhy()`: when phy == PROP_2_4GHZ, default freq to 2440000000 if zero, call applyProp24ghzChannelConfig<br>• Wire up in `RadioIF_setPropConfig()`: branch when freq_mhz ∈ [2400, 2484]<br>• Wire up in `runFsAndPostRx()`: case for `RADIO_IF_RF_MODE_PROP_2_4GHZ` returning `Prop24g_cmdFs` + `Prop24g_cmdPropRx`<br>• Wire up in `transmitRaw`: when isProp24ghzPhySelected, call `transmitProp24ghzRaw` |
| `control_task.c` | None expected (set_phy already passes the byte through) |

### 4.3 Hot-switch compatibility (F9)

`Prop24g_mode.rfMode = RF_MODE_MULTIPLE` and uses the multi_protocol CPE patch. F9's `switchRfMode()` will hot-switch between BLE/IEEE/Sub1G/Prop24g/433 by running the corresponding RadioSetup as a command on `s_non433_handle`. No changes required to F9 logic. Validated as part of F26 closure (test 5).

### 4.4 TX power

`s_tx_power_table_24g` (already in radio_if.c, populated for LP_CC1352P7-1) covers -20 to +5 dBm via std PA. `RadioIF_resolveTxPowerValue()` already selects this table when freq is in 2.4 GHz band — no change needed.

## 5. Validation strategy

### 5.1 Hardware smoke — `python/examples/lab/smoke_f26_prop_24ghz.py`

Auto-runs on 2 boards (TX `/dev/ttyACM5`, RX `/dev/ttyACM8`). Five tests:

1. **GFSK 250 kbps default** — both boards `set_phy(PHY.PROP_2_4GHZ)`, TX 20 markers, RX expects ≥10. Spec §F26 closure criterion #1.
2. **GFSK 1 Mbps custom** — both boards `configure_prop(symbol_rate=1000000, deviation=500000, rx_bw=0x59)`, TX 20 markers, RX expects ≥10. Spec §F26 closure criterion #2.
3. **CW @ 2402 MHz on PROP_2_4GHZ** — TX `tx_cw` on 2402 MHz via PROP_2_4GHZ PHY, RX scan BLE 1M ch37 → ambient drops near 0 (proves prop 2.4 GHz path emits on-air, validates F22+F26 integration).
4. **No-regression: BLE post-prop24g** — after step 3, set_phy(BLE_1M, ch37), start_rx, verify ≥30 ambient pkts. F9 hot-switch + F26 path don't degrade BLE.
5. **No-regression: Sub-1G post-prop24g** — set_phy(SUB_1GHZ_868), set_phy back to PROP_2_4GHZ, set_phy back to SUB_1GHZ_868. No timeout, no exception.

Pass criteria: tests 1 + 2 + 4 PASS. Tests 3 and 5 are nice-to-have evidence of integration.

### 5.2 Unit tests — `python/tests/test_prop_24ghz.py`

Hardware-free, run in CI:

1. `test_prop_2_4ghz_phy_enum_value` — `PHY.PROP_2_4GHZ == 8`.
2. `test_set_phy_prop_2_4ghz_default_payload` — `set_phy(PHY.PROP_2_4GHZ)` sends 1-byte payload `[0x08]`.
3. `test_set_phy_prop_2_4ghz_with_frequency_payload` — `set_phy(PHY.PROP_2_4GHZ, frequency_hz=2440000000)` sends 7-byte payload (PHY + channel + frequency).
4. `test_configure_prop_24ghz_freq_passes_through` — `configure_prop(frequency_hz=2440000000, ...)` builds correct CMD_SET_PROP_CONFIG payload (frequency field encodes 2.44 GHz).

### 5.3 Manual checkpoint (deferred)

Optional spectrum analyzer or third-board test:
- Spectrum analyzer at 2440 MHz: confirm GFSK 250 kbps spectrum shape.
- Third-board test against a Nordic ESB receiver or similar real-world peer if available.

Not gating for `v2.0-f26` tag.

## 6. File layout

| File | Change | LOC |
|---|---|---|
| `firmware/cc1352/src/smartrf_prop_2_4ghz.c` | new — RF_Mode + setup/FS/TX/RX cmd structs + overrides | ~250 |
| `firmware/cc1352/include/smartrf_prop_2_4ghz.h` | new — extern declarations | ~30 |
| `firmware/cc1352/include/phy_manager.h` | +1 enum entry | +1 |
| `firmware/cc1352/src/phy_manager.c` | +1 predicate, update supports map | +8 |
| `firmware/cc1352/include/radio_if.h` | +1 RF mode enum entry | +1 |
| `firmware/cc1352/src/radio_if.c` | new path: predicate + apply config + start backend + transmit + wire-ups | ~120 |
| `python/feralrf/enums.py` | +1 PHY entry (`PROP_2_4GHZ = 8`) | +1 |
| `python/tests/test_prop_24ghz.py` | new — 4 unit tests | ~70 |
| `python/examples/lab/smoke_f26_prop_24ghz.py` | new — 5 hardware tests | ~150 |
| `docs/superpowers/specs/2026-04-29-f26-prop-24ghz-design.md` | this file | (already counted) |
| `docs/superpowers/plans/2026-04-29-f26-prop-24ghz-plan.md` | next: writing-plans output | (next step) |

Total: **~630 LOC** new code. Comparable to F10 port (props sub-1g) in scope.

## 7. Risks

| # | Risk | Mitigation |
|---|---|---|
| f26-r1 | SmartRF studio config for 2.4 GHz prop has overrides incompatible with the multi_protocol CPE patch shared with BLE/IEEE | Test 4 (BLE post-prop24g) catches this. If breaks, evaluate per-mode overrides or revert to dedicated patch. |
| f26-r2 | F9 hot-switch from BLE/IEEE/Sub1G to PROP_2_4GHZ may fail because `switchRfMode` runs `Prop24g_cmdPropRadioSetup` (a `CMD_PROP_RADIO_SETUP_PA`) on a handle previously set up for `Ble5_0_cmdBle5RadioSetup`. The setup commands are different command numbers (0x3806 vs 0x1820) — TI driver may or may not accept this. | Validation tests 4+5 cover both directions. If fails, fall back to close+open for transitions to/from prop24g (small regression in switching speed only, F9 hot-switch otherwise preserved). |
| f26-r3 | TX power table 2.4 GHz `s_tx_power_table_24g` has values calibrated for BLE/IEEE; prop 2.4 GHz may need different entries for accurate dBm | Acceptable mismatch for closure — calibration is F23 territory. Document any observed delta. |
| f26-r4 | Lab 2.4 GHz is full of interferers (Wi-Fi, BLE, microwave). OTA marker tests may be flakier than Sub-1G | Use unique TX address / sync word + filter as F9 manual checkpoint pattern. Bump count or power if flaky. |
| f26-r5 | `centerFreq` is uint16_t (MHz); 2484 MHz fits. No overflow risk for the band 2400-2483 | None |

## 8. Closure criteria

- [ ] Unit tests 4/4 PASS in `test_prop_24ghz.py`.
- [ ] Hardware smoke tests 1, 2, 4 PASS on 2 boards.
- [ ] Python full suite no regression (current 299 + 4 = 303 expected).
- [ ] No regression on F9 6/6 PHY switch matrix (sanity 1 cycle).
- [ ] No regression on F22 smoke (5/5).
- [ ] Pre-commit clean.
- [ ] Plan in `docs/superpowers/plans/2026-04-29-f26-prop-24ghz-plan.md` covered checkbox by checkbox.
- [ ] `project_f26_done.md` memory entry written.
- [ ] Commit on `feature/f26-prop-24ghz`. Tag `v2.0-f26` after smoke PASSES. FF to `feature/ti-rtos-migration` per project pattern.

## 9. Open questions

None — design constraints are tight per spec §F26 + brainstorm decisions.

---

**Next step:** writing-plans skill → step-by-step implementation plan.
