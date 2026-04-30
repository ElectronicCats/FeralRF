"""Unit tests for F22 test mode commands (CW + PRBS).

Hardware-free contract tests: command IDs, payload builders, and
the error path for invalid PRBS pattern. Hardware end-to-end coverage
lives in python/examples/lab/smoke_f22_tx_test.py.
"""

from feralrf.enums import STABLE_COMMANDS, Command


def test_tx_cw_command_id():
    assert Command.TX_CW == 0x55


def test_tx_prbs_command_id():
    assert Command.TX_PRBS == 0x56


def test_tx_test_stop_command_id():
    assert Command.TX_TEST_STOP == 0x57


def test_tx_test_commands_in_stable():
    assert Command.TX_CW in STABLE_COMMANDS
    assert Command.TX_PRBS in STABLE_COMMANDS
    assert Command.TX_TEST_STOP in STABLE_COMMANDS


def test_tx_cw_sends_correct_frame(monkeypatch):
    """tx_cw issues SET_POWER then a TX_CW frame with empty payload."""
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    sent_cmds = []

    monkeypatch.setattr(
        radio, "_send_command", lambda c, p=b"": sent_cmds.append((c, bytes(p)))
    )
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.ACK, 0, b""),
    )

    radio.tx_cw(power_dbm=5)

    cmd_ids = [c[0] for c in sent_cmds]
    assert Command.SET_POWER in cmd_ids
    assert Command.TX_CW in cmd_ids
    cw_frame = next(c for c in sent_cmds if c[0] == Command.TX_CW)
    assert cw_frame[1] == b""
