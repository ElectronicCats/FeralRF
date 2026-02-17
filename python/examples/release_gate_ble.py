#!/usr/bin/env python3
"""
FeralRF - BLE release gate (single command)

Runs:
1) phase-2 smoke path
2) regression canary (smoke+short soak+stats checks)
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Optional

import serial.tools.list_ports


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def build_common_args(args: argparse.Namespace) -> List[str]:
    common = [
        "--baudrate",
        str(args.baudrate),
        "--phy",
        str(args.phy),
        "--channel",
        str(args.channel),
        "--power",
        str(args.power),
    ]
    if args.port:
        common.extend(["--port", args.port])
    return common


def build_common_args_for_port(args: argparse.Namespace, port: str) -> List[str]:
    common = [
        "--baudrate",
        str(args.baudrate),
        "--phy",
        str(args.phy),
        "--channel",
        str(args.channel),
        "--power",
        str(args.power),
        "--port",
        port,
    ]
    return common


def run_cmd(cmd: List[str]) -> int:
    return subprocess.run(cmd, check=False).returncode


def run_with_retries(name: str, cmd: List[str], attempts: int, delay_s: float) -> int:
    if attempts <= 0:
        attempts = 1

    for idx in range(1, attempts + 1):
        if idx > 1:
            print(f"[INFO] Retrying {name} ({idx}/{attempts}) after {delay_s:.1f}s...")
            time.sleep(max(delay_s, 0.0))
        rc = run_cmd(cmd)
        if rc == 0:
            return 0
        print(f"[WARN] {name} attempt {idx}/{attempts} failed (exit={rc})")
    return rc


def candidate_ports(preferred_port: Optional[str]) -> List[str]:
    ports = []
    for port in serial.tools.list_ports.comports():
        dev = port.device
        if not dev:
            continue
        if preferred_port is not None and dev == preferred_port:
            continue
        if ("ttyACM" in dev) or ("ttyUSB" in dev):
            ports.append(dev)
    return sorted(set(ports))


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF BLE release gate")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY number", type=int, default=0)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=37)
    parser.add_argument("--power", help="TX power in dBm", type=int, default=0)
    parser.add_argument(
        "--soak-duration", help="Canary short soak duration in seconds", type=int, default=60
    )
    parser.add_argument(
        "--report-every", help="Canary report interval in seconds", type=int, default=15
    )
    parser.add_argument(
        "--stats-timeout", help="Canary GET_STATS timeout in seconds", type=float, default=2.0
    )
    parser.add_argument("--stats-retries", help="Canary retries for GET_STATS", type=int, default=3)
    parser.add_argument(
        "--profile",
        help="Canary packet-floor profile",
        choices=("lab", "ci_manual", "quiet"),
        default="ci_manual",
    )
    parser.add_argument(
        "--min-packets",
        help="Canary min packets override (optional)",
        type=int,
        default=None,
    )
    parser.add_argument(
        "--smoke-attempts",
        help="Retry attempts for smoke gate",
        type=int,
        default=2,
    )
    parser.add_argument(
        "--canary-attempts",
        help="Retry attempts for canary gate",
        type=int,
        default=1,
    )
    parser.add_argument(
        "--retry-delay",
        help="Delay between retries in seconds",
        type=float,
        default=1.5,
    )
    parser.add_argument(
        "--disable-port-fallback",
        help="Disable automatic fallback probing to other serial ports when smoke fails",
        action="store_true",
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
    if args.smoke_attempts <= 0:
        fail("smoke-attempts must be > 0")
        return 2
    if args.canary_attempts <= 0:
        fail("canary-attempts must be > 0")
        return 2
    if args.retry_delay < 0:
        fail("retry-delay must be >= 0")
        return 2

    examples_dir = Path(__file__).resolve().parent
    smoke_script = examples_dir / "smoke_phase2.py"
    canary_script = examples_dir / "canary_regression.py"

    active_port = args.port

    print("FeralRF BLE Release Gate")
    print("========================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} channel={args.channel} "
        f"power={args.power} soak_duration={args.soak_duration}s report_every={args.report_every}s "
        f"stats_timeout={args.stats_timeout}s stats_retries={args.stats_retries} profile={args.profile} "
        f"smoke_attempts={args.smoke_attempts} canary_attempts={args.canary_attempts} "
        f"retry_delay={args.retry_delay}s port_fallback={'off' if args.disable_port_fallback else 'on'}"
    )
    print()

    step("Smoke gate")
    if active_port is None:
        smoke_common_args = build_common_args(args)
    else:
        smoke_common_args = build_common_args_for_port(args, active_port)
    smoke_cmd = [sys.executable, str(smoke_script), *smoke_common_args]
    rc = run_with_retries(
        name="smoke gate",
        cmd=smoke_cmd,
        attempts=args.smoke_attempts,
        delay_s=args.retry_delay,
    )
    if rc != 0 and not args.disable_port_fallback:
        print()
        if active_port is None:
            print("[WARN] Smoke failed in auto mode; probing serial ports explicitly...")
        else:
            print(
                f"[WARN] Smoke failed on fixed port {active_port}; probing alternative ports automatically..."
            )

        probes = candidate_ports(active_port)
        if not probes:
            print("[WARN] No /dev/ttyACM* or /dev/ttyUSB* ports detected for fallback probing.")

        for port in probes:
            print(f"[INFO] Trying fallback port: {port}")
            smoke_cmd = [
                sys.executable,
                str(smoke_script),
                *build_common_args_for_port(args, port),
            ]
            rc_probe = run_with_retries(
                name=f"smoke gate on {port}",
                cmd=smoke_cmd,
                attempts=1,
                delay_s=args.retry_delay,
            )
            if rc_probe == 0:
                active_port = port
                rc = 0
                print(f"[INFO] Fallback port selected: {active_port}")
                break

    if rc != 0:
        fail(f"Smoke gate failed (exit={rc})")
        return 10
    ok("Smoke gate PASS")
    print()

    step("Canary gate")
    if active_port is None:
        canary_common_args = build_common_args(args)
    else:
        canary_common_args = build_common_args_for_port(args, active_port)
    canary_cmd = [
        sys.executable,
        str(canary_script),
        *canary_common_args,
        "--soak-duration",
        str(args.soak_duration),
        "--report-every",
        str(args.report_every),
        "--stats-timeout",
        str(args.stats_timeout),
        "--stats-retries",
        str(args.stats_retries),
        "--profile",
        args.profile,
    ]
    if args.min_packets is not None:
        canary_cmd.extend(["--min-packets", str(args.min_packets)])
    rc = run_with_retries(
        name="canary gate",
        cmd=canary_cmd,
        attempts=args.canary_attempts,
        delay_s=args.retry_delay,
    )
    if rc != 0:
        fail(f"Canary gate failed (exit={rc})")
        return 20
    ok("Canary gate PASS")
    print()

    ok("BLE RELEASE GATE PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
