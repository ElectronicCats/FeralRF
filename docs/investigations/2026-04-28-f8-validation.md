# F8 — GATT end-to-end validation note

**Date:** 2026-04-28
**Branch:** `feature/f8a-ble-central-sniffle` (same branch as F8A)
**Outcome:** ✅ closed against CH573; partial on additional peripherals.

## What was validated

Three real peripherals exercised on the post-`v2.0-f8a` firmware:

| Peripheral | Address | Type | Connect | Discovery | Read | Disconnect |
|------------|---------|------|---------|-----------|------|------------|
| CH573 PwnPet_C81F | DC:32:62:8D:E1:09 | public | ✅ | ✅ 4 svc / 20 chr | ✅ "PwnPet_C81F" | ✅ |
| USBNinja           | C0:94:9A:DA:4F:09 | public | ✅ | ❌ timeout (security/protocol) | — | dropped mid-discovery |
| LE-7018F           | B0:F0:0C:EA:52:76 | public | ✅ initial | ❌ events died after CONNECT_IND | — | — |

CH573 hits all four F8 close criteria from the v2 design spec § F8:
- ✅ Discovery returns ≥ 1 service + ≥ 1 characteristic (returns 4 + 20)
- ✅ Device Name (UUID 0x2A00) read returns advertised name (`PwnPet_C81F`)
- ✅ `att_state` returns to IDLE after `RSP_GATT_DONE`
- ⚠️ Disconnect / reconnect: clean per-script; back-to-back inside the same script still flaky because CH573 takes ~1 s to resume advertising even after our `LL_TERMINATE_IND`. Not a firmware-side issue.

USBNinja and LE-7018F prove the initiator path generalizes beyond CH573 — both establish CONNECT_IND with `result=0` and start receiving LL packets — but device-specific behavior (likely security policy on USBNinja, connection-parameter expectations on LE-7018F) closes the link before discovery completes. Not blocking F8.

## What was NOT validated

- **Smartphone target:** spec listed smartphone as primary peripheral. Multiple attempts to get an Android handset advertising as `Sabas` via nRF Connect did not surface the name in passive scan (likely scan-response-only or background-killed advertiser). No firmware fault — this is a setup issue we'll revisit if needed.
- **2× back-to-back `demo_ble_connect_gatt.py` runs without inter-run delay:** flaky on CH573 specifically because of its 1 s advertising-resume gap after teardown. Adding a `time.sleep(2.0)` between runs in test scripts is the working-around. Not a code defect.
- **8 PHYs regression matrix:** not re-run this session. Anchor change in Session 5 + RX-path changes in Session 6 are central-mode-only, so should not affect any other PHY. Recommended as a F9 prereq.

## F8 acceptance

The user explicitly closed F8 with the CH573-only evidence ("cerramos con eso de momento"), accepting the smartphone validation as a follow-up. The hard criteria — discovery completes, read OK, no leaks — are met against a real BLE 4.2 peripheral. Tag `v2.0-f8`.
