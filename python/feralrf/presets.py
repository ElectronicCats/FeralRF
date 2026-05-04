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
    # PENDING NATIVE SUPPORT: validación OTA F29.b en 2026-05-03 falló
    # (escape M3 completo: FSK puro 0/10, GFSK 0/10, rx_bw 0x4A/0x46/0x42/0x4E
    # todos 0/10). CC1352 puede no soportar TS-UNB 396 baud nativamente sin
    # patch CPE custom. Preset retenido para preservar API ID; smoke F29.b
    # excluye del loop hasta que F29.c (con SmartRF Studio + custom override)
    # cierre el caso. Ver project_f29_done.md y master plan §F29.
    "mioty_868_tsunb": dict(
        frequency_hz=868000000,
        mod_type=0,
        symbol_rate=396,
        deviation=1,
        rx_bw=0x4A,
        sync_word=0x930B51DE,
    ),
    # === Wi-SUN FAN 1.0 NA-1 ch 0 — data rates restantes (F29.b) ===
    # Mod index implícito ~0.5; deviation pragmática heredada de templates.
    # F29 vuelta 1 lección: FSK puro >=150k requiere rx_bw=0x5A; 300k probablemente 0x5E.
    "wisun_915_fsk_100k": dict(
        frequency_hz=902200000,
        mod_type=0,
        symbol_rate=100000,
        deviation=150,
        rx_bw=0x56,  # match gfsk_868_100k validado
        sync_word=0x930B51DE,
    ),
    "wisun_915_fsk_150k": dict(
        frequency_hz=902200000,
        mod_type=0,
        symbol_rate=150000,
        deviation=200,
        rx_bw=0x5A,  # FSK puro >= 150k → más BW
        sync_word=0x930B51DE,
    ),
    "wisun_915_fsk_200k": dict(
        frequency_hz=902200000,
        mod_type=0,
        symbol_rate=200000,
        deviation=200,
        rx_bw=0x5A,  # F29 vuelta 1 lección
        sync_word=0x930B51DE,
    ),
    "wisun_915_fsk_300k": dict(
        frequency_hz=902200000,
        mod_type=0,
        symbol_rate=300000,
        deviation=200,
        rx_bw=0x5E,  # rate más alto requiere BW más amplio
        sync_word=0x930B51DE,
    ),
    # === NOTE: Sidewalk LR (LoRa-like) NO soportado por CC1352 ===
    # Sidewalk LR usa LoRa modulation que requiere chipset Semtech (SX1262 o
    # similar). Los presets sidewalk_* arriba cubren SOLO el FSK layer de
    # Sidewalk Sub-G (banda 915 MHz). Para LR, ver hardware Cat-LoRa /
    # Sidewalk LR via SX1262 fuera de este preset dict.
}
