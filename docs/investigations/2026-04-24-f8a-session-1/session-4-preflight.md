# F8A Session 4 — preflight (2026-04-27)

- Branch: `feature/f8a-ble-central-sniffle`
- HEAD: `b238930` (`wip(f8a): Session 3 groundwork — anchor + CSA#1 + Sniffle parity (NOSYNC persists)`)
- Working tree: clean (one untracked file — `docs/superpowers/plans/2026-04-27-f8a-session-4-investigate-no-tx.md`, the Session 4 plan itself, committed alongside this preflight)

## Hardware

| Board | SN     | Firmware                  | Bridge port    | LoRa port      | Shell port     |
|-------|--------|---------------------------|----------------|----------------|----------------|
| #1    | 504B32 | FeralRF `b238930`         | `/dev/ttyACM8` | `/dev/ttyACM9` | `/dev/ttyACM10` |
| #2    | 565932 | `sniffle_cc1352p7_1M.hex` | `/dev/ttyACM5` | `/dev/ttyACM6` | `/dev/ttyACM7` |

Discovered via:
```
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py devices
```

## CH573 alive check

Board #1 ran a 3-second BLE scan on ch 37 (2402 MHz):
- Target MAC `DC:32:62:8D:E1:09` (`PwnPet_C81F`).
- Result: **34 ADV_IND from CH573 / 233 total packets**.
- Verdict: alive ✅

First `init()` attempt timed out (response-timeout), suspected stale serial state. Retry succeeded after a 0.3 s sleep between `connect()` and `init()`. A non-fatal `Async RF error: code=0x2F` was emitted post-init but did not block the subsequent scan. Worth flagging if it recurs — it's a leftover from the previous session's `BLE_DONE_NOSYNC` chain.

## Bench is ready for Tasks 1-4.
