"""Checkpoint 3D pre-status and logical band1 contrast; intentionally no reset."""

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


def shell_exchange(device: str, shell: str, command: bytes, window: float) -> bytes:
    emit("SHELL_OPEN_ATTEMPT", device=device, shell=shell, command=command)
    port = serial.Serial(shell, 115200, timeout=0.05, write_timeout=1.0)
    try:
        emit("SHELL_OPENED", device=device, shell=shell, command=command)
        port.reset_input_buffer()
        sent = port.write(command + b"\r\n")
        emit("SHELL_COMMAND_SENT", device=device, shell=shell, command=command, bytes_written=sent)
        response = read_for(port, window)
        emit("SHELL_RESPONSE", device=device, shell=shell, command=command, response=response)
        return response
    finally:
        port.close()
        emit("SHELL_CLOSED", device=device, shell=shell, command=command)


def main() -> int:
    emit(
        "CHECKPOINT_3D_CONTROL_BEGIN",
        reset_performed=False,
        note="No boot, exit, Radio.reset_device, auto-reset, or equivalent reboot",
    )

    pre_status: dict[str, bytes] = {}
    for device, shell in DEVICES:
        response = shell_exchange(device, shell, b"status", 1.0)
        pre_status[shell] = response

    for device, shell in DEVICES:
        inherited = b"Band: 1" in pre_status[shell]
        emit("PRE_STATUS_CLASSIFICATION", device=device, shell=shell, inherited_band_1=inherited)
        if not inherited:
            raise RuntimeError(
                f"Pre-test state on {device} {shell} was not the required inherited Band: 1"
            )

    for device, shell in DEVICES:
        band_response = shell_exchange(device, shell, b"band1", 0.8)
        status = shell_exchange(device, shell, b"status", 1.0)
        accepted = b"2.4GHz Band" in band_response
        logical_band_0 = b"Band: 0" in status
        emit(
            "BAND1_CLASSIFICATION",
            device=device,
            shell=shell,
            accepted=accepted,
            logical_band_0=logical_band_0,
            level=(
                "LEVEL 2 / SOFTWARE STATE ONLY"
                if accepted and logical_band_0
                else "FAILED"
            ),
        )
        if not (accepted and logical_band_0):
            raise RuntimeError(f"Logical band1 verification failed for {device} on {shell}")

    emit("CHECKPOINT_3D_CONTROL_COMPLETE", reset_performed=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
