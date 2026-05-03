#!/usr/bin/env python3
"""
FeralRF - BLE soak test (default 30 minutes)

Runs continuous BLE RX and reports packet/metric evolution at fixed intervals.
"""

import argparse
import sys
import time

from feralrf import PHY, Radio, RxStreamError
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def warn(msg: str) -> None:
    print(f"[WARN] {msg}")


def get_stats_with_retry(radio: Radio, retries: int, timeout: float):
    last_exc = None
    for _ in range(retries):
        try:
            return radio.get_stats(timeout=timeout)
        except TimeoutError as exc:
            last_exc = exc
    if last_exc is not None:
        raise last_exc
    raise TimeoutError("Response timeout")


def format_ll_stats(stats) -> str:
    if stats.ll_kind_adv is None:
        return ""
    return (
        f" ll(unk={stats.ll_kind_unknown},adv={stats.ll_kind_adv},"
        f"scan={stats.ll_kind_scan},conn={stats.ll_kind_connect},data={stats.ll_kind_data})"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF BLE soak test")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY number", type=int, default=int(PHY.BLE_1M))
    parser.add_argument("--channel", "-c", help="Initial RF channel", type=int, default=37)
    parser.add_argument("--duration", help="Duration in seconds", type=int, default=1800)
    parser.add_argument("--report-every", help="Report interval in seconds", type=int, default=60)
    parser.add_argument(
        "--stats-timeout", help="Stats response timeout in seconds", type=float, default=2.0
    )
    parser.add_argument("--stats-retries", help="Retries for stats polling", type=int, default=3)
    parser.add_argument(
        "--poll-stats-live",
        help="Poll GET_STATS on every report while RX is running",
        action="store_true",
    )
    args = parser.parse_args()

    if args.duration <= 0:
        fail("Duration must be > 0")
        return 2
    if args.report_every <= 0:
        fail("Report interval must be > 0")
        return 2
    if args.stats_timeout <= 0:
        fail("Stats timeout must be > 0")
        return 2
    if args.stats_retries <= 0:
        fail("Stats retries must be > 0")
        return 2

    print("FeralRF BLE Soak Test")
    print("=====================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} duration={args.duration}s report_every={args.report_every}s "
        f"stats_timeout={args.stats_timeout}s stats_retries={args.stats_retries} "
        f"poll_stats_live={args.poll_stats_live}"
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

        step("Configure PHY + start RX")
        radio.set_phy(PHY(args.phy), args.channel)
        radio.start_rx()
        baseline = get_stats_with_retry(
            radio, retries=args.stats_retries, timeout=args.stats_timeout
        )
        ok(
            "RX started "
            f"(baseline ok={baseline.rx_ok} crc_err={baseline.rx_crc_err} "
            f"drop={baseline.rx_drop} ovf={baseline.rx_overflow}"
            f"{format_ll_stats(baseline)})"
        )
        print()

        start = time.monotonic()
        end = start + float(args.duration)
        total_packets = 0
        report_idx = 0
        prev = baseline

        while True:
            now = time.monotonic()
            if now >= end:
                break

            window = min(float(args.report_every), end - now)
            pkts = [
                p for p in radio.read_packets(timeout=window) if not isinstance(p, RxStreamError)
            ]
            total_packets += len(pkts)
            elapsed = int(time.monotonic() - start)
            report_idx += 1
            if args.poll_stats_live:
                try:
                    curr = get_stats_with_retry(
                        radio, retries=args.stats_retries, timeout=args.stats_timeout
                    )
                except TimeoutError:
                    print(
                        f"[RPT {report_idx:03d} @ {elapsed:4d}s] "
                        f"pkts_window={len(pkts):4d} stats=TIMEOUT(retries={args.stats_retries})"
                    )
                    continue

                delta_ok = curr.rx_ok - prev.rx_ok
                delta_crc_err = curr.rx_crc_err - prev.rx_crc_err
                delta_drop = curr.rx_drop - prev.rx_drop
                delta_ovf = curr.rx_overflow - prev.rx_overflow

                cum_ok = curr.rx_ok - baseline.rx_ok
                cum_crc_err = curr.rx_crc_err - baseline.rx_crc_err
                cum_drop = curr.rx_drop - baseline.rx_drop
                cum_ovf = curr.rx_overflow - baseline.rx_overflow

                print(
                    f"[RPT {report_idx:03d} @ {elapsed:4d}s] "
                    f"pkts_window={len(pkts):4d} "
                    f"delta(ok={delta_ok},crc_err={delta_crc_err},drop={delta_drop},ovf={delta_ovf}) "
                    f"cum(ok={cum_ok},crc_err={cum_crc_err},drop={cum_drop},ovf={cum_ovf})"
                    f"{format_ll_stats(curr)}"
                )
                prev = curr
            else:
                print(f"[RPT {report_idx:03d} @ {elapsed:4d}s] pkts_window={len(pkts):4d}")

        print()
        step("Stop RX + summary")
        stopped_cleanly = False
        try:
            radio.stop_rx(retries=max(args.stats_retries, 5), timeout=max(args.stats_timeout, 1.5))
            stopped_cleanly = True
            ok("RX_STOP ACK")
        except TimeoutError:
            warn("RX_STOP timeout after retries; continuing with disconnect")

        ok(f"packets_total={total_packets}")
        try:
            final_stats = get_stats_with_retry(
                radio,
                retries=max(args.stats_retries, 5),
                timeout=max(args.stats_timeout, 3.0),
            )
            ok(
                f"stats_total ok={final_stats.rx_ok - baseline.rx_ok} "
                f"crc_err={final_stats.rx_crc_err - baseline.rx_crc_err} "
                f"drop={final_stats.rx_drop - baseline.rx_drop} "
                f"ovf={final_stats.rx_overflow - baseline.rx_overflow}"
                f"{format_ll_stats(final_stats)}"
            )
        except TimeoutError:
            if stopped_cleanly:
                warn("final GET_STATS timed out; packets_total is valid, metrics unavailable")
            else:
                warn("final GET_STATS timed out (likely because RX_STOP did not ACK)")
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
        fail(f"Unexpected error: {exc}")
        return 6
    finally:
        try:
            radio.stop_rx(retries=1, timeout=0.2)
        except Exception:
            pass
        radio.disconnect()


if __name__ == "__main__":
    sys.exit(main())
