# F20.a.1.d Phase 1 — AdvA Comparison (software-side only)

**Date:** 2026-05-11
**Branch:** `feature/f20a1-peripheral-read` HEAD `7cb78e0`
**Smoke evidence:** `2026-05-11-f20a1d-phase0-smoke-200.txt`

## Hardware available

- 2 CatSniffer CC1352P7 boards (master on `/dev/ttyACM3`, slave on `/dev/ttyACM0`)
- **No 3rd CC1352 with Sniffle firmware** — Phase 1 wire-level dual-capture from the plan was not possible. Software-side AdvA comparison only.

## What we have

From the count=200 smoke run with both boards flashed at CRC `0xed8d796f`:

| Source | Field | Value |
|--------|-------|-------|
| Smoke harness | `--target-mac` | `DE:AD:BE:EF:CA:FE` |
| Slave firmware telemetry | `f21_adv_a` (RSP_DEBUG_SLAVE off 35-40, MSB-first display) | `DE:AD:BE:EF:CA:FE` |
| Central telemetry | `accessAddr` (after `ble_connect`) | `0x71FFAD04` |
| Central telemetry | `hopInterval` | 24 |
| Central telemetry | `supervTimeout` | 100 |

**Slave's `f21_adv_a` exactly matches `--target-mac`.** The Python harness passes `--target-mac` to `Radio.ble_connect(peer_addr=...)` on the central, which becomes the target of the CMD_BLE5_INITIATOR CONNECT_IND. So at the software layer, master and slave agree on the address.

Without a wire trace we cannot prove that the radio TX'es exactly those bytes — but the TI RF driver passing AdvA through `s_f21_bleAdvPar.pDeviceAddress` → `Ble5_0_cmdBleAdv` is a well-trodden path used elsewhere (F8a, F21). No reason to suspect radio-layer corruption.

## Decision per plan Task 9 Step 4 matrix

| Wire AdvA | Firmware f21_adv_a | Conclusion |
|-----------|--------------------|------------|
| Match | Match | AdvA NOT the issue → proceed to Task 10 Path B (ChSel/timing) |

We have **firmware match** and **no wire data**. Per the plan's risk tolerance for this debug phase, treat as **Match**. AdvA mismatch ruled out.

## Smoking gun for F20.a.1.e

The clean trace pipeline introduced in Phase 0 reveals the real bug surface:

| Field | Value | Meaning |
|-------|-------|---------|
| `f21_last_status` | `0x1402` | `BLE_DONE_NOSYNC` — CMD_BLE_ADV always exits with NOSYNC, never CONNECT |
| `f21_first_nonzero_status` | `0x1402` | First non-OK iter was already NOSYNC. **Every single iteration is NOSYNC.** |
| `advertise_iterations` | 200 | Full loop, no early break |
| `extract_first_pdu_type` | `0x01` | RX queue has 3 ambient ADV_DIRECT_IND, no CONNECT_IND (0x05) |

`BLE_DONE_NOSYNC` from `rf_ble_mailbox.h` means the radio attempted to sync to an incoming packet after TX'ing ADV_IND but did not find one matching its expected criteria. Combined with:
- Central confirms CONNECT_IND was sent (accessAddr/hopInterval/supervTimeout parsed)
- Slave's RX queue captures ambient ADV but no CONNECT_IND

→ The slave radio **does** open an RX window after each ADV_IND but **rejects** the CONNECT_IND from the master. NOSYNC on every iter from iter 1 is consistent with a **protocol-level rejection**, not a timing window miss (timing miss would show occasional NOSYNC mixed with OK).

## Hypotheses for F20.a.1.e, re-prioritized

Given uniform NOSYNC (not intermittent):

1. **(HIGH) ChSel#2 mismatch.** Master uses BLE 5 `CMD_BLE5_INITIATOR` (opcode 0x1828). Slave uses BLE 4.x `CMD_BLE_ADV` (`Ble5_0_cmdBleAdv`). BLE5 initiator sets ChSel bit in CONNECT_IND header (bit 5 of byte 0); BLE 4.x ADV may reject CONNECT_INDs with ChSel set. Uniform NOSYNC fits this pattern perfectly.

2. **(MED) `bAppendTimestamp = 1` on `s_f21_bleAdvPar.rxConfig`** could interfere with CONNECT_IND auto-detect in BLE 4.x mode. Cheap to flip and test.

3. **(LOW) Task_sleep blind window.** If the issue were timing, we'd see occasional iterations succeed. Uniform NOSYNC argues against this.

4. **(LOW) AdvA mismatch.** Ruled out above.

## Next action

Task 10 Path B: write `F20.a.1.e` plan stub focused on switching slave to a BLE5-compatible advertise command (`CMD_BLE5_ADV_LEGACY` or equivalent) and/or testing `bAppendTimestamp=0`.
