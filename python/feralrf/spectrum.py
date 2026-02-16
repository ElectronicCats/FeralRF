"""
FeralRF - Spectrum analyzer utilities
"""

from dataclasses import dataclass
from typing import List, Optional


@dataclass
class SpectrumPoint:
    """Single spectrum measurement point"""
    frequency_hz: int
    rssi_dbm: float


class SpectrumScan:
    """Spectrum scan results"""

    def __init__(self):
        self.points: List[SpectrumPoint] = []

    def add_point(self, freq_hz: int, rssi_dbm: float) -> None:
        self.points.append(SpectrumPoint(freq_hz, rssi_dbm))

    def to_list(self) -> List[tuple]:
        return [(p.frequency_hz, p.rssi_dbm) for p in self.points]

    def plot(self, filename: Optional[str] = None) -> None:
        """Plot spectrum (requires matplotlib)"""
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            raise ImportError("matplotlib required for plotting")

        freqs_mhz = [p.frequency_hz / 1e6 for p in self.points]
        rssis = [p.rssi_dbm for p in self.points]

        plt.figure(figsize=(10, 6))
        plt.plot(freqs_mhz, rssis, 'b-')
        plt.xlabel('Frequency (MHz)')
        plt.ylabel('RSSI (dBm)')
        plt.title('Spectrum Scan')
        plt.grid(True)

        if filename:
            plt.savefig(filename)
        else:
            plt.show()
