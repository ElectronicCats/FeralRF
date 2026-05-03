"""F8f #9 — assert PENDING_COMMAND_IDS does not collide with Command."""

from feralrf.enums import PENDING_COMMAND_IDS, Command


def test_pending_command_ids_disjoint_from_command():
    command_values = {c.value for c in Command}
    pending_values = set(PENDING_COMMAND_IDS.values())
    overlap = command_values & pending_values
    assert overlap == set(), (
        f"PENDING_COMMAND_IDS values {sorted(overlap)} collide with Command. "
        f"Remove the colliding entries or reassign to free IDs."
    )


def test_pending_command_ids_does_not_contain_spectrum():
    """Removed in F8f #9 — spectrum has no firmware allocation yet."""
    assert "SPECTRUM_SCAN" not in PENDING_COMMAND_IDS
    assert "SPECTRUM_MONITOR" not in PENDING_COMMAND_IDS
    assert "SPECTRUM_STOP" not in PENDING_COMMAND_IDS
