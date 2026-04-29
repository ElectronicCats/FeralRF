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


class TestEmulationModuleNotYetImplemented:
    """When F17 lands, replace these skips with real tests."""

    @pytest.mark.skip(reason="feralrf.emulation is an F17 deliverable")
    def test_ble_peripheral_advertising_starts(self):
        pass

    @pytest.mark.skip(reason="feralrf.emulation is an F17 deliverable")
    def test_ieee154_device_beacon(self):
        pass

    @pytest.mark.skip(reason="feralrf.emulation is an F17 deliverable")
    def test_sub1ghz_device_payload(self):
        pass

    @pytest.mark.skip(reason="feralrf.emulation is an F17 deliverable")
    def test_ook_device_codeword(self):
        pass
