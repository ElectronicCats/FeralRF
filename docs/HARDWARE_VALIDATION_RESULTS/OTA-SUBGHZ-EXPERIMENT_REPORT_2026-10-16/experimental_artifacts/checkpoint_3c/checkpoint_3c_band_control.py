"""Checkpoint 3C logical band control only; intentionally performs no reset."""

from __future__ import annotations

import time
from datetime import datetime, timezone

import serial


DEVICES = (
    ("TX CatSniffer #2", "COM35"),
    ("RX CatSniffer #3", "COM87"),
)


def now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def emit(event: str, **fields: object) -> None:
    values = " ".join(f"{key}={value!r}" for key, value in fields.items())
    print(f"{now()} {event}" + (f" {values}" if values else ""), flush=True)


def read_for(port: serial.Serial, seconds: float) -> bytes:
    deadline = time.monotonic() + seconds
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            data.extend(chunk)
        else:
            time.sleep(0.01)
    return bytes(data)


def select_band(device: str, shell: str) -> None:
    emit("BAND_OPEN_ATTEMPT", device=device, shell=shell)
    port = serial.Serial(shell, 115200, timeout=0.05, write_timeout=1.0)
    try:
        emit("BAND_OPENED", device=device, shell=shell)
        port.reset_input_buffer()
        sent = port.write(b"band2\r\n")
        emit("BAND2_SENT", device=device, shell=shell, bytes_written=sent)
        band_response = read_for(port, 0.8)
        emit("BAND2_RESPONSE", device=device, shell=shell, response=band_response)

        sent = port.write(b"status\r\n")
        emit("BAND_STATUS_SENT", device=device, shell=shell, bytes_written=sent)
        status = read_for(port, 1.0)
        emit("BAND_STATUS_RESPONSE", device=device, shell=shell, response=status)

        verified = b"SUB-GHz Band" in band_response and b"Band: 1" in status
        emit(
            "BAND_CLASSIFICATION",
            device=device,
            shell=shell,
            level="LEVEL 2 / SOFTWARE STATE ONLY" if verified else "FAILED",
        )
        if not verified:
            raise RuntimeError(f"Logical band verification failed for {device} on {shell}")
    finally:
        port.close()
        emit("BAND_CLOSED", device=device, shell=shell)


def main() -> int:
    emit(
        "CHECKPOINT_3C_BAND_BEGIN",
        reset_performed=False,
        note="No boot, exit, Radio.reset_device, auto-reset, or equivalent reboot",
    )
    for device, shell in DEVICES:
        select_band(device, shell)
    emit("CHECKPOINT_3C_BAND_COMPLETE", reset_performed=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
