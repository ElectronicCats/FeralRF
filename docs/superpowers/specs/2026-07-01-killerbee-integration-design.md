# KillerBee Integration (CatSniffer as a KillerBee device) — Design Spec

**Date:** 2026-07-01
**Branch:** feature/killerbee-integration (to be cut from `main`)
**Base commit:** 979eee2

## Problem

The CatSniffer (CC1352P7) should be usable as a device inside the [KillerBee](https://github.com/riverloopsec/killerbee) IEEE 802.15.4/Zigbee security framework, so KillerBee tools (`zbwireshark`, `zbdump`, `zbstumbler`, `zbreplay`, `zbassocflood`, `zbid`) run against it — in particular to drive a forced-leave/rejoin on the operator's *own* network and capture the key transport for auditing.

## Key finding: the firmware already does what KillerBee needs

FeralRF's IEEE 802.15.4 path (`radio_if.c`, `smartrf_ieee_15_4_0.c`) is already a full sniffer/injector — **no firmware changes are required for v1**:

- **Promiscuous RX**: `frameFiltOpt.frameFiltEn = 0`, `autoAckEn = 0`, all frame types accepted (`bAcceptFt0..7 = 1`).
- **Bad-FCS frames delivered**: `rxConfig.bAutoFlushCrc = 0`; RX processor sets `pkt.crc_ok = (corrcrc & 0x80) == 0` (radio_if.c:1549). Corrupt frames arrive flagged — needed for a faithful audit.
- **Per-frame RSSI + CORR/CRC + timestamp** appended.
- **Raw TX**: `RadioIF_transmitIeee154Raw()` → `CMD_IEEE_TX`, ≤125B, `txOpt.bIncludeCrc = 0` (RF core appends a valid FCS).
- **Channels 11–26.**
- Exposed on the host as `feralrf.Radio`: `set_phy(PHY.IEEE_802_15_4, ch)`, `set_channel`, `start_rx`, `read_packets` (→ `Packet{timestamp_us, channel, rssi_dbm, lqi, crc_ok, data}`), `transmit_frame`, `start_jam`/`stop_jam`, `list_devices`.

## Scope

**In (v1, host-only):** a KillerBee device driver that wraps `feralrf.Radio`, mapping KillerBee's device interface onto the existing API. Capabilities: `SNIFF`, `SETCHAN`, `INJECT`, `FREQ_2400`, `PHYJAM`.

**Decisions (confirmed by operator, 2026-07-01):**
- Adapter lives **inside FeralRF** as `feralrf/integrations/killerbee.py` (new subpackage). KillerBee is an **optional** dependency (`pip install feralrf[killerbee]`), imported lazily so core FeralRF never hard-depends on it.
- **Jamming is wired**: `jammer_on/off` → `Radio.start_jam/stop_jam` (FeralRF-experimental), and `PHYJAM` is advertised.

**Out:** any firmware change; bad-FCS *injection* (RF core appends valid FCS — deferred); reactive jamming; sub-GHz/other pages; upstreaming a maintained `dev_*.py` into KillerBee (only a documented one-line shim is provided).

## Architecture

```
KillerBee tools (zbdump/zbwireshark/…)
        │  KillerBee device interface (sniffer_on/pnext/inject/…)
        ▼
feralrf/integrations/killerbee.py   ← KillerBeeFeralRF adapter (this work)
        │  feralrf.Radio public API
        ▼
feralrf.Radio  →  COBS/CRC16  →  RP2040 passthrough  →  CC1352 radio_if.c (CMD_IEEE_RX/TX)
```

### Adapter interface mapping

| KillerBee method | FeralRF call |
|---|---|
| `__init__(dev)` | `Radio(port=dev).init()`; set capabilities |
| `set_channel(ch, page=0)` | `set_phy(PHY.IEEE_802_15_4, ch)` + `set_channel(ch)`; validate 11–26 |
| `sniffer_on(ch=None, page=0)` | set PHY/channel, `start_rx()` |
| `pnext(timeout=100)` | `read_one_packet(timeout/1000)` → KB dict `{0,1,2,bytes,validcrc,rssi,dbm,location,datetime}` or `None` |
| `sniffer_off()` | `stop_rx()` |
| `inject(pkt, channel=None, count=1, delay=0, page=0)` | set channel if given; `transmit_frame(mpdu)` ×count (strip trailing FCS if present) |
| `jammer_on(ch=None, page=0)` | `start_jam(channel=ch)` |
| `jammer_off()` | `stop_jam()` |
| `get_capabilities/check_capability/get_dev_info/close` | cached caps / `DeviceInfo` / `disconnect()` |
| detection | `Radio.list_devices()` (already matches Cat-Bridge CDC, VID 0x1209) |

### One required host-side change (FeralRF, not firmware)

KillerBee's `pnext()` is one-packet-at-a-time; `feralrf` exposes RX as a `read_packets(timeout)` stream. Add `Radio.read_one_packet(timeout) -> Optional[Packet]` (returns the next `Packet`, skips `RxStreamError`, `None` on timeout), refactoring the existing packet-parse out of `read_packets` into `Radio._parse_rx_packet(payload)` so both share it (DRY). No wire-protocol change.

## Testing

- **Unit (no hardware):** `read_one_packet` against `FakeSerial` (existing pattern in `test_radio_strict_responses.py`); the adapter against a `FakeRadio` stub (dependency-injected). Assert the `pnext` dict shape, channel validation, inject framing (FCS stripped, count), jam mapping.
- **Hardware (`-m hardware`):** sniff PAN `0x58bb`, compare to `smoke_phy4_ieee154.py` and the audit baseline `zigbee_pollo.pcapng`; inject a beacon-request and confirm on air; end-to-end forced-leave → rejoin capture → audit decrypt workflow.

## Scope/legal note

Operator's own network/devices only. Jamming is wired per operator request but remains FeralRF-experimental and legally restricted in many jurisdictions (see README warning); it corrupts frames, so it is not the preferred path for a clean key-transport capture (a controlled re-pair is).
