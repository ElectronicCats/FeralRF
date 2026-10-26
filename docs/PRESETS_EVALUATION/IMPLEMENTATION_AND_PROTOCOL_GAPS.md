# Implementation and protocol gaps

Updated: 2026-10-26.

## What proprietary presets actually send

All 31 [`PROP_PRESETS`](../../python/feralrf/presets.py) use one common path:

`PROP_PRESETS`
→ [`Radio.configure_prop()`](../../python/feralrf/radio.py)
→ `SET_PROP_CONFIG`
→ [`RadioIF_setPropConfig()`](../../firmware/cc1352/src/radio_if.c)
→ `CMD_PROP_RADIO_DIV_SETUP_PA`
→ `CMD_PROP_TX / CMD_PROP_RX`.

For supported modes, the effective RF frame is:

`4-byte preamble | 32-bit sync | 1-byte length | application bytes | CRC`.

- The host supplies application bytes.
- The RF Core supplies preamble, sync, length, and CRC.
- `format_conf=0` becomes `0x00A0`: 32-bit sync, MSB-first, no FEC, and no whitening.
- USB COBS framing and the USB transport CRC are not transmitted over RF.
- Protocol-labelled preset names do not select different encoders, decoders, MACs, or stacks.

## What proprietary RX returns

The RF Core data-entry layout is effectively:

`[length | application bytes | received CRC] | RSSI | timestamp[4] | status`.

Current Python packet content is:

`Packet.data = length + application bytes + received CRC`.

The application bytes remain usable for L2 payload matching.

### Known metadata defect

TI append order is:

`RSSI | timestamp[4] | status`.

[`RadioIF_processPropPackets()`](../../firmware/cc1352/src/radio_if.c) currently interprets:

`status | RSSI | timestamp[4]`.

| Field | Current reliability |
|---|---|
| Application bytes | Usable for identifiable L2 matching |
| `crc_ok` | `DEFECTIVE` |
| `rx_ok` as CRC-good count | `DEFECTIVE` |
| RSSI | `DEFECTIVE` |
| Timestamp | `DEFECTIVE` |
| Channel | Software state, not received-packet RF metadata |
| LQI | Zero/missing |

Historical metadata values remain recorded as software output but are not independent RF or CRC evidence.

## TX acknowledgement semantics

[`ControlTask_onTxRaw()`](../../firmware/cc1352/src/control_task.c) accepts/enqueues a pending TX operation. Actual RF execution occurs later in the control task.

**TX ACK = command acceptance/enqueue, not RF completion and not confirmed radiation.**

## Gap matrix by family

| Family | RF configuration | Framing | Stack | Raw communication | Current state | Main missing work |
|---|---|---|---|---|---|---|
| Raw FSK/GFSK | Supported proprietary FSK/GFSK parameters | TI variable-length proprietary frame | No higher stack required for raw role | YES for P09/P10/P20/P21 | `IMPLEMENTED` | L3 physical characterization |
| OOK | `modType=2`, genOOK patches and overrides | TI proprietary packet frame | No higher stack required for raw role | NO in current P11 controlled run | `PARTIAL` | Durable terminal state, completion-aware TX/stop, cleanup/recovery, waveform validation |
| Wireless M-Bus | Nominal S/T/C RF parameters; N catalogued | TI proprietary, not EN 13757 | No W-MBus state/decoder | YES raw for P13–P15 | `CONFIGURATION ONLY` | EN 13757 framing, coding, blocks/CRC, addressing/control, decoder, peer |
| Wi-SUN | Raw FSK; all named presets request 902.2 MHz | TI proprietary, not SUN-FSK/PHR | No MAC/FAN, IPv6/RPL, or security | YES raw for P26/P28–P31 | `CONFIGURATION ONLY` | Mode/profile, PHR, whitening/FEC as required, channel/hopping, FAN stack, peer |
| Sidewalk | Raw FSK at requested 915 MHz settings | TI proprietary, not Sidewalk | No Sidewalk MAC/security/stack | YES raw for P24/P25 | `CONFIGURATION ONLY` | Framing, MAC, authentication/security, integration, gateway/peer |
| MIOTY | Generic FSK, 868 MHz, 396 symbols/s, deviation field 1, RX BW `0x4A` | TI proprietary, not TS-UNB | No MIOTY service behavior | NO, P27 controlled 0/10 | `CONFIGURATION ONLY` | Splitting, coding, interleaving, bursts, time/frequency scheduling, IDs, reassembly, parser/generator |
| Proprietary 2.4 | 2440 MHz stored/requested | Active Sub-GHz helper applies 868 fallback | Raw role only | No valid 2.4 result | `BLOCKED BY CURRENT BACKEND` | Dedicated proprietary 2.4 setup/backend and later L3 |
| MSK/4-FSK/4-GFSK | Presets pass reserved values 4/5/6 | Result is not interpretable as named modulation | Raw role only | No valid named-modulation result | `BLOCKED BY CURRENT BACKEND` | Supported TI command/setup or explicit rejection |
| IEEE 802.15.4 | Separate IEEE setup | RF Core IEEE PHY framing/FCS around host MPDU/PSDU | Raw frame path; complete MAC absent | Raw IEEE evidence exists | Raw frame path `IMPLEMENTED`; MAC `PARTIAL` | MAC state behavior, association, filtering/auto-ACK if required |
| Zigbee | Can use IEEE radio foundation | IEEE alone is insufficient | NWK/APS/ZDO/ZCL/security absent | NOT VALIDATED | `MISSING` | Complete Zigbee stack, formation/security, and independent peer |

## Family-specific conclusions

### OOK P11

The OOK configuration, genOOK patch path, and packet framing exist. In the latest controlled run, configuration and RX start succeeded, TX #1 acknowledged, TX #2 timed out, TX #3–10 failed, no peer payload arrived, and `RX_STOP` timed out. A physical power cycle restored P09. Python warns about power cycling but does not expose or enforce a durable `requires_power_cycle` state.

### MIOTY P27

P27 requests FSK at 868 MHz and 396 symbols/s with deviation field 1, RX BW `0x4A`, and sync `0x930B51DE`. It uses ordinary proprietary packet framing.

**The current P27 preset is a generic proprietary packet-radio configuration labelled MIOTY; it is not a TS-UNB implementation.**

### Proprietary 2.4 P22/P23

The preset request is stored as 2440 MHz, but [`RadioIF_applySub1ghzChannelConfig()`](../../firmware/cc1352/src/radio_if.c) applies the 868 MHz fallback before proprietary TX/RX. This is a current backend defect, not a CC1352P7 silicon limitation.

### Reserved modulation values

The active TI `CMD_PROP_RADIO_DIV_SETUP_PA` contract defines 0=FSK, 1=GFSK, and 2=OOK; other relevant values are reserved. P12/P18/P19 therefore do not implement MSK, 4-FSK, or 4-GFSK.

## IEEE 802.15.4 and Zigbee

IEEE 802.15.4 is not a `PROP_PRESET`. FeralRF uses a separate `CMD_IEEE_TX/RX` backend configured through [`smartrf_ieee_15_4_0.c`](../../firmware/cc1352/src/smartrf_ieee_15_4_0.c). The host supplies MPDU/PSDU bytes; the RF Core handles PHY framing/FCS.

Raw IEEE frame support exists. A complete MAC is not present, and auto-ACK/filtering are disabled in the audited path.

**Zigbee is not implemented.** Missing layers include complete MAC behavior where required, NWK, APS, ZDO, ZCL/profiles, security, and network formation.

## Mini glossary

**Preset:** A predefined RF parameter set.

**Modulation:** How symbols modify the RF carrier.

**PHY:** Physical-layer rules for transmitting bits or symbols.

**Framing:** Packet structure surrounding application data.

**Payload:** Application bytes being transported.

**MAC:** Medium-access, addressing, and control layer.

**Protocol stack:** Required MAC, network, security, and application layers.

**Backend:** Concrete RF command/setup path used by FeralRF.

**Raw communication:** Identifiable application-byte transport without claiming the named protocol stack.

**Interoperability:** Communication with an independent standards-conformant implementation.

**L2 raw:** Raw payload observed between FeralRF peers.

**L3:** Independent physical RF/framing validation.

**L4:** Independent protocol interoperability.

## Additional source anchors

- [Command serialization](../../python/feralrf/commands.py)
- [CC command processing](../../firmware/cc1352/src/command_processor.c)
- [RX response serialization](../../firmware/cc1352/src/data_task.c)
- [Current link-layer pass-through](../../firmware/cc1352/src/ll_manager.c)
- [Proprietary SmartRF setup](../../firmware/cc1352/src/smartrf_prop_0.c)
- [Firmware build selection](../../firmware/cc1352/CMakeLists.txt)
- TI SDK `driverlib/rf_prop_cmd.h` and `driverlib/rf_ieee_cmd.h` under the repository SDK submodule
