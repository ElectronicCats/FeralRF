# F8e — Investigation Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close findings F5 (ATT discovery bounds guards) and F6 (`s_last_rx_rat` init) from `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md`. Defensive firmware-only cleanup.

**Architecture:** Two ATT response handlers (`handle_read_by_group_type_rsp`, `handle_read_by_type_rsp`) gain identical-shape guards against malformed peer `entry_len`, with a new application-specific error code (`ATT_ERR_MALFORMED_RSP = 0xFE`) and a new debug ring-buffer tag (`ATT_DBG_TAG_MALFORMED_RSP = 35`). The follower supervision baseline is initialized at the ADV→FOLLOWING state transition. No host API change, no wire-protocol change.

**Tech Stack:** C99 firmware (TI-RTOS 7, SDK 7.10.01.24), CMake build, catnip flash (TI ROM BSL via UART), Python smoke harness on hardware.

**Spec:** `docs/superpowers/specs/2026-05-03-f8e-investigation-cleanup-design.md`

**Source branch:** `feature/ti-rtos-migration` HEAD=`2861221` (spec commit on top of `56e09aa`)

---

## Task 0: Setup

**Files:** none (pure git + sanity checks)

**Why this is one task:** Cut the F8e branch and confirm the toolchain + boards are ready before touching code.

- [ ] **Step 0.1: Cut the F8e branch from `feature/ti-rtos-migration`**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git checkout feature/ti-rtos-migration
git pull --ff-only
git checkout -b feature/f8e-investigation-cleanup
git status --short
```

Expected: branch switched cleanly. `git status --short` shows only the local WIP files (`firmware/cc1352/include/radio_if.h`, `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md`). **Do not stage these.**

- [ ] **Step 0.2: Confirm baseline build is clean**

```bash
cd firmware/cc1352/build
cmake --build . -j2 2>&1 | tail -5
```

Expected: build succeeds, produces `feralrf_cc1352.elf` and `feralrf_cc1352.hex`, no new warnings.

- [ ] **Step 0.3: Confirm pytest baseline is unchanged**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
pytest -q --deselect tests/test_radio_strict_responses.py::test_read_response_ignores_echoed_command_frames 2>&1 | tail -3
```

Expected: `445 passed, 5 skipped, 1 deselected` (or whatever the current HEAD reports — F8e adds zero Python tests so this number must be **identical** before and after the implementation).

- [ ] **Step 0.4: Confirm at least one CatSniffer is present**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip devices 2>&1 | tail -5
```

Expected: at least one CatSniffer detected. Note the port (e.g. `/dev/ttyACM5` or `/dev/ttyACM8`). If none, ask the user to plug the board in.

---

## Task 1: ATT header — new error code + debug tag

**Files:**
- Modify: `firmware/cc1352/include/att_client.h` (one new `#define`, one new enum entry)

**Why this is one task:** Headers must compile clean before any `.c` file references the new symbols. Atomic header commit also documents the new contract before any caller depends on it.

- [ ] **Step 1.1: Add the error code near the existing constants**

Open `firmware/cc1352/include/att_client.h`. Find the section ending around line 145 (the `AttClient_DbgTag` enum closing brace `} AttClient_DbgTag;`). Look for an existing block of `#define` constants in the file (search for `#define ATT_DEFAULT_MTU` or similar). If there is no existing `#define` block, add one **above** the `AttClient_DbgTag` enum declaration (i.e. before the line `#define ATT_DBG_LOG_DEPTH 32u`). Add:

```c
/* F5 fix: vendor-specific status code surfaced via onDone(status) when an
 * ATT response from the peer fails our bounds checks (entry_len out of
 * range). 0xE0–0xFF is the application-specific range per BT Core Spec
 * Vol 3 Part F §3.4.1.1. */
#define ATT_ERR_MALFORMED_RSP 0xFEu
```

- [ ] **Step 1.2: Append `ATT_DBG_TAG_MALFORMED_RSP` to the debug tag enum**

In the same file, locate the `AttClient_DbgTag` enum (lines 110-145). The last entry currently is `ATT_DBG_TAG_POLL_TX_READ_UUID = 34,`. Append a new entry immediately after it (still inside the enum, before the closing `} AttClient_DbgTag;`):

```c
    ATT_DBG_TAG_MALFORMED_RSP = 35,
```

The full enum tail now reads:

```c
    ATT_DBG_TAG_READ_UUID_RSP = 33,
    ATT_DBG_TAG_POLL_TX_READ_UUID = 34,
    ATT_DBG_TAG_MALFORMED_RSP = 35,
} AttClient_DbgTag;
```

- [ ] **Step 1.3: Verify the header still compiles standalone**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j2 2>&1 | tail -5
```

Expected: build succeeds, no new warnings. Header-only changes; the build target rebuilds nothing user-visible yet.

- [ ] **Step 1.4: Run pre-commit on the header**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/include/att_client.h 2>&1 | tail -10
```

Expected: all hooks pass (or "Skipped" for hooks that don't apply). `clang-format` may auto-format; if it does, re-stage.

- [ ] **Step 1.5: Commit**

```bash
git add firmware/cc1352/include/att_client.h
git commit -m "$(cat <<'EOF'
feat(f8e): ATT_ERR_MALFORMED_RSP + ATT_DBG_TAG_MALFORMED_RSP constants

Header-only addition for the F5 bounds-guard handlers. ATT_ERR_MALFORMED_RSP
(0xFE) is in the application-specific range per BT Core Spec Vol 3 Part F.
ATT_DBG_TAG_MALFORMED_RSP (35) extends the existing debug ring-buffer.

Closes F5 prep from docs/investigations/2026-05-03-ti-rtos-migration-code-review.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit lands cleanly. `git log --oneline -1` shows the new commit.

---

## Task 2: F5a — `handle_read_by_group_type_rsp` bounds guard

**Files:**
- Modify: `firmware/cc1352/src/att_client.c` (lines 253-294)

**Why this is one task:** Single function, single guard, isolated change. Group-type and read-by-type handlers are kept in separate tasks because each requires its own commit message tying it to F5 — and because verifying one at a time keeps the diff trivially reviewable.

- [ ] **Step 2.1: Open the file and locate the handler**

`firmware/cc1352/src/att_client.c`. The function starts at line 253 with:

```c
static void handle_read_by_group_type_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (len < 4 || s_state != ATT_STATE_WAIT_DISCOVER_RSP) {
        att_dbg(ATT_DBG_TAG_GROUP_RSP, old);
        return;
    }
    s_request_pending = false;

    uint8_t entry_len = pdu[1]; /* length of each attribute data */
    uint8_t offset = 2;
```

- [ ] **Step 2.2: Insert the bounds guard between `entry_len` read and the consume loop**

Replace the block from `s_request_pending = false;` through `uint8_t offset = 2;` (lines 259-262 in current HEAD) with:

```c
    uint8_t entry_len = pdu[1]; /* length of each attribute data */

    /* F5 fix: validate entry_len before consuming. Spec: each entry is
     * [start:2][end:2][uuid:2|16] so minimum is 4 + 2 = 6 bytes; entry_len
     * (which excludes the 2-byte header) is therefore >= 4. entry_len == 0
     * would otherwise infinite-loop the consumer below. The upper bound
     * rejects entries that cannot fit even one full record after the
     * 2-byte [opcode][entry_len] header. */
    if (entry_len < 4u || entry_len > (uint8_t)(len - 2u)) {
        att_dbg(ATT_DBG_TAG_MALFORMED_RSP, old);
        s_state = ATT_STATE_IDLE;
        if (s_cb.onDone) {
            att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
            s_cb.onDone(ATT_ERR_MALFORMED_RSP);
        }
        return;
    }

    s_request_pending = false;
    uint8_t offset = 2;
```

The rest of the function (the `while (offset + entry_len <= len)` loop, the trailing `att_dbg(ATT_DBG_TAG_GROUP_RSP, old);`) stays unchanged.

- [ ] **Step 2.3: Verify compilation**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j2 2>&1 | tail -10
```

Expected: build succeeds, no new warnings. Note: the existing `len < 4` guard at the top of the handler is preserved; our new check is `entry_len < 4` which is a different field.

- [ ] **Step 2.4: Pre-commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/src/att_client.c 2>&1 | tail -10
```

Expected: green. `clang-format` may reformat braces — re-stage if it does.

- [ ] **Step 2.5: Commit**

```bash
git add firmware/cc1352/src/att_client.c
git commit -m "$(cat <<'EOF'
fix(f8e): F5a — entry_len bounds guard in handle_read_by_group_type_rsp

Reject malformed peer responses (entry_len < 4 or entry_len > len-2)
with ATT_ERR_MALFORMED_RSP via onDone callback. Prevents infinite
loop on entry_len=0 and underflow on entry_len < 4.

Closes F5 (group_type half) from
docs/investigations/2026-05-03-ti-rtos-migration-code-review.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: F5b — `handle_read_by_type_rsp` bounds guard

**Files:**
- Modify: `firmware/cc1352/src/att_client.c` (lines 296-322)

**Why this is one task:** Mirror of Task 2 with `entry_len < 5u` (handle:2 + properties:1 + value_handle:2). Separate commit so the per-handler intent is preserved in history.

- [ ] **Step 3.1: Locate the handler**

`firmware/cc1352/src/att_client.c` line 296:

```c
static void handle_read_by_type_rsp(const uint8_t *pdu, uint8_t len) {
    AttClient_State old = s_state;
    if (len < 4 || s_state != ATT_STATE_WAIT_CHAR_RSP) {
        att_dbg(ATT_DBG_TAG_TYPE_RSP, old);
        return;
    }
    s_request_pending = false;

    uint8_t entry_len = pdu[1];
    uint8_t offset = 2;
```

- [ ] **Step 3.2: Insert the bounds guard**

Replace lines 302-305 (`s_request_pending = false;` through `uint8_t offset = 2;`) with:

```c
    uint8_t entry_len = pdu[1];

    /* F5 fix: validate entry_len before consuming. Characteristic discovery
     * entries are [handle:2][properties:1][value_handle:2][uuid:2|16], so
     * entry_len (excluding the 2-byte header) is >= 5. entry_len == 0
     * would infinite-loop and < 5 underflows uuidLen below. */
    if (entry_len < 5u || entry_len > (uint8_t)(len - 2u)) {
        att_dbg(ATT_DBG_TAG_MALFORMED_RSP, old);
        s_state = ATT_STATE_IDLE;
        if (s_cb.onDone) {
            att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
            s_cb.onDone(ATT_ERR_MALFORMED_RSP);
        }
        return;
    }

    s_request_pending = false;
    uint8_t offset = 2;
```

The rest of the function (`while (offset + entry_len <= len)` loop and trailing `att_dbg`) stays unchanged.

- [ ] **Step 3.3: Verify compilation**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j2 2>&1 | tail -10
```

Expected: build succeeds, no new warnings.

- [ ] **Step 3.4: Pre-commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/src/att_client.c 2>&1 | tail -10
```

Expected: green.

- [ ] **Step 3.5: Commit**

```bash
git add firmware/cc1352/src/att_client.c
git commit -m "$(cat <<'EOF'
fix(f8e): F5b — entry_len bounds guard in handle_read_by_type_rsp

Reject malformed peer responses (entry_len < 5 or entry_len > len-2)
with ATT_ERR_MALFORMED_RSP via onDone callback. Symmetric with F5a;
characteristic discovery entries require entry_len >= 5.

Closes F5 (read_by_type half) from
docs/investigations/2026-05-03-ti-rtos-migration-code-review.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: F6 — initialize `s_last_rx_rat` at ADV → FOLLOWING transition

**Files:**
- Modify: `firmware/cc1352/src/ll_follower.c` (line 303)

**Why this is one task:** One line change. The supervision check at line 343 reads `RF_getCurrentTime() - s_last_rx_rat`; when `s_last_rx_rat == 0` (BSS zero-init) and no data packet has arrived yet, the difference is `RF_getCurrentTime()` itself — a huge value that may exceed `s_supervision * 40000u` and fire `LL_FOLLOWER_DONE_SUPERVISION` spuriously.

- [ ] **Step 4.1: Locate the state transition**

`firmware/cc1352/src/ll_follower.c` lines 302-306:

```c
        if (s_connect_ind_pending) {
            s_state = LL_FOLLOWER_STATE_FOLLOWING;
            s_connect_ind_pending = false;
            return;
        }
```

- [ ] **Step 4.2: Initialize `s_last_rx_rat` immediately before the state assignment**

Replace those four lines with:

```c
        if (s_connect_ind_pending) {
            /* F6 fix: baseline supervision timestamp at the moment we commit
             * to following. Without this, s_last_rx_rat stays at BSS zero
             * until the first data packet arrives, and the supervision
             * check (s_supervision * 40000u) may fire spuriously before
             * sync. */
            s_last_rx_rat = RF_getCurrentTime();
            s_state = LL_FOLLOWER_STATE_FOLLOWING;
            s_connect_ind_pending = false;
            return;
        }
```

- [ ] **Step 4.3: Verify compilation**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j2 2>&1 | tail -10
```

Expected: build succeeds, no new warnings. `s_last_rx_rat` is already declared at file scope (line 49); `RF_getCurrentTime()` is already used elsewhere in the same file (e.g. line 173, line 297, line 343) so no new include is required.

- [ ] **Step 4.4: Pre-commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/src/ll_follower.c 2>&1 | tail -10
```

Expected: green.

- [ ] **Step 4.5: Commit**

```bash
git add firmware/cc1352/src/ll_follower.c
git commit -m "$(cat <<'EOF'
fix(f8e): F6 — initialize s_last_rx_rat at ADV→FOLLOWING transition

Baseline the follower supervision timestamp at the moment we commit to
following the captured CONNECT_IND. Previously the variable stayed at
BSS zero until the first data packet, which caused the supervision
check at line 343 to read RF_getCurrentTime() - 0 — potentially huge —
and fire LL_FOLLOWER_DONE_SUPERVISION before sync.

Closes F6 from
docs/investigations/2026-05-03-ti-rtos-migration-code-review.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Build artifact + flash + smoke validation

**Files:** none (validation only)

**Why this is one task:** F8e is firmware-only and contributes no Python tests. The validation gate is the build artifact going onto a real CatSniffer and the existing F8c / F8b Track B smoke harnesses still passing. Hardware steps are grouped because they share the flash cycle.

- [ ] **Step 5.1: Final clean build**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j2 2>&1 | tail -10
ls -la feralrf_cc1352.hex
```

Expected: build green, `feralrf_cc1352.hex` exists with a fresh timestamp. Capture the file size; F8e's net firmware delta should be < 200 bytes vs `v2.0-f8d`.

- [ ] **Step 5.2: Flash the CC1352 (retry up to 2× before asking)**

Use the port from Task 0.4. Example uses `/dev/ttyACM5`; replace with the actual detected port.

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -5
```

Expected: flash success on first or second attempt. Per project rules, **retry the exact command up to 2 times** before asking the user to power-cycle the board. Always flash `.hex`, never `.bin`.

- [ ] **Step 5.3: F5 smoke regression — F8c discovery against Soundcore Boom 2**

Bring up the Soundcore (button hold to enter pairing). Then:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f8c.py 2>&1 | tail -40
```

Expected:
- Discovery completes (no `ATT_ERR_MALFORMED_RSP` raised; the Soundcore is spec-compliant).
- The smoke prints its existing PASS markers for MTU, Read by UUID, and Disconnect reason.
- **Pass criterion:** the regression smoke ends with the same PASS shape it produced under `v2.0-f8c` / `v2.0-f8d`. If `ATT_ERR_MALFORMED_RSP` (status 0xFE) surfaces against a real spec-compliant peer, **stop and investigate** — the bounds are wrong.

If the bocina is unavailable, substitute any BLE peripheral that exposes a discoverable GATT database (any phone in pairing mode works). Document the peer used.

- [ ] **Step 5.4: F6 smoke regression — F8b Track B passive follower**

Have a real BLE central+peripheral pair connecting in the air (e.g. phone ↔ Sony WH-CH720N). The harness on `/dev/ttyACM5` (or whichever port) runs the follower:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
python examples/lab/smoke_f8b_follower.py 2>&1 | tail -30
```

Expected:
- The follower captures ≥10 LL data packets in a normal session.
- It does **not** fire `LL_FOLLOWER_DONE_SUPERVISION` *before any data packet has been captured* (that was the F6 bug).
- **Pass criterion:** total packets captured >= 10, and if `FOLLOW_DONE` is reported, its reason is `PEER_TERMINATE` or a graceful end — not `SUPERVISION` with `s_packets_captured == 0`.

If the pair is not in range / not connecting, document this and skip — F6 is a one-line init that cannot regress the happy path; the smoke is a sanity check, not a gating test.

- [ ] **Step 5.5: Post-implementation pytest baseline check**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
pytest -q --deselect tests/test_radio_strict_responses.py::test_read_response_ignores_echoed_command_frames 2>&1 | tail -3
```

Expected: **identical** numbers to Task 0.3 (no Python changes in F8e). Any drift is a regression.

---

## Task 6: Tag + memory update + handoff

**Files:**
- Create: `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8e_done.md`
- Modify: `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` (add one index line)

**Why this is one task:** Closure-only — tag, memory note, hand back to the user for fast-forward decision.

- [ ] **Step 6.1: Tag `v2.0-f8e`**

After Task 5 smoke gates pass:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git tag -a v2.0-f8e -m "F8e — investigation cleanup (F5 ATT bounds + F6 follower init)"
git tag --list 'v2.0-f8*'
```

Expected: tag list shows `v2.0-f8a`, `v2.0-f8b-trackA`, `v2.0-f8b-trackB`, `v2.0-f8c`, `v2.0-f8d`, `v2.0-f8e`.

- [ ] **Step 6.2: Write `project_f8e_done.md` memory**

```bash
cat > /home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8e_done.md <<'EOF'
---
name: F8e closure
description: F5+F6 from TI-RTOS migration code review closed via defensive firmware cleanup. Tag v2.0-f8e on feature/f8e-investigation-cleanup.
type: project
---

F8e closed 2026-05-03 on branch feature/f8e-investigation-cleanup.

Scope:
- F5: bounds guards (entry_len ≥ 4 / ≥ 5; ≤ len-2) in handle_read_by_group_type_rsp + handle_read_by_type_rsp. New ATT_ERR_MALFORMED_RSP (0xFE) + ATT_DBG_TAG_MALFORMED_RSP (35).
- F6: s_last_rx_rat initialized at ADV→FOLLOWING transition.

~15 LOC firmware total. No host API change, no wire-protocol change, no Python tests added.

Validation:
- cmake build clean.
- F8c smoke regression (Soundcore / equivalent peer) — discovery still completes spec-compliant peers without raising ATT_ERR_MALFORMED_RSP.
- F8b Track B smoke — passive follower captures ≥10 packets without spurious LL_FOLLOWER_DONE_SUPERVISION before sync.
- pytest baseline unchanged (identical pass count to pre-F8e).

Tag v2.0-f8e ready for FF into feature/ti-rtos-migration once the user approves merging.

F8f (Python F8 has_handle + F9 PENDING_COMMAND_IDS) and F8g (F7 async error contract) remain deferred — each gets its own brainstorm + spec + plan cycle.
EOF
```

Expected: file written. Verify with `wc -l` (should be ~25 lines).

- [ ] **Step 6.3: Add MEMORY.md index entry**

Open `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md`. Find the existing project section and the last `project_f8d_done.md` entry. Insert immediately after it:

```markdown
- [project_f8e_done.md](project_f8e_done.md) — 2026-05-03 tag v2.0-f8e on feature/f8e-investigation-cleanup. F5 ATT bounds + F6 follower init. ~15 LOC. F8f (Python) + F8g (async errors) still deferred.
```

The entry MUST be a single line (the index truncates after 200 lines and entries longer than ~150 chars are hard to scan).

- [ ] **Step 6.4: Final report to user**

Hand back a summary:
- Branch `feature/f8e-investigation-cleanup` HEAD = (latest commit hash).
- Commits: 4 (header, F5a, F5b, F6).
- Tag: `v2.0-f8e`.
- Validation: build green; F8c smoke pass; F8b Track B smoke pass; pytest unchanged.
- Awaiting: user decision to FF into `feature/ti-rtos-migration`.

Do **not** auto-merge. The user runs the FF themselves (matches the F8c, F8d pattern).

---

## Self-review checklist

- **Spec coverage:** F5 (group_type + read_by_type) → Tasks 2-3. F6 → Task 4. New constants → Task 1. Smoke gating → Task 5.3-5.4. Build gating → Tasks 0.2, 1.3, 2.3, 3.3, 4.3, 5.1. Tag + memory → Task 6.
- **Out-of-scope respected:** No edits to other ATT handlers, no edits to `radio_if.h`, no Python tests.
- **No placeholders:** every code-changing step shows the exact code. Every command shows the exact invocation.
- **Type consistency:** `ATT_ERR_MALFORMED_RSP` and `ATT_DBG_TAG_MALFORMED_RSP` defined in Task 1, used by Tasks 2 and 3 with identical spelling. `s_last_rx_rat` and `RF_getCurrentTime()` already defined in `ll_follower.c` (verified in Task 4.3 rationale).
