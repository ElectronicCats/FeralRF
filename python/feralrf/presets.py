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
    # MSK presets — used by Sidewalk, Wi-SUN, some IoT protocols
    "msk_433_50k": dict(
        frequency_hz=433920000,
        mod_type=4,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    # 4-FSK / 4-GFSK 433 MHz
    "4fsk_433_50k": dict(
        frequency_hz=433920000,
        mod_type=5,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "4gfsk_433_50k": dict(
        frequency_hz=433920000,
        mod_type=6,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
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
    "msk_868_50k": dict(
        frequency_hz=868000000,
        mod_type=4,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
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
    "wireless_mbus_t_868": dict(
        frequency_hz=868950000,
        mod_type=1,
        symbol_rate=100000,
        deviation=200,
        rx_bw=0x57,
        sync_word=0x543D0000,
    ),
    "wireless_mbus_c_868": dict(
        frequency_hz=868950000,
        mod_type=1,
        symbol_rate=100000,
        deviation=180,
        rx_bw=0x57,
        sync_word=0x543D0000,
    ),
    "wireless_mbus_n_169_2k4": dict(
        frequency_hz=169450000,
        mod_type=1,
        symbol_rate=2400,
        deviation=154,
        rx_bw=0x44,
        sync_word=0x543D0000,
    ),
    "wireless_mbus_n_169_4k8": dict(
        frequency_hz=169450000,
        mod_type=1,
        symbol_rate=4800,
        deviation=154,
        rx_bw=0x44,
        sync_word=0x543D0000,
    ),
    # 4-FSK / 4-GFSK 868 MHz
    "4fsk_868_50k": dict(
        frequency_hz=868000000,
        mod_type=5,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "4gfsk_868_50k": dict(
        frequency_hz=868000000,
        mod_type=6,
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
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
    # === 902-928 MHz ISM Band (US / Sidewalk / Wi-SUN FAN 1.0) ===
    "sidewalk_915_fsk_50k": dict(
        frequency_hz=915000000,
        mod_type=0,  # FSK
        symbol_rate=50000,
        deviation=100,  # template GFSK 868 ya validado
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    "sidewalk_915_fsk_250k": dict(
        frequency_hz=915000000,
        mod_type=0,  # FSK
        symbol_rate=250000,
        deviation=200,  # escalada vs 50k variant
        rx_bw=0x5A,  # ancho aumentado vs 0x56 — 250k FSK puro requiere más BW
        sync_word=0x930B51DE,
    ),
    "wisun_915_fsk_50k": dict(
        frequency_hz=902200000,  # Wi-SUN FAN 1.0 NA-1 plan canal 0
        mod_type=0,  # FSK
        symbol_rate=50000,
        deviation=100,
        rx_bw=0x52,
        sync_word=0x930B51DE,
    ),
    # === MIOTY TS-UNB (ETSI TS 103 357) — 868 MHz EU SRD ===
    # WARNING: 396 baud es ultra-low-rate. Viability sobre CC1352 pendiente
    # validación OTA en Task 1 del plan F29.b. Si rx_bw mínimo del CC1352
    # no llega a ~5 kHz, escape M3: probar GFSK, escalación rx_bw, fallback
    # a "pending native support" excluido del smoke loop.
    "mioty_868_tsunb": dict(
        frequency_hz=868000000,
        mod_type=0,  # FSK puro como primer intento
        symbol_rate=396,  # ETSI TS 103 357
        deviation=1,  # ~250 Hz unidad CC1352 → mod index ~1.3
        rx_bw=0x4A,  # mínimo BW soportado típicamente ~5 kHz
        sync_word=0x930B51DE,
    ),
}
