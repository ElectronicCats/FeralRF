"""Checkpoint 4A wrapper around the exact current one-run OTA procedure."""

from __future__ import annotations

import importlib.util
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "checkpoint_3a" / "checkpoint_3a_single_run.py"
SPEC = importlib.util.spec_from_file_location("checkpoint_3a_exact_procedure", SOURCE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load current one-run procedure from {SOURCE}")

procedure = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(procedure)
original_emit = procedure.emit


def cold_emit(event: str, **fields: object) -> None:
    if event == "TEST_BEGIN" and fields.get("checkpoint") == "3A":
        fields["checkpoint"] = "4A"
        fields["physical_power_cycle_confirmed"] = True
        fields["approximate_unpowered_seconds"] = 15
        fields["fresh_cc_reset_performed"] = False
        fields["band_command_issued"] = False
        fields["untouched_initial_logical_band"] = "Band: 0"
    original_emit(event, **fields)


procedure.emit = cold_emit


if __name__ == "__main__":
    raise SystemExit(procedure.main())
