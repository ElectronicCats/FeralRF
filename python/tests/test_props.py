"""F13: Unit tests for proprietary radio presets and configure_prop wire format.

These tests do NOT require hardware — they verify the schema of PROP_PRESETS
and the byte-level encoding of CMD_SET_PROP_CONFIG.
"""

import struct

import pytest

from feralrf.commands import CommandBuilder
from feralrf.presets import PROP_PRESETS

REQUIRED_KEYS = {
    "frequency_hz",
    "mod_type",
    "symbol_rate",
    "deviation",
    "rx_bw",
    "sync_word",
}


class TestPresetsSchema:
    """Validate every entry in PROP_PRESETS conforms to the wire schema."""

    def test_presets_not_empty(self):
        assert len(PROP_PRESETS) > 0

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_required_keys_present(self, name):
        preset = PROP_PRESETS[name]
        missing = REQUIRED_KEYS - preset.keys()
        assert not missing, f"{name} missing keys: {missing}"

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_frequency_in_supported_bands(self, name):
        f = PROP_PRESETS[name]["frequency_hz"]
        # Bands derived from radio_if.c:1903-1917 loDivider table:
        # 169 / 287-359 / 359-431 / 431-527 / 779-930 / 1076-2360 / 2360+
        # PROP_PRESETS uses 169 / 433-915 / 2440 in practice
        assert (
            150_000_000 <= f <= 200_000_000
            or 400_000_000 <= f <= 970_000_000
            or 2_300_000_000 <= f <= 2_500_000_000
        ), f"{name} frequency {f} Hz outside supported bands"

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_mod_type_valid(self, name):
        # mod_type values per radio_if.c:1934-1962:
        # 0=FSK, 1=GFSK, 2=OOK, 4=MSK, 5=4-FSK, 6=4-GFSK
        assert PROP_PRESETS[name]["mod_type"] in {0, 1, 2, 4, 5, 6}

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_symbol_rate_within_radio_capability(self, name):
        # CC1352 prop radio supports 100 baud to 5 Mbaud (with overrides)
        rate = PROP_PRESETS[name]["symbol_rate"]
        assert 100 <= rate <= 5_000_000

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_rx_bw_byte_size(self, name):
        # rx_bw is 1 byte
        assert 0 <= PROP_PRESETS[name]["rx_bw"] <= 0xFF

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_sync_word_u32(self, name):
        # sync_word is u32
        sw = PROP_PRESETS[name]["sync_word"]
        assert 0 <= sw <= 0xFFFFFFFF


class TestSetPropConfigWire:
    """Wire format of CMD_SET_PROP_CONFIG payload (radio_if.c:289)."""

    def test_payload_length_18_bytes(self):
        payload = CommandBuilder.set_prop_config(
            frequency_hz=868_000_000,
            mod_type=1,
            symbol_rate=50_000,
            deviation=100,
            rx_bw=0x52,
            sync_word=0x930B51DE,
        )
        assert len(payload) == 18

    def test_payload_format_round_trip(self):
        """Pack then unpack — verify all fields preserved bit-for-bit."""
        payload = CommandBuilder.set_prop_config(
            frequency_hz=868_300_000,
            mod_type=1,
            symbol_rate=32_768,
            deviation=75,
            rx_bw=0x52,
            sync_word=0x543D0000,
            format_conf=0xABCD,
        )
        freq, mod, rate, dev, bw, sync, fmt = struct.unpack("<IBIHBIH", payload)
        assert freq == 868_300_000
        assert mod == 1
        assert rate == 32_768
        assert dev == 75
        assert bw == 0x52
        assert sync == 0x543D0000
        assert fmt == 0xABCD

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_every_preset_encodes_to_18_bytes(self, name):
        preset = PROP_PRESETS[name]
        payload = CommandBuilder.set_prop_config(**preset)
        assert len(payload) == 18

    @pytest.mark.parametrize("name", list(PROP_PRESETS.keys()))
    def test_preset_round_trip(self, name):
        preset = PROP_PRESETS[name]
        payload = CommandBuilder.set_prop_config(**preset)
        freq, mod, rate, dev, bw, sync, fmt = struct.unpack("<IBIHBIH", payload)
        assert freq == preset["frequency_hz"]
        assert mod == preset["mod_type"]
        assert rate == preset["symbol_rate"]
        assert dev == preset["deviation"]
        assert bw == preset["rx_bw"]
        assert sync == preset["sync_word"]


class TestPresetCoverage:
    """Sanity that the project's headline presets are present (regression gate)."""

    @pytest.mark.parametrize(
        "name",
        [
            # F10 hard-gated presets
            "gfsk_868_50k",
            "gfsk_915_50k",
            "gfsk_2440_50k",
            "msk_868_50k",
            "4fsk_868_50k",
            "4gfsk_868_50k",
            "wireless_mbus_s_868",
            "wireless_mbus_t_868",
            "wireless_mbus_c_868",
            "gfsk_433_50k",
            "fsk_433_50k",
            "msk_433_50k",
            "4fsk_433_50k",
            "4gfsk_433_50k",
            "ook_868_4k8",
            "ook_433_4k8",
        ],
    )
    def test_required_preset_present(self, name):
        assert name in PROP_PRESETS

    def test_ook_uses_mod_type_2(self):
        assert PROP_PRESETS["ook_868_4k8"]["mod_type"] == 2
        assert PROP_PRESETS["ook_433_4k8"]["mod_type"] == 2

    def test_4fsk_uses_mod_type_5(self):
        assert PROP_PRESETS["4fsk_868_50k"]["mod_type"] == 5
        assert PROP_PRESETS["4fsk_433_50k"]["mod_type"] == 5

    def test_4gfsk_uses_mod_type_6(self):
        assert PROP_PRESETS["4gfsk_868_50k"]["mod_type"] == 6
        assert PROP_PRESETS["4gfsk_433_50k"]["mod_type"] == 6

    def test_msk_uses_mod_type_4(self):
        assert PROP_PRESETS["msk_868_50k"]["mod_type"] == 4
        assert PROP_PRESETS["msk_433_50k"]["mod_type"] == 4


# F29 vuelta 1 — Sub-G 915 MHz presets


F29_PRESET_NAMES = ("sidewalk_915_fsk_50k", "sidewalk_915_fsk_250k", "wisun_915_fsk_50k")


@pytest.mark.parametrize("name", F29_PRESET_NAMES)
def test_f29_preset_present(name):
    """F29 vuelta 1 — los 3 presets nuevos están en PROP_PRESETS."""
    assert name in PROP_PRESETS, f"Preset {name} ausente"


@pytest.mark.parametrize("name", F29_PRESET_NAMES)
def test_f29_preset_in_915_band(name):
    """F29 vuelta 1 — frecuencia debe estar en banda 902-928 MHz US ISM."""
    f = PROP_PRESETS[name]["frequency_hz"]
    assert 902_000_000 <= f <= 928_000_000, f"{name}: freq {f} fuera de banda 902-928 MHz"


@pytest.mark.parametrize("name", F29_PRESET_NAMES)
def test_f29_preset_uses_fsk(name):
    """F29 vuelta 1 — usa FSK puro (mod_type=0). GFSK fallback queda para F29.b."""
    assert PROP_PRESETS[name]["mod_type"] == 0, f"{name}: mod_type debe ser 0 (FSK)"


def test_f29_preset_count():
    """F29 vuelta 1 — exactamente 3 presets nuevos en banda 915."""
    f29 = [n for n in PROP_PRESETS if n in F29_PRESET_NAMES]
    assert len(f29) == 3, f"Esperaba 3 F29 presets, encontré {len(f29)}: {f29}"
