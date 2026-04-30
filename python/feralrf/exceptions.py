"""
FeralRF - Exceptions
"""


class FeralRFError(Exception):
    """Base exception for FeralRF"""

    pass


class ConnectionError(FeralRFError):
    """Connection error (serial port, etc.)"""

    pass


class ProtocolError(FeralRFError):
    """Protocol error (invalid frame, CRC, etc.)"""

    pass


class CommandError(FeralRFError):
    """Command execution error"""

    def __init__(self, message: str, error_code: int = 0):
        super().__init__(message)
        self.error_code = error_code


class TimeoutError(FeralRFError):
    """Operation timeout"""

    pass


class RadioError(FeralRFError):
    """Base class for radio and hardware subsystem errors."""

    pass


class CryptoError(RadioError):
    """Raised when a hardware crypto operation fails (tag mismatch,
    HW error, driver init failure)."""

    pass
