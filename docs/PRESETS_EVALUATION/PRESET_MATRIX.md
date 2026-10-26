# Preset implementation and validation matrix

Updated: 2026-10-26.

## Interpretation

- `L0`: exists/documented.
- `L1`: configuration/control accepted.
- `L2 raw`: identifiable application bytes received by a second FeralRF peer.
- `L3`: independent RF/framing measurement.
- `L4`: interoperability with an independent standards-conformant implementation.

`YES — L2 raw` does not mean that frequency or modulation was measured, standard framing was used, or the named protocol was implemented. No proprietary preset currently reaches L3 or L4.

All 31 rows come from [`PROP_PRESETS`](../../python/feralrf/presets.py). Supported configurations use the common TI proprietary backend and framing described in [IMPLEMENTATION_AND_PROTOCOL_GAPS.md](IMPLEMENTATION_AND_PROTOCOL_GAPS.md).

## Matrix

| ID | Preset | Intended family/protocol | Requested modulation | Requested frequency | Rate | Raw CatSniffer↔CatSniffer communication | Validation | Implementation | Main missing item |
|---|---|---|---|---:|---:|---|---|---|---|
| P01 | `gfsk_433_50k` | Raw GFSK | GFSK (1) | 433.920 MHz | 50,000 | OUT OF SCOPE | L1 historical | `OUT OF CURRENT SCOPE` | Demonstrated current 433 hardware path and L3 |
| P02 | `gfsk_433_10k` | Raw GFSK | GFSK (1) | 433.920 MHz | 10,000 | OUT OF SCOPE | L1 | `OUT OF CURRENT SCOPE` | Demonstrated current 433 hardware path and L3 |
| P03 | `fsk_433_50k` | Raw FSK | FSK (0) | 433.920 MHz | 50,000 | OUT OF SCOPE | L1 historical | `OUT OF CURRENT SCOPE` | Comparable hardware and discriminating test |
| P04 | `ook_433_4k8` | Raw OOK | OOK (2) | 433.920 MHz | 4,800 | OUT OF SCOPE | L1 historical | `OUT OF CURRENT SCOPE` | 433 hardware plus safe OOK lifecycle |
| P05 | `ook_433_2k4` | Raw OOK | OOK (2) | 433.920 MHz | 2,400 | OUT OF SCOPE | L1 | `OUT OF CURRENT SCOPE` | 433 hardware plus safe OOK lifecycle |
| P06 | `msk_433_50k` | MSK label | Reserved value 4 | 433.920 MHz | 50,000 | OUT OF SCOPE | NOT VALIDATED | `OUT OF CURRENT SCOPE` | Supported MSK backend or explicit rejection |
| P07 | `4fsk_433_50k` | 4-FSK label | Reserved value 5 | 433.920 MHz | 50,000 | OUT OF SCOPE | NOT VALIDATED | `OUT OF CURRENT SCOPE` | Supported 4-FSK backend or explicit rejection |
| P08 | `4gfsk_433_50k` | 4-GFSK label | Reserved value 6 | 433.920 MHz | 50,000 | OUT OF SCOPE | NOT VALIDATED | `OUT OF CURRENT SCOPE` | Supported 4-GFSK backend or explicit rejection |
| P09 | `gfsk_868_50k` | Raw GFSK | GFSK (1) | 868.000 MHz | 50,000 | YES — L2 raw | L2 raw | `IMPLEMENTED` | L3 physical characterization |
| P10 | `gfsk_868_100k` | Raw GFSK | GFSK (1) | 868.000 MHz | 100,000 | YES — L2 raw | L2 raw | `IMPLEMENTED` | L3 physical characterization |
| P11 | `ook_868_4k8` | Raw OOK | OOK (2) | 868.000 MHz | 4,800 | NO — controlled 0/10 | L1 / BLOCKED | `PARTIAL` | Safe lifecycle/completion/cleanup/recovery and physical OOK |
| P12 | `msk_868_50k` | MSK label | Reserved value 4 | 868.000 MHz | 50,000 | NOT VALIDATED | NOT VALIDATED | `BLOCKED BY CURRENT BACKEND` | Supported MSK backend or explicit rejection |
| P13 | `wireless_mbus_s_868` | Wireless M-Bus S | GFSK (1) | 868.300 MHz | 32,768 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | EN 13757 framing/coding/block/CRC/addressing/decoder and peer |
| P14 | `wireless_mbus_t_868` | Wireless M-Bus T | GFSK (1) | 868.950 MHz | 100,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | EN 13757 framing/coding/block/CRC/addressing/decoder and peer |
| P15 | `wireless_mbus_c_868` | Wireless M-Bus C | GFSK (1) | 868.950 MHz | 100,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | EN 13757 framing/coding/block/CRC/addressing/decoder and peer |
| P16 | `wireless_mbus_n_169_2k4` | Wireless M-Bus N | GFSK (1) | 169.450 MHz | 2,400 | OUT OF SCOPE | L1 | `OUT OF CURRENT SCOPE` | Demonstrated 169 backend/frontend plus W-MBus N |
| P17 | `wireless_mbus_n_169_4k8` | Wireless M-Bus N | GFSK (1) | 169.450 MHz | 4,800 | OUT OF SCOPE | L1 | `OUT OF CURRENT SCOPE` | Demonstrated 169 backend/frontend plus W-MBus N |
| P18 | `4fsk_868_50k` | 4-FSK label | Reserved value 5 | 868.000 MHz | 50,000 | NOT VALIDATED | NOT VALIDATED | `BLOCKED BY CURRENT BACKEND` | Supported 4-FSK backend or explicit rejection |
| P19 | `4gfsk_868_50k` | 4-GFSK label | Reserved value 6 | 868.000 MHz | 50,000 | NOT VALIDATED | NOT VALIDATED | `BLOCKED BY CURRENT BACKEND` | Supported 4-GFSK backend or explicit rejection |
| P20 | `gfsk_915_50k` | Raw GFSK | GFSK (1) | 915.000 MHz | 50,000 | YES — L2 raw | L2 raw | `IMPLEMENTED` | L3 physical characterization |
| P21 | `gfsk_902_50k` | Raw GFSK | GFSK (1) | 902.200 MHz | 50,000 | YES — L2 raw | L2 raw | `IMPLEMENTED` | L3 physical characterization |
| P22 | `gfsk_2440_250k` | Proprietary 2.4 GHz | GFSK (1) | 2440.000 MHz | 250,000 | NOT VALIDATED | NOT VALIDATED | `BLOCKED BY CURRENT BACKEND` | Valid proprietary 2.4 backend; active path falls back to 868 |
| P23 | `gfsk_2440_50k` | Proprietary 2.4 GHz | GFSK (1) | 2440.000 MHz | 50,000 | NOT VALIDATED | NOT VALIDATED | `BLOCKED BY CURRENT BACKEND` | Valid proprietary 2.4 backend; active path falls back to 868 |
| P24 | `sidewalk_915_fsk_50k` | Sidewalk-labelled FSK | FSK (0) | 915.000 MHz | 50,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | Sidewalk framing/MAC/security/stack/gateway-peer |
| P25 | `sidewalk_915_fsk_250k` | Sidewalk-labelled FSK | FSK (0) | 915.000 MHz | 250,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | Sidewalk framing/MAC/security/stack/gateway-peer |
| P26 | `wisun_915_fsk_50k` | Wi-SUN-labelled FSK | FSK (0) | **902.200 MHz** | 50,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | SUN-FSK/PHR/profile, MAC/FAN, hopping, IPv6/RPL, security, peer |
| P27 | `mioty_868_tsunb` | MIOTY-labelled | FSK (0) | 868.000 MHz | 396 | NO — controlled 0/10 | L1 | `CONFIGURATION ONLY` | TS-UNB splitting/coding/interleaving/bursts/scheduling/reassembly/service |
| P28 | `wisun_915_fsk_100k` | Wi-SUN-labelled FSK | FSK (0) | **902.200 MHz** | 100,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | SUN-FSK/PHR/profile, MAC/FAN, hopping, IPv6/RPL, security, peer |
| P29 | `wisun_915_fsk_150k` | Wi-SUN-labelled FSK | FSK (0) | **902.200 MHz** | 150,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | SUN-FSK/PHR/profile, MAC/FAN, hopping, IPv6/RPL, security, peer |
| P30 | `wisun_915_fsk_200k` | Wi-SUN-labelled FSK | FSK (0) | **902.200 MHz** | 200,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | SUN-FSK/PHR/profile, MAC/FAN, hopping, IPv6/RPL, security, peer |
| P31 | `wisun_915_fsk_300k` | Wi-SUN-labelled FSK | FSK (0) | **902.200 MHz** | 300,000 | YES — L2 raw | L2 raw | `CONFIGURATION ONLY` | SUN-FSK/PHR/profile, MAC/FAN, hopping, IPv6/RPL, security, peer |

## Key interpretation

- P09/P10/P20/P21 are implemented for their intended raw proprietary role; they are not missing a protocol stack.
- P11 has OOK configuration and framing but an incomplete/unsafe lifecycle.
- P13–P15, P24/P25, P26/P28–P31, and P27 are nominal RF configurations, not implementations of the protocols in their names.
- All Wi-SUN-labelled presets request 902.2 MHz despite `915` in their names.
- P27 is a generic proprietary FSK configuration labelled MIOTY; it is not TS-UNB. The current 0/10 result is a raw delivery result, not “MIOTY failed.”
- P22/P23 are blocked by the current backend, not by a demonstrated silicon limitation.
- P12/P18/P19 cannot support their named modulations through the active command contract.
