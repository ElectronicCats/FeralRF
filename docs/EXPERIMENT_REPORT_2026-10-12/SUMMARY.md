# FeralRF Experimental Validation Summary

## Scope

This is the high-level entry point for the October 2026 FeralRF validation campaign. Two CatSniffer V3 devices were used as transmit and receive endpoints to evaluate the IEEE 802.15.4 raw path, FeralRF TX operations, and proprietary-PHY peer delivery. The summary separates physical OTA observations from device, control/API, static, inferred, and unevaluated evidence; it does not replace the detailed reports.

## Baseline

| Item | Evaluated reference and limitation |
| --- | --- |
| Hardware | Two CatSniffer V3 devices with RP2040 and CC1352P7; campaign Cat-Bridge endpoints were `COM33` and `COM88`. The boards reported different RP2040 firmware builds (`v3.1.0.0` and `v3.1.0.1`). |
| FeralRF source | Audited branch `main`, commit `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`. |
| Host package | Python package `feralrf` `0.3.0`. |
| TI SDK | `simplelink_cc13xx_cc26xx_sdk_8_30_01_01` (SDK submodule commit `5b31d0a4903351e544546e23ef3330eaa4291ceb`). |
| Installed CC1352 firmware | Both devices reported `feralrf_cc1352 (custom)`, firmware `1.0.0`, capabilities `0x07`. This reported version does **not** establish exact binary equivalence with the audited source commit. |
| Principal RF setup | IEEE 802.15.4 channel 25 (nominal 2475 MHz) for the demonstrated peer OTA path; proprietary presets were requested at 868, 902.2, 915, and 2440 MHz. Shell band state is device/control evidence, not GPIO, switch, or RF-path readback. |

Evidence labels follow the campaign matrix: A = direct physical RF observation; B = direct device/firmware behavior; C = control/API evidence; D = static implementation or documentation; E = inference; F = not evaluated. Status and evidence level are independent.

## Validation summary

| Area | Status | What the evidence establishes |
| --- | --- | --- |
| IEEE RAW | **PASS-RF within tested scope** | Known bytes were physically received with valid reported CRC in both CatSniffer directions on channel 25, historically and in the final synchronized repeat (A/B/C). This demonstrates the tested raw IEEE path, not range, power, sensitivity, PER, or a higher-layer protocol. |
| IEEE FRAME | **PASS-RF within tested scope** | Ten requested frames were physically observed in the tested `COM33` to `COM88` direction (A/C). Static analysis shows `TX_FRAME` delegates to the RAW implementation, so this validates the FRAME API contract, not an independent RF backend (D). |
| IEEE BURST | **PARTIAL** | Scheduling/API acceptance was observed and one attributable reception occurred in each `count=5` trial, including the host-open variant (A/C). The requested five physical receptions, actual emitted count, timing, and cause of the shortfall were not demonstrated. |
| IEEE CONTINUOUS, `interval_us=0` | **PASS-RF for repetition; PARTIAL overall** | Repeated attributable OTA receptions were physically observed, including 30 hits in the initial coordinated run and repeated hits in directed runs (A/B/C). Exact transmit count, rate, full-duration periodicity, and the complete RX-control gate were not validated. |
| IEEE CONTINUOUS, positive interval | **PARTIAL** | With `interval_us=250000`, one attributable packet was received against a five-hit criterion (A/C). Positive-interval repetition and timing were not validated. |
| IEEE STOP | **PASS-CONTROL; NOT FULLY VALIDATED at RF** | `TX_STOP` ACKs and later clean windows without the marker were observed (A/B/C), but buffering/backlog invalidated host arrival time as an RF clock. Immediate cessation causally attributable to STOP was not demonstrated. |
| Post-CONTINUOUS session behavior | **PARTIAL / INCONCLUSIVE** | Post-STOP RAW requests were accepted but not observed within loaded sessions, and `RX_STOP` timeouts recurred. A new session without reset recovered RAW at 9/10 and clean `RX_STOP`; the responsible component remains unknown. |
| Proprietary PHY peer campaign | **FAIL receiver criterion; physical cause INCONCLUSIVE** | The matrix confirms 24 runs, 240 successful `transmit()` returns, 19 unique presets, and zero delivered peer marker packets. This fails the tested peer-reception criterion (A-negative/C), but does not establish 240 RF emissions, “100% packet loss,” physical TX, physical proprietary RX, effective modulation, or root cause. |
| Zigbee stack | **NOT VALIDATED** | FeralRF received IEEE 802.15.4 traffic in a setup containing a source described as Zigbee, with limited source attribution. No Zigbee NWK/APS/ZCL, security, joining, or interoperability validation was performed. |
| Other named technologies / BLE | **DEFERRED or NOT VALIDATED** | Preset names do not establish W-MBus, Wi-SUN, Sidewalk, Thread, Matter, MIOTY, or other stack interoperability. Marked two-CatSniffer BLE peer OTA was not executed; Tier-C proprietary cases were blocked or deferred under the documented conditions. |

## Key findings

- The project is **partially validated**, not globally “working” or “not working.” The tested IEEE RAW, FRAME, and zero-interval repetition cases have direct RF evidence; the other TX behaviors have narrower results.
- The successful IEEE 2.4 GHz tests do not validate proprietary 2.4 GHz or the Sub-GHz path. The proprietary sweep failed peer reception but did not establish absent emission or global CatSniffer/FeralRF incompatibility.
- BURST/positive-interval explanations and the STOP lifecycle/queue explanation remain hypotheses. Static analysis establishes shared RAW dispatch and also prevents treating named MSK/4FSK/4GFSK presets as proof of those physical modulations.

## Limitations

- An ACK or successful `transmit()` return proves control-path acceptance, not RF emission or successful RF completion.
- Reported firmware version `1.0.0` does not tie the installed binary to source commit `0178721...`.
- The directed STOP work exposed substantial buffering between receiver timestamps and host delivery. Host print order cannot establish RF order around `TX_STOP`, and no independent RF timing reference completed that proof.
- Proprietary RF energy/waveform, CTF GPIO levels, U2 selection, and J1 output were not measured independently; physical TX/RX and root cause remain inconclusive.
- A received raw IEEE packet establishes OTA reception under the tested conditions only. It does not validate Zigbee or any other higher-layer protocol stack; likewise, a protocol-named preset establishes no interoperability.
- The final session changed board 1's Shell-reported `uart_overrun` counter from 0 to 50, with no intermediate reading. This does not invalidate the literal successful OTA receptions, but it prevents a claim that the broader bridge path was loss-free and cannot be assigned as the cause of the proprietary result.
- Some historical command/stdout records were incomplete; one STOP helper used an invalid timing assumption; standard observers can omit `RxStreamError`. The command audit limits each affected result.

## Conclusion

FeralRF demonstrated a functioning IEEE 802.15.4 OTA path for specific tested operations on the evaluated two-CatSniffer setup: RAW in both directions, FRAME in one direction through the shared RAW backend, and repeated CONTINUOUS reception at `interval_us=0`. BURST, positive-interval CONTINUOUS, post-CONTINUOUS session behavior, and immediate physical cessation after STOP remain only partially validated or inconclusive.

The proprietary campaign did not satisfy its peer-reception acceptance criterion across 24 runs and 19 unique presets. The available evidence does not determine whether the failure lies in physical transmission, effective RF configuration, front-end selection, reception/framing, host delivery, or another shared part of the proprietary path. Further preset sweeping would not resolve that uncertainty without independent physical observation or targeted instrumentation.

## Supporting documents

- [Execution report](execution_report.md) — authoritative current campaign state, final repeated IEEE results, provenance, limitations, and conclusions.
- [Canonical validation matrix (Markdown)](validation_matrix_20261012_113056.md) — decision-focused view of the 47 canonical validation rows and their evidence/status classifications.
- [Canonical validation matrix (CSV)](validation_matrix_20261012_113056.csv) — complete machine-readable matrix with all 57 recorded fields.
- [Historical command audit](historical_command_audit.md) — implementation checks, command correctness, incomplete evidence, and unsafe/obsolete helper findings.
- [Reproduction procedure](procedure.md) — test design, acceptance criteria, evidence model, and safe execution sequence.
- [Experiment 01: IEEE RAW, FRAME, and BURST](experiments/01_ieee_raw_frame_burst.md) — primary observations for bidirectional RAW, one-direction FRAME, and partial BURST behavior.
- [Experiment 02: IEEE CONTINUOUS and STOP](experiments/02_ieee_continuous_stop.md) — coordinated interval-zero and positive-interval results, post-STOP windows, and the initial RX-control anomaly.
- [Experiment 03: proprietary GFSK 868/915 MHz](experiments/03_proprietary_gfsk_868_915.md) — first four proprietary runs and the initial Sub-GHz interpretation.
- [Experiment 04: extended proprietary campaign](experiments/04_proprietary_extended_campaign.md) — accumulated 24-run/19-preset results, concurrent controls, and conservative physical conclusions.
- [Experiment 05: directed IEEE CONTINUOUS/STOP](experiments/05_ieee_continuous_stop_directed.md) — continuous-RX STOP investigation, buffering limitation, post-STOP anomaly, and recovery evidence.
