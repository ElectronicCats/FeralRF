"""Unit tests for F26 — Proprietary 2.4 GHz as a normal selectable PHY."""

from feralrf.enums import PHY, Command


def test_prop_2_4ghz_phy_enum_value():
    """PHY.PROP_2_4GHZ exists with value 8 (extends existing enum)."""
    assert PHY.PROP_2_4GHZ == 8
    assert PHY.PROP_2_4GHZ.name == "PROP_2_4GHZ"


def test_set_phy_prop_2_4ghz_default_payload(monkeypatch):
    """set_phy(PHY.PROP_2_4GHZ) sends 1-byte payload [0x08]."""
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    sent_cmds = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent_cmds.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.ACK, 0, b""),
    )

    radio.set_phy(PHY.PROP_2_4GHZ)

    set_phy_frame = next(c for c in sent_cmds if c[0] == Command.SET_PHY)
    assert set_phy_frame[1][0] == 0x08


def test_set_phy_prop_2_4ghz_with_frequency(monkeypatch):
    """set_phy(PHY.PROP_2_4GHZ, frequency_hz=2440000000) sends 7-byte payload."""
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    sent_cmds = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent_cmds.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.ACK, 0, b""),
    )

    radio.set_phy(PHY.PROP_2_4GHZ, frequency_hz=2440000000)

    set_phy_frame = next(c for c in sent_cmds if c[0] == Command.SET_PHY)
    payload = set_phy_frame[1]
    assert len(payload) == 7  # phy(1) + channel(2) + frequency_hz(4)
    assert payload[0] == 0x08  # PHY.PROP_2_4GHZ


def test_configure_prop_24ghz_freq(monkeypatch):
    """configure_prop with 2.4 GHz freq passes through correctly."""
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    sent_cmds = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent_cmds.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.ACK, 0, b""),
    )

    radio.configure_prop(
        frequency_hz=2440000000,
        mod_type=1,  # GFSK
        symbol_rate=250000,
        deviation=125000,
        rx_bw=0x59,
        sync_word=0x930B51DE,
        format_conf=0,
    )

    cfg_frame = next(c for c in sent_cmds if c[0] == Command.SET_PROP_CONFIG)
    payload = cfg_frame[1]
    # frequency_hz at offset 0 (4 bytes LE)
    freq_hz = int.from_bytes(payload[0:4], "little")
    assert freq_hz == 2440000000
