# F20.a.1.e — ChSel#2 / Timing Hypothesis Plan Stub

**Status:** stub (not ready for execution yet — needs spec refinement)
**Predecessor:** F20.a.1.d (`v2.0-f20.a.1.d-partial`), see `docs/superpowers/evidence/2026-05-11-f20a1d-phase1-advA-comparison.md`
**Branch:** continue on `feature/f20a1-peripheral-read`

## Smoking gun from F20.a.1.d

From clean trace pipeline (count=200 smoke 2026-05-11):
- `f21_last_status = 0x1402 (BLE_DONE_NOSYNC)` on EVERY iteration (200/200).
- `f21_first_nonzero_status = 0x1402` — first iter already NOSYNC.
- `f21_adv_a` matches central's `--target-mac` software-side (AdvA mismatch ruled out).
- Central confirms CONNECT_IND TX'd (parses accessAddr/hopInterval/supervTimeout).

→ Slave radio rejects CONNECT_IND uniformly. Not timing (intermittent would mix OK/NOSYNC).
→ Most likely cause: BLE 5 initiator vs BLE 4.x advertiser PDU header mismatch (ChSel bit).

## Hypotheses to test (ordered cheap → expensive)

### H1: `bAppendTimestamp = 1` interfering with BLE 4.x CONNECT_IND auto-detect

**Cost:** 1 line flip.
**Test:** in `RadioIF_transmitBleAdvLegacy`, set `s_f21_bleAdvPar.rxConfig.bAppendTimestamp = 0;` before invoking the command. Re-run smoke V2 count=200. If `f21_last_status` becomes `0x1404 BLE_DONE_CONNECT`, root cause found.
**Risk:** Without timestamp, `RadioIF_extractConnectIndParams` can't compute `connectIndEndRat` from packet TS. Need fallback to `RF_getCurrentTime()` — F8a-era code path.

### H2: Switch slave to `CMD_BLE5_ADV_LEGACY` (or equivalent BLE5 cmd)

**Cost:** Medium — new command struct + parameter mapping.
**Test:** Replace `Ble5_0_cmdBleAdv` (opcode 0x1805) with `CMD_BLE5_ADV_LEGACY` (verify opcode in `rf_ble_cmd.h`). Use `rfc_CMD_BLE5_ADV_LEGACY_t` params struct. Same channel/AdvA/data parameters but BLE5-aware so ChSel#2 CONNECT_INDs are accepted.
**Risk:** `radio_if.c:577` notes `multi_protocol does NOT support CMD_BLE5_ADV_AUX (hangs)`. Investigate whether `CMD_BLE5_ADV_LEGACY` shares the patch issue. Test against the F8 master path (which works fine with BLE5 initiator) to bracket.
**Reference:** TI SDK 8.30 SimpleLink BLE examples typically use `CMD_BLE5_ADV_AUX` or similar for connectable adv. Compare against `~/Documents/electroniccats/ti-examples/` simple_peripheral.

### H3: Force master to clear ChSel bit before TX'ing CONNECT_IND

**Cost:** Low (1 bit flip on master's prepared CONNECT_IND PDU).
**Test:** Inspect master's CMD_BLE5_INITIATOR output before issuing. If it sets `chSel = 1` in its CONNECT_IND parameters, force `chSel = 0`. Should pre-empt H2.
**Risk:** Changes master's behavior; may break other F8 paths that rely on CSA#2. Scope ONLY to the F20.a.1 peripheral-read flow.

### H4: Multi-channel ADV (deferred — uniform NOSYNC argues against)

Single-channel (37 only) ADV at 10 ms interval is unlikely to cause uniform NOSYNC, but worth eliminating as variable in a final sweep.

## Suggested task order for F20.a.1.e

1. **Task 1:** Apply H1 (bAppendTimestamp=0) + add fallback `connectIndEndRat = RF_getCurrentTime() + estimated airtime`. Smoke V2 count=200. If PASS → tag and close.
2. **Task 2** (if H1 fails): Inspect master's CONNECT_IND chSel field via `Ble5_0_cmdBle5Initiator.pParams.chSel` (verify field name in `rf_ble_cmd.h`). If chSel=1, apply H3 first (cheaper than H2). Smoke. If PASS → tag and close.
3. **Task 3** (if H3 fails): Apply H2 (switch to BLE5 ADV legacy cmd). This is a larger change — proper plan needed before starting. Defer to F20.a.1.f if H3 isn't conclusive.
4. **Task 4:** Endurance smoke 3x count=5000 + sustain ≥10 events.
5. **Task 5:** Update memory, tag `v2.0-f20.a.1`, FF coordination with user.

## Open questions

- Does `rfc_bleAdvPar_t` have a `chSel` flag? (TI SDK 8.30 docs may say BLE 4.x ADV ignores it; need to confirm.)
- Is `CMD_BLE5_ADV_LEGACY` available in SDK 8.30 multi_protocol pPatch? Compare to `CMD_BLE5_ADV_AUX` which is documented as broken in `radio_if.c:577`.
- What is `BLE_DONE_NOSYNC` precisely supposed to indicate per TI mailbox docs? Need to read `rf_ble_mailbox.h` comments to disambiguate "no peer found" vs "peer found but rejected".

## Confounders to address before testing

- After 5000-iter loop the slave stays `s_pending_ready=true` for minutes (F20.a.1.c memory). Smoke harness needs forced reset between runs OR longer post-run join timeout.
- `f21_adv_a` display in smoke V2 has a missing column separator (cosmetic, not blocker).

## Done criteria

- `f21_last_status` returns `0x1404 (BLE_DONE_CONNECT)` or `0x140A (CONNECT_CHSEL0)` on connect.
- Slave RX ring has ≥1 entry with non-zero `nRxOk`.
- 3 endurance runs sustain ≥10 events each.
- Tag `v2.0-f20.a.1` pushed.

This stub will be expanded into a full implementation plan via `superpowers:writing-plans` when ready to execute F20.a.1.e.
