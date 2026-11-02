"""Checkpoint 4A untouched post-power-cycle status: one status per explicit Shell."""

from __future__ import annotations

import time
from datetime import datetime, timezone

import serial


DEVICES = (
    ("CatSniffer #2", "COM35"),
    ("CatSniffer #3", "COM87"),
)


def now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


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


for device, shell in DEVICES:
    print(f"{now()} INITIAL_STATUS_OPEN_ATTEMPT device={device!r} shell={shell!r}", flush=True)
    port = serial.Serial(shell, 115200, timeout=0.05, write_timeout=1.0)
    try:
        print(f"{now()} INITIAL_STATUS_OPENED device={device!r} shell={shell!r}", flush=True)
        port.reset_input_buffer()
        sent = port.write(b"status\r\n")
        print(
            f"{now()} INITIAL_STATUS_SENT device={device!r} shell={shell!r} bytes_written={sent}",
            flush=True,
        )
        response = read_for(port, 1.0)
        print(
            f"{now()} INITIAL_STATUS_RESPONSE device={device!r} shell={shell!r} literal={response!r}",
            flush=True,
        )
    finally:
        port.close()
        print(f"{now()} INITIAL_STATUS_CLOSED device={device!r} shell={shell!r}", flush=True)
