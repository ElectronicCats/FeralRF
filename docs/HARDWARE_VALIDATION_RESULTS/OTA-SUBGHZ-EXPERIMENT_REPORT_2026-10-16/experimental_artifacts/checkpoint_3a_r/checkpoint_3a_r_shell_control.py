"""Checkpoint 3A-R explicit Shell reset and logical band control only."""

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


def reset_device(device: str, shell: str) -> None:
    emit("RESET_OPEN_ATTEMPT", device=device, shell=shell)
    port = serial.Serial(shell, 115200, timeout=0.05, write_timeout=1.0)
    try:
        emit("RESET_OPENED", device=device, shell=shell)
        port.reset_input_buffer()
        sent = port.write(b"boot\r\n")
        emit("BOOT_SENT", device=device, shell=shell, bytes_written=sent)
        boot_response = read_for(port, 0.5)
        emit("BOOT_WAIT_COMPLETE", device=device, shell=shell, response=boot_response)

        sent = port.write(b"exit\r\n")
        emit("EXIT_SENT", device=device, shell=shell, bytes_written=sent)
        exit_response = read_for(port, 0.3)
        emit("EXIT_WAIT_COMPLETE", device=device, shell=shell, response=exit_response)

        settle_start = time.monotonic()
        emit("SETTLE_BEGIN", device=device, shell=shell, required_seconds=3.5)
        time.sleep(3.5)
        emit(
            "SETTLE_COMPLETE",
            device=device,
            shell=shell,
            elapsed_seconds=time.monotonic() - settle_start,
        )

        sent = port.write(b"status\r\n")
        emit("RESET_STATUS_SENT", device=device, shell=shell, bytes_written=sent)
        status = read_for(port, 1.0)
        emit("RESET_STATUS_RESPONSE", device=device, shell=shell, response=status)

        combined = boot_response + exit_response
        verified = b"BOOT" in combined and b"PASSTHROUGH" in combined and b"CC1352 FW:" in status
        emit("RESET_CLASSIFICATION", device=device, shell=shell, verified=verified)
        if not verified:
            raise RuntimeError(f"Reset control-path verification failed for {device} on {shell}")
    finally:
        port.close()
        emit("RESET_CLOSED", device=device, shell=shell)


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
    emit("CHECKPOINT_3A_R_SHELL_BEGIN")
    for device, shell in DEVICES:
        reset_device(device, shell)
    for device, shell in DEVICES:
        select_band(device, shell)
    emit("CHECKPOINT_3A_R_SHELL_COMPLETE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
