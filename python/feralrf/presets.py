"""
FeralRF - Proprietary Radio Presets

Pre-configured radio parameters for common protocols and frequencies.
Use with radio.configure_prop(**PROP_PRESETS['name']).

WARNING: OOK presets (mod_type=2) lock the radio to that frequency.
Call radio.reset_device() after OOK to use other modes.
"""

PROP_PRESETS = {
    # === 433 MHz ISM Band ===
    "gfsk_433_50k": dict(
        frequency_hz=433920000,
        mod_type=1,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "gfsk_433_10k": dict(
        frequency_hz=433920000,
        mod_type=1,
        symbol_rate=10000,
        deviation=50,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "fsk_433_50k": dict(
        frequency_hz=433920000,
        mod_type=0,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    # OOK presets — WARNING: locks radio, call reset_device() after use
    "ook_433_4k8": dict(
        frequency_hz=433920000,
        mod_type=2,
        symbol_rate=4800,
        deviation=0,
        rx_bw=76,
        sync_word=0x930B51DE,
    ),
    "ook_433_2k4": dict(
        frequency_hz=433920000,
        mod_type=2,
        symbol_rate=2400,
        deviation=0,
        rx_bw=76,
        sync_word=0x930B51DE,
    ),
    # === 868 MHz ISM Band (EU) ===
    "gfsk_868_50k": dict(
        frequency_hz=868000000,
        mod_type=1,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "gfsk_868_100k": dict(
        frequency_hz=868000000,
        mod_type=1,
        symbol_rate=100000,
        deviation=150,
        rx_bw=0x56,
        sync_word=0x930B51DE,
    ),
    "ook_868_4k8": dict(
        frequency_hz=868000000,
        mod_type=2,
        symbol_rate=4800,
        deviation=0,
        rx_bw=76,
        sync_word=0x930B51DE,
    ),
    "wireless_mbus_s_868": dict(
        frequency_hz=868300000,
        mod_type=1,
        symbol_rate=32768,
        deviation=75,
        rx_bw=0x52,
        sync_word=0x543D0000,
    ),
    # === 915 MHz ISM Band (US) ===
    "gfsk_915_50k": dict(
        frequency_hz=915000000,
        mod_type=1,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "gfsk_902_50k": dict(
        frequency_hz=902200000,
        mod_type=1,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    # === 2.4 GHz Proprietary ===
    "gfsk_2440_250k": dict(
        frequency_hz=2440000000,
        mod_type=1,
        symbol_rate=250000,
        deviation=320,
        rx_bw=0x59,
        sync_word=0x930B51DE,
    ),
    "gfsk_2440_50k": dict(
        frequency_hz=2440000000,
        mod_type=1,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
}
