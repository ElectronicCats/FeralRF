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
    """Command IDs currently exposed by firmware.

    Notes:
        - Most commands below are part of the stable baseline.
        - Jamming commands are implemented but treated as experimental.
        - Spectrum and other future commands are intentionally omitted until
          firmware support exists and the host API is finalized.
    """

    # Configuration
    RADIO_INIT = 0x01
    SET_CHANNEL = 0x02
    SET_POWER = 0x03
    SET_PHY = 0x04
    GET_INFO = 0x05
    GET_STATS = 0x06
    SET_ADV_HOP = 0x07
    SET_PROP_CONFIG = 0x08
    SET_BLE_ADDR = 0x09
    SET_BLE_SCAN_MODE = 0x0B

    # RX Operations
    RX_START = 0x10
    RX_STOP = 0x11

    # TX Operations
    TX_RAW = 0x20
    TX_CONTINUOUS = 0x21
    TX_BURST = 0x22
    TX_FRAME = 0x23
    TX_STOP = 0x24

    # Test modes (F22)
    TX_CW = 0x55
    TX_PRBS = 0x56
    TX_TEST_STOP = 0x57

    # Crypto HW (F25)
    CMD_RANDOM = 0x59
    CMD_AES_ECB = 0x5A
    CMD_AES_CCM = 0x5B
    CMD_AES_CTR = 0x5C
    CMD_AES_CBC = 0x5D
    CMD_AES_GCM = 0x5E
    CMD_SHA256 = 0x5F
    CMD_ECDH = 0x60
    CMD_ECDSA_SIGN = 0x61
    CMD_ECDSA_VERIFY = 0x62

    # Jamming
    JAM_CONTINUOUS = 0x30
    JAM_STOP = 0x33

    # BLE Connection
    CONNECT = 0x40
    DISCONNECT = 0x41
    CONN_STATUS = 0x42

    # GATT
    GATT_DISCOVER = 0x43
    GATT_READ = 0x45
    GATT_WRITE = 0x46

    # Diagnostics
    DEBUG_TIMING = 0x47
    DEBUG_CONN_PARAMS = 0x48


class Response(IntEnum):
    """Response IDs"""

    ACK = 0x80
    ERROR = 0x81
    RX_PACKET = 0x90
    STATS = 0x93
    INFO = 0x94

    # Crypto HW (F25)
    RSP_RANDOM = 0x95
    RSP_AES = 0x96
    RSP_AES_CCM = 0x97
    RSP_AES_GCM = 0x98
    RSP_SHA256 = 0x99
    RSP_ECDH = 0x9A
    RSP_ECDSA_SIG = 0x9B
    RSP_ECDSA_VERIFY = 0x9C

    # BLE Connection
    CONN_RESULT = 0xA0
    CONN_STATUS = 0xA1

    # GATT
    GATT_SERVICE = 0xA2
    GATT_CHAR = 0xA3
    GATT_READ_VALUE = 0xA4
    GATT_DONE = 0xA5

    # Diagnostics
    DEBUG_TIMING = 0xA8
    DEBUG_CONN_PARAMS = 0xA9


STABLE_COMMANDS = (
    Command.RADIO_INIT,
    Command.SET_CHANNEL,
    Command.SET_POWER,
    Command.SET_PHY,
    Command.GET_INFO,
    Command.GET_STATS,
    Command.SET_ADV_HOP,
    Command.SET_PROP_CONFIG,
    Command.SET_BLE_ADDR,
    Command.SET_BLE_SCAN_MODE,
    Command.RX_START,
    Command.RX_STOP,
    Command.TX_RAW,
    Command.TX_CONTINUOUS,
    Command.TX_BURST,
    Command.TX_FRAME,
    Command.TX_STOP,
    Command.TX_CW,
    Command.TX_PRBS,
    Command.TX_TEST_STOP,
    Command.CMD_RANDOM,
    Command.CMD_AES_ECB,
    Command.CMD_AES_CCM,
    Command.CMD_AES_CTR,
    Command.CMD_AES_CBC,
    Command.CMD_AES_GCM,
    Command.CMD_SHA256,
    Command.CMD_ECDH,
    Command.CMD_ECDSA_SIGN,
    Command.CMD_ECDSA_VERIFY,
)

EXPERIMENTAL_COMMANDS = (
    Command.JAM_CONTINUOUS,
    Command.JAM_STOP,
)

PENDING_COMMAND_IDS = {
    "JAM_REACTIVE": 0x31,
    "JAM_PATTERN": 0x32,
    "SPECTRUM_SCAN": 0x40,
    "SPECTRUM_MONITOR": 0x41,
    "SPECTRUM_STOP": 0x42,
}
