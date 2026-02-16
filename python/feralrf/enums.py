"""
FeralRF - Enumerations
"""

from enum import IntEnum


class PHY(IntEnum):
    """Supported PHY types"""

    BLE_1M = 0
    BLE_2M = 1
    BLE_CODED_S8 = 2
    BLE_CODED_S2 = 3
    IEEE_802_15_4 = 4
    SUB_1GHZ_868 = 5
    SUB_1GHZ_915 = 6
    PROPRIETARY_GFSK = 7


class Command(IntEnum):
    """Command IDs"""

    # Configuration
    RADIO_INIT = 0x01
    SET_CHANNEL = 0x02
    SET_POWER = 0x03
    SET_PHY = 0x04
    GET_INFO = 0x05

    # RX Operations
    RX_START = 0x10
    RX_STOP = 0x11
    RX_SET_FILTER = 0x12
    RX_SET_PROMISCUOUS = 0x13

    # TX Operations
    TX_RAW = 0x20
    TX_CONTINUOUS = 0x21
    TX_BURST = 0x22

    # Jamming
    JAM_CONTINUOUS = 0x30
    JAM_REACTIVE = 0x31
    JAM_PATTERN = 0x32
    JAM_STOP = 0x33

    # Spectrum Analysis
    SPECTRUM_SCAN = 0x40
    SPECTRUM_MONITOR = 0x41
    SPECTRUM_STOP = 0x42

    # Autonomous Policies
    POLICY_SET = 0x50
    POLICY_START = 0x51
    POLICY_STOP = 0x52

    # Bootloader
    ENTER_BOOTLOADER = 0xF0
    BOOTLOADER_VERSION = 0xF1


class Response(IntEnum):
    """Response IDs"""

    ACK = 0x80
    ERROR = 0x81
    RX_PACKET = 0x90
    SPECTRUM_DATA = 0x91
    STATUS = 0x92
    STATS = 0x93
    INFO = 0x94
    JAM_EVENT = 0x95
