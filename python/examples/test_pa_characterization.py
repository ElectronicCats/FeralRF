#!/usr/bin/env python3
"""
FeralRF - PA (Power Amplifier) Characterization Test

Measures gain and compression (P1dB) for external PAs connected via SMA
to a CatSniffer TX board.  A second CatSniffer acts as RX reference,
measuring per-packet RSSI across a TX power sweep.

Setup:
  Board TX  --SMA--> [PA under test] --SMA--> Antenna
  Board RX  --antenna--> Measures RSSI per packet

Test flow for each configuration (baseline, PA1, PA2):
  1. Prompt user to connect the right PA (or bypass for baseline)
  2. Sweep TX power from -20 to +20 dBm
  3. At each level: TX burst → RX collects RSSI
  4. Record stats per level

Output: comparative table with gain and estimated P1dB.
"""

import argparse
import csv
import math
import sys
import time
from dataclasses import dataclass, field
from typing import List, Optional


POWER_SWEEP_DBM = [-20, -15, -10, -5, 0, 5, 10, 15, 20]

DEFAULT_CONFIGS = ["baseline", "SKY65162", "TQP7M9104"]


@dataclass
class LevelResult:
    tx_power_dbm: int
    rssi_values: List[int] = field(default_factory=list)
    packets_expected: int = 0
    packets_received: int = 0

    @property
    def rssi_mean(self) -> float:
        if not self.rssi_values:
            return float("nan")
        return sum(self.rssi_values) / len(self.rssi_values)

    @property
    def rssi_std(self) -> float:
        if len(self.rssi_values) < 2:
            return float("nan")
        mean = self.rssi_mean
        variance = sum((r - mean) ** 2 for r in self.rssi_values) / (
            len(self.rssi_values) - 1
        )
        return math.sqrt(variance)

    @property
    def rssi_min(self) -> int:
        return min(self.rssi_values) if self.rssi_values else 0

    @property
    def rssi_max(self) -> int:
        return max(self.rssi_values) if self.rssi_values else 0

    @property
    def per(self) -> float:
        """Packet Error Rate (0.0 to 1.0)."""
        if self.packets_expected == 0:
            return float("nan")
        return 1.0 - (self.packets_received / self.packets_expected)


@dataclass
class ConfigResult:
    name: str
    levels: List[LevelResult] = field(default_factory=list)


def step(title: str) -> None:
    print(f"\n[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def run_sweep(
    radio_tx,
    radio_rx,
    config_name: str,
    phy_id: int,
    channel: int,
    packet: bytes,
    packets_per_level: int,
    interval_us: int,
    rx_settle_s: float,
) -> ConfigResult:
    """Run a full power sweep for one PA configuration."""
    from feralrf import PHY

    result = ConfigResult(name=config_name)

    step(f"Sweep for '{config_name}'")

    for pwr in POWER_SWEEP_DBM:
        level = LevelResult(tx_power_dbm=pwr, packets_expected=packets_per_level)

        # Configure TX power
        radio_tx.set_power(pwr)

        # Start RX
        radio_rx.start_rx()
        time.sleep(rx_settle_s)

        # Drain any stale packets
        for _ in radio_rx.read_packets(timeout=0.3):
            pass

        # TX burst
        radio_tx.transmit_burst(
            packet, count=packets_per_level, interval_us=interval_us
        )

        # Wait for all packets to arrive:
        # burst_duration + margin
        burst_duration_s = (packets_per_level * interval_us) / 1_000_000.0
        rx_window = burst_duration_s + 1.0

        for pkt in radio_rx.read_packets(timeout=rx_window):
            if pkt.crc_ok:
                level.rssi_values.append(pkt.rssi_dbm)

        level.packets_received = len(level.rssi_values)
        radio_rx.stop_rx()

        per_pct = level.per * 100.0 if not math.isnan(level.per) else 0.0
        print(
            f"  {pwr:+4d} dBm -> "
            f"rx={level.packets_received:4d}/{level.packets_expected} "
            f"RSSI={level.rssi_mean:+6.1f} dBm "
            f"(std={level.rssi_std:4.1f}) "
            f"PER={per_pct:5.1f}%"
        )

        result.levels.append(level)

    return result


def compute_gain(
    baseline: ConfigResult, pa: ConfigResult
) -> List[Optional[float]]:
    """Compute gain (dB) per power level: PA_rssi - baseline_rssi."""
    gains = []
    for bl, pl in zip(baseline.levels, pa.levels):
        if math.isnan(bl.rssi_mean) or math.isnan(pl.rssi_mean):
            gains.append(None)
        else:
            gains.append(pl.rssi_mean - bl.rssi_mean)
    return gains


def estimate_p1db(gains: List[Optional[float]]) -> Optional[str]:
    """
    Estimate input P1dB: the TX power level where gain drops 1 dB
    below the peak gain observed at lower power levels.
    """
    valid = [(i, g) for i, g in enumerate(gains) if g is not None]
    if len(valid) < 3:
        return None

    peak_gain = max(g for _, g in valid)

    for i, g in valid:
        if peak_gain - g >= 1.0:
            return (
                f"TX {POWER_SWEEP_DBM[i]:+d} dBm "
                f"(gain={g:+.1f} dB, peak={peak_gain:+.1f} dB, "
                f"compression={peak_gain - g:.1f} dB)"
            )
    return None


def print_results(
    configs: List[ConfigResult], phy_id: int, channel: int, packets_per_level: int
):
    """Print comparative results table."""
    baseline = configs[0]
    pa_configs = configs[1:]

    print("\n" + "=" * 80)
    print("PA CHARACTERIZATION RESULTS")
    print("=" * 80)
    print(f"  PHY: {phy_id}  Channel: {channel}  Packets/level: {packets_per_level}")
    print(f"  Configs: {', '.join(c.name for c in configs)}")
    print("-" * 80)

    # Header
    hdr = f"{'TX_pwr':>7s} | {'Baseline':>10s}"
    for pa in pa_configs:
        hdr += f" | {pa.name + ' RSSI':>14s} | {'Gain':>7s} | {'PER':>6s}"
    print(hdr)
    print("-" * 80)

    # Gain arrays for P1dB
    all_gains = {}
    for pa in pa_configs:
        all_gains[pa.name] = compute_gain(baseline, pa)

    # Rows
    for idx, bl_level in enumerate(baseline.levels):
        pwr = bl_level.tx_power_dbm
        bl_rssi = bl_level.rssi_mean
        row = f"{pwr:+4d} dBm | {bl_rssi:+8.1f}  "

        for pa in pa_configs:
            pa_level = pa.levels[idx]
            pa_rssi = pa_level.rssi_mean
            gain = all_gains[pa.name][idx]
            per_pct = pa_level.per * 100.0 if not math.isnan(pa_level.per) else 0.0
            gain_str = f"{gain:+6.1f}" if gain is not None else "   N/A"
            row += f" | {pa_rssi:+12.1f}   | {gain_str} dB | {per_pct:5.1f}%"

        print(row)

    print("-" * 80)

    # P1dB estimates
    print("\nEstimated P1dB (input-referred):")
    for pa in pa_configs:
        p1db = estimate_p1db(all_gains[pa.name])
        if p1db:
            print(f"  {pa.name}: {p1db}")
        else:
            print(f"  {pa.name}: not detected (no 1 dB compression in sweep range)")

    print("=" * 80)


def export_csv(
    filepath: str, configs: List[ConfigResult], phy_id: int, channel: int
):
    """Export all results to CSV."""
    baseline = configs[0]
    pa_configs = configs[1:]
    all_gains = {pa.name: compute_gain(baseline, pa) for pa in pa_configs}

    with open(filepath, "w", newline="") as f:
        writer = csv.writer(f)

        # Header
        header = ["tx_power_dbm", "baseline_rssi_mean", "baseline_rssi_std",
                  "baseline_rx", "baseline_per"]
        for pa in pa_configs:
            header += [
                f"{pa.name}_rssi_mean", f"{pa.name}_rssi_std",
                f"{pa.name}_rx", f"{pa.name}_per", f"{pa.name}_gain_db",
            ]
        writer.writerow(header)

        # Rows
        for idx, bl in enumerate(baseline.levels):
            row = [
                bl.tx_power_dbm,
                f"{bl.rssi_mean:.2f}",
                f"{bl.rssi_std:.2f}",
                bl.packets_received,
                f"{bl.per:.4f}",
            ]
            for pa in pa_configs:
                pl = pa.levels[idx]
                gain = all_gains[pa.name][idx]
                row += [
                    f"{pl.rssi_mean:.2f}",
                    f"{pl.rssi_std:.2f}",
                    pl.packets_received,
                    f"{pl.per:.4f}",
                    f"{gain:.2f}" if gain is not None else "",
                ]
            writer.writerow(row)

    ok(f"Results exported to {filepath}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="FeralRF PA Characterization - power sweep with gain & P1dB analysis"
    )
    parser.add_argument("--port-tx", required=True, help="TX board serial port")
    parser.add_argument("--port-rx", required=True, help="RX board serial port")
    parser.add_argument("-b", "--baudrate", type=int, default=921600)
    parser.add_argument(
        "--phy", type=int, default=0,
        help="PHY id (0=BLE_1M, 4=IEEE_802_15_4)"
    )
    parser.add_argument("--channel", "-c", type=int, default=37, help="RF channel")
    parser.add_argument(
        "--packets-per-level", type=int, default=200,
        help="Packets to TX at each power level"
    )
    parser.add_argument(
        "--interval-us", type=int, default=5000,
        help="Inter-packet interval in microseconds"
    )
    parser.add_argument(
        "--tx-packet", default="a1b2c3d4e5f6",
        help="TX payload hex string"
    )
    parser.add_argument(
        "--configs", nargs="+", default=DEFAULT_CONFIGS,
        help="Configuration names (first is baseline)"
    )
    parser.add_argument(
        "--output", "-o", default=None,
        help="Export results to CSV file"
    )
    parser.add_argument(
        "--rx-settle", type=float, default=0.3,
        help="Seconds to wait after starting RX before TX"
    )
    args = parser.parse_args()

    packet = bytes.fromhex(args.tx_packet)
    if len(packet) == 0:
        fail("TX packet must not be empty")
        return 2

    from feralrf import PHY, Radio

    radio_tx = Radio(port=args.port_tx, baudrate=args.baudrate)
    radio_rx = Radio(port=args.port_rx, baudrate=args.baudrate)

    try:
        step("Init TX board")
        info_tx = radio_tx.init()
        radio_tx.set_phy(PHY(args.phy), args.channel)
        radio_tx.set_channel(args.channel)
        ok(f"TX ready: fw={info_tx.firmware_version}")

        step("Init RX board")
        info_rx = radio_rx.init()
        radio_rx.set_phy(PHY(args.phy), args.channel)
        radio_rx.set_channel(args.channel)
        ok(f"RX ready: fw={info_rx.firmware_version}")

        all_results: List[ConfigResult] = []

        for config_name in args.configs:
            print(f"\n{'='*60}")
            print(f"  Configure: {config_name}")
            if config_name.lower() == "baseline":
                print("  Connect TX board directly to antenna (no PA)")
            else:
                print(f"  Connect PA '{config_name}' between TX and antenna")
            print(f"{'='*60}")
            input("  Press ENTER when ready...")

            result = run_sweep(
                radio_tx=radio_tx,
                radio_rx=radio_rx,
                config_name=config_name,
                phy_id=args.phy,
                channel=args.channel,
                packet=packet,
                packets_per_level=args.packets_per_level,
                interval_us=args.interval_us,
                rx_settle_s=args.rx_settle,
            )
            all_results.append(result)

        # Print comparative results
        print_results(all_results, args.phy, args.channel, args.packets_per_level)

        # Export CSV if requested
        if args.output:
            export_csv(args.output, all_results, args.phy, args.channel)

        return 0

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    except Exception as exc:
        fail(f"Error: {exc}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        try:
            radio_tx.stop_transmit(timeout=0.5)
        except Exception:
            pass
        try:
            radio_rx.stop_rx()
        except Exception:
            pass
        radio_tx.disconnect()
        radio_rx.disconnect()


if __name__ == "__main__":
    sys.exit(main())
