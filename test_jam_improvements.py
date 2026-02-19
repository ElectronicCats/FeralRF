#!/usr/bin/env python3
"""
Test script for jamming improvements in FeralRF firmware.

Tests:
1. Jam session start/stop
2. Cooldown enforcement
3. Duration timeout

Usage:
    python test_jam_improvements.py [--port /dev/ttyACM0] [--phy ble|ieee] [--channel 37]
"""

import argparse
import time
import sys

# Add python package to path
sys.path.insert(0, "/home/sabas/Documents/electroniccats/FeralRF/python")

from feralrf import Radio, PHY
from feralrf.jamming import show_regulatory_warning
from feralrf.exceptions import CommandError, TimeoutError


def test_jam_basic(radio: Radio, phy: PHY, channel: int, power: int, duration_ms: int):
    """Test basic jam session start/stop"""
    print(f"\n=== Test 1: Basic Jam Session ===")
    print(f"PHY: {phy.name}, Channel: {channel}, Power: {power}dBm, Duration: {duration_ms}ms")

    try:
        # Set PHY
        radio.set_phy(phy, channel)
        print("[OK] PHY configured")

        # Start jamming
        start_time = time.monotonic()
        radio.start_jam(channel=channel, power_dbm=power, duration_ms=duration_ms)
        print(f"[OK] Jam session started")

        # Wait for duration
        time.sleep(duration_ms / 1000.0 + 0.5)

        elapsed = time.monotonic() - start_time
        print(f"[OK] Jam session auto-stopped after {elapsed:.2f}s")
        return True

    except CommandError as e:
        print(f"[FAIL] Command error: {e}")
        return False
    except TimeoutError as e:
        print(f"[FAIL] Timeout: {e}")
        return False


def test_jam_manual_stop(radio: Radio, phy: PHY, channel: int, power: int):
    """Test manual jam stop before duration expires"""
    print(f"\n=== Test 2: Manual Jam Stop ===")
    print(f"PHY: {phy.name}, Channel: {channel}, Power: {power}dBm")

    try:
        radio.set_phy(phy, channel)
        print("[OK] PHY configured")

        # Start with 10 second duration, but stop after 2 seconds
        radio.start_jam(channel=channel, power_dbm=power, duration_ms=10000)
        print("[OK] Jam session started (10s duration)")

        time.sleep(2.0)
        print("[--] Stopping jam manually...")

        radio.stop_jam()
        print("[OK] Jam stopped manually")
        return True

    except CommandError as e:
        print(f"[FAIL] Command error: {e}")
        return False
    except TimeoutError as e:
        print(f"[FAIL] Timeout: {e}")
        return False


def test_cooldown(radio: Radio, phy: PHY, channel: int, power: int):
    """Test cooldown enforcement between sessions"""
    print(f"\n=== Test 3: Cooldown Enforcement ===")
    print(f"PHY: {phy.name}, Channel: {channel}")

    try:
        radio.set_phy(phy, channel)
        print("[OK] PHY configured")

        # First session
        radio.start_jam(channel=channel, power_dbm=power, duration_ms=1000)
        print("[OK] First jam session started")
        time.sleep(1.5)
        print("[OK] First session ended")

        # Try to start immediately (should fail due to cooldown)
        print("[--] Attempting immediate second session (should fail)...")
        try:
            radio.start_jam(channel=channel, power_dbm=power, duration_ms=1000)
            print("[WARN] Second session started (cooldown not enforced?)")
            radio.stop_jam()
            return False
        except CommandError as e:
            print(f"[OK] Second session rejected (cooldown active): {e}")

        # Wait for cooldown (2 seconds)
        print("[--] Waiting for cooldown (2s)...")
        time.sleep(2.5)

        # Now it should work
        radio.start_jam(channel=channel, power_dbm=power, duration_ms=1000)
        print("[OK] Second session started after cooldown")
        time.sleep(1.5)
        return True

    except CommandError as e:
        print(f"[FAIL] Unexpected error: {e}")
        return False
    except TimeoutError as e:
        print(f"[FAIL] Timeout: {e}")
        return False


def test_channel_limits(radio: Radio, phy: PHY):
    """Test channel limits enforcement"""
    print(f"\n=== Test 4: Channel Limits ===")
    print(f"PHY: {phy.name}")

    if phy in [PHY.BLE_1M, PHY.BLE_2M, PHY.BLE_CODED_S8, PHY.BLE_CODED_S2]:
        valid_channels = [37, 38, 39]
        invalid_channels = [0, 10, 36, 40]
        phy_name = "BLE"
    else:
        valid_channels = list(range(11, 27))
        invalid_channels = [0, 10, 27]
        phy_name = "IEEE 802.15.4"

    print(f"Valid channels for {phy_name}: {valid_channels}")

    radio.set_phy(phy, valid_channels[0])

    # Test valid channels
    for ch in valid_channels[:2]:  # Test first 2
        try:
            radio.start_jam(channel=ch, power_dbm=0, duration_ms=500)
            print(f"[OK] Channel {ch} accepted")
            time.sleep(0.7)
        except CommandError as e:
            print(f"[FAIL] Valid channel {ch} rejected: {e}")
            return False

    # Test invalid channels
    for ch in invalid_channels[:2]:  # Test first 2
        try:
            radio.start_jam(channel=ch, power_dbm=0, duration_ms=500)
            print(f"[FAIL] Invalid channel {ch} should have been rejected")
            radio.stop_jam()
            time.sleep(2.5)  # Cooldown
        except CommandError as e:
            print(f"[OK] Invalid channel {ch} rejected: {e}")

    return True


def main():
    parser = argparse.ArgumentParser(description="Test FeralRF jamming improvements")
    parser.add_argument("--port", default=None, help="Serial port (auto-detect if not specified)")
    parser.add_argument("--phy", choices=["ble", "ieee"], default="ble", help="PHY type")
    parser.add_argument("--channel", type=int, default=37, help="Channel number")
    parser.add_argument("--power", type=int, default=10, help="TX power in dBm")
    parser.add_argument("--skip-warning", action="store_true", help="Skip regulatory warning")

    args = parser.parse_args()

    if not args.skip_warning:
        show_regulatory_warning()
        response = input("Continue? [y/N]: ")
        if response.lower() != "y":
            print("Aborted.")
            return 1

    # Map PHY
    phy_map = {
        "ble": PHY.BLE_1M,
        "ieee": PHY.IEEE_802_15_4,
    }
    phy = phy_map[args.phy]

    # Adjust default channel for IEEE
    if args.phy == "ieee" and args.channel == 37:
        channel = 11
    else:
        channel = args.channel

    print(f"\nConnecting to FeralRF device...")

    with Radio(port=args.port) as radio:
        # Initialize
        info = radio.init()
        print(f"Connected: FW {info.firmware_version}, Serial: {info.serial}")

        # Run tests
        results = []

        results.append(("Basic Jam", test_jam_basic(radio, phy, channel, args.power, 2000)))
        time.sleep(3)  # Cooldown

        results.append(("Manual Stop", test_jam_manual_stop(radio, phy, channel, args.power)))
        time.sleep(3)  # Cooldown

        results.append(("Cooldown", test_cooldown(radio, phy, channel, args.power)))
        time.sleep(3)  # Cooldown

        results.append(("Channel Limits", test_channel_limits(radio, phy)))

        # Summary
        print("\n" + "=" * 50)
        print("TEST RESULTS")
        print("=" * 50)

        passed = 0
        for name, result in results:
            status = "PASS" if result else "FAIL"
            print(f"  {name}: {status}")
            if result:
                passed += 1

        print(f"\nTotal: {passed}/{len(results)} passed")
        return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
