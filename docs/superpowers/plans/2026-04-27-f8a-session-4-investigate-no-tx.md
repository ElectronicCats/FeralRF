# F8A Session 4 — investigate why `nTxEntryDone == 0` (master never TX) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Determine the root cause of `BLE_DONE_NOSYNC (0x1402)` with `nTxEntryDone == 0` on every master event after CONNECT_IND with CH573. Either close F8A by fixing the bug, or close Session 4 with a documented root-cause hypothesis backed by hard evidence and a clear next-experiment list for Session 5.

**Architecture:** Diagnostic-first, not feature-first. Four falsifiable experiments in cheapest-info-first order, plus a closeout task. Each experiment produces a saved artifact (pcap / JSON / hex captured before+after) and a written verdict in the investigations dir. No more blind tweaks — every code change must be motivated by an experiment that already ran. The plan extends one piece of permanent infrastructure (per-event `pktStatus` telemetry in `RSP_DEBUG_TIMING`); everything else is investigative.

**Tech Stack:** C99 firmware on TI SimpleLink CC13xx/CC26xx SDK 8.30.01.01 / TI-RTOS 7. Python 3.9+ host (`feralrf` package). Sniffle CC1352P7 firmware on board #2 as oracle. CatSniffer v3.x boards. CH573 BLE 4.2 peer (`PwnPet_C81F` / `DC:32:62:8D:E1:09`).

**Pre-existing context — read before starting:**
- `docs/investigations/2026-04-24-f8a-session-1/session-3-closeout.md` (the 4 next-steps that motivate this plan)
- `docs/investigations/2026-04-24-f8a-session-1/session-2-closeout.md` (InitA RFU bug, fixed)
- `docs/investigations/2026-04-24-f8a-session-1/session-1-closeout.md` (Option A baseline)
- `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §5 F8A (exit criterion)

**Hypotheses already falsified — DO NOT re-test:**

| ID | Hypothesis | Outcome |
|----|-----------|---------|
| H1 | skip `RF_cancelCmd`/`RF_flushCmd` before initiator | neutral, NOSYNC same |
| H2 | WinSize 3→10 | peer kept link longer but every event NOSYNC |
| H3 | alternate anchor formula + `bDynamicWinOffset=0` | NOSYNC |
| H4 | anchor offset = `transmitWindowDelay + WinOffset×1.25 ms` | math correct, NOSYNC persists |
| H5 | `bAutoFlushEmpty 1→0` (Sniffle parity) | neutral |
| H6 | CSA#1 fallback + `chSel=0` (CH573 is BLE 4.2) | neutral |

`CMD_BLE5_GENERIC_TX` does not exist in SDK 8.30 — do not attempt to rewrite around it.

**Hardware required throughout:**
- **Board #1 (CatSniffer 504B32):** FeralRF firmware under test. UART bridge through RP2040 → `/dev/ttyACM*` (varies). Discover with `python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py devices`.
- **Board #2 (CatSniffer 565932):** Sniffle stock `sniffle_cc1352p7_1M.hex` (already flashed). Used as on-air oracle.
- **CH573 BLE 4.2 peer:** `DC:32:62:8D:E1:09` (`PwnPet_C81F`), advertising at conn interval 30 ms. Must be powered on and within 1 m of both boards for every test.

**Branch:** `feature/f8a-ble-central-sniffle` (HEAD `b238930` at plan start). Do NOT create a sub-branch — Session 4 commits go straight onto this branch like Sessions 1-3.

**Tag at close:** `v2.0-f8a` ONLY if Task 5 closes the connection end-to-end against CH573. Otherwise keep the branch open and leave the tag deferred to Session 5.

---

## File Structure

| Path | Role |
|------|------|
| `docs/investigations/2026-04-24-f8a-session-1/session-4-preflight.md` | Captured port mapping + CH573 alive check (Task 0). |
| `python/examples/lab/f8a_session4_sniffle_pinned.py` | NEW. Drives Sniffle (board #2) via `sniffle_hw` to pin a specific data channel + AA, captures for N seconds (Task 1). |
| `docs/investigations/2026-04-24-f8a-session-1/session-4-pinned-channel.md` | Verdict + raw capture filenames for Task 1. |
| `docs/investigations/2026-04-24-f8a-session-1/session-4-pktstatus-evidence.md` | Verdict for Task 2 (per-event `pktStatus`). |
| `docs/investigations/2026-04-24-f8a-session-1/session-4-sniffle-central-ab.md` | Verdict for Task 3 (Sniffle CENTRAL A/B). |
| `docs/investigations/2026-04-24-f8a-session-1/session-4-slow-conn.md` | Verdict for Task 4 (slow `connInterval`/`supervTimeout`). |
| `docs/investigations/2026-04-24-f8a-session-1/session-4-closeout.md` | Final close-out (Task 5). |
| `firmware/cc1352/include/radio_if.h` | Extend `RadioIF_bleCentral` with output stats struct (Task 2). |
| `firmware/cc1352/src/radio_if.c` | Populate stats from `rfc_bleMasterSlaveOutput_t` (Task 2). |
| `firmware/cc1352/include/ble_conn_mgr.h` | Extend `BleConnMgr_DbgTimingEntry` (Task 2). |
| `firmware/cc1352/src/ble_conn_mgr.c` | Wire stats into ring buffer (Task 2). |
| `firmware/cc1352/src/command_processor.c` | New 17-byte wire layout for `RSP_DEBUG_TIMING` (Task 2). Optional `connInterval`/`supervTimeout` in `CMD_CONNECT` payload (Task 4). |
| `python/feralrf/_responses.py` | Extend `DebugTimingEntry` parser (Task 2). |
| `python/feralrf/commands.py` | Optional `interval` + `supervision_timeout` for `ble_connect` (Task 4). |
| `python/feralrf/radio.py` | Pass-through optional kwargs (Task 4). |
| `python/tests/test_responses.py` | Parser unit test for new fields (Task 2). |
| `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8a_session4.md` | Replace `project_f8a_session3.md` as latest session memory (Task 5). |

---

## Task 0: Pre-flight verification

**Files:**
- Create: `docs/investigations/2026-04-24-f8a-session-1/session-4-preflight.md`

This task fails fast if the hardware bench is not in the assumed state. The four experiments below all rely on board #1 + board #2 + CH573 being live and on the correct ports. Skipping this is how Session 3 burned 30 minutes on phantom failures.

- [ ] **Step 1: Verify branch + clean tree**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git branch --show-current
git log --oneline -3
git status
```

Expected output:
```
feature/f8a-ble-central-sniffle
b238930 wip(f8a): Session 3 groundwork — anchor + CSA#1 + Sniffle parity (NOSYNC persists)
1388c16 telemetry(f8a): paired FeralRF/Sniffle capture + offset analyzer
f6304a5 feat(f8a): add CMD_DEBUG_TIMING / RSP_DEBUG_TIMING (0x47/0xA8)
On branch feature/f8a-ble-central-sniffle
nothing to commit, working tree clean
```

If branch is wrong: `git checkout feature/f8a-ble-central-sniffle`. If HEAD is not `b238930`: STOP and ask the user — Session 3 commits may have been amended.

- [ ] **Step 2: Discover ports for both boards**

```bash
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py devices
```

Expected: 2 entries, each with a `/dev/ttyACM*` device path. Record which serial number is which board (504B32 = board #1 = FeralRF; 565932 = board #2 = Sniffle). The ttyACM numbers change on every USB hub re-plug — every subsequent task will need to re-read these.

- [ ] **Step 3: Verify CH573 is advertising**

Substitute `<ACM_BOARD1>` with the FeralRF board path from Step 2, then run:

```bash
cd python && timeout 6 python3 -c "
from feralrf import Radio
from feralrf.enums import PHY
import time
r = Radio('<ACM_BOARD1>'); r.connect(); r.init()
r.set_phy(PHY.BLE_1M, 37, 2402_000_000); r.start_rx()
time.sleep(3)
pkts = list(r.read_packets()); r.stop_rx()
target = bytes.fromhex('09E18D6232DC')
hits = sum(1 for p in pkts if target in p.data)
print(f'CH573 ADV_IND: {hits}/{len(pkts)}')
"
```

Expected: `hits >= 1`. If 0 hits: power-cycle CH573, retry once. If still 0 after retry: STOP and tell the user — every downstream task is wasted effort without a live peer.

- [ ] **Step 4: Write preflight artifact**

Create `docs/investigations/2026-04-24-f8a-session-1/session-4-preflight.md` with this exact content (substitute the captured values):

```markdown
# F8A Session 4 — preflight (YYYY-MM-DD HH:MM)

- Branch: `feature/f8a-ble-central-sniffle`
- HEAD: `b238930`
- Working tree: clean

## Hardware

| Board | SN | Firmware | Port |
|-------|----|----|----|
| #1 | 504B32 | FeralRF b238930 | `/dev/ttyACM<N>` |
| #2 | 565932 | sniffle_cc1352p7_1M | `/dev/ttyACM<M>` |

## CH573 alive check

CH573 ADV_IND on ch 37 (2402 MHz): **<HITS>/<TOTAL>** in 3 s window.
Verdict: alive ✅ / dead ❌
```

- [ ] **Step 5: Commit preflight**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/session-4-preflight.md
git commit -m "$(cat <<'EOF'
docs(f8a): land Session 4 preflight (port + CH573 verification)

Records bench state at the start of Session 4 so subsequent task
artifacts can reference port assignments without ambiguity.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: pre-commit hook passes, one commit lands. If pre-commit fails: fix the underlying issue (NEVER `--no-verify`; see memory `feedback_precommit.md`).

---

## Task 1: Sniffle pinned to a single data channel — does our master TX go on air at all?

**Why:** Session 3 ran Sniffle in `--hop` (auto-follow) mode and saw `TRANSITION: DATA from STATIC` followed by `TRANSITION: STATIC from DATA` ~1 s later, with **no** data PDUs in between. If our CSA#1 hop computation is even slightly off, the auto-follow would also miss the master TX. Pinning Sniffle to one specific data channel + AA bypasses the follow logic and shows whether energy reaches the air.

**Hypothesis under test:** "Master TX never reaches the air" vs "Master TX reaches the air but CH573 misses it."

**Files:**
- Create: `python/examples/lab/f8a_session4_sniffle_pinned.py`
- Create: `docs/investigations/2026-04-24-f8a-session-1/session-4-pinned-channel.md`
- Capture artifacts (binary, not committed): `/tmp/f8a-s4-pinned-ch{N}.pcap` (kept on disk only; the verdict doc references them by absolute path).

**Reference for Sniffle internal API:** `~/Documents/electroniccats/Sniffle/python_cli/sniffle/sniffle_hw.py:133` (`cmd_chan_aa_phy`).

- [ ] **Step 1: Determine the first data channel for our connection**

Our CONNECT_IND uses a **random** `hopIncrement` ∈ [5,16] (see `firmware/cc1352/src/ble_conn.c`). For CSA#1 with `chSel=0` (Session 3 default) the first data channel is `hopIncrement % 37`. To make the experiment reproducible, the script will:

1. Run a FeralRF connection attempt.
2. Read the AA + first data channel from telemetry (`r.conn_status()` exposes nothing about hop today — instead, the script will pin Sniffle to a sweep of plausible channels: ch=5, 10, 15 covers `hopIncrement ∈ [5,16]` with high probability of hitting one channel where master TX should land).

This is robust — if master TX never goes on air, no channel pin will see it. If it does, sweeping 3 candidate channels is enough to land at least one capture.

- [ ] **Step 2: Write the pinned-capture script**

Create `python/examples/lab/f8a_session4_sniffle_pinned.py`:

```python
"""F8A Session 4 — Sniffle pinned-channel passive capture.

Pins board #2 (Sniffle stock) to a fixed data channel + access address
and records every PDU heard for `--duration` seconds while board #1
(FeralRF) attempts a CONNECT_IND.

Usage:
    python3 f8a_session4_sniffle_pinned.py \\
        --feralrf-port /dev/ttyACM8 \\
        --sniffle-port /dev/ttyACM5 \\
        --target DC:32:62:8D:E1:09 \\
        --addr-type 0 \\
        --channel 10 \\
        --duration 5 \\
        --pcap /tmp/f8a-s4-pinned-ch10.pcap

Prints a count of (n_master_pkts, n_slave_pkts) detected on the pinned
channel matching the connection's AA. Exits non-zero if no PDUs at all.
"""

import argparse
import os
import sys
import time
from pathlib import Path

# Add Sniffle's python_cli to sys.path so we can import sniffle_hw
SNIFFLE_PATH = Path("/home/sabas/Documents/electroniccats/Sniffle/python_cli")
sys.path.insert(0, str(SNIFFLE_PATH))

from sniffle.sniffle_hw import SniffleHW, PhyMode, BLE_ADV_AA, BLE_ADV_CRCI  # noqa: E402
from sniffle.pcap import PcapBleWriter  # noqa: E402

from feralrf import Radio  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--feralrf-port", required=True)
    parser.add_argument("--sniffle-port", required=True)
    parser.add_argument("--target", required=True, help="Peer MAC AA:BB:CC:DD:EE:FF")
    parser.add_argument("--addr-type", type=int, default=0)
    parser.add_argument(
        "--channel",
        type=int,
        required=True,
        help="Data channel to pin Sniffle to (0..36)",
    )
    parser.add_argument("--duration", type=float, default=5.0)
    parser.add_argument("--pcap", required=True)
    args = parser.parse_args()

    if not (0 <= args.channel <= 36):
        sys.exit(f"channel must be 0..36, got {args.channel}")

    sniffle = SniffleHW(args.sniffle_port)
    sniffle.cmd_chan_aa_phy(args.channel, BLE_ADV_AA, PhyMode.PHY_1M, BLE_ADV_CRCI)
    sniffle.cmd_pause_done(False)
    sniffle.cmd_rssi(-128)
    sniffle.cmd_mac()
    sniffle.cmd_endtrim(0x10)
    sniffle.cmd_auxadv(False)
    sniffle.cmd_setaddr(b"\x00" * 6)
    sniffle.mark_and_flush()

    feralrf = Radio(args.feralrf_port)
    feralrf.connect()
    feralrf.init()

    addr_le = bytes(reversed(bytes.fromhex(args.target.replace(":", ""))))

    pcap_writer = PcapBleWriter(args.pcap)
    n_pkts = 0
    deadline = time.monotonic() + args.duration

    print(f"[+] Pinned ch {args.channel}, AA=0x{BLE_ADV_AA:08x}, listening {args.duration}s")
    print(f"[+] Triggering CONNECT_IND on board #1...")
    feralrf.ble_connect(addr_le, addr_type=args.addr_type)

    while time.monotonic() < deadline:
        msg = sniffle.recv_and_decode()
        if msg is None:
            continue
        ts = msg.ts_epoch_ns if hasattr(msg, "ts_epoch_ns") else int(time.time() * 1e9)
        body = getattr(msg, "body", None) or getattr(msg, "pdu_bytes", None)
        if body is None:
            continue
        pcap_writer.write_packet(
            ts, getattr(msg, "aa", BLE_ADV_AA), args.channel, getattr(msg, "rssi", 0), body
        )
        n_pkts += 1

    pcap_writer.close()

    try:
        feralrf.ble_disconnect()
    except Exception:
        pass
    feralrf.disconnect()

    print(f"[+] Captured {n_pkts} PDU(s) on ch {args.channel} → {args.pcap}")
    sys.exit(0 if n_pkts > 0 else 1)


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Verify imports work — dry run**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python3 -c "
import sys
sys.path.insert(0, '/home/sabas/Documents/electroniccats/Sniffle/python_cli')
from sniffle.sniffle_hw import SniffleHW, PhyMode, BLE_ADV_AA, BLE_ADV_CRCI
from sniffle.pcap import PcapBleWriter
print('Sniffle imports: OK')
"
```

Expected: `Sniffle imports: OK`. If `ModuleNotFoundError: No module named 'sniffle.pcap'`: stop, search for the actual pcap writer module name in `~/Documents/electroniccats/Sniffle/python_cli/sniffle/`, and update the import.

- [ ] **Step 4: Run the sweep — three pinned captures**

Substitute `<ACM_BOARD1>` and `<ACM_BOARD2>` from Task 0. Run **three** captures (one per channel), each as a fresh CONNECT_IND attempt, **with the user manually power-cycling CH573 between runs** if the peer is still in DATA state (Sniffle log will show STATIC if it's back to advertising).

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate

for CH in 5 10 15; do
    echo "=== Channel $CH ==="
    python3 examples/lab/f8a_session4_sniffle_pinned.py \
        --feralrf-port <ACM_BOARD1> \
        --sniffle-port <ACM_BOARD2> \
        --target DC:32:62:8D:E1:09 \
        --addr-type 0 \
        --channel $CH \
        --duration 5 \
        --pcap /tmp/f8a-s4-pinned-ch${CH}.pcap
    sleep 2
done
```

Note the printed packet counts. **Hardware step:** between runs, if CH573 doesn't return to advertising within ~1 s, ask the user to press its reset button.

- [ ] **Step 5: Decode each pcap and count master vs slave PDUs**

For each non-empty pcap, decode and tally direction:

```bash
for CH in 5 10 15; do
    PCAP=/tmp/f8a-s4-pinned-ch${CH}.pcap
    if [ -s "$PCAP" ]; then
        echo "=== ch $CH ==="
        python3 -c "
import sys
sys.path.insert(0, '/home/sabas/Documents/electroniccats/Sniffle/python_cli')
from sniffle.pcap_decoder import decode_file
total, master, slave = 0, 0, 0
for pkt in decode_file('$PCAP'):
    total += 1
    # In data-channel BLE, LLID + TxAdd not applicable; direction is
    # inferred only by knowing who sent first. Without sequence info,
    # report total + first-byte distribution.
    print(f'  ts={pkt.ts_epoch_ns} len={len(pkt.body)} first={pkt.body[0]:02x}')
print(f'  TOTAL: {total} PDUs')
"
    else
        echo "=== ch $CH: empty ==="
    fi
done
```

If `pcap_decoder.decode_file` doesn't exist (older Sniffle), fall back to:
```bash
python3 -c "
import dpkt
for ts, buf in dpkt.pcap.Reader(open('$PCAP', 'rb')):
    print(f'ts={ts:.6f} len={len(buf)} first16={buf[:16].hex()}')
"
```

(`dpkt` is already in the dev deps via pyserial-asyncio's transitive deps. If not: `pip install dpkt`.)

- [ ] **Step 6: Write the verdict**

Create `docs/investigations/2026-04-24-f8a-session-1/session-4-pinned-channel.md`:

```markdown
# F8A Session 4 — Task 1: Sniffle pinned-channel evidence

**Date:** YYYY-MM-DD
**Goal:** Determine whether master TX reaches the air despite `nTxEntryDone == 0`.

## Method

`f8a_session4_sniffle_pinned.py` pinned board #2 (Sniffle stock) to data
channel C ∈ {5, 10, 15} on AA `0x8E89BED6` (the BLE advertising AA — but our
data-channel AA is connection-random; the script pins on advertising AA as a
control which is wrong for this purpose, see addendum).

**ADDENDUM (read first):** the connection AA is randomized per CONNECT_IND.
The script as written pins on the advertising AA, which means it will only
catch ADV_IND, **not** post-CONNECT_IND data PDUs. Two paths:

  (a) Patch `firmware/cc1352/src/ble_conn.c` to use a fixed deterministic
      AA (e.g. `0x12345678`) and rebuild before this experiment. Revert
      after Task 1.
  (b) Sniff in `--hop` mode first to learn the AA, then re-pin. Sniffle's
      stock `--hop` mode learned the AA in Session 3.

Path (a) is simpler. The plan executor MUST do (a) before running Step 4.

## Captures

| Channel | PDUs captured | Path |
|---------|---------------|------|
| 5 | <N> | `/tmp/f8a-s4-pinned-ch5.pcap` |
| 10 | <N> | `/tmp/f8a-s4-pinned-ch10.pcap` |
| 15 | <N> | `/tmp/f8a-s4-pinned-ch15.pcap` |

## Verdict

- [ ] **Master TX detected on at least one channel.** → bug is downstream
      of TX (CH573 misses the packet). Move to Task 2 to inspect RX state.
- [ ] **Zero PDUs on all three channels.** → master never radiates. The bug
      is in the RF core's master state machine entry. Move to Task 2 to
      confirm with `pktStatus`, then escalate to Task 3 (Sniffle CENTRAL A/B).
- [ ] **Both Master AND Slave PDUs detected.** → connection is actually
      alive on the wire but our software thinks it isn't (queue / output
      struct misread). Move to Task 2 with this expectation.

## Resolution
<one-paragraph interpretation written by the executor>
```

- [ ] **Step 7: If Step 6 addendum applies — patch AA, rebuild, re-run**

If the executor takes path (a) from the addendum:

Edit `firmware/cc1352/src/ble_conn.c` to replace the random AA assignment with `0x12345678`. The exact line is around `BleConn_initiate` where `s_state.accessAddr` is filled. Rebuild:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352
mkdir -p build && cd build
cmake .. > /dev/null && make -j$(nproc) 2>&1 | tail -10
```

Flash board #1:
```bash
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py \
    flash --port <ACM_BOARD1> \
    --hex /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

(Retry flash 2× before asking the user — see memory `feedback_flash_retry.md`.)

Then re-run Step 4 with `--channel 5/10/15` AND update the script's pinned AA to `0x12345678`. **REVERT THE AA CHANGE BEFORE TASK 2** so subsequent experiments use the standard random AA.

- [ ] **Step 8: Commit Task 1 artifacts**

```bash
git add python/examples/lab/f8a_session4_sniffle_pinned.py \
        docs/investigations/2026-04-24-f8a-session-1/session-4-pinned-channel.md
git commit -m "$(cat <<'EOF'
investigate(f8a): Task 1 — Sniffle pinned-channel evidence

Pins board #2 to a single data channel during a FeralRF CONNECT_IND
attempt to determine whether master TX reaches the air independently
of CSA hop math. Captures the verdict that frames Task 2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

DO NOT commit the binary pcaps under `/tmp/`. The verdict markdown references them by absolute path; if Task 5 closeout needs them, they get copied into `docs/investigations/...` at that point.

---

## Task 2: Per-event `pktStatus` telemetry — what state does the master command actually exit in?

**Why:** Today `RSP_DEBUG_TIMING` only carries the final mailbox `status` and `nTxEntryDone`. It cannot distinguish (a) "command never reached TX state" from (b) "command TX'd but never RX'd". Per `rfc_bleMasterSlaveOutput_t` in `firmware/sdk/.../driverlib/rf_ble_cmd.h:2643`, the per-event output struct already holds `nRxOk`, `nRxNok`, `nRxIgnored`, and a `pktStatus` byte (bitfield `bTimeStampValid:1`, `bLastCrcErr:1`, `bLastIgnored:1`, `bLastEmpty:1`, `bLastCtrl:1`, `bLastMd:1`, `bLastAck:1`, 1 reserved). Surfacing those four fields per event is the cheapest piece of data left.

**Hypothesis under test:** "If `nRxOk == 0` and `bTimeStampValid == 0` for every event, the master never even attempted RX. If `bTimeStampValid == 1` and `nRxOk == 0`, RX ran but no slave packet decoded."

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h` (add `RadioIF_BleCentralStats` struct + new param to `RadioIF_bleCentral`)
- Modify: `firmware/cc1352/src/radio_if.c` (populate stats from `output`)
- Modify: `firmware/cc1352/include/ble_conn_mgr.h` (extend `BleConnMgr_DbgTimingEntry` from 13 to 17 wire bytes)
- Modify: `firmware/cc1352/src/ble_conn_mgr.c` (capture stats in ring entry)
- Modify: `firmware/cc1352/src/command_processor.c` (new wire layout, 1 + 16×17 = 273 bytes)
- Modify: `python/feralrf/_responses.py` (extend parser)
- Test: `python/tests/test_responses.py` (parser unit test)
- Create: `docs/investigations/2026-04-24-f8a-session-1/session-4-pktstatus-evidence.md`

- [ ] **Step 1: Write the failing parser test**

Add to `python/tests/test_responses.py`:

```python
def test_debug_timing_parses_extended_entry():
    """Wire layout per entry (Session 4 extension):
        event_idx  u16  LE
        start_rat  u32  LE
        end_rat    u32  LE
        status     u16  LE
        num_sent   u8
        n_rx_ok    u8
        n_rx_nok   u8
        n_rx_ignored u8
        pkt_status u8 (raw bitfield byte)
      = 17 bytes per entry; frame = count(u8) + count*17 bytes.
    """
    from feralrf._responses import DebugTimingResponse

    # one entry: idx=5, start=0xDEAD, end=0xBEEF, status=0x1402,
    # num_sent=0, rx_ok=0, rx_nok=0, rx_ignored=0, pkt_status=0x00
    payload = bytes([
        0x01,                   # count
        0x05, 0x00,             # event_idx = 5
        0xAD, 0xDE, 0x00, 0x00, # start_rat = 0xDEAD
        0xEF, 0xBE, 0x00, 0x00, # end_rat   = 0xBEEF
        0x02, 0x14,             # status    = 0x1402 (NOSYNC)
        0x00,                   # num_sent
        0x00,                   # n_rx_ok
        0x00,                   # n_rx_nok
        0x00,                   # n_rx_ignored
        0x00,                   # pkt_status
    ])
    rsp = DebugTimingResponse.parse(payload)
    assert len(rsp.entries) == 1
    e = rsp.entries[0]
    assert e.event_idx == 5
    assert e.start_rat == 0xDEAD
    assert e.end_rat == 0xBEEF
    assert e.status == 0x1402
    assert e.num_sent == 0
    assert e.n_rx_ok == 0
    assert e.n_rx_nok == 0
    assert e.n_rx_ignored == 0
    assert e.pkt_status == 0x00


def test_debug_timing_pkt_status_bits():
    """pkt_status bits:
        bit0 = bTimeStampValid
        bit1 = bLastCrcErr
        bit2 = bLastIgnored
        bit3 = bLastEmpty
        bit4 = bLastCtrl
        bit5 = bLastMd
        bit6 = bLastAck
    """
    from feralrf._responses import DebugTimingEntry

    e = DebugTimingEntry(
        event_idx=0, start_rat=0, end_rat=0, status=0,
        num_sent=0, n_rx_ok=0, n_rx_nok=0, n_rx_ignored=0,
        pkt_status=0b0100_0001,  # bTimeStampValid + bLastAck
    )
    assert e.b_time_stamp_valid is True
    assert e.b_last_crc_err is False
    assert e.b_last_ack is True
```

- [ ] **Step 2: Run the test — confirm it fails**

```bash
cd python && source .venv/bin/activate
pytest tests/test_responses.py::test_debug_timing_parses_extended_entry -v
```

Expected: FAIL — either an `AttributeError: 'DebugTimingEntry' object has no attribute 'n_rx_ok'`, or a struct unpack error because the parser expects 13-byte entries.

- [ ] **Step 3: Extend the Python parser**

Edit `python/feralrf/_responses.py`. Locate `DebugTimingEntry` (~line 109) and `DebugTimingResponse.parse` (~line 129). Replace the entry dataclass and parser with:

```python
@dataclass(frozen=True)
class DebugTimingEntry:
    """One captured master-event timing record (17 wire bytes)."""

    event_idx: int       # u16 — BleConnMgr s_event_counter at capture time
    start_rat: int       # u32 — RAT tick fed to RadioIF_bleCentral as startTime
    end_rat: int         # u32 — RAT tick fed as endTime
    status: int          # u16 — RF status code (BLE_DONE_NOSYNC=0x1402, OK=0x1400)
    num_sent: int        # u8  — pOutput.nTxEntryDone
    n_rx_ok: int         # u8  — pOutput.nRxOk
    n_rx_nok: int        # u8  — pOutput.nRxNok
    n_rx_ignored: int    # u8  — pOutput.nRxIgnored
    pkt_status: int      # u8  — packed bitfield from pOutput.pktStatus

    @property
    def b_time_stamp_valid(self) -> bool:
        return bool(self.pkt_status & 0x01)

    @property
    def b_last_crc_err(self) -> bool:
        return bool(self.pkt_status & 0x02)

    @property
    def b_last_ignored(self) -> bool:
        return bool(self.pkt_status & 0x04)

    @property
    def b_last_empty(self) -> bool:
        return bool(self.pkt_status & 0x08)

    @property
    def b_last_ctrl(self) -> bool:
        return bool(self.pkt_status & 0x10)

    @property
    def b_last_md(self) -> bool:
        return bool(self.pkt_status & 0x20)

    @property
    def b_last_ack(self) -> bool:
        return bool(self.pkt_status & 0x40)


@dataclass(frozen=True)
class DebugTimingResponse:
    entries: list  # list[DebugTimingEntry]

    @classmethod
    def parse(cls, payload: bytes) -> "DebugTimingResponse":
        if not payload:
            raise ValueError("DEBUG_TIMING payload empty")
        count = payload[0]
        expected = 1 + count * 17
        if len(payload) != expected:
            raise ValueError(
                f"DEBUG_TIMING payload size mismatch: got {len(payload)}, "
                f"expected {expected} for count={count}"
            )
        entries = []
        for i in range(count):
            base = 1 + i * 17
            event_idx, start_rat, end_rat, status, num_sent, \
                n_rx_ok, n_rx_nok, n_rx_ignored, pkt_status = struct.unpack(
                    "<HIIHBBBBB", payload[base : base + 17]
                )
            entries.append(
                DebugTimingEntry(
                    event_idx=event_idx,
                    start_rat=start_rat,
                    end_rat=end_rat,
                    status=status,
                    num_sent=num_sent,
                    n_rx_ok=n_rx_ok,
                    n_rx_nok=n_rx_nok,
                    n_rx_ignored=n_rx_ignored,
                    pkt_status=pkt_status,
                )
            )
        return cls(entries=entries)
```

- [ ] **Step 4: Run the parser test — confirm it passes**

```bash
pytest tests/test_responses.py::test_debug_timing_parses_extended_entry tests/test_responses.py::test_debug_timing_pkt_status_bits -v
```

Expected: 2 PASS.

- [ ] **Step 5: Run the full Python test suite — catch regressions**

```bash
pytest -q
```

Expected: all tests pass. If any test that previously consumed `DebugTimingEntry` now fails, update it to provide the 4 new fields.

- [ ] **Step 6: Extend `RadioIF_bleCentral` to surface stats**

Edit `firmware/cc1352/include/radio_if.h`. Locate the `RadioIF_bleCentral` declaration (around line 81) and add this struct + extend the signature:

```c
/* Per-event RF stats lifted from rfc_bleMasterSlaveOutput_t after the
 * CMD_BLE5_MASTER mailbox finishes. Lives in radio_if so callers don't
 * need to include rf_ble_cmd.h. */
typedef struct {
    uint8_t nRxOk;       /* pOutput->nRxOk */
    uint8_t nRxNok;      /* pOutput->nRxNok */
    uint8_t nRxIgnored;  /* pOutput->nRxIgnored */
    uint8_t pktStatus;   /* packed bitfield byte from pOutput->pktStatus */
} RadioIF_BleCentralStats;

int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit,
                       dataQueue_t *pTxQueue, uint32_t startTime,
                       uint32_t endTime, uint32_t *pNumSent,
                       RadioIF_BleCentralStats *pStats);
```

- [ ] **Step 7: Populate stats in `RadioIF_bleCentral`**

Edit `firmware/cc1352/src/radio_if.c`. Locate `RadioIF_bleCentral` (line 2298). Replace the function body from line 2300 to line 2350 with:

```c
int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit, dataQueue_t *pTxQueue,
                       uint32_t startTime, uint32_t endTime, uint32_t *pNumSent,
                       RadioIF_BleCentralStats *pStats) {
    rfc_bleMasterSlaveOutput_t output = {0};

    if (s_rf_handle == NULL || chan >= 37) {
        return -2;
    }

    Ble5_0_cmdBle5Master.channel = chan;
    Ble5_0_cmdBle5Master.whitening.init = 0x40 + chan;
    Ble5_0_cmdBle5Master.whitening.bOverride = 1;
    Ble5_0_cmdBle5Master.phyMode.mainMode = 0; /* 1M */
    Ble5_0_cmdBle5Master.phyMode.coding = 0;
    Ble5_0_cmdBle5Master.pOutput = &output;

    Ble5_0_cmdBle5Master.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5Master.pParams->pTxQ = pTxQueue;
    Ble5_0_cmdBle5Master.pParams->accessAddress = accessAddr;
    Ble5_0_cmdBle5Master.pParams->crcInit0 = crcInit & 0xFF;
    Ble5_0_cmdBle5Master.pParams->crcInit1 = (crcInit >> 8) & 0xFF;
    Ble5_0_cmdBle5Master.pParams->crcInit2 = (crcInit >> 16) & 0xFF;
    Ble5_0_cmdBle5Master.pParams->maxRxPktLen = 0xFF;

    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushIgnored = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushCrcErr = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushEmpty = 0;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bIncludeLenByte = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bIncludeCrc = 0;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendRssi = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendStatus = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendTimestamp = 1;

    if (startTime == 0) {
        Ble5_0_cmdBle5Master.startTrigger.triggerType = TRIG_NOW;
    } else {
        Ble5_0_cmdBle5Master.startTrigger.triggerType = TRIG_ABSTIME;
        Ble5_0_cmdBle5Master.startTrigger.pastTrig = 1;
        Ble5_0_cmdBle5Master.startTime = startTime;
    }

    Ble5_0_cmdBle5Master.pParams->endTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Master.pParams->endTime = endTime;

    Ble5_0_cmdBle5Master.status = 0;

    RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5Master, RF_PriorityNormal, &RadioIF_rfCallback,
              RF_EventRxEntryDone);

    *pNumSent = output.nTxEntryDone;
    if (pStats != NULL) {
        pStats->nRxOk = output.nRxOk;
        pStats->nRxNok = output.nRxNok;
        pStats->nRxIgnored = output.nRxIgnored;
        /* Pack the 7 bitfield bits into one byte in declared order. The C
         * spec doesn't pin bitfield layout, so do it explicitly to match
         * the Python parser. */
        pStats->pktStatus =
            (uint8_t)((output.pktStatus.bTimeStampValid ? 0x01 : 0) |
                      (output.pktStatus.bLastCrcErr     ? 0x02 : 0) |
                      (output.pktStatus.bLastIgnored    ? 0x04 : 0) |
                      (output.pktStatus.bLastEmpty      ? 0x08 : 0) |
                      (output.pktStatus.bLastCtrl       ? 0x10 : 0) |
                      (output.pktStatus.bLastMd         ? 0x20 : 0) |
                      (output.pktStatus.bLastAck        ? 0x40 : 0));
    }

    /* Return raw status for debugging. Caller checks for success codes. */
    return (int)Ble5_0_cmdBle5Master.status;
}
```

- [ ] **Step 8: Extend the ring-buffer entry**

Edit `firmware/cc1352/include/ble_conn_mgr.h`. Replace the `BleConnMgr_DbgTimingEntry` struct (lines 29-35) with:

```c
typedef struct {
    uint16_t eventIdx;     /* s_event_counter at capture time */
    uint32_t startRAT;     /* curHopTime fed to RadioIF_bleCentral */
    uint32_t endRAT;       /* s_next_hop_time fed to RadioIF_bleCentral */
    uint16_t status;       /* RF status code returned by the command */
    uint8_t  numSent;      /* pOutput.nTxEntryDone */
    uint8_t  nRxOk;        /* pOutput.nRxOk (Session 4) */
    uint8_t  nRxNok;       /* pOutput.nRxNok (Session 4) */
    uint8_t  nRxIgnored;   /* pOutput.nRxIgnored (Session 4) */
    uint8_t  pktStatus;    /* packed pktStatus bitfield (Session 4) */
} BleConnMgr_DbgTimingEntry;
```

- [ ] **Step 9: Wire stats into the ring**

Edit `firmware/cc1352/src/ble_conn_mgr.c`. The current `BleConnMgr_poll` (around line 280-301) invokes `RadioIF_bleCentral` with 7 args; update both the call and the snapshot block:

Replace lines 280-301 with:

```c
    uint32_t startTime = curHopTime;
    uint32_t endTime = s_next_hop_time;
    uint32_t numSent = 0;
    RadioIF_BleCentralStats stats = {0};

    int status = RadioIF_bleCentral(chan, st->accessAddr, st->crcInit, &txq, startTime, endTime,
                                    &numSent, &stats);
    s_last_status = status;
    s_dbg_total_tx_done += numSent;

    /* Snapshot timing for host-side correlation (Session 3 + Session 4 telemetry). */
    {
        BleConnMgr_DbgTimingEntry *e = &s_dbg_timing[s_dbg_timing_head];
        e->eventIdx = s_event_counter;
        e->startRAT = startTime;
        e->endRAT = endTime;
        e->status = (uint16_t)status;
        e->numSent = (uint8_t)numSent;
        e->nRxOk = stats.nRxOk;
        e->nRxNok = stats.nRxNok;
        e->nRxIgnored = stats.nRxIgnored;
        e->pktStatus = stats.pktStatus;
        s_dbg_timing_head = (uint8_t)((s_dbg_timing_head + 1u) % BLE_CONN_MGR_DBG_TIMING_DEPTH);
        if (s_dbg_timing_count < BLE_CONN_MGR_DBG_TIMING_DEPTH) {
            s_dbg_timing_count++;
        }
    }
```

- [ ] **Step 10: Update wire layout in `command_processor.c`**

Edit `firmware/cc1352/src/command_processor.c`. Replace the `case CMD_DEBUG_TIMING:` block (lines 576-605) with:

```c
    case CMD_DEBUG_TIMING: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Wire layout: count(u8) + count × 17 bytes per entry, where the
         * 17 bytes are: eventIdx(u16) startRAT(u32) endRAT(u32) status(u16)
         * numSent(u8) nRxOk(u8) nRxNok(u8) nRxIgnored(u8) pktStatus(u8).
         * 1 + 16*17 = 273 bytes max. */
        uint8_t rsp[1u + BLE_CONN_MGR_DBG_TIMING_DEPTH * 17u];
        BleConnMgr_DbgTimingEntry entries[BLE_CONN_MGR_DBG_TIMING_DEPTH];
        uint8_t n = BleConnMgr_getDebugTiming(entries, BLE_CONN_MGR_DBG_TIMING_DEPTH);
        rsp[0] = n;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t *p = &rsp[1u + (uint16_t)i * 17u];
            p[0]  = (uint8_t)(entries[i].eventIdx & 0xFFu);
            p[1]  = (uint8_t)(entries[i].eventIdx >> 8);
            p[2]  = (uint8_t)(entries[i].startRAT & 0xFFu);
            p[3]  = (uint8_t)((entries[i].startRAT >> 8) & 0xFFu);
            p[4]  = (uint8_t)((entries[i].startRAT >> 16) & 0xFFu);
            p[5]  = (uint8_t)((entries[i].startRAT >> 24) & 0xFFu);
            p[6]  = (uint8_t)(entries[i].endRAT & 0xFFu);
            p[7]  = (uint8_t)((entries[i].endRAT >> 8) & 0xFFu);
            p[8]  = (uint8_t)((entries[i].endRAT >> 16) & 0xFFu);
            p[9]  = (uint8_t)((entries[i].endRAT >> 24) & 0xFFu);
            p[10] = (uint8_t)(entries[i].status & 0xFFu);
            p[11] = (uint8_t)((entries[i].status >> 8) & 0xFFu);
            p[12] = entries[i].numSent;
            p[13] = entries[i].nRxOk;
            p[14] = entries[i].nRxNok;
            p[15] = entries[i].nRxIgnored;
            p[16] = entries[i].pktStatus;
        }
        send_response(RSP_DEBUG_TIMING, seq, rsp, (uint16_t)(1u + (uint16_t)n * 17u));
        return;
    }
```

- [ ] **Step 11: Build firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352
mkdir -p build && cd build
cmake .. > /dev/null 2>&1
make -j$(nproc) 2>&1 | tail -20
```

Expected: clean build. If errors mention `output.pktStatus.bTimeStampValid`: confirm SDK 8.30 path is on `TI_SDK_PATH` (CMake should handle this) and the include order pulls `rf_ble_cmd.h`.

- [ ] **Step 12: Flash board #1 (retry up to 2×)**

```bash
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py \
    flash --port <ACM_BOARD1> \
    --hex /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

If first flash fails: re-run once. If second attempt fails: ask the user to power-cycle the board.

- [ ] **Step 13: Run a connection capture and dump the new fields**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python3 examples/lab/f8a_session3_capture.py \
    --port <ACM_BOARD1> \
    --target DC:32:62:8D:E1:09 \
    --addr-type 0 \
    --linger 3 \
    --out /tmp/f8a-s4-pktstatus.json
```

Then inspect the new fields:

```bash
python3 -c "
import json
data = json.load(open('/tmp/f8a-s4-pktstatus.json'))
for e in data['debug_timing']:
    print(f\"ev={e['event_idx']:3d} status=0x{e['status']:04x} \"
          f\"sent={e['num_sent']} rxOk={e['n_rx_ok']} \"
          f\"rxNok={e['n_rx_nok']} rxIgn={e['n_rx_ignored']} \"
          f\"pktSt=0x{e['pkt_status']:02x}\")
"
```

(`f8a_session3_capture.py` dumps `dataclasses.asdict(timing.entries[i])` so the new fields flow through automatically.)

- [ ] **Step 14: Write the verdict**

Create `docs/investigations/2026-04-24-f8a-session-1/session-4-pktstatus-evidence.md`:

```markdown
# F8A Session 4 — Task 2: per-event pktStatus evidence

**Date:** YYYY-MM-DD
**Capture:** `/tmp/f8a-s4-pktstatus.json`

## Per-event dump

| Event | Status | nTxEntryDone | nRxOk | nRxNok | nRxIgnored | pktStatus |
|-------|--------|---------------|-------|--------|-----------|-----------|
| 0 | 0x???? | ? | ? | ? | ? | 0x?? |
| ... | | | | | | |

## Interpretation

- **All `nRxOk == 0` AND `bTimeStampValid == 0` (pktStatus bit 0 = 0):**
  master never reaches RX. Bug is in the master state machine entry — it
  exits before TX/RX. Strongly suggests config / chain failure inside
  `CMD_BLE5_MASTER` itself. Move directly to Task 3 (Sniffle CENTRAL A/B).

- **`bTimeStampValid == 1` AND `nRxOk == 0`:**
  master TX'd, RX window opened, but no slave PDU decoded. Either CRC
  mismatch (`nRxNok > 0`) or the peer never replied. Move to Task 3 to
  determine whether the same hardware works when the controller is
  Sniffle's TI-RTOS code instead of ours.

- **`nRxOk > 0`:**
  the connection is alive on the wire — the bug is in our drain logic.
  Audit `RadioIF_bleDrainRxQueue` and the queue reset.

## Verdict
<one paragraph>
```

- [ ] **Step 15: Commit Task 2**

```bash
git add firmware/cc1352/include/radio_if.h \
        firmware/cc1352/src/radio_if.c \
        firmware/cc1352/include/ble_conn_mgr.h \
        firmware/cc1352/src/ble_conn_mgr.c \
        firmware/cc1352/src/command_processor.c \
        python/feralrf/_responses.py \
        python/tests/test_responses.py \
        docs/investigations/2026-04-24-f8a-session-1/session-4-pktstatus-evidence.md
git commit -m "$(cat <<'EOF'
feat(f8a): expose per-event pOutput stats in RSP_DEBUG_TIMING

Extends the master-event ring entry with nRxOk, nRxNok, nRxIgnored, and
the pktStatus bitfield byte (bTimeStampValid, bLastCrcErr, bLastIgnored,
bLastEmpty, bLastCtrl, bLastMd, bLastAck). Wire layout grows from 13 to
17 bytes per entry; 1 + 16*17 = 273 bytes max frame.

Lets Session 4 distinguish "master never TX'd" from "master TX'd but
peer never replied" — the cheapest remaining diagnostic for the
nTxEntryDone=0 mystery.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Sniffle CENTRAL A/B — does the same hardware connect when controlled by Sniffle's firmware?

**Why:** If FeralRF can't connect to CH573 but Sniffle CENTRAL **can** on the same board with the same antenna and the same peer, the bug is 100 % in our SDK config / state-machine setup. If Sniffle CENTRAL **also** can't connect, the bug is at a layer below us (CH573 quirk, RF calibration, antenna) and our master is fine — F8A's exit criterion needs to change.

**Hypothesis under test:** "Our hardware can connect to CH573 — only our firmware can't."

**Files:**
- Create: `docs/investigations/2026-04-24-f8a-session-1/session-4-sniffle-central-ab.md`
- Capture artifacts (kept on disk, referenced by absolute path): `/tmp/f8a-s4-sniffle-initiator-{stdout,stderr}.log`.

**Hardware procedure — destructive to board #1 firmware:** this task re-flashes board #1 with stock Sniffle, runs the experiment, then re-flashes board #1 back to FeralRF. Save the FeralRF hex BEFORE re-flashing.

- [ ] **Step 1: Snapshot the FeralRF hex**

```bash
cp /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex \
   /tmp/f8a-s4-feralrf-snapshot.hex
md5sum /tmp/f8a-s4-feralrf-snapshot.hex
```

Record the md5 in the verdict doc so the restore step (Step 6) can verify.

- [ ] **Step 2: Flash board #1 with Sniffle stock**

```bash
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py \
    flash --port <ACM_BOARD1> \
    --hex /home/sabas/Documents/electroniccats/CatSniffer-Tools/catsnifferv2/release_board-v3.x-v2.0.0/sniffle_cc1352p7_1M.hex
```

Retry up to 2× before asking the user.

- [ ] **Step 3: Run Sniffle initiator against CH573**

The user **must power-cycle CH573** so it's freshly advertising before Step 3 runs.

```bash
cd ~/Documents/electroniccats/Sniffle/python_cli
python3 initiator.py -s <ACM_BOARD1> DC:32:62:8D:E1:09 -p 2>&1 | tee /tmp/f8a-s4-sniffle-initiator-stdout.log
```

(`-p` = public address, matching `--addr-type 0` in our CLI.)

Let it run for 10 seconds, then Ctrl-C. Inspect the log for:

- `Connected to ...` line + repeated `data ch=` lines → **Sniffle CENTRAL connects**.
- `Connection setup timeout` or no `Connected` line → **Sniffle CENTRAL also fails**.

- [ ] **Step 4: If Sniffle CENTRAL connected — capture its master TX with board #2**

Only run this step if Step 3 showed a successful connection. Otherwise skip to Step 5.

In a second terminal, before re-running Sniffle initiator:

```bash
cd ~/Documents/electroniccats/Sniffle/python_cli
python3 sniff_receiver.py -s <ACM_BOARD2> -H -m DC:32:62:8D:E1:09 \
    -o /tmp/f8a-s4-sniffle-central-followed.pcap &
SNIFFER_PID=$!
sleep 1

# Re-run initiator (CH573 should still be in its supervisionTimeout window
# from Step 3; if not, ask user to power-cycle)
python3 initiator.py -s <ACM_BOARD1> DC:32:62:8D:E1:09 -p &
INIT_PID=$!
sleep 8
kill $INIT_PID $SNIFFER_PID 2>/dev/null
wait
```

The pcap at `/tmp/f8a-s4-sniffle-central-followed.pcap` is the gold reference for what a working master should look like on the air. Save its size + first 5 PDU summaries to the verdict doc.

- [ ] **Step 5: Restore FeralRF on board #1**

```bash
md5sum /tmp/f8a-s4-feralrf-snapshot.hex  # confirm matches Step 1 md5
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py \
    flash --port <ACM_BOARD1> \
    --hex /tmp/f8a-s4-feralrf-snapshot.hex
```

- [ ] **Step 6: Smoke-test FeralRF post-restore**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python3 -c "
from feralrf import Radio
from feralrf.enums import PHY
import time
r = Radio('<ACM_BOARD1>'); r.connect(); r.init()
r.set_phy(PHY.BLE_1M, 37, 2402_000_000); r.start_rx()
time.sleep(2); pkts = list(r.read_packets()); r.stop_rx()
print(f'Post-restore BLE scan: {len(pkts)} pkts')
"
```

Expected: ≥1 packet. If 0 → re-flash failed silently, retry from Step 5.

- [ ] **Step 7: Write the verdict**

Create `docs/investigations/2026-04-24-f8a-session-1/session-4-sniffle-central-ab.md`:

```markdown
# F8A Session 4 — Task 3: Sniffle CENTRAL A/B

**Date:** YYYY-MM-DD
**Hardware:** board #1 (CatSniffer 504B32) re-flashed with Sniffle stock,
then restored to FeralRF (md5 `<MD5>`).
**Peer:** CH573 `DC:32:62:8D:E1:09`.

## Result

Sniffle initiator output (`/tmp/f8a-s4-sniffle-initiator-stdout.log`):
- Connection established: yes / no
- Time to first data ch packet: <ms> / N/A
- Connection sustained: <duration> / N/A

If yes: gold-standard master pcap at
`/tmp/f8a-s4-sniffle-central-followed.pcap` (<bytes> bytes,
<n_pkts> PDUs).

## Interpretation

- **Sniffle CENTRAL connects, FeralRF doesn't:** bug is in our SDK
  config / state-machine setup. The Session 5 plan is to diff
  `Ble5_0_cmdBle5Master` and `Ble5_0_cmdBle5Initiator` field-by-field
  against Sniffle's `RF_cmdBleAdv` / `RF_cmdBleSlave` configs in
  `~/Documents/electroniccats/Sniffle/fw/RFQueue.c` and equivalents.
  Reference our gold pcap to know what wire packets we should match.
- **Sniffle CENTRAL also fails:** F8A's exit criterion against CH573 is
  unrealistic. Move to a different peer (e.g. ESP32, smartphone in
  peripheral mode, or Sniffle peripheral on a third board). Update the
  spec §5 F8A criterion accordingly.
- **Both work intermittently:** flag CH573 reliability issue, document,
  pick a different peer.

## Verdict
<one paragraph>
```

- [ ] **Step 8: Commit Task 3**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/session-4-sniffle-central-ab.md
git commit -m "$(cat <<'EOF'
investigate(f8a): Task 3 — Sniffle CENTRAL A/B against CH573

Determines whether the same board #1 + CH573 + antenna can sustain a
BLE central connection when the controller is Sniffle stock instead of
FeralRF. Frames whether the remaining work is "fix our config" or
"change the exit-criterion peer".

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Slow `connInterval` and `supervTimeout` — does a wider listening window land at least one event?

**Why:** Today we use 30 ms / 1 s. If the timing is off by a constant factor (master TX inside CH573's listening window in some events but not others), a slower 100 ms / 5 s connection interval might land enough events to see at least one `nRxOk > 0`. If even at 100 ms every event NOSYNCs, the bug is unconditional and timing-independent.

**Hypothesis under test:** "Master TX is happening but we're consistently outside the peer's window — at 100 ms / 5 s the wider windows make landing 1+ event statistically likely."

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c` (extend `CMD_CONNECT` payload to optionally carry `connInterval` + `supervTimeout`)
- Modify: `python/feralrf/commands.py` (extend `CommandBuilder.ble_connect`)
- Modify: `python/feralrf/radio.py` (extend `Radio.ble_connect`)
- Test: `python/tests/test_commands.py` (verify backward-compat: 7-byte payload still valid; 11-byte payload sets new fields)
- Create: `docs/investigations/2026-04-24-f8a-session-1/session-4-slow-conn.md`

**Wire layout for `CMD_CONNECT`:**
- Existing: 7 bytes = `addr[6] + addr_type[1]` → defaults to interval 24 (30 ms), timeout 100 (1 s).
- Extended: 11 bytes = `addr[6] + addr_type[1] + interval_units[2 LE] + timeout_units[2 LE]`. Interval in 1.25 ms units (24 = 30 ms, 80 = 100 ms). Timeout in 10 ms units (100 = 1 s, 500 = 5 s).
- Lengths other than 7 and 11 must yield `ERR_INVALID_PAYLOAD`.

- [ ] **Step 1: Write the failing test**

Add to `python/tests/test_commands.py`:

```python
def test_ble_connect_default_payload_is_7_bytes():
    from feralrf.commands import CommandBuilder
    p = CommandBuilder.ble_connect(b"\x01\x02\x03\x04\x05\x06", addr_type=0)
    assert p == b"\x01\x02\x03\x04\x05\x06\x00"


def test_ble_connect_with_interval_timeout_is_11_bytes():
    from feralrf.commands import CommandBuilder
    p = CommandBuilder.ble_connect(
        b"\x01\x02\x03\x04\x05\x06",
        addr_type=1,
        interval=80,            # 100 ms
        supervision_timeout=500,  # 5 s
    )
    assert p == bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01,
                       0x50, 0x00, 0xF4, 0x01])
    assert len(p) == 11
```

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && source .venv/bin/activate
pytest tests/test_commands.py::test_ble_connect_default_payload_is_7_bytes \
       tests/test_commands.py::test_ble_connect_with_interval_timeout_is_11_bytes -v
```

Expected: `test_default_payload_is_7_bytes` PASSES (current behaviour). `test_with_interval_timeout_is_11_bytes` FAILS (`TypeError: ble_connect() got an unexpected keyword argument 'interval'`).

- [ ] **Step 2: Extend `CommandBuilder.ble_connect`**

Edit `python/feralrf/commands.py:155`. Replace `ble_connect` with:

```python
    @staticmethod
    def ble_connect(
        addr_le: bytes,
        addr_type: int,
        interval: int | None = None,
        supervision_timeout: int | None = None,
    ) -> bytes:
        """Payload for CMD_CONNECT.

        Default 7 bytes: addr[6] + addr_type[1]. Firmware uses interval=24
        (30 ms), supervTimeout=100 (1 s).

        Extended 11 bytes: addr[6] + addr_type[1] + interval[2 LE] +
        supervision_timeout[2 LE]. Pass both or neither.

        Args:
            addr_le: Peer address in little-endian wire order.
            addr_type: 0 for public, 1 for random.
            interval: connInterval in 1.25 ms units (e.g. 80 = 100 ms).
            supervision_timeout: in 10 ms units (e.g. 500 = 5 s).
        """
        if len(addr_le) != 6:
            raise ValueError("addr_le must be exactly 6 bytes")
        head = bytes(addr_le) + bytes([addr_type & 0xFF])
        if interval is None and supervision_timeout is None:
            return head
        if interval is None or supervision_timeout is None:
            raise ValueError("interval and supervision_timeout must both be set or both None")
        if not 0 < interval < 0x10000:
            raise ValueError(f"interval out of range: {interval}")
        if not 0 < supervision_timeout < 0x10000:
            raise ValueError(f"supervision_timeout out of range: {supervision_timeout}")
        return head + interval.to_bytes(2, "little") + supervision_timeout.to_bytes(2, "little")
```

- [ ] **Step 3: Pass-through in `Radio.ble_connect`**

Edit `python/feralrf/radio.py:538`. Locate `ble_connect` and replace its signature + the `CommandBuilder.ble_connect(...)` call:

```python
    def ble_connect(
        self,
        addr_le: bytes,
        addr_type: int,
        timeout: float = 8.0,
        interval: int | None = None,
        supervision_timeout: int | None = None,
    ) -> "ConnectionResult":
        """Open a BLE central connection.

        Args:
            addr_le: 6-byte little-endian peer address.
            addr_type: 0 for public, 1 for random.
            timeout: seconds to wait for RSP_CONN_RESULT.
            interval: optional connInterval override in 1.25 ms units.
            supervision_timeout: optional supervTimeout override in 10 ms units.
                Both `interval` and `supervision_timeout` must be set together.
        """
        self._send_command(
            Command.BLE_CONNECT,
            CommandBuilder.ble_connect(
                addr_le, addr_type,
                interval=interval,
                supervision_timeout=supervision_timeout,
            ),
        )
        # ... existing _read_response + ConnectionResult parse code unchanged ...
```

(Keep the rest of the method body — `_read_response`, `ConnectionResult.parse` — unchanged.)

- [ ] **Step 4: Run the Python tests**

```bash
cd python && source .venv/bin/activate
pytest tests/test_commands.py -v
```

Expected: both new tests PASS, all previously-green tests still PASS.

- [ ] **Step 5: Extend the firmware `CMD_CONNECT` handler**

Edit `firmware/cc1352/src/command_processor.c`. Locate `case CMD_CONNECT:` (line 446). Replace the block (until line ~458) with:

```c
    case CMD_CONNECT: {
        /* Payload: 7 bytes default = addr[6] + addr_type(1).
         * Payload: 11 bytes extended = addr[6] + addr_type(1) +
         *                              interval[2 LE] + supervTimeout[2 LE]. */
        uint16_t intervalUnits = 24u;   /* 30 ms */
        uint16_t timeoutUnits = 100u;   /* 1 s */
        if (payload_len == 11u) {
            intervalUnits = (uint16_t)(payload[7] | ((uint16_t)payload[8] << 8));
            timeoutUnits  = (uint16_t)(payload[9] | ((uint16_t)payload[10] << 8));
            if (intervalUnits == 0u || timeoutUnits == 0u) {
                send_error(seq, ERR_INVALID_PAYLOAD);
                return;
            }
        } else if (payload_len != 7u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        BleConn_Result res = BleConn_initiate(payload, payload[6], intervalUnits, timeoutUnits);
        /* ... rest of the existing handler unchanged ... */
```

(Preserve every line below `BleConn_initiate(...)` — the `res` handling, ack/error response, etc.)

- [ ] **Step 6: Build firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build.

- [ ] **Step 7: Flash board #1**

```bash
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py \
    flash --port <ACM_BOARD1> \
    --hex /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

- [ ] **Step 8: Capture with default 30 ms / 1 s (regression check)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python3 examples/lab/f8a_session3_capture.py \
    --port <ACM_BOARD1> \
    --target DC:32:62:8D:E1:09 \
    --addr-type 0 \
    --linger 3 \
    --out /tmp/f8a-s4-slow-30ms.json
```

Expected: same NOSYNC pattern as Session 3 (regression check — extending the payload should not change default behaviour).

- [ ] **Step 9: Capture with 100 ms / 5 s**

Write a one-shot script `/tmp/run_slow.py`:

```python
import sys, time, json, dataclasses
sys.path.insert(0, "/home/sabas/Documents/electroniccats/FeralRF/python")
from feralrf import Radio

r = Radio(sys.argv[1])
r.connect(); r.init()
addr = bytes(reversed(bytes.fromhex("DC32628DE109")))
t0 = time.time_ns()
res = r.ble_connect(addr, addr_type=0, interval=80, supervision_timeout=500)
time.sleep(6.0)  # supervision timeout = 5 s, give it room
status = r.conn_status()
timing = r.debug_timing()
t1 = time.time_ns()
try: r.ble_disconnect()
except Exception: pass
r.disconnect()

out = {
    "conn_result": int(res.result),
    "conn_status": dataclasses.asdict(status),
    "debug_timing": [dataclasses.asdict(e) for e in timing.entries],
    "wallclock_capture_start_unix_ns": t0,
    "wallclock_capture_end_unix_ns": t1,
}
with open(sys.argv[2], "w") as f: json.dump(out, f, indent=2, default=str)
print(f"events={status.events} last_status=0x{status.last_status:04x}")
```

Run:
```bash
python3 /tmp/run_slow.py <ACM_BOARD1> /tmp/f8a-s4-slow-100ms.json
```

Expected at minimum: `events>0`. The interesting datum is whether **any** entry shows `nRxOk > 0` or `bTimeStampValid == 1` with this slower cadence.

- [ ] **Step 10: Compare per-event records**

```bash
python3 -c "
import json
for label, path in [('30ms', '/tmp/f8a-s4-slow-30ms.json'),
                    ('100ms', '/tmp/f8a-s4-slow-100ms.json')]:
    data = json.load(open(path))
    entries = data['debug_timing']
    rx_ok_total = sum(e.get('n_rx_ok', 0) for e in entries)
    tsv_count = sum(1 for e in entries if e.get('pkt_status', 0) & 0x01)
    print(f'{label}: events={len(entries)} rx_ok_total={rx_ok_total} '
          f'bTimeStampValid_count={tsv_count}')
"
```

- [ ] **Step 11: Write the verdict**

Create `docs/investigations/2026-04-24-f8a-session-1/session-4-slow-conn.md`:

```markdown
# F8A Session 4 — Task 4: slow connInterval / supervTimeout

**Date:** YYYY-MM-DD
**Captures:**
- 30 ms / 1 s: `/tmp/f8a-s4-slow-30ms.json`
- 100 ms / 5 s: `/tmp/f8a-s4-slow-100ms.json`

## Comparison

| Param | 30 ms / 1 s | 100 ms / 5 s |
|-------|-------------|---------------|
| Events captured | <N> | <N> |
| Total nRxOk | <N> | <N> |
| Events with bTimeStampValid | <N> | <N> |
| First/last status | 0x???? / 0x???? | 0x???? / 0x???? |

## Interpretation

- **100 ms shows ANY rxOk > 0:** timing window was the issue, fix the
  anchor formula. (Unlikely given Session 3 anchor work, but possible.)
- **100 ms shows bTimeStampValid > 0 even with rxOk == 0:** master TX is
  reaching air, peer just isn't responding (CH573 listening-window or
  software issue on peer side).
- **100 ms identical to 30 ms (every event NOSYNC, all stats zero):**
  the bug is timing-independent. Only Task 3's Sniffle CENTRAL A/B
  result can move the needle further.

## Verdict
<one paragraph>
```

- [ ] **Step 12: Commit Task 4**

```bash
git add firmware/cc1352/src/command_processor.c \
        python/feralrf/commands.py \
        python/feralrf/radio.py \
        python/tests/test_commands.py \
        docs/investigations/2026-04-24-f8a-session-1/session-4-slow-conn.md
git commit -m "$(cat <<'EOF'
feat(f8a): optional connInterval/supervTimeout in CMD_CONNECT

CMD_CONNECT payload was strictly 7 bytes (addr+addr_type). Adds an
11-byte form that carries interval (1.25 ms units) + supervTimeout
(10 ms units). 7-byte form preserves the previous defaults of 24
(30 ms) and 100 (1 s).

Lets Session 4 widen the connection cadence to 100 ms / 5 s without
firmware re-flash to test whether NOSYNC is timing-dependent.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Close-out — synthesize the four experiments and decide F8A's fate

**Files:**
- Create: `docs/investigations/2026-04-24-f8a-session-1/session-4-closeout.md`
- Create: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8a_session4.md`
- Modify: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` (replace `project_f8a_session3.md` reference)

- [ ] **Step 1: Re-read every Task verdict + the original closeouts**

```bash
cat docs/investigations/2026-04-24-f8a-session-1/session-4-preflight.md \
    docs/investigations/2026-04-24-f8a-session-1/session-4-pinned-channel.md \
    docs/investigations/2026-04-24-f8a-session-1/session-4-pktstatus-evidence.md \
    docs/investigations/2026-04-24-f8a-session-1/session-4-sniffle-central-ab.md \
    docs/investigations/2026-04-24-f8a-session-1/session-4-slow-conn.md
```

- [ ] **Step 2: Decide the outcome**

Three branches:

1. **F8A closes.** Some task (most likely Task 2 or Task 3) revealed an actionable bug, you fixed it as part of executing that task, and `demo_ble_connect_gatt.py` (or equivalent) now connects, discovers, reads, and disconnects cleanly against CH573. → tag `v2.0-f8a`.

2. **F8A pivots peer.** Task 3 showed Sniffle CENTRAL also fails against CH573. CH573 is the wrong exit-criterion peer. Update `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §5 F8A criterion to use whatever peer Task 3 verified works, and re-run F8A's existing telemetry against that peer. → may close in this session if the new peer also reveals the bug, otherwise leaves F8A open with new exit criterion documented.

3. **F8A still open, root cause hypothesised.** No task fixed the bug, but the four pieces of evidence converge on a single hypothesis (e.g. "RF core never enters TX state — likely cause: missing FS calibration command between CONNECT_IND TX and first master poll"). Document hypothesis + Session 5 first-experiment in the closeout. → branch stays open, no tag.

- [ ] **Step 3: Write the closeout**

Create `docs/investigations/2026-04-24-f8a-session-1/session-4-closeout.md`:

```markdown
# F8A Session 4 — close-out report

**Date:** YYYY-MM-DD
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 3:** `b238930..HEAD`
**Outcome:** [✅ F8A closed] / [⚠️ F8A pivoted to peer X] / [❌ F8A still open]

## Tasks executed

| Task | Subject | Verdict |
|------|---------|---------|
| 0 | Preflight | OK / Failed |
| 1 | Sniffle pinned-channel | <one line> |
| 2 | Per-event pktStatus | <one line> |
| 3 | Sniffle CENTRAL A/B | <one line> |
| 4 | Slow connInterval / supervTimeout | <one line> |

## Evidence summary

<three to five paragraphs, citing each verdict file and the key numeric
observation from each. No padding, no recap of the plan.>

## What the four experiments together imply

<one or two paragraphs. Be falsifiable: "Hypothesis H7 is now most
consistent with the data because <X>; H7 predicts <experiment Y> would
show <Z>; that experiment is Session 5's first task".>

## What lands in this branch

| Commit | Subject |
|--------|---------|
| <SHA> | docs(f8a): land Session 4 preflight |
| <SHA> | investigate(f8a): Task 1 — Sniffle pinned-channel evidence |
| <SHA> | feat(f8a): expose per-event pOutput stats in RSP_DEBUG_TIMING |
| <SHA> | investigate(f8a): Task 3 — Sniffle CENTRAL A/B against CH573 |
| <SHA> | feat(f8a): optional connInterval/supervTimeout in CMD_CONNECT |
| <this> | docs(f8a): Session 4 close-out |

## Tag decision

[Tag `v2.0-f8a` because GATT round-trip succeeded — see <evidence>] /
[Tag deferred to Session N — F8A's exit criterion is not met because <reason>]
```

- [ ] **Step 4: Update memory**

Create `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8a_session4.md`:

```markdown
---
name: F8A Session 4 status
description: Session 4 ran 4 diagnostic experiments (pinned-channel Sniffle, per-event pktStatus telemetry, Sniffle CENTRAL A/B, slow connInterval). <ONE-LINE OUTCOME>. F8A <CLOSED / OPEN>.
type: project
---
## F8A Session 4 — what landed and what it proved

**Date:** YYYY-MM-DD
**Branch:** feature/f8a-ble-central-sniffle
**HEAD:** <SHA>

**What's permanent infrastructure:**
- `RSP_DEBUG_TIMING` ring entries grew from 13 to 17 bytes — now carry
  per-event nRxOk, nRxNok, nRxIgnored, packed pktStatus byte. Use
  `r.debug_timing()` and inspect `entry.b_time_stamp_valid`,
  `entry.n_rx_ok` for diagnosis.
- `CMD_CONNECT` payload supports an 11-byte extended form with
  interval + supervTimeout overrides. 7-byte form keeps Session 1-3
  defaults (30 ms / 1 s).

**What this session ruled out / confirmed:**
<one paragraph linking to docs/investigations/2026-04-24-f8a-session-1/
session-4-closeout.md>

**For Session 5:**
<one or two next-step bullets>
```

- [ ] **Step 5: Update memory index**

Edit `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md`. Locate the line `- [project_f8a_session3.md](project_f8a_session3.md) — F8A Session 3 ...` and add immediately after it (don't delete the Session 3 entry — both are part of the audit trail):

```markdown
- [project_f8a_session4.md](project_f8a_session4.md) — Session 4: 4 diagnostic experiments, F8A <CLOSED / OPEN>, telemetry now exposes per-event pktStatus
```

- [ ] **Step 6: Commit closeout**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/session-4-closeout.md
git commit -m "$(cat <<'EOF'
docs(f8a): Session 4 close-out — <one-line outcome>

<3-line summary of what the 4 experiments together imply.>

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

(Memory files live outside the repo and don't get committed.)

- [ ] **Step 7: Tag (only if F8A closed)**

Skip if outcome is "F8A pivoted" or "F8A still open".

```bash
git tag -a v2.0-f8a -m "F8A — BLE central rewrite Sniffle-style closes against CH573"
```

Do not push the tag — that requires explicit user approval.

- [ ] **Step 8: Final report to user**

Print to the user (text response, not a commit):

- Outcome (closed / pivoted / open).
- Commits landed this session (use `git log --oneline b238930..HEAD`).
- Open questions for the user (e.g. "should we push the tag?", "which peer should F8A use going forward?", "do you want me to rip out the random AA bypass we used in Task 1 if it never got reverted?").
- Next-experiment recommendation (1 short paragraph).

---

## Self-Review Notes

**Spec coverage:** All four next-steps from session-3-closeout § Remaining mystery are covered (Task 1=passive Sniffle, Task 2=pktStatus telemetry, Task 3=Sniffle CENTRAL A/B, Task 4=slow connInterval). Pre-flight (Task 0) and closeout (Task 5) bracket them. F8A exit criterion (`demo_ble_connect_gatt` succeeds) handled in Task 5 Step 2 branch 1.

**Placeholder scan:** Every code step contains the actual edit. The investigations docs use `<placeholder>` tokens deliberately — those are values the executor records during the experiment; they are not "TBD"s for the implementation.

**Type consistency:** `RadioIF_BleCentralStats` defined in Task 2 Step 6 with 4 `uint8_t` fields — used identically in Task 2 Step 7 and Step 9. `BleConnMgr_DbgTimingEntry` 17-byte layout from Task 2 Step 8 matches the Python parser fields in Task 2 Step 3 and the wire packing in Task 2 Step 10. `CommandBuilder.ble_connect` extended kwargs in Task 4 Step 2 match the firmware payload parsing in Task 4 Step 5.

**Hardware loops:** Task 1 may need a firmware AA-fix detour (Task 1 Step 7) — flagged inline as a precondition to get a reproducible pinned-channel experiment. Task 3 destructively re-flashes board #1; restore is enforced (Task 3 Step 5-6). Task 5 Step 7 only tags if F8A actually closed — no premature tagging.
