"""Checkpoint 3A only: one controlled wisun_915_fsk_50k OTA execution.

Experimental helper. Fixed explicit Cat-Bridge ports, one preset, one direction,
ten TX attempts, and no automatic test retry. Shell reset and band control are
performed and recorded separately before this helper is invoked.
"""

from __future__ import annotations

import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "python"))

from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError  # noqa: E402


TX_PORT = "COM33"
RX_PORT = "COM88"
if len(sys.argv) != 4:
    raise SystemExit(
        "Usage: python manual_preset_runner.py <preset> <marker_hex> <label>"
    )

PRESET_NAME = sys.argv[1]
MARKER = bytes.fromhex(sys.argv[2])
LABEL = sys.argv[3]

if PRESET_NAME not in PROP_PRESETS:
    raise SystemExit(f"Unknown preset: {PRESET_NAME}")

if len(MARKER) != 4:
    raise SystemExit("Marker must contain exactly 4 bytes")
TX_POWER_DBM = 0
TX_COUNT = 10


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def emit(event: str, **fields: object) -> None:
    rendered = " ".join(f"{key}={value!r}" for key, value in fields.items())
    print(f"{utc_now()} {event}" + (f" {rendered}" if rendered else ""), flush=True)


def main() -> int:
    preset = PROP_PRESETS[PRESET_NAME].copy()
    emit(
        "TEST_BEGIN",
        checkpoint="manual-campaign",
        label=LABEL,
        tx_port=TX_PORT,
        rx_port=RX_PORT,
        preset_name=PRESET_NAME,
        preset=preset,
        marker=MARKER.hex(" ").upper(),
        tx_power_dbm=TX_POWER_DBM,
        tx_count=TX_COUNT,
    )

    tx = Radio(port=TX_PORT, baudrate=921600)
    rx = Radio(port=RX_PORT, baudrate=921600)
    rx_started = False
    tx_accepted = 0
    received = []
    rx_async_errors = []
    tx_async_errors = []
    fatal: BaseException | None = None

    try:
        emit("TX_INIT_BEGIN", port=TX_PORT)
        tx_info = tx.init()
        emit("TX_INIT_OK", info=tx_info)

        emit("RX_INIT_BEGIN", port=RX_PORT)
        rx_info = rx.init()
        emit("RX_INIT_OK", info=rx_info)

        emit("TX_SET_PHY_BEGIN", phy="PROPRIETARY_GFSK", channel=0)
        tx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        emit("TX_SET_PHY_ACK")

        emit("RX_SET_PHY_BEGIN", phy="PROPRIETARY_GFSK", channel=0)
        rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        emit("RX_SET_PHY_ACK")

        emit("TX_CONFIGURE_PROP_BEGIN", preset=preset)
        tx.configure_prop(**preset)
        emit("TX_CONFIGURE_PROP_ACK")

        emit("RX_CONFIGURE_PROP_BEGIN", preset=preset)
        rx.configure_prop(**preset)
        emit("RX_CONFIGURE_PROP_ACK")

        emit("TX_SET_POWER_BEGIN", power_dbm=TX_POWER_DBM)
        tx.set_power(TX_POWER_DBM)
        emit("TX_SET_POWER_ACK")

        emit("TX_STATS_BEFORE_BEGIN")
        tx_stats_before = tx.get_stats()
        emit("TX_STATS_BEFORE", stats=tx_stats_before)

        emit("RX_STATS_BEFORE_BEGIN")
        rx_stats_before = rx.get_stats()
        emit("RX_STATS_BEFORE", stats=rx_stats_before)

        emit("RX_START_BEGIN")
        rx.start_rx()
        rx_started = True
        emit("RX_START_ACK")

        delay_start = time.monotonic()
        emit("PRE_TX_WAIT_BEGIN", requested_seconds=0.3)
        time.sleep(0.3)
        emit("PRE_TX_WAIT_COMPLETE", elapsed_seconds=time.monotonic() - delay_start)

        for index in range(1, TX_COUNT + 1):
            emit("TX_ATTEMPT_BEGIN", index=index, payload=MARKER.hex(" ").upper())
            try:
                result = tx.transmit(MARKER, power_dbm=TX_POWER_DBM)
            except Exception as exc:
                emit(
                    "TX_ATTEMPT_EXCEPTION",
                    index=index,
                    exception_type=type(exc).__name__,
                    exception=repr(exc),
                )
            else:
                tx_accepted += 1
                emit("TX_ATTEMPT_ACK", index=index, return_value=result)

            spacing_start = time.monotonic()
            time.sleep(0.1)
            emit(
                "TX_SPACING_COMPLETE",
                index=index,
                elapsed_seconds=time.monotonic() - spacing_start,
            )

        post_tx_start = time.monotonic()
        emit("POST_TX_WAIT_BEGIN", requested_seconds=1.0)
        time.sleep(1.0)
        emit("POST_TX_WAIT_COMPLETE", elapsed_seconds=time.monotonic() - post_tx_start)

        rx_window_start = time.monotonic()
        emit("RX_OBSERVATION_BEGIN", requested_seconds=2.0)
        for item in rx.read_packets(timeout=2.0):
            if isinstance(item, RxStreamError):
                rx_async_errors.append(item)
                emit(
                    "RX_ASYNC_ERROR",
                    error_code=item.error_code,
                    context=item.context,
                )
                continue

            received.append(item)
            emit(
                "RX_PACKET",
                ordinal=len(received),
                device_timestamp_us=item.timestamp_us,
                channel=item.channel,
                rssi_dbm=item.rssi_dbm,
                lqi=item.lqi,
                crc_ok=item.crc_ok,
                payload_hex=item.data.hex(" ").upper(),
                marker_present=MARKER in item.data,
            )
        emit(
            "RX_OBSERVATION_COMPLETE",
            elapsed_seconds=time.monotonic() - rx_window_start,
            packets=len(received),
            async_errors=len(rx_async_errors),
        )

        tx_error_window_start = time.monotonic()
        emit("TX_ASYNC_OBSERVATION_BEGIN", requested_seconds=0.5)
        for item in tx.read_packets(timeout=0.5):
            if isinstance(item, RxStreamError):
                tx_async_errors.append(item)
                emit(
                    "TX_ASYNC_ERROR",
                    error_code=item.error_code,
                    context=item.context,
                )
            else:
                emit(
                    "TX_UNEXPECTED_RX_PACKET",
                    device_timestamp_us=item.timestamp_us,
                    channel=item.channel,
                    rssi_dbm=item.rssi_dbm,
                    lqi=item.lqi,
                    crc_ok=item.crc_ok,
                    payload_hex=item.data.hex(" ").upper(),
                )
        emit(
            "TX_ASYNC_OBSERVATION_COMPLETE",
            elapsed_seconds=time.monotonic() - tx_error_window_start,
            async_errors=len(tx_async_errors),
        )

        emit("RX_STOP_BEGIN")
        rx.stop_rx(retries=1, timeout=1.0)
        rx_started = False
        emit("RX_STOP_ACK")

        emit("TX_STATS_AFTER_BEGIN")
        tx_stats_after = tx.get_stats()
        emit("TX_STATS_AFTER", stats=tx_stats_after)

        emit("RX_STATS_AFTER_BEGIN")
        rx_stats_after = rx.get_stats()
        emit("RX_STATS_AFTER", stats=rx_stats_after)

    except BaseException as exc:
        fatal = exc
        emit("TEST_FATAL_EXCEPTION", exception_type=type(exc).__name__, exception=repr(exc))
        traceback.print_exc()
    finally:
        if rx_started:
            emit("RX_STOP_CLEANUP_BEGIN")
            try:
                rx.stop_rx(retries=1, timeout=1.0)
            except Exception as exc:
                emit(
                    "RX_STOP_CLEANUP_EXCEPTION",
                    exception_type=type(exc).__name__,
                    exception=repr(exc),
                )
            else:
                emit("RX_STOP_CLEANUP_ACK")

        emit("TX_DISCONNECT_BEGIN", port=TX_PORT)
        try:
            tx.disconnect()
        except Exception as exc:
            emit(
                "TX_DISCONNECT_EXCEPTION",
                exception_type=type(exc).__name__,
                exception=repr(exc),
            )
        else:
            emit("TX_DISCONNECT_COMPLETE", port=TX_PORT)

        emit("RX_DISCONNECT_BEGIN", port=RX_PORT)
        try:
            rx.disconnect()
        except Exception as exc:
            emit(
                "RX_DISCONNECT_EXCEPTION",
                exception_type=type(exc).__name__,
                exception=repr(exc),
            )
        else:
            emit("RX_DISCONNECT_COMPLETE", port=RX_PORT)

    crc_good_marker = sum(1 for pkt in received if pkt.crc_ok and MARKER in pkt.data)
    emit(
        "TEST_SUMMARY",
        tx_accepted=tx_accepted,
        tx_requested=TX_COUNT,
        total_peer_packets=len(received),
        peer_crc_good_marker=crc_good_marker,
        peer_expected=TX_COUNT,
        rx_async_errors=len(rx_async_errors),
        tx_async_errors=len(tx_async_errors),
        fatal_exception=None if fatal is None else repr(fatal),
    )
    if fatal is not None:
        classification = "BLOCKED"
    elif (
        crc_good_marker == TX_COUNT
        and tx_accepted == TX_COUNT
        and not rx_async_errors
        and not tx_async_errors
    ):
        classification = "PASS-RF"
    elif 1 <= crc_good_marker < TX_COUNT:
        classification = "PARTIAL"
    elif crc_good_marker == 0:
        classification = "FAIL"
    else:
        classification = "INCONCLUSIVE"

    emit(
        "TEST_CLASSIFICATION",
        label=LABEL,
        preset=PRESET_NAME,
        classification=classification,
    )

    exit_code = 0 if classification == "PASS-RF" else 2
    emit("TEST_END", exit_code=exit_code)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
