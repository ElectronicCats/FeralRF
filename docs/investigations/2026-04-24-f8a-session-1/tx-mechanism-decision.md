# Task 5 — TX-mechanism decision for CONNECT_IND

> Output of Task 5 (investigation gate) of `docs/superpowers/plans/2026-04-24-f8a-session-1-connect-ind-first-anchor.md`. Written 2026-04-24, before Task 6.

---

## Constraint discovered while writing the plan

The F8A spec (`docs/superpowers/plans/2026-04-24-f8a-ble-central-sniffle-rewrite.md`) names `CMD_BLE5_GENERIC_TX` as the TX primitive for a manual CONNECT_IND. **That command does not exist in SDK 8.30**. Verified by listing every BLE opcode in `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ble_cmd.h`:

```
CMD_BLE5_RADIO_SETUP    0x1820   CMD_BLE5_ADV         0x182B
CMD_BLE5_SLAVE          0x1821   CMD_BLE5_ADV_DIR     0x182C
CMD_BLE5_MASTER         0x1822   CMD_BLE5_ADV_NC      0x182D
CMD_BLE5_ADV_EXT        0x1823   CMD_BLE5_ADV_SCAN    0x182E
CMD_BLE5_ADV_AUX        0x1824   CMD_BLE5_TX_TEST     0x182A
CMD_BLE5_ADV_PER        0x1825   CMD_BLE5_INITIATOR   0x1828
CMD_BLE5_SCANNER_PER    0x1826   CMD_BLE5_GENERIC_RX  0x1829
CMD_BLE5_SCANNER        0x1827
```

There is also a *legacy* `CMD_BLE_TX_TEST` (0x180A) but no generic-TX equivalent for arbitrary BLE PDU types. The advertising-family commands (`ADV_NC`/`ADV`/`ADV_DIR`/`ADV_SCAN`) all *fix the PDU type via the opcode* — `rfc_bleAdvPar_s` exposes `pDeviceAddress` and `advLen` but the radio prepends a header whose PDU-type bits are forced by which `CMD_BLE5_ADV_*` you ran. A CONNECT_IND has PDU type `0b0101` (CONNECT_IND, an advertising-channel PDU), distinct from `ADV_NONCONN_IND` (`0b0010`). **There is no opcode that lets us TX a BLE5 LL PDU with arbitrary header bits.**

Sniffle confirms this read of the SDK — its INITIATING path uses `CMD_BLE5_INITIATOR` (`Sniffle/fw/RadioWrapper.c:645–737`), the same opaque command FeralRF currently uses. There is no "manual TX of CONNECT_IND" hiding in Sniffle's source.

The F8A spec's framing — "Sniffle builds its own CONNECT_IND" — is partially accurate: Sniffle builds the **22-byte LLData** itself and hands it to `CMD_BLE5_INITIATOR` via `pConnectReqData`. The SDK then wraps it in the CONNECT_IND PDU and emits it on air. **FeralRF does the same already**: `firmware/cc1352/src/ble_conn.c:142` calls `ble_conn_build_ll_data(...)` (now delegating to `BleConnPdu_buildLlData()` via Task 4) and assigns it to `pConnectReqData` (line 176). There is no architectural-rewrite step still owed to Sniffle parity at the TX layer.

**Conclusion:** the Session 1 work is *not* a TX-command rewrite. It is finding the parameter or RF-state-lineage delta that makes Sniffle's `CMD_BLE5_INITIATOR` produce a peripheral-honored anchor and FeralRF's does not.

---

## Parameter delta — FeralRF (`ble_conn.c` + `radio_if.c`) vs. Sniffle (`RadioWrapper_initiate`)

Direct field-by-field comparison of `Ble5_0_cmdBle5Initiator.pParams->...` setup:

| Field | FeralRF | Sniffle | Δ |
|-------|---------|---------|---|
| `commandNo` | 0x1828 (struct default) | 0x1828 (struct default) | — |
| `channel` | **hard-coded 37** | parameter `chan` from CommandTask STATIC state | **YES** |
| `whitening.init` | `0x40 + 37` | `0x40 + chan` | follows channel |
| `phyMode.mainMode` | 0 (1M) | param `phy` mapped: 1M→0, 2M→1, Coded S2→2 | equivalent for 1M case |
| `phyMode.coding` | 0 | param `phy`: Coded S2 → 6, else 4 | **YES — FeralRF=0, Sniffle=4 for 1M** |
| `pRxQ` | set by `RadioIF_bleInitiate` to `s_rf_data_queue` | set to `dataQueue` | equivalent |
| `rxConfig.bAutoFlushIgnored` | 1 | 1 | — |
| `rxConfig.bAutoFlushCrcErr` | 1 | 1 | — |
| `rxConfig.bAutoFlushEmpty` | 0 | 0 | — |
| `rxConfig.bIncludeLenByte` | 1 | 1 | — |
| `rxConfig.bIncludeCrc` | 0 | 0 | — |
| `rxConfig.bAppendRssi` | 1 | 1 | — |
| `rxConfig.bAppendStatus` | 1 | 1 | — |
| `rxConfig.bAppendTimestamp` | 1 | 1 | — |
| `initConfig.bUseWhiteList` | 0 | 0 | — |
| `initConfig.bDynamicWinOffset` | 1 | 1 | — |
| `initConfig.deviceAddrType` | 1 (random, hard-coded) | `initRandom ? 1 : 0` | equivalent (FeralRF own addr is random static) |
| `initConfig.peerAddrType` | parameter | `peerRandom ? 1 : 0` | equivalent |
| `initConfig.bStrictLenFilter` | 1 | 1 | — |
| `initConfig.chSel` | 1 | 1 | — |
| `randomState` | 0 | 0 | — |
| `connectReqLen` | 22 | 22 | — |
| `pConnectReqData` | `s_ll_data` (built by `BleConnPdu_buildLlData`) | `connReqData` (built by Sniffle CommandTask) | byte-equivalent intent |
| `pDeviceAddress` | `s_own_addr_u16` (random static, fixed at boot) | `initAddr` from CommandTask | equivalent |
| `pWhiteList` (used as peer addr) | `s_peer_addr_u16` from CMD_CONNECT payload | `peerAddr` from CommandTask | equivalent |
| `connectTime` | `RF_getCurrentTime() + 4000u` (set in `radio_if.c:2262`) | `RF_getCurrentTime() + 4000` | — |
| `maxWaitTimeForAuxCh` | 0xFFFF | 0xFFFF | — |
| `endTrigger.triggerType` | **TRIG_ABSTIME** | **TRIG_NEVER** (when `forever=true`) | **YES** |
| `endTime` | **`RF_getCurrentTime() + 20000000u`** (5 s) | **0** | **YES** |
| `timeoutTrigger.triggerType` | TRIG_NEVER | TRIG_NEVER | — |
| `timeoutTime` | 0 | 0 | — |

### Indirect / RF-state deltas

These do not appear in the param-table but are different at runtime:

1. **Pre-initiate state hygiene.** FeralRF (`firmware/cc1352/src/radio_if.c:2227–2245`) explicitly cancels the prior RX command and flushes the queue before posting `Ble5_0_cmdBle5Initiator`:
   ```c
   if (s_rf_rx_cmd >= 0) {
       RF_cancelCmd(s_rf_handle, s_rf_rx_cmd, 0);
       RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
       s_rf_rx_cmd = RF_SCHEDULE_CMD_ERROR;
   }
   ```
   Sniffle does not — it just posts the initiator. Cancel+flush takes some milliseconds and re-arms the RAT timer; if `bDynamicWinOffset=1`'s calibration depends on continuity of RF state, this is suspicious.

2. **Channel iteration.** FeralRF only ever issues the initiator on chan 37. Sniffle's INITIATING state is entered after STATIC state has chosen a channel from CommandTask — host-side, can be any of 37/38/39. CH573 advertises round-robin 37→38→39; if the peer's first ADV_IND we observe is on 38 or 39, FeralRF cannot react.

3. **`phyMode.coding` for 1M.** FeralRF sets it to `0`. Sniffle sets it to `4`. The SDK's `rf_ble_cmd.h` documents this as "Coding to use for TX if coded PHY is selected. See the Technical Reference Manual for details." For non-coded PHYs (mainMode=0 or 1) the field is documented as ignored, but Sniffle nonetheless writes 4. This is borderline — could be a stale-state issue if the field is consulted by certain CPE patch internals.

---

## Three options to evaluate

### Option A — Parameter alignment (align FeralRF with Sniffle field-for-field)

**Effort:** low. Edits limited to `firmware/cc1352/src/ble_conn.c:151–190` and `firmware/cc1352/src/radio_if.c:2227–2295`.

**Concrete changes:**
1. `endTrigger.triggerType = TRIG_NEVER`, `endTime = 0` (drop the 5 s timeout — bring back later as a host-side timeout if needed).
2. Drop the `endTime = now + 20000000u` line in `RadioIF_bleInitiate()` — `connectTime = now + 4000u` stays.
3. `phyMode.coding = 4` (Sniffle parity for 1M; documented as ignored but harmless).
4. **Defer**: making channel a parameter — increases scope. Today's CH573 is reachable on 37 within ≤30 ms; if Sniffle works on 37 alone, FeralRF should too. Document but do not fix in Session 1.
5. **Defer**: removing the `RF_cancelCmd + RF_flushCmd` pre-initiate hygiene. It exists for a reason (mode switching); auditing it is a Session 2 telemetry task once we can see the timing.

**Falsifiable test:** flash, run `ble_connect("DC:32:62:8D:E1:09", 0)`, capture `conn_status`. If `events>0` after, this option closes Session 1's "first anchor" goal as a side effect. If still `last_status=0x1402` (NOSYNC) after the param align, Session 2 needs telemetry — but Session 1 still produces correct manual CONNECT_IND on the wire (Task 8 oracle still works).

**Risk:** Sniffle works on this hardware and FeralRF doesn't even after the deltas above are zeroed → there's a fifth, unknown delta. Mitigation: Session 1 ends with the on-wire CONNECT_IND validated regardless, so we are not blocked on this option succeeding.

### Option B — `CMD_BLE5_ADV_NC` payload abuse

**Effort:** high. Build a 38-byte buffer with a forged CONNECT_IND header (`0x05` PDU type) as the first byte and feed it to `pAdvPkt` of `CMD_BLE5_ADV_NC`. Hope the radio respects our header byte rather than rewriting it from the opcode.

**Why probably won't work:** TI's RFC patches construct the advertising header internally based on the opcode. Even if it accepts arbitrary `pAdvPkt` bytes, peers will receive a PDU that decodes as `ADV_NONCONN_IND` (the air header byte the radio actually transmitted, not the one we put in the buffer). And `CMD_BLE5_ADV_NC` does not return a `connectTime` analogue — we lose the anchor info we came for.

**Verdict:** drop.

### Option C — Pre-scan + same-RF-handle pivot to `CMD_BLE5_INITIATOR`

**Effort:** medium. Replace the `RF_cancelCmd + RF_flushCmd` pre-initiate hygiene with: keep `CMD_BLE5_GENERIC_RX` running until we observe an ADV_IND from the target MAC, then issue `CMD_BLE5_INITIATOR` directly without flushing — letting `bDynamicWinOffset=1` calibrate against the just-observed adv timing.

**Why this is interesting:** `bDynamicWinOffset` is documented to compute `WinOffset` based on the time between the observed adv and the to-be-transmitted CONNECT_IND. If the radio loses RAT continuity at the cancel/flush boundary, the calibration runs against a stale anchor. Sniffle's flow keeps the radio "warm".

**Verdict:** valid, but the implementation cost is real (filter callbacks, RX-then-TX state machine across two RF commands without `RF_cancelCmd`). Better as a Session 2 follow-up if Option A doesn't fix NOSYNC. Document the option, do not implement now.

---

## Recommendation

**Option A** for Session 1 Task 6.

Reasoning:
1. It is the smallest delta from current FeralRF and the smallest delta from Sniffle simultaneously — minimal risk of introducing new failures.
2. It is independent of whether the actual root cause is one of the three captured deltas or the unknown fifth: even if `events>0` does not happen, Session 1 still produces the byte-known CONNECT_IND that Task 8 needs.
3. Options B/C are still on the table for Session 2 if telemetry shows NOSYNC persisting after parameter alignment.

**Out-of-scope deferrals for Session 1 (open as Session 2 questions):**
- Channel iteration (37/38/39) — defer; CH573 is reachable on 37.
- Removing pre-initiate `RF_cancelCmd + RF_flushCmd` — defer; needs telemetry to assess safe removal.
- Option C "warm RX→TX pivot" — defer; viable if Option A is insufficient.

---

## What Task 6 will edit

Files: `firmware/cc1352/src/ble_conn.c` and `firmware/cc1352/src/radio_if.c`.

Lines (will be re-checked at edit time):
- `ble_conn.c:155` — `phyMode.coding = 0` → `phyMode.coding = 4`.
- `ble_conn.c:187–188` — `endTrigger.triggerType = TRIG_ABSTIME` → `TRIG_NEVER`; `endTime = 0` (already 0).
- `radio_if.c:2262–2263` — drop the `endTime = now + 20000000u;` line.
- `ble_conn.c:142` — switch byte-pack call site from internal helper to `BleConnPdu_buildLlData()` (refactor ride-along).

Nothing else.
