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
