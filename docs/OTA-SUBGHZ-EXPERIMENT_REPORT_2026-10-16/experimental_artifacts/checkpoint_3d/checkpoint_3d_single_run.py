"""Checkpoint 3D wrapper around the exact Checkpoint 3A one-run OTA procedure."""

from __future__ import annotations

import importlib.util
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "checkpoint_3a" / "checkpoint_3a_single_run.py"
SPEC = importlib.util.spec_from_file_location("checkpoint_3a_exact_procedure", SOURCE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load exact 3A procedure from {SOURCE}")

procedure = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(procedure)
original_emit = procedure.emit


def contrast_emit(event: str, **fields: object) -> None:
    if event == "TEST_BEGIN" and fields.get("checkpoint") == "3A":
        fields["checkpoint"] = "3D"
        fields["fresh_cc_reset_performed"] = False
        fields["rp2040_logical_band"] = "band1 / Band: 0 / 2.4 GHz"
    original_emit(event, **fields)


procedure.emit = contrast_emit


if __name__ == "__main__":
    raise SystemExit(procedure.main())
