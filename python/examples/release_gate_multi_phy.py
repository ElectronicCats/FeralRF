#!/usr/bin/env python3
"""
FeralRF - Multi-PHY release gate (BLE baseline + PHY4 RX/TX/burst/continuous + BLE TX)
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Optional


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def run_cmd(cmd: List[str]) -> int:
    return subprocess.run(cmd, check=False).returncode


def run_with_retries(
    name: str,
    cmd: List[str],
    attempts: int,
    delay_s: float,
    on_retry=None,
) -> int:
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
        if idx < attempts and on_retry is not None:
            try:
                on_retry(idx, rc)
            except Exception as exc:  # pragma: no cover - defensive
                print(f"[WARN] Retry hook failed for {name}: {exc}")
    return rc


def maybe_port_args(port: Optional[str]) -> List[str]:
    if port:
        return ["--port", port]
    return []


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF multi-PHY release gate")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--retry-delay", help="Delay between retries", type=float, default=2.0)
    parser.add_argument("--ble-attempts", help="Retries for BLE gate", type=int, default=2)
    parser.add_argument("--phy4-rx-attempts", help="Retries for PHY4 RX smoke", type=int, default=2)
    parser.add_argument("--phy4-tx-attempts", help="Retries for PHY4 TX smoke", type=int, default=2)
    parser.add_argument(
        "--phy4-tx-burst-attempts",
        help="Retries for PHY4 TX burst smoke",
        type=int,
        default=2,
    )
    parser.add_argument(
        "--phy4-tx-cont-attempts",
        help="Retries for PHY4 TX continuous smoke",
        type=int,
        default=2,
    )
    parser.add_argument("--ble-tx-attempts", help="Retries for BLE TX smoke", type=int, default=2)
    parser.add_argument(
        "--ble-tx-recovery-smoke-attempts",
        help="Recovery smoke attempts before each BLE TX retry",
        type=int,
        default=1,
    )

    parser.add_argument("--ble-phy", type=int, default=0)
    parser.add_argument("--ble-channel", type=int, default=37)
    parser.add_argument("--ble-power", type=int, default=0)
    parser.add_argument("--ble-soak-duration", type=int, default=60)
    parser.add_argument("--ble-report-every", type=int, default=15)
    parser.add_argument("--ble-stats-timeout", type=float, default=2.0)
    parser.add_argument("--ble-stats-retries", type=int, default=3)
    parser.add_argument("--ble-profile", choices=("lab", "ci_manual", "quiet"), default="ci_manual")
    parser.add_argument("--ble-min-packets", type=int, default=None)
    parser.add_argument("--ble-smoke-attempts", type=int, default=2)
    parser.add_argument("--ble-canary-attempts", type=int, default=1)

    parser.add_argument("--phy4-rx-channel", type=int, default=25)
    parser.add_argument("--phy4-rx-duration", type=float, default=10.0)

    parser.add_argument("--phy4-tx-channel", type=int, default=25)
    parser.add_argument("--phy4-tx-power", type=int, default=0)
    parser.add_argument("--phy4-tx-packet-hex", default="01020304")
    parser.add_argument("--phy4-tx-timeout", type=float, default=10.0)
    parser.add_argument("--phy4-tx-burst-count", type=int, default=5)
    parser.add_argument("--phy4-tx-burst-interval-us", type=int, default=5000)
    parser.add_argument("--phy4-tx-cont-interval-us", type=int, default=5000)
    parser.add_argument("--phy4-tx-cont-run-seconds", type=float, default=1.0)

    parser.add_argument("--ble-tx-channel", type=int, default=37)
    parser.add_argument("--ble-tx-power", type=int, default=0)
    parser.add_argument("--ble-tx-payload-hex", default="020106")
    parser.add_argument("--ble-tx-timeout", type=float, default=10.0)

    args = parser.parse_args()

    if args.retry_delay < 0:
        fail("retry-delay must be >= 0")
        return 2

    examples_dir = Path(__file__).resolve().parent
    ble_gate_script = examples_dir / "release_gate_ble.py"
    smoke_phase2_script = examples_dir / "smoke_phase2.py"
    phy4_rx_script = examples_dir / "smoke_phy4_ieee154.py"
    phy4_tx_script = examples_dir / "smoke_tx_phase1.py"
    phy4_tx_burst_script = examples_dir / "smoke_tx_burst_phase1.py"
    phy4_tx_cont_script = examples_dir / "smoke_tx_continuous_phase1.py"
    ble_tx_script = examples_dir / "smoke_tx_ble_phase1.py"

    print("FeralRF Multi-PHY Release Gate")
    print("==============================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} retry_delay={args.retry_delay}s "
        f"ble_attempts={args.ble_attempts} phy4_rx_attempts={args.phy4_rx_attempts} "
        f"phy4_tx_attempts={args.phy4_tx_attempts} "
        f"phy4_tx_burst_attempts={args.phy4_tx_burst_attempts} "
        f"phy4_tx_cont_attempts={args.phy4_tx_cont_attempts} "
        f"ble_tx_attempts={args.ble_tx_attempts} "
        f"ble_tx_recovery_smoke_attempts={args.ble_tx_recovery_smoke_attempts}"
    )
    print(
        f"ble(phy={args.ble_phy},ch={args.ble_channel},power={args.ble_power},"
        f"soak={args.ble_soak_duration}s,profile={args.ble_profile}) "
        f"phy4_rx(ch={args.phy4_rx_channel},dur={args.phy4_rx_duration}s) "
        f"phy4_tx(ch={args.phy4_tx_channel},power={args.phy4_tx_power}) "
        f"phy4_tx_burst(count={args.phy4_tx_burst_count},interval_us={args.phy4_tx_burst_interval_us}) "
        f"phy4_tx_cont(interval_us={args.phy4_tx_cont_interval_us},run={args.phy4_tx_cont_run_seconds}s) "
        f"ble_tx(ch={args.ble_tx_channel},power={args.ble_tx_power})"
    )
    print()

    step("BLE baseline gate (smoke + canary)")
    ble_gate_cmd = [
        sys.executable,
        str(ble_gate_script),
        "--baudrate",
        str(args.baudrate),
        "--phy",
        str(args.ble_phy),
        "--channel",
        str(args.ble_channel),
        "--power",
        str(args.ble_power),
        "--soak-duration",
        str(args.ble_soak_duration),
        "--report-every",
        str(args.ble_report_every),
        "--stats-timeout",
        str(args.ble_stats_timeout),
        "--stats-retries",
        str(args.ble_stats_retries),
        "--profile",
        args.ble_profile,
        "--smoke-attempts",
        str(args.ble_smoke_attempts),
        "--canary-attempts",
        str(args.ble_canary_attempts),
        "--retry-delay",
        str(args.retry_delay),
        *maybe_port_args(args.port),
    ]
    if args.ble_min_packets is not None:
        ble_gate_cmd.extend(["--min-packets", str(args.ble_min_packets)])
    rc = run_with_retries("BLE baseline gate", ble_gate_cmd, args.ble_attempts, args.retry_delay)
    if rc != 0:
        fail(f"BLE baseline gate failed (exit={rc})")
        return 10
    ok("BLE baseline gate PASS")
    print()

    step("PHY4 RX smoke")
    phy4_rx_cmd = [
        sys.executable,
        str(phy4_rx_script),
        "--baudrate",
        str(args.baudrate),
        "--channel",
        str(args.phy4_rx_channel),
        "--duration",
        str(args.phy4_rx_duration),
        *maybe_port_args(args.port),
    ]
    rc = run_with_retries("PHY4 RX smoke", phy4_rx_cmd, args.phy4_rx_attempts, args.retry_delay)
    if rc != 0:
        fail(f"PHY4 RX smoke failed (exit={rc})")
        return 20
    ok("PHY4 RX smoke PASS")
    print()

    step("PHY4 TX smoke")
    phy4_tx_cmd = [
        sys.executable,
        str(phy4_tx_script),
        "--baudrate",
        str(args.baudrate),
        "--phy",
        "4",
        "--channel",
        str(args.phy4_tx_channel),
        "--power",
        str(args.phy4_tx_power),
        "--packet-hex",
        args.phy4_tx_packet_hex,
        "--tx-timeout",
        str(args.phy4_tx_timeout),
        *maybe_port_args(args.port),
    ]
    rc = run_with_retries("PHY4 TX smoke", phy4_tx_cmd, args.phy4_tx_attempts, args.retry_delay)
    if rc != 0:
        fail(f"PHY4 TX smoke failed (exit={rc})")
        return 30
    ok("PHY4 TX smoke PASS")
    print()

    step("PHY4 TX burst smoke")
    phy4_tx_burst_cmd = [
        sys.executable,
        str(phy4_tx_burst_script),
        "--baudrate",
        str(args.baudrate),
        "--phy",
        "4",
        "--channel",
        str(args.phy4_tx_channel),
        "--power",
        str(args.phy4_tx_power),
        "--packet-hex",
        args.phy4_tx_packet_hex,
        "--count",
        str(args.phy4_tx_burst_count),
        "--interval-us",
        str(args.phy4_tx_burst_interval_us),
        "--tx-timeout",
        str(args.phy4_tx_timeout),
        *maybe_port_args(args.port),
    ]
    rc = run_with_retries(
        "PHY4 TX burst smoke",
        phy4_tx_burst_cmd,
        args.phy4_tx_burst_attempts,
        args.retry_delay,
    )
    if rc != 0:
        fail(f"PHY4 TX burst smoke failed (exit={rc})")
        return 35
    ok("PHY4 TX burst smoke PASS")
    print()

    step("PHY4 TX continuous smoke")
    phy4_tx_cont_cmd = [
        sys.executable,
        str(phy4_tx_cont_script),
        "--baudrate",
        str(args.baudrate),
        "--phy",
        "4",
        "--channel",
        str(args.phy4_tx_channel),
        "--power",
        str(args.phy4_tx_power),
        "--packet-hex",
        args.phy4_tx_packet_hex,
        "--interval-us",
        str(args.phy4_tx_cont_interval_us),
        "--run-seconds",
        str(args.phy4_tx_cont_run_seconds),
        "--tx-timeout",
        str(args.phy4_tx_timeout),
        *maybe_port_args(args.port),
    ]
    rc = run_with_retries(
        "PHY4 TX continuous smoke",
        phy4_tx_cont_cmd,
        args.phy4_tx_cont_attempts,
        args.retry_delay,
    )
    if rc != 0:
        fail(f"PHY4 TX continuous smoke failed (exit={rc})")
        return 37
    ok("PHY4 TX continuous smoke PASS")
    print()

    step("BLE TX smoke")
    ble_tx_cmd = [
        sys.executable,
        str(ble_tx_script),
        "--baudrate",
        str(args.baudrate),
        "--channel",
        str(args.ble_tx_channel),
        "--power",
        str(args.ble_tx_power),
        "--payload-hex",
        args.ble_tx_payload_hex,
        "--tx-timeout",
        str(args.ble_tx_timeout),
        *maybe_port_args(args.port),
    ]

    def ble_tx_retry_hook(attempt_idx: int, rc: int) -> None:
        print(
            f"[INFO] BLE TX recovery before retry {attempt_idx + 1}/{args.ble_tx_attempts} "
            f"(last exit={rc})"
        )
        recovery_cmd = [
            sys.executable,
            str(smoke_phase2_script),
            "--baudrate",
            str(args.baudrate),
            "--phy",
            str(args.ble_phy),
            "--channel",
            str(args.ble_tx_channel),
            "--power",
            str(args.ble_tx_power),
            *maybe_port_args(args.port),
        ]
        recovery_rc = run_with_retries(
            "BLE TX recovery smoke",
            recovery_cmd,
            args.ble_tx_recovery_smoke_attempts,
            args.retry_delay,
        )
        if recovery_rc == 0:
            print("[INFO] BLE TX recovery smoke PASS")
        else:
            print(f"[WARN] BLE TX recovery smoke failed (exit={recovery_rc})")

    rc = run_with_retries(
        "BLE TX smoke",
        ble_tx_cmd,
        args.ble_tx_attempts,
        args.retry_delay,
        on_retry=ble_tx_retry_hook,
    )
    if rc != 0:
        fail(f"BLE TX smoke failed (exit={rc})")
        return 40
    ok("BLE TX smoke PASS")
    print()

    ok("MULTI-PHY RELEASE GATE PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
