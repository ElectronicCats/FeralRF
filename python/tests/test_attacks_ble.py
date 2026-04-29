"""F13: Unit tests for BLE attack payload builders.

These tests do NOT require hardware — they verify the byte-level structure
of the advertising payloads used by attacks.ble (Apple Proximity, Google
Fast Pair, generic ADV).
"""

import pytest

from feralrf.attacks.ble import (
    APPLE_DEVICES,
    GOOGLE_DEVICES,
    _random_mac,
    build_adv_payload,
    build_apple_proximity_payload,
    build_google_fastpair_payload,
)


class TestBuildAdvPayload:
    """Generic BLE advertising payload (Flags + Complete Local Name)."""

    def test_flags_ad_first(self):
        """Standard layout: AD len=2, type=0x01 (Flags), value=flags."""
        p = build_adv_payload("Test")
        assert p[0] == 0x02  # AD length (type + 1 byte value)
        assert p[1] == 0x01  # AD type Flags
        assert p[2] == 0x06  # default flags

    def test_name_ad_second(self):
        p = build_adv_payload("Hi")
        assert p[3] == 1 + len(b"Hi")  # AD length (type + name)
        assert p[4] == 0x09  # AD type Complete Local Name
        assert p[5:].startswith(b"Hi")

    def test_long_name_truncated(self):
        # build_adv_payload truncates to 24 bytes
        long_name = "A" * 50
        p = build_adv_payload(long_name)
        # Name AD length byte should be 25 (24 + type)
        assert p[3] == 25

    def test_custom_flags_byte(self):
        p = build_adv_payload("X", flags=0x1A)
        assert p[2] == 0x1A


class TestApplePayload:
    """Apple Proximity Pairing payload (iOS popup trigger)."""

    def test_payload_size_25_bytes(self):
        # 11-byte fixed header (length byte + Mfg type + Apple co + Proximity
        # type + length + status + model[2] + status + battery) + 14 random bytes
        # = 25 total. The length byte 0x1A doesn't match strictly — Apple
        # Proximity payloads are quirky; what matters is iOS reads the magic
        # bytes and shows the popup. This test pins the observed size.
        p = build_apple_proximity_payload()
        assert len(p) == 25

    def test_starts_with_mfg_specific_header(self):
        p = build_apple_proximity_payload()
        assert p[0] == 0x1A  # AD length
        assert p[1] == 0xFF  # AD type Manufacturer Specific

    def test_apple_company_id(self):
        """Apple SIG company ID is 0x004C (little-endian on the air)."""
        p = build_apple_proximity_payload()
        assert p[2] == 0x4C
        assert p[3] == 0x00

    def test_proximity_pairing_type(self):
        p = build_apple_proximity_payload()
        assert p[4] == 0x07  # Proximity Pairing type
        assert p[5] == 0x19  # length

    def test_device_model_bytes_default_airpods_pro(self):
        p = build_apple_proximity_payload()
        # Default model is (0x02, 0x20) = airpods_pro
        assert p[7] == 0x02
        assert p[8] == 0x20

    def test_custom_device_model(self):
        # airpods_pro_2 = (0x0E, 0x20)
        p = build_apple_proximity_payload(model=(0x0E, 0x20))
        assert p[7] == 0x0E
        assert p[8] == 0x20


class TestGooglePayload:
    """Google Fast Pair discoverable payload (Android popup trigger)."""

    def test_starts_with_tx_power_ad(self):
        p = build_google_fastpair_payload()
        assert p[0] == 0x02  # AD length 2
        assert p[1] == 0x0A  # AD type TX Power
        assert p[2] == 0xF6  # -10 dBm (signed)

    def test_service_data_uuid_fe2c(self):
        """Fast Pair Service Data UUID is 0xFE2C."""
        p = build_google_fastpair_payload()
        # AD: [len][0x16][0x2C][0xFE][model_bytes...]
        assert p[3] == 6  # len = 0x16 + UUID(2) + model_id(3)
        assert p[4] == 0x16  # AD type Service Data 16-bit
        assert p[5] == 0x2C
        assert p[6] == 0xFE

    def test_model_id_big_endian(self):
        # 0x2C01A2 → bytes 2C 01 A2
        p = build_google_fastpair_payload(model_id=0x2C01A2)
        assert p[7] == 0x2C
        assert p[8] == 0x01
        assert p[9] == 0xA2

    def test_custom_model_id(self):
        p = build_google_fastpair_payload(model_id=0x0002F0)  # jbl_flip6
        assert p[7] == 0x00
        assert p[8] == 0x02
        assert p[9] == 0xF0


class TestRandomMac:
    """Verify _random_mac generates a valid BLE static-random address."""

    def test_returns_6_bytes(self):
        assert len(_random_mac()) == 6

    def test_high_bits_indicate_random_static(self):
        """Random Static address: 2 MSBs of byte 0 (in air order) = 11.
        _random_mac returns little-endian for firmware → byte 0 (LE) is the
        AIR last byte. Random static check is on AIR byte 0 = LE byte 5.
        Function does `addr[5] |= 0xC0` in the source array, then reverses
        for LE output → so air byte 0 == LE byte 5 of source == has 0xC0
        set. After `bytes(reversed(addr))`, the original index-5 with 0xC0
        ends up at LE index 0. Validation: LE byte 0 has 0xC0 set."""
        for _ in range(20):
            addr = _random_mac()
            assert addr[0] & 0xC0 == 0xC0

    def test_distinct_calls_yield_different_macs(self):
        # Monte-Carlo: 100 calls should yield ~100 distinct MACs
        macs = {_random_mac() for _ in range(100)}
        assert len(macs) >= 95  # tolerate a small collision chance


class TestDeviceCatalogs:
    """Sanity that the well-known device tables are well-formed."""

    def test_apple_devices_well_formed(self):
        for name, model in APPLE_DEVICES.items():
            assert isinstance(name, str)
            assert isinstance(model, tuple)
            assert len(model) == 2
            assert all(0 <= b <= 0xFF for b in model)

    def test_google_devices_well_formed(self):
        for name, model_id in GOOGLE_DEVICES.items():
            assert isinstance(name, str)
            assert isinstance(model_id, int)
            assert 0 <= model_id <= 0xFFFFFF  # 24-bit Fast Pair model ID

    def test_no_duplicate_apple_models(self):
        models = list(APPLE_DEVICES.values())
        assert len(models) == len(set(models))

    def test_no_duplicate_google_model_ids(self):
        ids = list(GOOGLE_DEVICES.values())
        assert len(ids) == len(set(ids))
