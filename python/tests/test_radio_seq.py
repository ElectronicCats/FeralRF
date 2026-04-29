"""Sequence-number generator tests.

The firmware reserves seq=0xFF for asynchronous error frames
(see firmware/cc1352/src/control_task.c:353 and host_if_task.c:96).
The Python parser at radio.py:395 drops any frame with seq=0xFF as
"async error", so the host MUST never emit a regular command with
that seq value — otherwise the legitimate response is discarded and
the call times out. These tests guard that invariant.

Pre-fix bug: capture_and_replay timed out on the 253rd transmit because
init(2) + set_phy(1) + 252 transmits put _seq exactly at 0xFF.
"""

from feralrf.radio import Radio


def test_next_seq_initial_state_returns_zero():
    radio = Radio()
    assert radio._next_seq() == 0


def test_next_seq_never_returns_0xff():
    radio = Radio()
    seen = {radio._next_seq() for _ in range(1024)}
    assert 0xFF not in seen, "seq=0xFF is reserved for firmware async errors"


def test_next_seq_cycle_covers_all_non_reserved_values():
    radio = Radio()
    seen = {radio._next_seq() for _ in range(512)}
    assert seen == set(
        range(0xFF)
    ), f"expected every value in 0..0xFE to be reachable, got {len(seen)} distinct"
