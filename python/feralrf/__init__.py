"""
FeralRF - Python API for FeralRF/CatSniffer RF tools

This package provides an interface to control the FeralRF firmware
on CatSniffer hardware for RF operations including sniffing, TX/RX,
jamming, and spectrum analysis.
"""

from feralrf.radio import Radio
from feralrf.enums import PHY, Command, Response
from feralrf.exceptions import FeralRFError, ConnectionError, ProtocolError

__version__ = "0.1.0"
__author__ = "Electronic Cats"

__all__ = [
    "Radio",
    "PHY",
    "Command",
    "Response",
    "FeralRFError",
    "ConnectionError",
    "ProtocolError",
]
