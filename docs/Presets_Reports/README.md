# FeralRF preset evaluation

Updated: 2026-10-26.

## Scope

This documentation evaluates the 31 current `PROP_PRESETS` using:

- the implementation at `feature/hardware-validation`, commit `f2744e400f743df30499e20f03f156f08ad15bd8`;
- TI SDK submodule `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`simplelink_cc13xx_cc26xx_sdk_8_30_01_01`);
- preserved controlled experiments;
- the CP4–CP8 campaign completed on 2026-10-26.

It separates requested configuration, actual backend and framing, raw communication, protocol-stack implementation, physical validation, and interoperability.

## Key conclusion

FeralRF currently provides configurable TI proprietary packet radio with functional raw FSK/GFSK profiles and a separate raw IEEE 802.15.4 path. Protocol-labelled presets such as Wireless M-Bus, Wi-SUN, Sidewalk, and MIOTY currently represent nominal RF configurations rather than implementations of their complete framing or protocol stacks.

The strongest proprietary evidence is L2 raw communication: identifiable application bytes received by a second CatSniffer. No proprietary preset currently reaches L3 independent physical validation or L4 interoperability.

## Current status at a glance

| Area | Current status | Safest statement |
|---|---|---|
| P09/P10/P20/P21 raw FSK/GFSK | `IMPLEMENTED`, L2 raw | Raw proprietary payload delivery works; L3 RF characterization remains |
| P11 OOK | `PARTIAL` | RF configuration exists; lifecycle/cleanup is unsafe and physical OOK is unverified |
| P13–P15 Wireless M-Bus | `CONFIGURATION ONLY`, L2 raw | Raw TI proprietary transport, not EN 13757 |
| P24/P25 Sidewalk | `CONFIGURATION ONLY`, L2 raw | Raw proprietary FSK, not Sidewalk |
| P26/P28–P31 Wi-SUN | `CONFIGURATION ONLY`, L2 raw | Raw proprietary FSK at requested 902.2 MHz settings, not Wi-SUN FAN |
| P27 MIOTY-labelled | `CONFIGURATION ONLY`, L1 | Generic 396-symbol/s proprietary FSK, not TS-UNB |
| P22/P23 proprietary 2.4 | `BLOCKED BY CURRENT BACKEND` | 2440 MHz is requested, but the active path falls back to 868 MHz |
| P12/P18/P19 | `BLOCKED BY CURRENT BACKEND` | `modType=4/5/6` are reserved in the active TI command |
| P01–P08 and P16/P17 | `OUT OF CURRENT SCOPE` | Catalogued, but current 433/169 implementation-depth conclusions are excluded |

## Important limitations

- Requested RF parameters are configuration values, not independent physical measurements.
- Raw L2 communication is not protocol interoperability.
- Proprietary RX metadata is defective: payload matching is usable, but `crc_ok`, RSSI, timestamp, channel, and LQI are not reliable RF evidence.
- TX ACK means command acceptance/enqueue, not RF completion or confirmed radiation.
- Protocol names do not select protocol-specific encoders, decoders, MACs, security, or higher stacks.

## Documents

- [PRESET_MATRIX.md](PRESET_MATRIX.md) — primary P01–P31 matrix.
- [IMPLEMENTATION_AND_PROTOCOL_GAPS.md](IMPLEMENTATION_AND_PROTOCOL_GAPS.md) — actual TX/RX framing and family-level implementation gaps.
- [EXPERIMENTAL_EVIDENCE.md](EXPERIMENTAL_EVIDENCE.md) — concise evidence supporting current statuses.
- [NEXT_WORK.md](NEXT_WORK.md) — ordered correctness, implementation, measurement, and interoperability work.

## Primary source anchors

- [Preset catalog](../../python/feralrf/presets.py)
- [Host radio API](../../python/feralrf/radio.py)
- [Command serialization](../../python/feralrf/commands.py)
- [CC command processor](../../firmware/cc1352/src/command_processor.c)
- [Control task](../../firmware/cc1352/src/control_task.c)
- [RF implementation](../../firmware/cc1352/src/radio_if.c)
- [Proprietary SmartRF setup](../../firmware/cc1352/src/smartrf_prop_0.c)
- [IEEE SmartRF setup](../../firmware/cc1352/src/smartrf_ieee_15_4_0.c)

This set is self-contained within the FeralRF repository and does not require external documentation.
