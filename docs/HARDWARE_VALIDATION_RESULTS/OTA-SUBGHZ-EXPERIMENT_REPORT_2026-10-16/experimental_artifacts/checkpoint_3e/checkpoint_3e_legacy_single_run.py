"""Checkpoint 3E: one fixed legacy-helper OTA procedure, no reset or band command."""

from __future__ import annotations

import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "python"))

from feralrf import PHY, PROP_PRESETS, Packet, Radio, RxStreamError  # noqa: E402


TX_PORT = "COM33"
RX_PORT = "COM88"
PRESET_NAME = "wisun_915_fsk_50k"
MARKER = bytes.fromhex("DE AD BE EF")
TX_POWER_DBM = 0
TX_COUNT = 10


def now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def emit(event: str, **fields: object) -> None:
    values = " ".join(f"{key}={value!r}" for key, value in fields.items())
    print(f"{now()} {event}" + (f" {values}" if values else ""), flush=True)


def emit_item(window: str, ordinal: int, item: object) -> None:
    if isinstance(item, Packet):
        emit(
            f"{window}_PACKET",
            ordinal=ordinal,
            device_timestamp_us=item.timestamp_us,
            channel=item.channel,
            rssi_dbm=item.rssi_dbm,
            lqi=item.lqi,
            crc_ok=item.crc_ok,
            payload_hex=item.data.hex(" ").upper(),
            marker_present=MARKER in item.data,
            crc_good_marker=item.crc_ok and MARKER in item.data,
        )
    elif isinstance(item, RxStreamError):
        emit(
            f"{window}_ASYNC_ERROR",
            ordinal=ordinal,
            error_code=item.error_code,
            context=item.context,
        )
    else:
        emit(
            f"{window}_OTHER_OBJECT",
            ordinal=ordinal,
            object_type=type(item).__name__,
            value=repr(item),
        )


def summarize(items: list[object]) -> dict[str, int]:
    packets = [item for item in items if isinstance(item, Packet)]
    errors = [item for item in items if isinstance(item, RxStreamError)]
    others = [item for item in items if not isinstance(item, (Packet, RxStreamError))]
    return {
        "objects": len(items),
        "packets": len(packets),
        "async_errors": len(errors),
        "other_objects": len(others),
        "crc_good_packets": sum(1 for item in packets if item.crc_ok),
        "marker_packets": sum(1 for item in packets if MARKER in item.data),
        "crc_good_marker": sum(1 for item in packets if item.crc_ok and MARKER in item.data),
    }


def main() -> int:
    preset = PROP_PRESETS[PRESET_NAME].copy()
    emit(
        "TEST_BEGIN",
        checkpoint="3E",
        procedure="legacy 3.0 s negative / ten TX / 5.0 s positive",
        tx_port=TX_PORT,
        rx_port=RX_PORT,
        preset_name=PRESET_NAME,
        preset=preset,
        marker=MARKER.hex(" ").upper(),
        tx_power_dbm=TX_POWER_DBM,
        tx_count=TX_COUNT,
        fresh_cc_reset=False,
        band_command_issued=False,
        inherited_logical_band="Band: 0",
    )

    tx = Radio(port=TX_PORT, baudrate=921600)
    rx = Radio(port=RX_PORT, baudrate=921600)
    rx_started = False
    rx_stopped = False
    tx_accepted = 0
    neg: list[object] = []
    pos: list[object] = []
    fatal: BaseException | None = None
    tx_stats_after = None
    rx_stats_after = None

    try:
        emit("TX_CONNECT_BEGIN", port=TX_PORT)
        tx.connect()
        emit("TX_CONNECT_COMPLETE", port=TX_PORT)
        emit("RX_CONNECT_BEGIN", port=RX_PORT)
        rx.connect()
        emit("RX_CONNECT_COMPLETE", port=RX_PORT)

        emit("TX_INIT_BEGIN", port=TX_PORT)
        tx_info = tx.init()
        emit("TX_INIT_OK", info=tx_info)
        emit("RX_INIT_BEGIN", port=RX_PORT)
        rx_info = rx.init()
        emit("RX_INIT_OK", info=rx_info)

        emit("TX_SET_PHY_BEGIN", phy="PROPRIETARY_GFSK", channel=0)
        tx.set_phy(PHY.PROPRIETARY_GFSK, 0)
        emit("TX_SET_PHY_ACK")
        emit("TX_CONFIGURE_PROP_BEGIN", preset=preset)
        tx.configure_prop(**preset)
        emit("TX_CONFIGURE_PROP_ACK")

        emit("RX_SET_PHY_BEGIN", phy="PROPRIETARY_GFSK", channel=0)
        rx.set_phy(PHY.PROPRIETARY_GFSK, 0)
        emit("RX_SET_PHY_ACK")
        emit("RX_CONFIGURE_PROP_BEGIN", preset=preset)
        rx.configure_prop(**preset)
        emit("RX_CONFIGURE_PROP_ACK")

        emit("TX_SET_POWER_BEGIN", power_dbm=TX_POWER_DBM)
        tx.set_power(TX_POWER_DBM)
        emit("TX_SET_POWER_ACK")

        emit("RX_START_BEGIN")
        rx.start_rx()
        rx_started = True
        emit("RX_START_ACK")

        neg_start = time.monotonic()
        emit("NEGATIVE_WINDOW_BEGIN", requested_seconds=3.0)
        neg = list(rx.read_packets(timeout=3.0))
        neg_elapsed = time.monotonic() - neg_start
        for ordinal, item in enumerate(neg, 1):
            emit_item("NEGATIVE", ordinal, item)
        emit("NEGATIVE_WINDOW_COMPLETE", elapsed_seconds=neg_elapsed, **summarize(neg))

        for index in range(1, TX_COUNT + 1):
            emit("TX_ATTEMPT_BEGIN", index=index, payload=MARKER.hex(" ").upper())
            result = tx.transmit(MARKER, power_dbm=TX_POWER_DBM, timeout=2.0)
            tx_accepted += 1
            emit("TX_ATTEMPT_ACK", index=index, return_value=result)
            spacing_start = time.monotonic()
            time.sleep(0.1)
            emit(
                "TX_SPACING_COMPLETE",
                index=index,
                elapsed_seconds=time.monotonic() - spacing_start,
            )

        pos_start = time.monotonic()
        emit("POSITIVE_WINDOW_BEGIN", requested_seconds=5.0)
        pos = list(rx.read_packets(timeout=5.0))
        pos_elapsed = time.monotonic() - pos_start
        for ordinal, item in enumerate(pos, 1):
            emit_item("POSITIVE", ordinal, item)
        emit("POSITIVE_WINDOW_COMPLETE", elapsed_seconds=pos_elapsed, **summarize(pos))

        emit("RX_STOP_BEGIN")
        rx.stop_rx(retries=1, timeout=1.0)
        rx_stopped = True
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
        if rx_started and not rx_stopped:
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
                rx_stopped = True
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

    neg_summary = summarize(neg)
    pos_summary = summarize(pos)
    emit(
        "TEST_SUMMARY",
        tx_accepted=tx_accepted,
        tx_requested=TX_COUNT,
        negative=neg_summary,
        positive=pos_summary,
        tx_stats_after=tx_stats_after,
        rx_stats_after=rx_stats_after,
        fatal_exception=None if fatal is None else repr(fatal),
    )
    emit("TEST_END", exit_code=0 if fatal is None else 1)
    return 0 if fatal is None else 1


if __name__ == "__main__":
    raise SystemExit(main())
