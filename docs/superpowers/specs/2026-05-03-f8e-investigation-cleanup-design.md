# F8e — Investigation Cleanup: ATT Bounds Guards + Follower Supervision Init

**Date:** 2026-05-03
**Branch (target):** `feature/f8e-investigation-cleanup` cut from `feature/ti-rtos-migration` HEAD=`56e09aa`
**Tag (target):** `v2.0-f8e`
**Source:** `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md`, findings F5 + F6
**Sibling sub-projects (deferred):**
- F8f — Python bug fixes (review F8 + F9), separate brainstorm
- F8g — async error contract (review F7), separate brainstorm

## Goal

Close findings F5 and F6 from the TI-RTOS migration code review with minimal,
defensive firmware changes. No host API changes. No wire-protocol changes.

## Findings recap

### F5 — ATT discovery handlers loop / underflow on malformed peer responses

`firmware/cc1352/src/att_client.c`:

- `handle_read_by_group_type_rsp` (line 253) reads `entry_len = pdu[1]` without
  validating `entry_len >= 4`. When `entry_len == 0`, the inner `while (offset
  + entry_len <= len)` loop never advances → infinite loop in the RF task.
- `handle_read_by_type_rsp` (line 296) has the same shape but needs
  `entry_len >= 5` (handle:2 + properties:1 + value_handle:2).
- Too-small values cause `uuidLen = entry_len - 4u` (or `- 5u`) to underflow
  to a near-`UINT8_MAX` value, then the host callback reads past buffer.

### F6 — Follower supervision timestamp uninitialized before first data packet

`firmware/cc1352/src/ll_follower.c`:

- `s_last_rx_rat` declared at line 49 in BSS (zero-init).
- Only assigned when a data packet arrives (line 173, `s_on_data_packet`).
- Supervision check at line 343 reads `RF_getCurrentTime() - s_last_rx_rat` on
  every poll, including before any data packet has arrived.
- With `s_last_rx_rat == 0`, the difference is `RF_getCurrentTime()` itself
  (potentially huge), so `LL_FOLLOWER_DONE_SUPERVISION` may fire spuriously
  before the follower captures its first packet.

## Design

### F5 — Bounds guards with telemetry

Both ATT discovery handlers gain identical-shape guards:

```c
uint8_t entry_len = pdu[1];
if (entry_len < MIN_ENTRY_LEN || entry_len > len - 2u) {
    att_dbg(ATT_DBG_TAG_MALFORMED_RSP, old);
    s_state = ATT_STATE_IDLE;
    if (s_cb.onDone) {
        att_dbg(ATT_DBG_TAG_DONE_CB, s_state);
        s_cb.onDone(ATT_ERR_MALFORMED_RSP);
    }
    return;
}
```

Where `MIN_ENTRY_LEN = 4u` for `handle_read_by_group_type_rsp` and `5u` for
`handle_read_by_type_rsp`.

The upper bound `entry_len > len - 2u` rejects entries that cannot fit even
one full record after the 2-byte header (`opcode + entry_len`).

### F5 — New constants

`firmware/cc1352/include/att_client.h`:

- New error code: `#define ATT_ERR_MALFORMED_RSP 0xFEu`
  - 0xE0–0xFF is the application-specific range per BLE Core Spec.
  - Surfaces to the host via the existing `onDone(status)` callback. No new
    response opcode, no new wire field.
- New debug tag: `ATT_DBG_TAG_MALFORMED_RSP = 35` appended to the existing
  enum.

### F6 — Initialize `s_last_rx_rat` at ADV → FOLLOWING transition

`firmware/cc1352/src/ll_follower.c` line 303 (transition from
`LL_FOLLOWER_STATE_SCAN_ADV` to `LL_FOLLOWER_STATE_FOLLOWING`):

```c
s_last_rx_rat = RF_getCurrentTime();
s_state = LL_FOLLOWER_STATE_FOLLOWING;
```

Init at the transition point (not at `LlFollower_start()`) because ADV scan
may run for several seconds; supervision must measure from the moment we
commit to following the connection, not from session start.

## Out of scope (explicit)

- Other ATT response handlers (`handle_mtu_rsp`, `handle_error_rsp`,
  `handle_read_rsp`, `handle_write_rsp`, `handle_read_uuid_rsp`). They may
  benefit from similar hardening but are not flagged in the review.
- `firmware/cc1352/include/radio_if.h` — has unstaged WIP, must not be touched.
- Refactor / extract shared bounds-validation helper. YAGNI for two call sites.
- Pre-existing F8c / F8d behavior. F8e does not modify connect, disconnect,
  MTU, read-by-UUID, or follower data path.

## Affected files

| File | Change | Estimated LOC |
|------|--------|---------------|
| `firmware/cc1352/include/att_client.h` | `ATT_ERR_MALFORMED_RSP` + `ATT_DBG_TAG_MALFORMED_RSP` | +2 |
| `firmware/cc1352/src/att_client.c` | Two bounds-guard blocks (group_type + type) | +12 |
| `firmware/cc1352/src/ll_follower.c` | One init line at state transition | +1 |
| **Total firmware** | | **+15** |

No Python changes. No test additions (rationale below).

## Testing strategy

F8e is firmware-only defensive code. No host-side contract change. Two
validation gates:

### Build + pre-commit

```bash
cmake --build firmware/cc1352/build -j2
pre-commit run --files <changed-files>
```

Gating: exit 0, no new warnings, clang-format clean.

### Hardware smoke regression

| Smoke | Purpose | Pass criterion |
|-------|---------|----------------|
| `tests/smoke/smoke_f8c_*.py` vs Soundcore Boom 2 | F5 happy-path: spec-compliant peer still discovers OK | 3/3 PASS, no `ATT_ERR_MALFORMED_RSP` raised |
| `tests/smoke/smoke_f8b_track_b_*.py` | F6: passive follower no longer fires spurious `LL_FOLLOWER_DONE_SUPERVISION` before first data packet | ≥10 packets captured in normal session |

### Why no unit tests

- Repo has no C unit-test framework on the firmware side; introducing one is
  out of scope for a defensive cleanup.
- Adversarial peer simulator (to inject malformed PDUs) would be larger work
  than the fix itself.
- Both bugs are static-analysis-grade; visual review + build + happy-path
  smoke covers the realistic risk surface.

## Commits (planned sequence)

1. `feat(f8e): ATT_ERR_MALFORMED_RSP + ATT_DBG_TAG_MALFORMED_RSP constants`
   — header-only.
2. `fix(f8e): F5 — entry_len bounds guards in ATT discovery handlers`
   — both `handle_read_by_group_type_rsp` and `handle_read_by_type_rsp` in one
   commit (shared pattern).
3. `fix(f8e): F6 — initialize s_last_rx_rat at ADV→FOLLOWING transition`.
4. `docs(f8e): close F5+F6 from TI-RTOS migration code review` — references
   the investigation doc in the commit message body. The investigation doc
   itself is **not** modified (it is a historical snapshot).

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| F5 false positive on spec-compliant peer | Very low | Discovery aborts cleanly via `onDone(ATT_ERR_MALFORMED_RSP)` instead of completing | Bounds chosen from spec minimums; F8c smoke covers ≥2 real peers |
| F6 init in wrong place (e.g. before ADV scan) | Low | Unchanged spurious supervision risk | Init explicitly at the state-transition line, not in start path |
| Unstaged `radio_if.h` WIP collides with F8e branch | Low | None | F8e does not touch `radio_if.h`; WIP can be carried forward verbatim |

## Rollout

- Branch: `feature/f8e-investigation-cleanup`.
- Tag: `v2.0-f8e` after all gates green + smoke PASS.
- Merge: fast-forward into `feature/ti-rtos-migration`. Same pattern as
  v2.0-f8c, v2.0-f8d.
- Memory update: new `project_f8e_done.md` index entry with closure
  summary.
