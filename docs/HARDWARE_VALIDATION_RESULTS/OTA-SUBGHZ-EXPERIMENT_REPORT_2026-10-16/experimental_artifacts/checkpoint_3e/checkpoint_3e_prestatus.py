"""Checkpoint 3E status-only gate. Sends no reset or band command."""

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


def main() -> int:
    emit(
        "CHECKPOINT_3E_PRESTATUS_BEGIN",
        reset_performed=False,
        band_command_issued=False,
    )
    observations: list[tuple[str, str, bytes]] = []
    for device, shell in DEVICES:
        emit("STATUS_OPEN_ATTEMPT", device=device, shell=shell)
        port = serial.Serial(shell, 115200, timeout=0.05, write_timeout=1.0)
        try:
            emit("STATUS_OPENED", device=device, shell=shell)
            port.reset_input_buffer()
            sent = port.write(b"status\r\n")
            emit("STATUS_SENT", device=device, shell=shell, bytes_written=sent)
            response = read_for(port, 1.0)
            emit("STATUS_RESPONSE", device=device, shell=shell, response=response)
            observations.append((device, shell, response))
        finally:
            port.close()
            emit("STATUS_CLOSED", device=device, shell=shell)

    for device, shell, response in observations:
        band_0 = b"Band: 0" in response
        emit("STATUS_CLASSIFICATION", device=device, shell=shell, inherited_band_0=band_0)
        if not band_0:
            raise RuntimeError(f"Required inherited Band: 0 not observed on {device} {shell}")

    emit(
        "CHECKPOINT_3E_PRESTATUS_COMPLETE",
        reset_performed=False,
        band_command_issued=False,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
