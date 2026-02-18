#!/usr/bin/env python3
"""
FeralRF - Regression Canary (smoke + short soak + stats monotonic checks)
"""

import argparse
import math
import sys
import time
from typing import Optional

from feralrf import PHY, Radio
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError
from feralrf.radio import DeviceStats


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


CANARY_PROFILE_MIN_PACKETS_60S = {
    "lab": 500,
    "ci_manual": 50,
    "quiet": 1,
}


def ll_summary(stats: DeviceStats) -> str:
    if stats.ll_kind_adv is None:
        return ""
    return (
        f" ll(unk={stats.ll_kind_unknown},adv={stats.ll_kind_adv},"
        f"scan={stats.ll_kind_scan},conn={stats.ll_kind_connect},data={stats.ll_kind_data})"
    )


def get_stats_with_retry(radio: Radio, retries: int, timeout: float) -> DeviceStats:
    last_exc: Optional[Exception] = None
    for _ in range(retries):
        try:
            return radio.get_stats(timeout=timeout)
        except TimeoutError as exc:
            last_exc = exc
    if last_exc is not None:
        raise last_exc
    raise TimeoutError("Response timeout")


def stats_monotonic(curr: DeviceStats, prev: DeviceStats) -> bool:
    if curr.rx_ok < prev.rx_ok:
        return False
    if curr.rx_crc_err < prev.rx_crc_err:
        return False
    if curr.rx_drop < prev.rx_drop:
        return False
    if curr.rx_overflow < prev.rx_overflow:
        return False

    prev_ll_kind_unknown = prev.ll_kind_unknown or 0
    prev_ll_kind_adv = prev.ll_kind_adv or 0
    prev_ll_kind_scan = prev.ll_kind_scan or 0
    prev_ll_kind_connect = prev.ll_kind_connect or 0
    prev_ll_kind_data = prev.ll_kind_data or 0

    curr_ll_kind_unknown = curr.ll_kind_unknown or 0
    curr_ll_kind_adv = curr.ll_kind_adv or 0
    curr_ll_kind_scan = curr.ll_kind_scan or 0
    curr_ll_kind_connect = curr.ll_kind_connect or 0
    curr_ll_kind_data = curr.ll_kind_data or 0

    if curr_ll_kind_unknown < prev_ll_kind_unknown:
        return False
    if curr_ll_kind_adv < prev_ll_kind_adv:
        return False
    if curr_ll_kind_scan < prev_ll_kind_scan:
        return False
    if curr_ll_kind_connect < prev_ll_kind_connect:
        return False
    if curr_ll_kind_data < prev_ll_kind_data:
        return False
    return True


def run_smoke(radio: Radio, phy: int, channel: int, power: int) -> None:
    step("Smoke: SET_PHY/SET_CHANNEL/SET_POWER/RX_START/RX_STOP")
    radio.set_phy(PHY(phy), channel)
    radio.set_channel(channel)
    radio.set_power(power)
    radio.start_rx()
    radio.stop_rx(retries=3, timeout=1.0)
    ok("Smoke ACK path OK")


def run_soak(
    radio: Radio,
    phy: int,
    channel: int,
    power: int,
    duration: int,
    report_every: int,
    stats_timeout: float,
    stats_retries: int,
    min_packets: int,
) -> None:
    step("Soak: start RX + stats monotonic validation")
    radio.set_phy(PHY(phy), channel)
    radio.set_channel(channel)
    radio.set_power(power)
    radio.start_rx()

    baseline = get_stats_with_retry(radio, retries=stats_retries, timeout=stats_timeout)
    prev = baseline
    total_packets = 0
    start = time.monotonic()
    end = start + float(duration)
    report_idx = 0

    while True:
        now = time.monotonic()
        if now >= end:
            break

        window = min(float(report_every), end - now)
        packets = list(radio.read_packets(timeout=window))
        total_packets += len(packets)
        report_idx += 1

        curr = get_stats_with_retry(radio, retries=stats_retries, timeout=stats_timeout)
        if not stats_monotonic(curr, prev):
            raise RuntimeError("Non-monotonic stats detected")

        elapsed = int(time.monotonic() - start)
        print(
            f"[RPT {report_idx:03d} @ {elapsed:4d}s] "
            f"pkts_window={len(packets):4d} "
            f"cum(ok={curr.rx_ok - baseline.rx_ok},crc_err={curr.rx_crc_err - baseline.rx_crc_err},"
            f"drop={curr.rx_drop - baseline.rx_drop},ovf={curr.rx_overflow - baseline.rx_overflow})"
            f"{ll_summary(curr)}"
        )
        prev = curr

    step("Soak: RX_STOP + final stats")
    radio.stop_rx(retries=max(stats_retries, 5), timeout=max(stats_timeout, 1.5))
    final_stats = get_stats_with_retry(
        radio,
        retries=max(stats_retries, 5),
        timeout=max(stats_timeout, 2.5),
    )
    if not stats_monotonic(final_stats, prev):
        raise RuntimeError("Non-monotonic final stats detected")

    if total_packets < min_packets:
        raise RuntimeError(
            f"Packet floor not met: packets_total={total_packets} < min_packets={min_packets}"
        )

    ok(f"packets_total={total_packets}")
    ok(
        f"stats_total ok={final_stats.rx_ok - baseline.rx_ok} "
        f"crc_err={final_stats.rx_crc_err - baseline.rx_crc_err} "
        f"drop={final_stats.rx_drop - baseline.rx_drop} "
        f"ovf={final_stats.rx_overflow - baseline.rx_overflow}"
        f"{ll_summary(final_stats)}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF regression canary")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY number", type=int, default=int(PHY.BLE_1M))
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=37)
    parser.add_argument("--power", help="TX power in dBm", type=int, default=0)
    parser.add_argument(
        "--soak-duration", help="Short soak duration in seconds", type=int, default=60
    )
    parser.add_argument("--report-every", help="Report interval in seconds", type=int, default=15)
    parser.add_argument(
        "--stats-timeout", help="GET_STATS timeout in seconds", type=float, default=2.0
    )
    parser.add_argument("--stats-retries", help="Retries for GET_STATS", type=int, default=3)
    parser.add_argument(
        "--profile",
        help="Packet-floor profile when --min-packets is not provided",
        choices=tuple(CANARY_PROFILE_MIN_PACKETS_60S.keys()),
        default="ci_manual",
    )
    parser.add_argument(
        "--min-packets",
        help="Minimum packets expected in soak window (override profile-derived floor)",
        type=int,
        default=None,
    )
    args = parser.parse_args()

    if args.soak_duration <= 0:
        fail("soak-duration must be > 0")
        return 2
    if args.report_every <= 0:
        fail("report-every must be > 0")
        return 2
    if args.stats_timeout <= 0:
        fail("stats-timeout must be > 0")
        return 2
    if args.stats_retries <= 0:
        fail("stats-retries must be > 0")
        return 2
    if args.min_packets is not None and args.min_packets < 0:
        fail("min-packets must be >= 0")
        return 2

    profile_floor_60s = CANARY_PROFILE_MIN_PACKETS_60S[args.profile]
    profile_scaled_floor = int(math.ceil((profile_floor_60s * args.soak_duration) / 60.0))
    effective_min_packets = (
        args.min_packets if args.min_packets is not None else profile_scaled_floor
    )

    print("FeralRF Regression Canary")
    print("=========================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} channel={args.channel} "
        f"power={args.power} soak_duration={args.soak_duration}s report_every={args.report_every}s "
        f"stats_timeout={args.stats_timeout}s stats_retries={args.stats_retries} "
        f"profile={args.profile} min_packets={effective_min_packets}"
    )
    if args.min_packets is None:
        print(
            f"profile_floor_60s={profile_floor_60s} -> scaled_floor={profile_scaled_floor} "
            f"(duration={args.soak_duration}s)"
        )
    print()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        step("Connect + init")
        info = radio.init()
        ok(
            f"INFO firmware={info.firmware_version} capabilities=0x{info.capabilities:02X} "
            f"serial={info.serial or 'n/a'}"
        )

        run_smoke(radio, args.phy, args.channel, args.power)
        run_soak(
            radio=radio,
            phy=args.phy,
            channel=args.channel,
            power=args.power,
            duration=args.soak_duration,
            report_every=args.report_every,
            stats_timeout=args.stats_timeout,
            stats_retries=args.stats_retries,
            min_packets=effective_min_packets,
        )

        print()
        ok("CANARY PASS")
        return 0

    except ValueError as exc:
        fail(f"Invalid argument: {exc}")
        return 2
    except ConnectionError as exc:
        fail(f"Connection error: {exc}")
        return 3
    except TimeoutError as exc:
        fail(f"Timeout waiting for response: {exc}")
        return 4
    except FeralRFError as exc:
        fail(f"Protocol/command error: {exc}")
        return 5
    except KeyboardInterrupt:
        fail("Interrupted by user")
        return 130
    except Exception as exc:
        fail(f"Canary regression failed: {exc}")
        return 6
    finally:
        try:
            radio.stop_rx(retries=1, timeout=0.2)
        except Exception:
            pass
        radio.disconnect()


if __name__ == "__main__":
    sys.exit(main())
