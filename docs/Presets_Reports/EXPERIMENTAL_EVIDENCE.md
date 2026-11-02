# Experimental evidence supporting the preset evaluation

Updated: 2026-10-26.

## Evidence rules

- Raw communication means identifiable application bytes were received by a second CatSniffer.
- TX ACK proves command acceptance/enqueue, not RF completion or radiation.
- Requested frequency and modulation are not independent physical measurements.
- Raw L2 does not prove protocol framing, stack behavior, or interoperability.
- Proprietary metadata is not used as acceptance evidence because its parser is defective.

## Latest controlled CP4–CP8 campaign

Direction was CatSniffer A to CatSniffer B. Each stage used a unique run ID and packet sequence 1–10. Requested TX power was 0 dBm. Interfaces were identified by USB serial and MI number rather than COM arithmetic.

| CP | Preset | TX ACK | Unique peer payload | Cleanup | Interpretation |
|---|---|---:|---:|---|---|
| CP4 | P09 `gfsk_868_50k` | 10/10 | 10/10 | Healthy; `RX_STOP` ACK | `PASS — L2 raw only` |
| CP5 | P27 `mioty_868_tsunb` | 10/10 | 0/10 | Healthy; `RX_STOP` ACK | Raw peer-delivery FAIL; not MIOTY/TS-UNB FAIL |
| CP6 | P09, no reset/power-cycle after P27 | 10/10 | 10/10 | Healthy; `RX_STOP` ACK; one optional post-stop stats timeout recorded | Post-P27 recovery PASS |
| CP7 | P11 `ook_868_4k8` | 1 ACK, then timeout/errors | 0/10 | `RX_STOP` timeout; RF state ambiguous | OOK lifecycle `BLOCKED` |
| CP8 | P09 after complete physical power-cycle | 10/10 | 10/10 | Healthy; `RX_STOP` ACK | Recovery PASS |

`P09 PASS → P27 raw FAIL → P09 PASS → P11 OOK BLOCKED → power-cycle → P09 PASS`.

### Interpretation

- P09 is reproducibly functional for identifiable raw L2 delivery.
- P27 did not leave persistent poisoning detectable by the immediate P09 control.
- P11 shows a repeatable lifecycle/cleanup failure pattern.
- Complete physical power removal restores the known-good P09 raw GFSK path.
- The sequence does not prove physical frequency, GFSK, OOK, deviation, bandwidth, or output power.
- P27 is not a TS-UNB implementation, so its raw result is not a MIOTY result.

## Current raw-L2 coverage

Identifiable peer payload has been observed for 14 exact proprietary presets:

- P09, P10;
- P13–P15;
- P20, P21;
- P24–P26;
- P28–P31.

For protocol-labelled presets, this demonstrates only common TI proprietary transport.

Tracked repository evidence includes:

- [October 6–12 execution report](../OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/execution_report.md)
- [October 15 recovery run](../OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_RECOVERY_RUN.md)
- [October 15–16 validation matrix](../OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/VALIDATION_MATRIX_261016)
- [Historical aggregate validation matrix](../VALIDATION_MATRIX.md)

Those reports preserve their original procedures and outputs. Where they used repeated markers, they demonstrate identifiable delivery but not exact per-attempt PDR.

## Historically important contradictions

| Area | Historical record | Current conclusion |
|---|---|---|
| OOK 868 | Older positive aggregate claim versus two later controlled first-ACK/timeout/failure/cleanup-failure sequences | Historical discrepancy remains unresolved; current controlled behavior is repeatable lifecycle failure |
| Proprietary 2.4 | Older positive claim versus later no-peer result and current source-confirmed 868 fallback | Historical effective frequency is unknown; there is no valid current 2.4 GHz validation |
| Early proprietary campaign | 240 TX returns and 0 peer, followed later by L2 results for 14 exact presets | Cause is unresolved; later evidence supports current raw L2 capability for those exact presets |
| F29 Sidewalk/Wi-SUN set | Positive aggregate claim, later negative subset, then recovery and per-preset positives | Current L2 statements rely on later identifiable delivery, not on averaging the history |
| 433 MHz | Marginal, negative, and positive claims appear under incompletely comparable hardware conditions | Presets remain catalogued but outside the current scope |

## Experimental metadata warning

Historical reports may contain `crc_ok=True`, RSSI, timestamps, channel values, or LQI. These are preserved as historical software outputs.

Current source establishes that proprietary metadata is parsed in the wrong order. Therefore:

- current L2 conclusions rely on identifiable application bytes;
- `crc_ok` is not independent CRC evidence;
- RSSI and timestamp are not RF measurements;
- channel is software state;
- LQI is zero/missing.

## Validation limits

| Level | Current proprietary status |
|---|---|
| L0 | 31 presets catalogued |
| L1 | Configuration/control evidence exists |
| L2 raw | 14 exact presets |
| L3 | None |
| L4 | None |

The raw IEEE 802.15.4 evidence uses a separate backend and does not validate any `PROP_PRESET`.

## Reproducibility limits

- Historical installed firmware images are not durably tied to source hashes.
- `FERALRF1` is a logical firmware identifier, not a unique hardware identifier.
- Logical band state does not prove physical switch/frontend routing.
- No independent spectrum/IQ record establishes actual frequency or modulation.
- The detailed CP4–CP8 values required for the current conclusions are reproduced above; this document does not depend on an external notebook or untracked artifact directory.
