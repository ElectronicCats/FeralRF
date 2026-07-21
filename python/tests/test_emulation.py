"""F17: Unit tests for device emulation payloads + personalities.

F13 retro-fill seeded ANKER_PAYLOAD + FASTPAIR_DISCOVERABLE fixtures here.
F17 vuelta 1 (this commit family) implemented `feralrf.emulation` and added
TestIeee154Personalities / TestSub1GhzPersonalities / TestOokPersonalities
classes below.
"""

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


class TestSub1GhzPersonalities:
    """F17 — Sub-1GHz device personalities (S1-S3)."""

    def test_gfsk_868_sensor_uses_existing_preset(self):
        from feralrf.emulation import GFSK_868_SENSOR
        from feralrf.presets import PROP_PRESETS

        assert GFSK_868_SENSOR.preset_name in PROP_PRESETS
        assert GFSK_868_SENSOR.preset_name == "gfsk_868_50k"

    def test_gfsk_433_sensor_uses_existing_preset(self):
        from feralrf.emulation import GFSK_433_SENSOR
        from feralrf.presets import PROP_PRESETS

        assert GFSK_433_SENSOR.preset_name == "gfsk_433_50k"
        assert GFSK_433_SENSOR.preset_name in PROP_PRESETS

    def test_wmbus_uses_msk_868_fallback(self):
        """W-MBus T1 specific preset doesn't exist; fallback to msk_868_50k."""
        from feralrf.emulation import WMBUS_T1_METER
        from feralrf.presets import PROP_PRESETS

        assert WMBUS_T1_METER.preset_name == "msk_868_50k"
        assert WMBUS_T1_METER.preset_name in PROP_PRESETS

    def test_sub1ghz_personalities_count(self):
        from feralrf.emulation import SUB1GHZ_PERSONALITIES

        assert len(SUB1GHZ_PERSONALITIES) == 3

    def test_sub1ghz_personalities_payloads_well_formed(self):
        from feralrf.emulation import SUB1GHZ_PERSONALITIES

        for p in SUB1GHZ_PERSONALITIES:
            assert isinstance(p.payload, bytes)
            assert len(p.payload) >= 4


class TestOokPersonalities:
    """F17 — OOK device personalities (O1-O3)."""

    def test_pt2262_garage_uses_ook_preset(self):
        from feralrf.emulation import PT2262_GARAGE_433
        from feralrf.presets import PROP_PRESETS

        assert PT2262_GARAGE_433.preset_name == "ook_433_4k8"
        assert PT2262_GARAGE_433.preset_name in PROP_PRESETS

    def test_ev1527_sensor_uses_lower_rate_preset(self):
        from feralrf.emulation import EV1527_SENSOR_433
        from feralrf.presets import PROP_PRESETS

        assert EV1527_SENSOR_433.preset_name == "ook_433_2k4"
        assert EV1527_SENSOR_433.preset_name in PROP_PRESETS

    def test_hormann_garage_uses_868_preset(self):
        from feralrf.emulation import HORMANN_GARAGE_868
        from feralrf.presets import PROP_PRESETS

        assert HORMANN_GARAGE_868.preset_name == "ook_868_4k8"
        assert HORMANN_GARAGE_868.preset_name in PROP_PRESETS

    def test_ook_personalities_count(self):
        from feralrf.emulation import OOK_PERSONALITIES

        assert len(OOK_PERSONALITIES) == 3

    def test_ook_payloads_have_expected_lengths(self):
        """PT2262/EV1527 = 24-bit codes (3 bytes); Hörmann = 64-bit (8 bytes)."""
        from feralrf.emulation import EV1527_SENSOR_433, HORMANN_GARAGE_868, PT2262_GARAGE_433

        assert len(PT2262_GARAGE_433.payload) >= 3
        assert len(EV1527_SENSOR_433.payload) >= 3
        assert len(HORMANN_GARAGE_868.payload) >= 8


# F17 implementations replace the placeholder skip stubs that lived here pre-F17.
