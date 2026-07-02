# Driving the CatSniffer from KillerBee (+ key-capture attack runbook)

This wires the CatSniffer (CC1352, via FeralRF) into the [KillerBee](https://github.com/riverloopsec/killerbee) 802.15.4/Zigbee toolkit, so `zbdump`, `zbwireshark`, `zbid`, `zbstumbler`, `zbreplay`, etc. drive it. The end goal is the audit workflow: force a device to re-join and capture the key transport.

## What was built

- **Adapter** `feralrf.integrations.killerbee.KillerBeeFeralRF` — a KillerBee device driver that wraps `feralrf.Radio` (sniff, inject, jam, `pnext`, capabilities, discovery). Ships in the FeralRF package.
- **KillerBee hooks** (small, in `docs/killerbee-catsniffer.patch`): a `dev_feralcat.py` shim (`FERALCAT = KillerBeeFeralRF`), an `iscatsniffer()` probe + `devlist()` branch in `kbutils.py`, a `DEV_ENABLE_CATSNIFFER` flag in `config.py`, and two dispatch branches in `__init__.py` (forced `hardware="feralcat"` and serial auto-detect).

Capabilities advertised: SNIFF, SETCHAN, INJECT, FREQ_2400, PHYJAM. Channels 11–26.

## Status: proven vs. needs hardware

- **Proven here (no hardware):** `feralrf` unit + integration suite is green (600 passed, 1 skipped). `test_killerbee_dispatch.py` verifies the adapter against the *real* `KBCapabilities`, the shim, `iscatsniffer`/`devlist` detection, and that `KillerBee(hardware="feralcat")` constructs our driver. `zbdump --help` loads through the integrated KillerBee.
- **Needs the bench (CatSniffer + your own network):** the actual RF — live sniff, injection on air, and the forced-rejoin/key-capture. That is the runbook below.

## Install on the Linux host

```bash
# 1. System backend for KillerBee's USB enumeration (zbid/USB dongles)
sudo apt-get install -y libusb-1.0-0 libgcrypt20-dev

# 2. FeralRF (this repo) + the KillerBee bridge extra
pip install -e /path/to/FeralRF/python        # provides feralrf + the adapter

# 3. KillerBee + its Python deps, then apply the CatSniffer hooks
git clone https://github.com/riverloopsec/killerbee
cd killerbee
git apply /path/to/FeralRF/docs/killerbee-catsniffer.patch   # adds dev_feralcat.py + hooks
pip install pyusb pyserial pycryptodome rangeparser scapy
pip install -e .            # builds zigbee_crypt (needs libgcrypt) — only for decode tools
```

Verify detection (CatSniffer plugged in):

```bash
zbid            # should list: <port>  FeralRF CatSniffer (CC1352)
```

> `zbid` needs a working libusb backend even to reach serial devices (it enumerates USB first). On Linux that is present; without it `zbid` raises `NoBackendError` before serial detection — that is the backend, not the integration.

## Using it

```bash
# auto-detect (serial probe picks the CatSniffer):
zbdump -i /dev/ttyACM0 -c 15 -w capture.pcap
# or force the driver explicitly:
zbdump -i /dev/ttyACM0 -d feralcat -c 15 -w capture.pcap
# live to Wireshark:
zbwireshark -i /dev/ttyACM0 -d feralcat -c 15
```

## Attack runbook — force re-join and capture the key transport

**Scope: your own network only** (PAN `0x58bb`, per `../../zigbee_audit_report.md`). Jamming radiates into the shared 2.4 GHz band and is legally restricted; prefer the clean re-pair.

1. **Find the channel.** `zbstumbler -i /dev/ttyACM0 -d feralcat` (or sweep 11–26 with `zbdump`) until you see beacons/traffic for PAN `0x58bb`.
2. **Start capturing the join channel:**
   `zbdump -i /dev/ttyACM0 -d feralcat -c <channel> -w rejoin.pcap`
3. **Provoke a fresh join (must be a *fresh* join, not a plain rejoin — only a fresh join re-sends the key):**
   - **Clean (recommended):** factory-reset the target device, or remove + re-pair it from your coordinator (Z2M/ZHA). Produces uncorrupted frames — best for a clean key-transport capture.
   - **Attack-style (the "recreate the attack" path):** disrupt the target so it drops its parent and re-joins — e.g. `zbassocflood`, or jamming via the adapter's `jammer_on()` (bounded to 30 s per call). Corrupts frames, so it is less reliable for a clean capture; use only to demonstrate the external-attacker scenario.
4. **Extract the network key.** In the captured join, the Trust Center sends the NWK key in an APS *Transport Key* command encrypted with the well-known default global link key `ZigBeeAlliance09` (`5A:69:67:42:65:65:41:6C:6C:69:61:6E:63:65:30:39`). Decrypt it — either with KillerBee/`zbdecode`, or tshark:
   ```bash
   tshark -r rejoin.pcap \
     -o 'uat:zbee_pc_keys:"5A:69:67:42:65:65:41:6C:6C:69:61:6E:63:65:30:39","Normal","tclk"' \
     -Y zbee_aps -V | grep -iA3 "transport key"
   ```
   If the device joined with an **install code** (unique per-device link key) instead of the default, the capture alone won't reveal the NWK key — you also need that device's install code.
5. **Finish the audit.** Feed the recovered NWK key into the earlier workflow (`catsniffer-zigbee-tshark-decode` / `zbee_pc_keys`) to decrypt `zigbee_pollo.pcapng` and read the APS/application layer.
