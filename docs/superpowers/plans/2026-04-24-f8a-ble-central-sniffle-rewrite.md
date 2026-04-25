# F8A — BLE Central rewrite based on Sniffle (spec + investigation log)

> **Status:** spec only — not yet implemented. 2-3 sessions of firmware work estimated. Blocks F8 (GATT validation).

**Goal:** Replace the current CMD_BLE5_INITIATOR-based BLE central with a Sniffle-style implementation: construct CONNECT_IND ourselves, own the anchor time, and drive MASTER events with full timing control. This unblocks F8 because the current implementation cannot reliably sustain a connection past the first master event.

**Architecture rationale:** FeralRF currently uses TI's opaque `CMD_BLE5_INITIATOR` (commit `84dc0ea` / `4a78c57`), which handles the CONNECT_IND TX internally and reports a `connTime` on completion. Testing on 2026-04-24 showed this `connTime` does not correspond reliably to the peer's listening window: the first `CMD_BLE5_MASTER` fires at `connTime` and the peer never responds (`BLE_DONE_NOSYNC = 0x1402`). Even a Sniffle-style `WinOffset` sweep across `[0, connInterval]` in 1.25 ms steps failed to find the peer. Sniffle builds its own CONNECT_IND and controls the anchor with microsecond precision, which works reliably. This plan adopts that approach.

**Tech Stack:** TI-RTOS 7 (SysBIOS), CC1352P7 RF driver (SDK 8.30), C11.

**References:**
- Sniffle source at `/home/sabas/Documents/electroniccats/Sniffle/fw/`:
  - `RadioTask.c:174-335` — `handleConnReq()` (CONNECT_IND construction + anchor).
  - `RadioTask.c:432-530` — CENTRAL state handler (master event loop + WinOffset sweep fallback).
  - `RadioWrapper.c:444-510` — `RadioWrapper_central()` wrapping CMD_BLE5_MASTER.
- This session's investigation branch: `fix/uart-starvation-during-conn` (keep as reference, do not merge).
- Evidence:
  - `/tmp/sniffle_ch573_initiator.txt` — Sniffle firmware successfully connecting to CH573 at `DC:32:62:8D:E1:09` on this board (IEEE `00:12:4B:00:2A:79:BF:F1`), confirming hardware/peer/environment are fine.
  - `/tmp/fix_uart_sweep_debug.txt` — FeralRF `WinOffset` sweep across 24 offsets never finds the peer (`tx=0 rx=0 events=14 last=0x1402`), then supervision times out.

---

## What F8A must deliver

### 1. Replace CMD_BLE5_INITIATOR with manual CONNECT_IND

- Drop `CMD_BLE5_INITIATOR` from the central path. Keep the struct around only for reference (`firmware/cc1352/src/smartrf_ble5_0.c:Ble5_0_cmdBle5Initiator*`).
- New flow (modeled after Sniffle `handleConnReq`):
  1. Scan channel 37/38/39 (already works via `CMD_BLE5_SCANNER`) until target advertiser found.
  2. Build CONNECT_IND LL PDU manually:
     - PDU header: 0b0101 (CONNECT_IND) + adv/init address types.
     - InitA (6 B), AdvA (6 B), LL data (22 B): AA, CRCInit, WinSize, WinOffset, Interval, Latency, Timeout, ChM, Hop+SCA.
  3. TX CONNECT_IND on the advertising channel **exactly** at the advertiser's T_IFS (150 µs) after its ADV_IND ended.
  4. Capture the RAT tick of CONNECT_IND TX end — that is our authoritative `connTime`.
  5. First master event anchor = `connTime + transmitWindowOffset + transmitWindowDelay` where `transmitWindowDelay = 1.25 ms` and `transmitWindowOffset` is whatever we put in the CONNECT_IND (typically 0 or a value we choose).
- Reuse CSA#2 (`csa2.c`) for channel hopping. Already in tree, already tested.

### 2. Move BleConnMgr_poll to RfTask (F8-prereq, already validated)

Already implemented on branch `fix/uart-starvation-during-conn` commit `f125473`. Re-apply cleanly on the F8A branch. This fixes UART starvation. The follow-up `WinOffset` sweep (commit `5b7325a` on that same branch) is **not** kept — once CONNECT_IND is manual, the anchor is known exactly.

### 3. Keep existing wins from current central code

- `csa2.c` (from Sniffle, GPLv3) — CSA#2 channel computation. Keep.
- `ble_conn.c/h` — state structure (`BleConn_State` with accessAddr, crcInit, channelMap, etc.). Adapt: `connTime` now comes from our own TX timestamp.
- `ble_conn_mgr.c` — connection event loop. Keep the structure, replace `CMD_BLE5_INITIATOR` dependency with manual CONNECT_IND + `CMD_BLE5_GENERIC_TX`/`CMD_BLE5_MASTER`.
- `tx_queue.c/h` — TX queue. Keep.
- `att_client.c/h` — GATT/ATT state machine. Keep, runs unchanged over the new LL.

### 4. Retire the ICall/BLE5-Stack vestiges

- `firmware/cc1352/startup/osal_icall_ble.c` — delete.
- `firmware/cc1352/syscfg/ti_ble_config.c/h` — delete.
- `firmware/cc1352/include/config.h` — drop ICall `#if 0` blocks.
- `main_rtos.c` — drop the `#if 0 ICall_init()` block.

Decision #18 in the master spec already says "raw RF Sniffle-style, NO ICall/BLE5-Stack". This is overdue cleanup.

---

## Estimated session breakdown

**Session 1 — CONNECT_IND construction + first anchor (~4-5 h)**
- Study Sniffle's `handleConnReq()` in detail. Read `BLE Core Spec v5.0 Vol 6 Part B §2.3.3.1` for CONNECT_IND layout.
- Port the CONNECT_IND builder into FeralRF.
- Wire `command_processor CMD_CONNECT` to: (a) scan → wait for target ADV_IND → (b) TX CONNECT_IND → (c) capture `connTime` → (d) `BleConnMgr_start()`.
- Remove `CMD_BLE5_INITIATOR` usage from `RadioIF_bleInitiate()`.
- Checkpoint: CONNECT_IND goes on the wire (verify by sniffing with Sniffle on another CatSniffer).

**Session 2 — First master event + sustained connection (~4-5 h)**
- With known `connTime`, fire first master event at `connTime + transmitWindowOffset`. Expected BLE_DONE_OK.
- Second event at `connTime + transmitWindowOffset + connInterval`. Validate anchor drift handling.
- Handle LL control PDUs (LL_FEATURE_REQ, LL_VERSION_IND) — already done in `ble_conn_mgr.c:handle_ll_ctrl`, ensure it still works over the new LL.
- Checkpoint: CH573 connects and stays up for 10+ events. `conn_status` shows `connected=True events>0 tx>0 rx>0 last_status=0x1400`.

**Session 3 — GATT validation + ICall cleanup (~3-4 h)**
- Run F8's `demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 --read`. Expected: discovery + read pass.
- Delete `osal_icall_ble.c`, `ti_ble_config.*`, ICall `#if 0` blocks.
- Validate regression: BLE/IEEE/Sub-1GHz scan still work; 8/8 PHYs smoke test passes.
- Cleanup + commit + tag `v2.0-f8a`.
- Unblocks F8 T12 checkpoint humano.

---

## Constraints / risks

- **R1 — RAT timing precision for CONNECT_IND TX.** BLE spec requires 150 µs T_IFS after ADV_IND end. The RF driver's `CMD_BLE5_ADV_NC` or `CMD_BLE5_GENERIC_TX` with `TRIG_REL_PREVEND` can schedule it, but calibrating the offset is firmware work. Sniffle does this — read their code.
- **R2 — Filter-policy on scanner.** We need to scan for ONE target MAC and TX CONNECT_IND on its ADV_IND without racing other advertisers. `CMD_BLE5_SCANNER` with a whitelist or `rxConfig.bDeviceAddrType`/`pDeviceAddress` in the RX params.
- **R3 — Existing advertising attacks on `main` branch.** Don't break them. They don't use central mode, but we touch `smartrf_ble5_0.c` and `radio_if.c` — regression check in Session 3.
- **R4 — Supervision timeout on first event.** If the first master event misses by more than `supervTimeout / connInterval` events, the peripheral drops us. With `connTime` correctly captured, this should be a non-issue.

---

## Out of scope for F8A

- BLE peripheral mode emulation (that's F17 in master plan).
- BLE scanner active (SCAN_REQ/SCAN_RSP) — that's F12.
- ATT MTU exchange — only needed if we read characteristics larger than 20 B during F8 validation. Defer to F8B if hit.
- L2CAP SDU reassembly for >27 B payloads — not required for F8 validation against CH573.

---

## Handoff state at end of 2026-04-24 session

- **Board:** CatSniffer #3 (CC1352P7, IEEE `00:12:4B:00:2A:79:BF:F1`, 704 KB flash). Flashed with FeralRF pre-fix firmware (`feature/f8-gatt-validation` HEAD firmware source = `41b81fe`).
- **Board #1 (IEEE `...C1:82`):** appears to have antenna/RF issue — does not receive BLE even with Sniffle firmware. Set aside, not tested further.
- **Board #2 (IEEE `...72:AC`):** flashes fail (timeout on Sync). Package detected as "CC1350 PG2.0 - 352 KB Flash" — might be CC1352P (not P7). Skipped.
- **Reference branch:** `fix/uart-starvation-during-conn` with commits:
  - `b6f1bea` docs(f8-prereq): add plan for UART starvation fix
  - `55d934b` fix(ble): move BleConnMgr_poll (initial, reverted)
  - `a572f7e` Revert of 55d934b
  - `f125473` fix(ble): move BleConnMgr_poll (re-applied)
  - `5b7325a` fix(ble): port WinOffset sweep from Sniffle (incomplete — doesn't solve NOSYNC)
- **Target peer:** WCH CH573 (BLE 4.2 dev board) at `DC:32:62:8D:E1:09` (public), conn interval 30 ms per Sniffle's capture.
- **Evidence files:** all under `/tmp/fix_uart_*.txt` and `/tmp/sniffle_ch573_initiator.txt` (ephemeral — may not survive reboot).

---

## When this plan gets executed

- Write a proper implementation plan (format: `docs/superpowers/plans/YYYY-MM-DD-f8a-ble-central-sniffle-rewrite-impl.md`) using the `writing-plans` skill.
- Plan breaks down per-session tasks in the TDD-style format this repo already uses (see `2026-04-24-f8-validate-gatt.md`).
- After F8A closes, resume F8 T12 checkpoint humano on the board that supports BLE central.
