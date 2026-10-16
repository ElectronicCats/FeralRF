# FeralRF Validation

This directory contains the reproducible validation record for the
current FeralRF hardware campaign using two CatSniffer V3 devices.

Start here:
- validation_matrix.md — canonical human-readable test/result matrix.
- validation_matrix.csv — machine-readable canonical matrix.
- execution_report.md — current validation state and conclusions.
- procedure.md — safe reproduction procedure and acceptance criteria.
- historical_command_audit.md — audit of historical commands and incomplete runs.
- experiments/ — primary experimental reports and preserved observations.

Important:
- Command ACK does not imply RF transmission.
- A named preset does not imply protocol-stack interoperability.
- PASS/PARTIAL/FAIL/INCONCLUSIVE are independent from evidence levels A–F.
- Installed firmware provenance is not yet cryptographically tied to the audited source HEAD.