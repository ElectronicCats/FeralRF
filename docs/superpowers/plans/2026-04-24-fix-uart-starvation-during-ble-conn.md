# Fix UART starvation during BLE connection — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate UART starvation during active BLE connections by moving `BleConnMgr_poll()` from `UartTask` to `RfTask`. Result: `conn_status`, `gatt_discover`, `gatt_read`, `gatt_write` respond while the connection is live.

**Architecture:** Align with Sniffle's task separation — `RadioTask` owns all RF timing-sensitive operations (including BLE central connection events), while the UART input path stays responsive. In FeralRF the split is `RfTask` (RF) vs `UartTask` (host protocol). Today `BleConnMgr_poll()` lives in `UartTask`, which `Task_sleep()`s up to one conn interval per event — blocking `HostIFTask_poll()`. Fix: move the call to `RfTask` and add only the minimal cross-task synchronization needed for the shared state (`TXQueue`, `AttClient` pending request, `BleConn_State`).

**Tech Stack:** TI-RTOS 7 (SysBIOS), CC1352P7 RF Driver (SDK 8.30), C11.

**Reference:** Sniffle `fw/RadioTask.c` (central mode runs in RadioTask, not in a host-IO task) and `fw/main.c` (three tasks: RadioTask, PacketTask, CommandTask — all prio 3).

**Depends on:** `feature/f8-gatt-validation` HEAD (`0a77484`). No firmware changes have shipped in F8 (Python-only), so this branch starts from the firmware state of `feature/ti-rtos-migration` HEAD (`41b81fe`).

**Blocks:** F8 T12 checkpoint humano (GATT validation against CH573 at `DC:32:62:8D:E1:09` and any other peripheral). Without this fix, every host command issued while the CC1352 is connected times out.

---

## Why this blocks F8 (evidence)

Reproduced on `feature/f8-gatt-validation` @ `0a77484` with board CC1352P7 `IEEE=00:12:4B:00:2A:79:BF:F1`, target CH573 at `DC:32:62:8D:E1:09`:

```
Scan: 8 advertisers OK (RSSI -46…-88)
ble_connect(CH573, public) → Connection result: 0 (OK)
conn_status()    → TimeoutError: Response timeout   ← starved
gatt_discover()  → TimeoutError: Response timeout   ← starved
ble_disconnect() → OK                               ← works AFTER supervision drops
```

Source evidence — `firmware/cc1352/src/main_rtos.c:112-143` (UartTask):
```c
while (1) {
    HostIFTask_poll();
    if (BleConnMgr_isRunning()) {
        BleConnMgr_poll();   // Task_sleep(up to conn interval)
    }
    Task_yield();
}
```

And `firmware/cc1352/src/ble_conn_mgr.c:225-230`:
```c
uint32_t wait = curHopTime - now;
if (wait < 0x80000000u && wait > 2000u) {
    Task_sleep(wait / 40u);  // <-- blocks UartTask up to ~1 conn interval
}
```

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `firmware/cc1352/src/main_rtos.c` | Modify (2 edits) | Remove BleConnMgr_poll call from UartTask; add it to RfTask loop |
| `firmware/cc1352/src/tx_queue.c` | Modify (if needed, Task 4) | Bracket `TXQueue_insert/take/flush` with `HwiP_disable/restore` if concurrent access breaks things |
| `firmware/cc1352/src/att_client.c` | Modify (if needed, Task 4) | Guard mutable request state (`s_pending`, `s_state`) similarly if concurrent access breaks things |
| (no new files) | — | — |

Tasks 4's modifications are **contingent** on the test outcome in Task 3. If Task 3 passes clean, skip Task 4.

---

### Task 1: Create branch and capture baseline failure

**Files:** none modified.

- [ ] **Step 1: Confirm starting branch + head**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git status
git log --oneline -1
```

Expected: on `feature/f8-gatt-validation`, HEAD at `0a77484`, working tree clean.

- [ ] **Step 2: Create fix branch**

Run:
```bash
git checkout -b fix/uart-starvation-during-conn
```

Expected: new branch created, HEAD still at `0a77484`.

- [ ] **Step 3: Build current firmware as baseline reference**

Run:
```bash
cd firmware/cc1352 && rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc) 2>&1 | tail -5
ls -lh feralrf_cc1352.hex
```

Expected: build OK, `feralrf_cc1352.hex` produced (~250 KB), text ≈ 87 KB.

- [ ] **Step 4: Flash baseline firmware on board**

Run:
```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -5
```

Expected: `Verified match` + `Device restart complete`. Retry once if timeout.

- [ ] **Step 5: Reproduce the timeout to confirm the failure before the fix**

Run (CH573 must be advertising on `DC:32:62:8D:E1:09`, type=public):
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python -c "
import time, sys
from feralrf import Radio
from feralrf.enums import PHY

addr_le = bytes.fromhex('DC32628DE109')[::-1]
r = Radio(port='/dev/ttyACM0')
r.init()
r.set_phy(PHY.BLE_1M, channel=37)
res = r.ble_connect(addr_le, addr_type=0, timeout=8.0)
print(f'connect: result={res.result}')
if not res.is_ok: sys.exit(1)
try:
    s = r.conn_status(timeout=2.5)
    print(f'conn_status OK: connected={s.connected} events={s.events}')
except Exception as e:
    print(f'conn_status FAILED (expected pre-fix): {type(e).__name__}: {e}')
try: r.ble_disconnect(timeout=3.0)
except: pass
r.disconnect()"
```

Expected output (pre-fix baseline):
```
connect: result=0
conn_status FAILED (expected pre-fix): TimeoutError: Response timeout
```

If `conn_status` returned OK, the bug is not reproducing — STOP and investigate before continuing (maybe the firmware on the board isn't what we just built, or the peer is different).

---

### Task 2: Move `BleConnMgr_poll()` call from `UartTask` to `RfTask`

**Files:**
- Modify: `firmware/cc1352/src/main_rtos.c`

- [ ] **Step 1: Remove the call from `UartTask_taskFxn`**

In `firmware/cc1352/src/main_rtos.c`, find this block (around lines 127–142):

```c
    /* UART polling loop — also runs BLE central mode when connected.
     * BLE connection events run here (same task context as initiator)
     * because the RF handle is opened from this task. */
    uint32_t led_counter = 0;
    while (1) {
        HostIFTask_poll();

        if (BleConnMgr_isRunning()) {
            BleConnMgr_poll();
        }

        /* LED blink */
        led_counter++;
        if (led_counter >= 50000u) {
            led_counter = 0;
            GPIO_toggleDio(LED_PIN);
        }

        Task_yield(); /* Cooperative — let RF task run */
    }
```

Replace with:

```c
    /* UART polling loop.
     *
     * Previously this task also drove BleConnMgr_poll() (see
     * fix/uart-starvation-during-conn: 2026-04-24). That coupled
     * host-command latency to the BLE connection interval via
     * Task_sleep() inside BleConnMgr_poll(), so host commands
     * timed out whenever a connection was live. BleConnMgr_poll()
     * now runs in RfTask, aligned with Sniffle's RadioTask model.
     */
    uint32_t led_counter = 0;
    while (1) {
        HostIFTask_poll();

        /* LED blink */
        led_counter++;
        if (led_counter >= 50000u) {
            led_counter = 0;
            GPIO_toggleDio(LED_PIN);
        }

        Task_yield();
    }
```

- [ ] **Step 2: Add the call inside `RfTask_taskFxn`**

In the same file, find `RfTask_taskFxn` (around lines 147–176). The existing loop is:

```c
    DataTask_init();
    while (1) {
        DataTask_poll();
        Task_yield();
    }
```

Replace with:

```c
    DataTask_init();
    while (1) {
        DataTask_poll();

        if (BleConnMgr_isRunning()) {
            BleConnMgr_poll();
        }

        Task_yield();
    }
```

- [ ] **Step 3: Add the header include if not already present**

Near the top of `main_rtos.c`, ensure `#include "ble_conn_mgr.h"` is present. If the line already exists in `UartTask_taskFxn`'s includes, leave it. If adding, put it in the block with other FeralRF headers (alphabetical order).

- [ ] **Step 4: Build and verify it compiles clean**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: `[100%] Built target feralrf_cc1352.elf` with no new warnings. Text size shouldn't change by more than a few bytes.

- [ ] **Step 5: Commit**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add firmware/cc1352/src/main_rtos.c
git commit -m "fix(ble): move BleConnMgr_poll from UartTask to RfTask"
```

The commit message body (use `-m` for body if desired, or `git commit --amend` to add):
```
UartTask previously slept up to one conn interval per BLE event
via Task_sleep() inside BleConnMgr_poll(). That starved
HostIFTask_poll(), so every host command issued while a BLE
connection was active (conn_status, gatt_discover, etc.) timed
out. Move the poll to RfTask, aligned with Sniffle's RadioTask
model: RF in the RF task, host I/O in the UART task.

Reproducer pre-fix (CH573 peripheral):
  ble_connect -> OK
  conn_status -> TimeoutError
  gatt_discover -> TimeoutError
  ble_disconnect -> OK (after supervision drops connection)
```

---

### Task 3: Flash and validate the fix against the CH573 peripheral

**Files:** none modified.

- [ ] **Step 1: Flash the patched firmware**

Run:
```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -6
```

Expected: `Verified match` + `Device restart complete`. Retry once if timeout.

- [ ] **Step 2: Re-run the exact pre-fix reproducer from Task 1 Step 5**

Run the same Python block as Task 1 Step 5.

Expected output (post-fix):
```
connect: result=0
conn_status OK: connected=True events=<n>  (n >= 1)
```

If `conn_status` still times out, **STOP and proceed to Task 4** (contingent synchronization).

- [ ] **Step 3: Run the full demo with `--read`**

Run (CH573 advertising on `DC:32:62:8D:E1:09`, type=public):
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
timeout 45 python examples/lab/demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 --read 2>&1 | tee /tmp/fix_uart_ch573_run1.txt
```

Expected:
- `Connection result: OK`
- `connected=True events=<n>` (non-zero)
- At least 1 service discovered, at least 1 characteristic discovered.
- `GATT done (status=0)`
- Reading a readable characteristic returns bytes (UTF-8 or hex printed).
- `Disconnected.`

- [ ] **Step 4: Back-to-back reconnect (no reset between runs)**

Run the exact same command twice more in succession:
```bash
python examples/lab/demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 2>&1 | tee /tmp/fix_uart_ch573_run2.txt
python examples/lab/demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 2>&1 | tee /tmp/fix_uart_ch573_run3.txt
```

Expected: both runs complete with successful GATT discovery, no manual reset required between.

If any run fails, record the failure — it indicates a remaining disconnect-cleanup bug (F8 criterion violation).

---

### Task 4 (CONTINGENT — only if Task 3 Step 2 timeout persists): Add minimal cross-task synchronization

Execute this task **only** if `conn_status` still times out after Task 2. The root cause in that case is concurrent access to shared state (TXQueue / AttClient) corrupting one side.

**Files:**
- Modify: `firmware/cc1352/src/tx_queue.c`
- Modify: `firmware/cc1352/src/att_client.c`

- [ ] **Step 1: Read current TXQueue implementation**

Run:
```bash
grep -n "TXQueue_insert\|TXQueue_take\|TXQueue_flush\|static" /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/src/tx_queue.c | head -20
```

Identify the shared state (the circular buffer indices / entries).

- [ ] **Step 2: Bracket TXQueue mutations with Hwi disable**

Add `#include <ti/drivers/dpl/HwiP.h>` at the top if not present. Wrap the body of `TXQueue_insert()`, `TXQueue_take()`, and `TXQueue_flush()` with:

```c
uintptr_t key = HwiP_disable();
/* ... existing body ... */
HwiP_restore(key);
```

Keep the critical section as short as possible — only the index / entry manipulation, not copies of buffer data (payload copies are local to a single caller, so they're already safe).

- [ ] **Step 3: Bracket AttClient pending-request mutation similarly**

In `att_client.c`, identify where `s_state` and `s_pending` (or equivalent request-tracking variables) are written from `AttClient_startDiscover`, `AttClient_startRead`, `AttClient_startWrite` (called from UartTask via command_processor) vs where they're read/updated from `AttClient_poll` or `AttClient_onRxPacket` (called from RfTask via BleConnMgr_poll).

Wrap each mutation in `HwiP_disable/restore`.

- [ ] **Step 4: Rebuild + flash + re-run Task 3 Step 2**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -5
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -5
```

Then re-run Task 3 Step 2 and Step 3.

Expected: `conn_status` returns OK, demo completes full GATT flow.

If still fails, escalate — the issue is deeper (likely RF_Handle cross-task constraint or an atomicity gap not covered by HwiP). Stop and report.

- [ ] **Step 5: Commit the sync fix**

Run:
```bash
git add firmware/cc1352/src/tx_queue.c firmware/cc1352/src/att_client.c
git commit -m "fix(ble): guard TXQueue + AttClient state from concurrent UartTask/RfTask access"
```

---

### Task 5: Regression — confirm no other PHYs broke

**Files:** none modified.

This task exists because the move touches the task architecture; a regression in IEEE/Sub-1GHz RX or BLE TX would go unnoticed without a quick check.

- [ ] **Step 1: BLE 1M adv scan still works (no connection)**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python -c "
import time
from feralrf import Radio
from feralrf.enums import PHY
r = Radio(port='/dev/ttyACM0')
r.init()
r.set_phy(PHY.BLE_1M, channel=37)
r.set_adv_hop(True)
r.start_rx()
time.sleep(6)
r.stop_rx()
s = r.get_stats()
print(f'BLE hop 6s: rx_ok={s.rx_ok} crc_err={s.rx_crc_err} adv={s.ll_kind_adv}')
r.disconnect()
assert s.rx_ok > 50, f'Too few BLE packets: {s.rx_ok}'"
```

Expected: `rx_ok > 50`, assertion passes. If fails, the move broke the scan path — STOP.

- [ ] **Step 2: IEEE 802.15.4 RX still works on ch25**

Run:
```bash
python -c "
import time
from feralrf import Radio
from feralrf.enums import PHY
r = Radio(port='/dev/ttyACM0')
r.init()
r.set_phy(PHY.IEEE_802_15_4, channel=25)
r.start_rx()
time.sleep(4)
r.stop_rx()
s = r.get_stats()
print(f'IEEE ch25 4s: rx_ok={s.rx_ok} crc_err={s.rx_crc_err}')
r.disconnect()
assert s.rx_ok + s.rx_crc_err > 0, 'IEEE RX appears dead'"
```

Expected: at least 1 packet (ok or crc err). Assertion passes.

- [ ] **Step 3: Python unit tests still pass**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
pytest tests/test_gatt_api.py tests/test_commands_contract.py tests/test_protocol.py -v 2>&1 | tail -15
```

Expected: all tests PASS (including the 26 F8 unit tests).

---

### Task 6: Tag and report

**Files:** none modified.

- [ ] **Step 1: Tag the fix commit**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git tag -a v2.0-f8-prereq-uart-fix -m "F8 prerequisite: UART starvation during BLE connection resolved"
git log --oneline -5
```

Expected: tag created, log shows the fix commit (and optionally a sync commit from Task 4).

- [ ] **Step 2: Produce a concise handoff report to the user**

Output in chat (do not commit):
- Branch: `fix/uart-starvation-during-conn`
- Tag: `v2.0-f8-prereq-uart-fix`
- Commits: `git log --oneline feature/f8-gatt-validation..HEAD` output
- Whether Task 4 (sync fix) was required (yes / no)
- Summary of Task 3 + Task 5 evidence (GATT flow green, BLE/IEEE regression green).
- Next step: merge this fix back into `feature/f8-gatt-validation` (user decides squash vs merge commit), then continue F8 T12 checkpoint humano.

---

## Self-Review

- **Root cause covered:** Task 2 moves `BleConnMgr_poll()` to `RfTask`. ✅
- **Failure reproduction before fix:** Task 1 Step 5 captures baseline failure; without that we can't prove the fix worked. ✅
- **Fix validation with real peripheral:** Task 3 runs the CH573 demo, back-to-back reconnects, and `--read`. ✅
- **Regression check:** Task 5 covers BLE scan, IEEE RX, and unit tests. ✅
- **Contingent sync fix:** Task 4 only runs if Task 3 shows residual corruption. Minimal scope — HwiP critical sections on TXQueue and AttClient only. ✅
- **No placeholders:** every step has exact commands and expected outputs. ✅
- **Naming consistency:** `BleConnMgr_poll`, `BleConnMgr_isRunning`, `HostIFTask_poll`, `RfTask_taskFxn`, `UartTask_taskFxn` match firmware usage. ✅
