"""F13: Unit tests for device emulation payloads.

Emulation as a module (`feralrf.emulation`) is an F17 deliverable. For now
this file validates the byte structure of the captured payloads used in
the lab demos (demo_emulate_soundcore.py) so that future emulation work
inherits a regression gate.
"""

import pytest

# Captured from real Soundcore Boom 2 — see demo_emulate_soundcore.py
ANKER_PAYLOAD = bytes.fromhex("02010a0505daf57b010ffff42b7d355a0e0000000000000000")
FASTPAIR_DISCOVERABLE = bytes.fromhex("020af606162cfe8f95f8")


class TestSoundcorePayloads:
    """Soundcore Boom 2 Fast Pair Model ID = 0x8F95F8."""

    def test_anker_payload_size(self):
        # 25 bytes total (includes Anker Manufacturer Specific block)
        assert len(ANKER_PAYLOAD) == 25

    def test_anker_starts_with_flags_ad(self):
        assert ANKER_PAYLOAD[0] == 0x02
        assert ANKER_PAYLOAD[1] == 0x01
        # Flags 0x0A = LE General Discoverable + BR/EDR Not Supported
        assert ANKER_PAYLOAD[2] == 0x0A

    def test_fastpair_payload_size(self):
        # TX Power AD (3) + Service Data AD with model ID (7) = 10 bytes
        assert len(FASTPAIR_DISCOVERABLE) == 10

    def test_fastpair_starts_with_tx_power(self):
        assert FASTPAIR_DISCOVERABLE[0] == 0x02
        assert FASTPAIR_DISCOVERABLE[1] == 0x0A
        # 0xF6 = -10 dBm (signed)
        assert FASTPAIR_DISCOVERABLE[2] == 0xF6

    def test_fastpair_service_data_uuid_fe2c(self):
        # AD: [len=6][type=0x16][uuid LE: 2C FE][model_id BE: 8F 95 F8]
        assert FASTPAIR_DISCOVERABLE[3] == 0x06
        assert FASTPAIR_DISCOVERABLE[4] == 0x16
        assert FASTPAIR_DISCOVERABLE[5] == 0x2C
        assert FASTPAIR_DISCOVERABLE[6] == 0xFE

    def test_fastpair_soundcore_model_id(self):
        # Model ID 0x8F95F8 in big-endian
        assert FASTPAIR_DISCOVERABLE[7] == 0x8F
        assert FASTPAIR_DISCOVERABLE[8] == 0x95
        assert FASTPAIR_DISCOVERABLE[9] == 0xF8


# F17 vuelta 1 — Device emulation


class TestBlePersonalities:
    """F17 — BLE peripheral personalities (P1-P3)."""

    def test_soundcore_boom_2_present(self):
        from feralrf.emulation import SOUNDCORE_BOOM_2

        assert SOUNDCORE_BOOM_2.name == "Soundcore Boom 2"

    def test_soundcore_boom_2_uses_pinned_payload(self):
        """Soundcore identity = ANKER_PAYLOAD + FASTPAIR_DISCOVERABLE."""
        from feralrf.emulation import SOUNDCORE_BOOM_2

        assert SOUNDCORE_BOOM_2.advertising_payload == (ANKER_PAYLOAD + FASTPAIR_DISCOVERABLE)

    def test_apple_airpods_pro_uses_apple_mfg(self):
        """AirPods identity = Apple Mfg ID 0x004C + Proximity payload."""
        from feralrf.emulation import APPLE_AIRPODS_PRO

        payload = APPLE_AIRPODS_PRO.advertising_payload
        idx = payload.find(b"\xff")
        assert idx > 0, "No 0xFF (Mfg Specific Data) AD type in payload"
        assert payload[idx + 1 : idx + 3] == b"\x4c\x00", "Apple Mfg ID 0x004C mismatch"

    def test_google_fastpair_uses_uuid_fe2c(self):
        """Google Fast Pair identity = Service Data UUID 0xFE2C."""
        from feralrf.emulation import GOOGLE_FASTPAIR_GENERIC

        payload = GOOGLE_FASTPAIR_GENERIC.advertising_payload
        idx = payload.find(b"\x16\x2c\xfe")
        assert idx > 0, "No Service Data 0xFE2C AD in payload"

    def test_ble_personalities_count(self):
        from feralrf.emulation import BLE_PERSONALITIES

        assert len(BLE_PERSONALITIES) == 3

    def test_ble_personalities_have_str_target_mac(self):
        """target_mac es string AA:BB:... porque adv_spoof lo recibe así."""
        from feralrf.emulation import BLE_PERSONALITIES

        for p in BLE_PERSONALITIES:
            assert isinstance(p.target_mac, str)
            assert len(p.target_mac.split(":")) == 6


class TestIeee154Personalities:
    """F17 — IEEE 802.15.4 device personalities (I1-I2)."""

    def test_beacon_coordinator_present(self):
        from feralrf.emulation import BEACON_COORDINATOR

        assert BEACON_COORDINATOR.name == "Beacon-enabled coordinator"

    def test_beacon_coordinator_uses_channel_15(self):
        from feralrf.emulation import BEACON_COORDINATOR

        assert BEACON_COORDINATOR.channel == 15

    def test_beacon_coordinator_pan_id(self):
        from feralrf.emulation import BEACON_COORDINATOR

        assert BEACON_COORDINATOR.pan_id == 0x1234
        assert BEACON_COORDINATOR.short_addr == 0x0001

    def test_data_poll_end_device(self):
        from feralrf.emulation import DATA_POLL_END_DEVICE

        assert DATA_POLL_END_DEVICE.name == "Data-poll end device"
        assert DATA_POLL_END_DEVICE.short_addr == 0x0042

    def test_ieee154_personalities_count(self):
        from feralrf.emulation import IEEE154_PERSONALITIES

        assert len(IEEE154_PERSONALITIES) == 2

    def test_ieee154_personalities_have_payload(self):
        from feralrf.emulation import IEEE154_PERSONALITIES

        for p in IEEE154_PERSONALITIES:
            assert isinstance(p.payload, bytes)
            assert len(p.payload) >= 3  # at least FCF (2) + seq (1)


# F17 implementations replace the placeholder skip stubs that lived here pre-F17.
