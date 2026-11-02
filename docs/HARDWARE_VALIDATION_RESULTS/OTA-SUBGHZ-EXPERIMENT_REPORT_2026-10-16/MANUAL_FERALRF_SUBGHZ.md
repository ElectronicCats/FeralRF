**# Manual FeralRF Sub-GHz OTA Runbook**

This runbook was prepared from the current source and recovery evidence. No command below was executed while preparing it.

Fixed device mapping:

\| Unit | Role | Bridge | Shell |

\|---|---|---:|---:|

\| CatSniffer #2 | TX | \`COM33\` | \`COM35\` |

\| CatSniffer #3 | RX | \`COM88\` | \`COM87\` |

Never derive a Shell port from a Bridge port.

The validated reference is CatSniffer #2 → #3 using \`wisun_915_fsk_50k\`, whose actual configured frequency is 902.2 MHz. Formal recovery evidence is 60/60 peer CRC-good marker packets.

**---**

**## 1. Test-state model**

These are separate state domains:

\| State | What it contains | Closing Python/PowerShell | Closing Shell port | Full power removal |

\|---|---|---|---|---|

\| A. PowerShell | Current directory, \`PYTHONPATH\`, session functions | Lost when that PowerShell closes | No effect | No direct effect |

\| B. Python environment | Imported checkout, Python process, \`Radio\` objects | Objects and imports are destroyed | No effect | No direct effect |

\| C. Bridge USB connection | Open Windows handle to \`COM33\`/\`COM88\` | Normally released when process exits | Independent | Device disappears and re-enumerates |

\| D. RP2040 band state | Cached \`Band\` value and intended GPIO selection | Survives host process closure; 3D→3E demonstrated this | Survives Shell closure | Reinitialized on RP power-up; the exact cold state is not guaranteed universally |

\| E. CC1352 runtime | MCU/RAM, firmware state, TI RF-driver state | Can continue after host disconnect | Shell closure alone does not reset it | Reinitialized if the CC is genuinely unpowered |

\| F. Configured CC PHY | Last \`set_phy()\` and \`configure_prop()\` state | Persistence after host closure is not established; every formal run reconfigured it | No direct effect | Lost/reinitialized |

\| G. RX session | Whether firmware RX is active | May continue if cleanup was skipped; do not rely on process exit | Independent | Ends |

\| H. TX session | Individual TX commands and last CC configuration | A completed \`transmit()\` is discrete; configuration persistence is not established | Independent | Ends/reinitializes |

Important consequences:

\- Resetting the CC1352 is not the same as selecting the RP2040 external band.

\- \`band2\` does not configure the CC1352 PHY.

\- \`set_phy()\` does not start RX.

\- \`start_rx()\` must be issued after the intended PHY and proprietary parameters are configured.

\- Merely closing a serial terminal sends no command. Typing the Shell command \`exit\` is different: it changes RP passthrough mode and resets the CC1352.

\- Disconnecting USB is a full power-cycle only if no other power source remains attached. Otherwise the electrical state is UNKNOWN.

\- Always create fresh \`Radio\` objects, explicitly configure both endpoints, stop RX, and disconnect cleanly.

**---**

**## 2. Manual PC setup**

Open a fresh PowerShell:

\`\`\`powershell

Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF'

$env\:PYTHONPATH = (Resolve-Path '.\python').Path

python --version

python -c "import sys, feralrf; print('Python:',sys.version); print('feralrf version:',feralrf.\_\_version\_\_); print('feralrf source:',feralrf.\_\_file\_\_); print('sys.path[0]:',sys.path[0])"

python -c "from pathlib import Path; import feralrf; expected=(Path.cwd()/'python'/'feralrf').resolve(); actual=Path(feralrf.\_\_file\_\_).resolve().parent; print('expected:',expected); print('actual:',actual); assert actual==expected, 'WRONG FeralRF import'; print('IMPORT VERIFIED')"

python -c "from feralrf import PROP_PRESETS; print(PROP_PRESETS['wisun_915_fsk_50k'])"

\`\`\`

Expected source path:

\`\`\`text

C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF\python\feralrf\\\_\_init\_\_.py

\`\`\`

Expected reference dictionary:

\`\`\`python

{

    'frequency_hz': 902200000,

    'mod_type': 0,

    'symbol_rate': 50000,

    'deviation': 100,

    'rx_bw': 82,

    'sync_word': 2466996702

}

\`\`\`

\`2466996702\` is decimal \`0x930B51DE\`.

Optionally verify Windows enumeration without opening any port:

\`\`\`powershell

Get-CimInstance Win32_SerialPort |

    Sort-Object DeviceID |

    Select-Object DeviceID, Name, PNPDeviceID |

    Format-List

\`\`\`

Confirm explicitly:

\- CatSniffer #2: Bridge \`COM33\`, Shell \`COM35\`

\- CatSniffer #3: Bridge \`COM88\`, Shell \`COM87\`

If enumeration differs, stop and establish the mapping through USB identity and MI interfaces. Do not apply arithmetic.

**---**

**## 3. Baseline Shell-state recording**

Shell baud rate is \`115200\`.

Query CatSniffer #2 once:

\`\`\`powershell

python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5,write_timeout=1); s.reset_input_buffer(); s.write(b'status\r\n'); s.flush(); time.sleep(1); print(s.read_all().decode(errors='replace'),end=''); s.close()"

\`\`\`

Query CatSniffer #3 once:

\`\`\`powershell

python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5,write_timeout=1); s.reset_input_buffer(); s.write(b'status\r\n'); s.flush(); time.sleep(1); print(s.read_all().decode(errors='replace'),end=''); s.close()"

\`\`\`

Record:

\- \`Mode\`

\- \`Band\`

\- \`Radio\`

\- \`LoRa\`

\- \`LoRa Mode\`

\- RP \`FW\`

\- displayed \`CC1352 FW\`

\- \`uart_overrun\`

\- \`ring_dropped\`

The one-shot commands close the Shell without sending \`exit\`. Do not type \`exit\` merely to close a serial terminal.

A \`status\` response is software-state evidence. It does not establish GPIO8/9/10, CTF1/2/3, the U2 throw, RF energy, or antenna-path loss.

**---**

**## 4. Four preparation modes**

**### Mode A — Minimal/current working baseline**

This is the preferred first campaign mode.

Do not send:

\`\`\`text

boot

exit

band1

band2

\`\`\`

Do not call:

\`\`\`python

Radio.reset_device()

\`\`\`

For every run:

1\. Record Shell \`status\`.

2\. Create fresh \`Radio\` objects.

3\. Call \`init()\` on both.

4\. Call \`set_phy(PHY.PROPRIETARY_GFSK, channel=0)\` on both.

5\. Apply the same preset dictionary to both with \`configure_prop()\`.

6\. Set TX power to 0 dBm.

7\. Start RX first.

8\. Perform exactly one scheduled run.

9\. Stop RX and disconnect.

This reproduces the essential 4A/current manual condition. Checkpoint 4A passed 10/10 after a genuine power-cycle with no explicit reset or band command.

**### Mode B — Explicit CC1352 reset control**

Do not use \`Radio.reset_device()\` because current production code still derives the Shell as Bridge+2.

Define this temporary PowerShell function:

\`\`\`powershell

function Invoke-ExplicitCcReset {

    param(

        [Parameter(Mandatory=$true)]

        [ValidateSet('COM35','COM87')]

        [string]$ShellPort

    )

    @'

import serial

import sys

import time

port_name = sys.argv[1]

def read_for(port, seconds):

    deadline = time.monotonic() + seconds

    data = bytearray()

    while time.monotonic() < deadline:

        chunk = port.read(port.in_waiting or 1)

        if chunk:

            data.extend(chunk)

        else:

            time.sleep(0.01)

    return bytes(data)

print(f"RESET_BEGIN shell={port_name}", flush=True)

port = serial.Serial(port_name, 115200, timeout=0.05, write_timeout=1.0)

try:

    port.reset_input_buffer()

    port.write(b"boot\r\n")

    port.flush()

    boot_response = read_for(port, 0.5)

    print("BOOT_RESPONSE_BEGIN")

    print(boot_response.decode(errors="replace"), end="")

    print("BOOT_RESPONSE_END")

    port.write(b"exit\r\n")

    port.flush()

    exit_response = read_for(port, 0.3)

    print("EXIT_RESPONSE_BEGIN")

    print(exit_response.decode(errors="replace"), end="")

    print("EXIT_RESPONSE_END")

    print("SETTLE_BEGIN seconds=3.5", flush=True)

    time.sleep(3.5)

    print("SETTLE_COMPLETE", flush=True)

    port.write(b"status\r\n")

    port.flush()

    status_response = read_for(port, 1.0)

    print("STATUS_RESPONSE_BEGIN")

    print(status_response.decode(errors="replace"), end="")

    print("STATUS_RESPONSE_END")

finally:

    port.close()

    print(f"RESET_END shell={port_name}", flush=True)

'@ | python - $ShellPort

}

\`\`\`

Reset the endpoints independently:

\`\`\`powershell

Invoke-ExplicitCcReset -ShellPort COM35

\`\`\`

Review and save its complete response before continuing:

\`\`\`powershell

Invoke-ExplicitCcReset -ShellPort COM87

\`\`\`

Expected command responses:

\`\`\`text

BOOT

PASSTHROUGH

\`\`\`

Source behavior:

\- \`boot\` places the CC1352 boot pin into boot mode, resets the CC1352, and changes the RP-to-CC UART to 500000 baud.

\- \`exit\` returns the boot pin to input/pull-up, resets the CC1352 again, and restores the RP-to-CC UART to 921600 baud.

\- This resets the CC1352. It does not reset the RP2040.

\- It does not intentionally change the RP cached band state.

\- It does not prove the electrical U2 state.

\- A literal \`BOOT\`/\`PASSTHROUGH\` response proves RP command handling. It is not, by itself, independent electrical measurement of the reset pin.

The recovery campaign used:

\- 0.5 seconds after \`boot\`

\- 0.3 seconds after \`exit\`

\- at least 3.5 seconds final settling

Current evidence does not show this reset is required for every successful run. It remains an optional experimental control.

**### Mode C — Explicit logical Sub-GHz control**

Define:

\`\`\`powershell

function Invoke-ExplicitBand2 {

    param(

        [Parameter(Mandatory=$true)]

        [ValidateSet('COM35','COM87')]

        [string]$ShellPort

    )

    @'

import serial

import sys

import time

port_name = sys.argv[1]

def read_for(port, seconds):

    deadline = time.monotonic() + seconds

    data = bytearray()

    while time.monotonic() < deadline:

        chunk = port.read(port.in_waiting or 1)

        if chunk:

            data.extend(chunk)

        else:

            time.sleep(0.01)

    return bytes(data)

port = serial.Serial(port_name, 115200, timeout=0.05, write_timeout=1.0)

try:

    port.reset_input_buffer()

    port.write(b"band2\r\n")

    port.flush()

    response = read_for(port, 0.8)

    print("BAND2_RESPONSE_BEGIN")

    print(response.decode(errors="replace"), end="")

    print("BAND2_RESPONSE_END")

    port.write(b"status\r\n")

    port.flush()

    status = read_for(port, 1.0)

    print("STATUS_RESPONSE_BEGIN")

    print(status.decode(errors="replace"), end="")

    print("STATUS_RESPONSE_END")

finally:

    port.close()

'@ | python - $ShellPort

}

\`\`\`

Apply independently:

\`\`\`powershell

Invoke-ExplicitBand2 -ShellPort COM35

\`\`\`

Review it, then:

\`\`\`powershell

Invoke-ExplicitBand2 -ShellPort COM87

\`\`\`

Expected command response:

\`\`\`text

SUB-GHz Band

\`\`\`

Expected software status:

\`\`\`text

Band: 1

\`\`\`

Current RP source maps \`band2\` / \`SUBGIG_1\` to:

\| Signal | Intended output |

\|---|---:|

\| GPIO8 / CTF1 | 0 |

\| GPIO9 / CTF2 | 0 |

\| GPIO10 / CTF3 | 1 |

The audited hardware design interprets CTF \`0,0,1\` as the intended external Sub-GHz route through U2 toward the shared antenna path.

Evidence boundaries:

\`\`\`text

"SUB-GHz Band" response

    = Shell command accepted

Band: 1

    = RP software state reported

CTF 0,0,1 in source

    = intended GPIO behavior

Actual GPIO/CTF levels

    = NOT MEASURED

Actual U2 throw and loss

    = UNVERIFIED

\`\`\`

The current RP \`change_band()\` implementation returns early when the requested band equals the cached band. Therefore, issuing \`band2\` while status already says \`Band: 1\` may not rewrite GPIO outputs. This is another reason not to call Shell state electrical proof.

**### Mode D — Reset plus explicit band selection**

Use this order:

\`\`\`text

reset both CC1352 endpoints

→ complete all settle intervals

→ issue band2 and status on both

→ create fresh Radio objects

→ configure PHY/preset

\`\`\`

Commands:

\`\`\`powershell

Invoke-ExplicitCcReset -ShellPort COM35

Invoke-ExplicitCcReset -ShellPort COM87

Invoke-ExplicitBand2 -ShellPort COM35

Invoke-ExplicitBand2 -ShellPort COM87

\`\`\`

Why this order:

\- It exactly matches successful Checkpoints 3A and 3A-R.

\- \`boot\`/\`exit\` changes CC mode and UART state but does not call \`change_band()\`.

\- Applying the logical band control last makes it the final explicit RP control action before opening Bridge ports.

\- Source does not establish that this ordering is electrically necessary.

\- The early-return caveat still applies if the cached state is already \`Band: 1\`.

**---**

**## 5. Cleanup and return-to-reference**

**### Clean Radio cleanup**

The intended Python cleanup order is:

\`\`\`python

rx.stop_rx(retries=1, timeout=1.0)

tx.disconnect()

rx.disconnect()

\`\`\`

Use \`finally\` blocks so cleanup still runs after an exception. Preserve cleanup exceptions rather than silently discarding them.

After the Python processes exit:

1\. Confirm no Python process remains holding the Bridge ports.

2\. Close any Shell terminal without typing \`exit\`.

3\. Record both Shell statuses.

4\. Decide whether the next experiment requires preserving the current state or creating a new power boundary.

**### Restore logical Band: 0 explicitly**

Only do this if the test plan specifically requires reported \`Band: 0\`.

CatSniffer #2:

\`\`\`powershell

python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5,write_timeout=1); s.reset_input_buffer(); s.write(b'band1\r\n'); s.flush(); time.sleep(.8); s.write(b'status\r\n'); s.flush(); time.sleep(1); print(s.read_all().decode(errors='replace'),end=''); s.close()"

\`\`\`

CatSniffer #3:

\`\`\`powershell

python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5,write_timeout=1); s.reset_input_buffer(); s.write(b'band1\r\n'); s.flush(); time.sleep(.8); s.write(b'status\r\n'); s.flush(); time.sleep(1); print(s.read_all().decode(errors='replace'),end=''); s.close()"

\`\`\`

Expected:

\`\`\`text

2.4GHz Band

...

Band: 0

\`\`\`

This restores reported logical state only. It does not establish the physical U2 position.

**### Full power boundary**

For a cold-start reference:

1\. Stop RX.

2\. Disconnect both \`Radio\` objects.

3\. Let both Python processes exit.

4\. Close Shell sessions.

5\. Disconnect all USB/power from both CatSniffers.

6\. Leave both completely unpowered for at least 10 seconds.

7\. Reconnect both.

8\. Re-enumerate by USB identity and MI interface.

9\. Query each Shell \`status\` once.

10\. Do not assume the COM numbers or cold logical state until observed.

One recorded cold start produced:

\`\`\`text

Mode: 0

Band: 0

Radio: LoRa

LoRa: initialized

LoRa Mode: Stream

FW: v3.1.0.1

CC1352 FW: feralrf_cc1352 (custom)

\`\`\`

That is one observed startup, not a guarantee for every boot.

A full power-cycle gives a stronger electrical boundary than sending a final \`band1\`, but it does not guarantee a particular U2 state without measurement.

**---**

**## 6. Two-PowerShell one-packet known-good control**

Run the RX command first. When it prints \`RX_READY\`, run the TX command in the second PowerShell. Neither script uses \`input()\`.

The 15-second RX window is deliberately longer than the formal checkpoint window to accommodate manual coordination. Use the single-process formal procedure in Section 7 for timing-controlled 10-packet results.

**### PowerShell 1 — RX on CatSniffer #3 / COM88**

\`\`\`powershell

Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF'

$env\:PYTHONPATH = (Resolve-Path '.\python').Path

@'

from feralrf import Radio, PHY, PROP_PRESETS, Packet, RxStreamError

PORT = "COM88"

PRESET = "wisun_915_fsk_50k"

MARKER = bytes.fromhex("DE AD BE EF")

rx = Radio(port=PORT, baudrate=921600)

rx_started = False

packets = []

errors = []

try:

    info = rx.init()

    print("RX DeviceInfo:", info, flush=True)

    preset = PROP_PRESETS[PRESET].copy()

    print("RX preset name:", PRESET, flush=True)

    print("RX preset dictionary:", preset, flush=True)

    rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)

    print("RX set_phy ACK", flush=True)

    rx.configure_prop(\*\*preset)

    print("RX configure_prop ACK", flush=True)

    stats_before = rx.get_stats()

    print("RX stats before:", stats_before, flush=True)

    rx.start_rx()

    rx_started = True

    print("RX_READY window_seconds=15.0", flush=True)

    for item in rx.read_packets(timeout=15.0):

        if isinstance(item, RxStreamError):

            errors.append(item)

            print(

                "RX_STREAM_ERROR",

                "error_code=", item.error_code,

                "context=", item.context,

                flush=True,

            )

            continue

        if isinstance(item, Packet):

            packets.append(item)

            marker_match = item.crc_ok and MARKER in item.data

            print(

                "PACKET",

                "timestamp_us=", item.timestamp_us,

                "channel=", item.channel,

                "rssi_dbm=", item.rssi_dbm,

                "lqi=", item.lqi,

                "crc_ok=", item.crc_ok,

                "data=", item.data.hex(" ").upper(),

                "marker_present=", MARKER in item.data,

                "crc_good_marker=", marker_match,

                flush=True,

            )

        else:

            print("UNKNOWN_RX_OBJECT", repr(item), flush=True)

finally:

    if rx_started:

        try:

            rx.stop_rx(retries=1, timeout=1.0)

            print("RX stop ACK", flush=True)

        except Exception as exc:

            print("RX stop ERROR:", repr(exc), flush=True)

    try:

        print("RX stats after:", rx.get_stats(), flush=True)

    except Exception as exc:

        print("RX stats after ERROR:", repr(exc), flush=True)

    rx.disconnect()

    print("RX disconnected", flush=True)

hits = sum(1 for packet in packets if packet.crc_ok and MARKER in packet.data)

print(

    "RX SUMMARY",

    "packets=", len(packets),

    "crc_good_marker_hits=", hits,

    "stream_errors=", len(errors),

    flush=True,

)

'@ | python -

\`\`\`

**### PowerShell 2 — TX on CatSniffer #2 / COM33**

Run only after PowerShell 1 prints \`RX_READY\`:

\`\`\`powershell

Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF'

$env\:PYTHONPATH = (Resolve-Path '.\python').Path

@'

from feralrf import Radio, PHY, PROP_PRESETS

PORT = "COM33"

PRESET = "wisun_915_fsk_50k"

MARKER = bytes.fromhex("DE AD BE EF")

POWER_DBM = 0

tx = Radio(port=PORT, baudrate=921600)

try:

    info = tx.init()

    print("TX DeviceInfo:", info, flush=True)

    preset = PROP_PRESETS[PRESET].copy()

    print("TX preset name:", PRESET, flush=True)

    print("TX preset dictionary:", preset, flush=True)

    tx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)

    print("TX set_phy ACK", flush=True)

    tx.configure_prop(\*\*preset)

    print("TX configure_prop ACK", flush=True)

    tx.set_power(POWER_DBM)

    print("TX set_power ACK requested_dBm=", POWER_DBM, flush=True)

    result = tx.transmit(MARKER, power_dbm=POWER_DBM, timeout=5.0)

    print(

        "TX API COMPLETE",

        "return_value=", repr(result),

        "payload=", MARKER.hex(" ").upper(),

        flush=True,

    )

finally:

    tx.disconnect()

    print("TX disconnected", flush=True)

'@ | python -

\`\`\`

A normal \`transmit()\` return is currently \`None\` after the command ACK. It is API/control evidence, not OTA evidence. The peer packet is the OTA evidence.

**---**

**## 7. Ten-packet formal validation**

**### Reference timing**

The selected formal procedure is the repeatedly successful 3A/3A-R/3C/3D/4A procedure:

\`\`\`text

RX_START

0.3 s pre-TX delay

10 transmit() calls

0.1 s after every TX call

1.0 s post-TX delay

2.0 s RX observation

0.5 s TX asynchronous-error observation

RX_STOP

stats

disconnect

\`\`\`

Checkpoint 3E also passed using a 3-second negative window and a 5-second positive window. That proves the legacy timing can currently work, but the 3A procedure is chosen because it is the repeated recovery reference and avoids adding an unnecessary 3-second state interval.

**### Define the session-local formal runner**

Paste this once into a prepared PowerShell:

\`\`\`powershell

function Invoke-FeralManualPreset {

    param(

        [Parameter(Mandatory=$true)]

        [string]$Preset,

        [Parameter(Mandatory=$true)]

        [string]$MarkerHex,

        [Parameter(Mandatory=$true)]

        [string]$Label

    )

    @'

from \_\_future\_\_ import annotations

import sys

import time

import traceback

from datetime import datetime, timezone

from feralrf import Radio, PHY, PROP_PRESETS, Packet, RxStreamError

TX_PORT = "COM33"

RX_PORT = "COM88"

PRESET_NAME = sys.argv[1]

MARKER = bytes.fromhex(sys.argv[2])

LABEL = sys.argv[3]

TX_POWER_DBM = 0

TX_COUNT = 10

def now():

    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

def emit(event, \*\*fields):

    rendered = " ".join(f"{key}={value!r}" for key, value in fields.items())

    print(f"{now()} {event}" + (f" {rendered}" if rendered else ""), flush=True)

if PRESET_NAME not in PROP_PRESETS:

    raise SystemExit(f"Unknown preset: {PRESET_NAME}")

preset = PROP_PRESETS[PRESET_NAME].copy()

tx = Radio(port=TX_PORT, baudrate=921600)

rx = Radio(port=RX_PORT, baudrate=921600)

rx_started = False

tx_accepted = 0

packets = []

rx_errors = []

tx_errors = []

fatal = None

tx_stats_before = None

rx_stats_before = None

tx_stats_after = None

rx_stats_after = None

emit(

    "TEST_BEGIN",

    label=LABEL,

    tx_port=TX_PORT,

    rx_port=RX_PORT,

    preset_name=PRESET_NAME,

    preset=preset,

    marker=MARKER.hex(" ").upper(),

    requested_power_dbm=TX_POWER_DBM,

    tx_count=TX_COUNT,

)

try:

    tx_info = tx.init()

    emit("TX_DEVICE_INFO", info=tx_info)

    rx_info = rx.init()

    emit("RX_DEVICE_INFO", info=rx_info)

    tx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)

    emit("TX_SET_PHY_ACK", phy="PROPRIETARY_GFSK", channel=0)

    rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)

    emit("RX_SET_PHY_ACK", phy="PROPRIETARY_GFSK", channel=0)

    tx.configure_prop(\*\*preset)

    emit("TX_CONFIGURE_PROP_ACK", preset=preset)

    rx.configure_prop(\*\*preset)

    emit("RX_CONFIGURE_PROP_ACK", preset=preset)

    tx.set_power(TX_POWER_DBM)

    emit("TX_SET_POWER_ACK", requested_power_dbm=TX_POWER_DBM)

    tx_stats_before = tx.get_stats()

    rx_stats_before = rx.get_stats()

    emit("TX_STATS_BEFORE", stats=tx_stats_before)

    emit("RX_STATS_BEFORE", stats=rx_stats_before)

    rx.start_rx()

    rx_started = True

    emit("RX_START_ACK")

    wait_start = time.monotonic()

    time.sleep(0.3)

    emit("PRE_TX_WAIT_COMPLETE", elapsed_seconds=time.monotonic() - wait_start)

    for index in range(1, TX_COUNT + 1):

        emit(

            "TX_ATTEMPT",

            index=index,

            payload=MARKER.hex(" ").upper(),

        )

        try:

            result = tx.transmit(

                MARKER,

                power_dbm=TX_POWER_DBM,

                timeout=5.0,

            )

        except Exception as exc:

            emit(

                "TX_EXCEPTION",

                index=index,

                exception_type=type(exc).\_\_name\_\_,

                exception=repr(exc),

            )

        else:

            tx_accepted += 1

            emit(

                "TX_API_COMPLETE",

                index=index,

                return_value=result,

            )

        spacing_start = time.monotonic()

        time.sleep(0.1)

        emit(

            "TX_SPACING_COMPLETE",

            index=index,

            elapsed_seconds=time.monotonic() - spacing_start,

        )

    wait_start = time.monotonic()

    time.sleep(1.0)

    emit("POST_TX_WAIT_COMPLETE", elapsed_seconds=time.monotonic() - wait_start)

    observation_start = time.monotonic()

    for item in rx.read_packets(timeout=2.0):

        if isinstance(item, RxStreamError):

            rx_errors.append(item)

            emit(

                "RX_STREAM_ERROR",

                error_code=item.error_code,

                context=item.context,

            )

        elif isinstance(item, Packet):

            packets.append(item)

            emit(

                "RX_PACKET",

                ordinal=len(packets),

                device_timestamp_us=item.timestamp_us,

                channel=item.channel,

                rssi_dbm=item.rssi_dbm,

                lqi=item.lqi,

                crc_ok=item.crc_ok,

                payload_hex=item.data.hex(" ").upper(),

                marker_present=MARKER in item.data,

                crc_good_marker=item.crc_ok and MARKER in item.data,

            )

        else:

            emit(

                "RX_UNKNOWN_OBJECT",

                object_type=type(item).\_\_name\_\_,

                value=repr(item),

            )

    emit(

        "RX_OBSERVATION_COMPLETE",

        elapsed_seconds=time.monotonic() - observation_start,

        packets=len(packets),

        async_errors=len(rx_errors),

    )

    tx_observation_start = time.monotonic()

    for item in tx.read_packets(timeout=0.5):

        if isinstance(item, RxStreamError):

            tx_errors.append(item)

            emit(

                "TX_STREAM_ERROR",

                error_code=item.error_code,

                context=item.context,

            )

        elif isinstance(item, Packet):

            emit(

                "TX_UNEXPECTED_PACKET",

                channel=item.channel,

                rssi_dbm=item.rssi_dbm,

                lqi=item.lqi,

                crc_ok=item.crc_ok,

                payload_hex=item.data.hex(" ").upper(),

            )

        else:

            emit(

                "TX_UNKNOWN_OBJECT",

                object_type=type(item).\_\_name\_\_,

                value=repr(item),

            )

    emit(

        "TX_OBSERVATION_COMPLETE",

        elapsed_seconds=time.monotonic() - tx_observation_start,

        async_errors=len(tx_errors),

    )

    rx.stop_rx(retries=1, timeout=1.0)

    rx_started = False

    emit("RX_STOP_ACK")

    tx_stats_after = tx.get_stats()

    rx_stats_after = rx.get_stats()

    emit("TX_STATS_AFTER", stats=tx_stats_after)

    emit("RX_STATS_AFTER", stats=rx_stats_after)

except BaseException as exc:

    fatal = exc

    emit(

        "FATAL_EXCEPTION",

        exception_type=type(exc).\_\_name\_\_,

        exception=repr(exc),

    )

    traceback.print_exc()

finally:

    if rx_started:

        try:

            rx.stop_rx(retries=1, timeout=1.0)

        except Exception as exc:

            emit("RX_STOP_CLEANUP_ERROR", exception=repr(exc))

        else:

            emit("RX_STOP_CLEANUP_ACK")

    try:

        tx.disconnect()

    except Exception as exc:

        emit("TX_DISCONNECT_ERROR", exception=repr(exc))

    else:

        emit("TX_DISCONNECTED")

    try:

        rx.disconnect()

    except Exception as exc:

        emit("RX_DISCONNECT_ERROR", exception=repr(exc))

    else:

        emit("RX_DISCONNECTED")

hits = sum(

    1 for packet in packets

    if packet.crc_ok and MARKER in packet.data

)

if fatal is not None:

    classification = "BLOCKED"

elif hits == 10:

    classification = "PASS-RF"

elif 1 <= hits <= 9:

    classification = "PARTIAL"

else:

    classification = "FAIL"

emit(

    "TEST_SUMMARY",

    classification=classification,

    tx_requested=TX_COUNT,

    tx_api_completions=tx_accepted,

    peer_packets=len(packets),

    peer_crc_good_marker_hits=hits,

    rx_stream_errors=len(rx_errors),

    tx_stream_errors=len(tx_errors),

    tx_stats_before=tx_stats_before,

    tx_stats_after=tx_stats_after,

    rx_stats_before=rx_stats_before,

    rx_stats_after=rx_stats_after,

    fatal_exception=None if fatal is None else repr(fatal),

)

raise SystemExit(0 if classification == "PASS-RF" else 2)

'@ | python - $Preset $MarkerHex $Label

}

\`\`\`

**### Known-good 10-packet invocation**

\`\`\`powershell

Invoke-FeralManualPreset \`

    -Preset 'wisun_915_fsk_50k' \`

    -MarkerHex 'DEADBEEF' \`

    -Label 'known-good-902p2-control'

\`\`\`

This runner performs exactly one run and has no automatic retry.

**### Marker policy**

\- Preserve \`DE AD BE EF\` for the known-good reference.

\- Give every new preset/run a unique marker.

\- Use an even number of hexadecimal characters.

\- Four bytes is the established minimum used by recovery.

\- Record the marker before running.

\- Do not reuse a marker while old packets could remain buffered.

Example unique markers:

\| Preset | Suggested marker |

\|---|---|

\| \`gfsk_902_50k\` | \`A5010001\` |

\| \`wisun_915_fsk_100k\` | \`A5020001\` |

\| \`wisun_915_fsk_150k\` | \`A5030001\` |

\| \`wisun_915_fsk_200k\` | \`A5040001\` |

\| \`wisun_915_fsk_300k\` | \`A5050001\` |

\| \`gfsk_915_50k\` | \`A5060001\` |

\| \`sidewalk_915_fsk_50k\` | \`A5070001\` |

\| \`sidewalk_915_fsk_250k\` | \`A5080001\` |

\| \`gfsk_868_50k\` | \`A5090001\` |

\| \`gfsk_868_100k\` | \`A50A0001\` |

\| \`wireless_mbus_s_868\` | \`A50B0001\` |

\| \`wireless_mbus_t_868\` | \`A50C0001\` |

\| \`wireless_mbus_c_868\` | \`A50D0001\` |

These are campaign labels, not protocol frames.

**---**

**## 8. Eligible preset list**

All listed dictionaries omit \`format_conf\`, so the current \`configure_prop()\` default is \`format_conf=0\`.

\`rx_bw\` is a firmware register/configuration value, not a bandwidth in hertz.

**### Known-good control**

\| Preset | Hz | mod | Rate | Dev | RX BW | Sync | Status |

\|---|---:|---:|---:|---:|---:|---:|---|

\| \`wisun_915_fsk_50k\` | 902200000 | 0 FSK | 50000 | 100 | \`0x52\` | \`0x930B51DE\` | Formal 60/60 reference |

Despite its name, this preset configures 902.2 MHz, not 915 MHz.

**### Generic GFSK**

\| Preset | Hz | mod | Rate | Dev | RX BW | Sync |

\|---|---:|---:|---:|---:|---:|---:|

\| \`gfsk_902_50k\` | 902200000 | 1 GFSK | 50000 | 100 | \`0x52\` | \`0x930B51DE\` |

\| \`gfsk_915_50k\` | 915000000 | 1 GFSK | 50000 | 100 | \`0x52\` | \`0x930B51DE\` |

\| \`gfsk_868_50k\` | 868000000 | 1 GFSK | 50000 | 100 | \`0x52\` | \`0x930B51DE\` |

\| \`gfsk_868_100k\` | 868000000 | 1 GFSK | 100000 | 150 | \`0x56\` | \`0x930B51DE\` |

There is no current generic \`fsk_868\_\*\` preset.

**### Wi-SUN-named raw FSK**

\| Preset | Hz | mod | Rate | Dev | RX BW | Sync |

\|---|---:|---:|---:|---:|---:|---:|

\| \`wisun_915_fsk_50k\` | 902200000 | 0 FSK | 50000 | 100 | \`0x52\` | \`0x930B51DE\` |

\| \`wisun_915_fsk_100k\` | 902200000 | 0 FSK | 100000 | 150 | \`0x56\` | \`0x930B51DE\` |

\| \`wisun_915_fsk_150k\` | 902200000 | 0 FSK | 150000 | 200 | \`0x5A\` | \`0x930B51DE\` |

\| \`wisun_915_fsk_200k\` | 902200000 | 0 FSK | 200000 | 200 | \`0x5A\` | \`0x930B51DE\` |

\| \`wisun_915_fsk_300k\` | 902200000 | 0 FSK | 300000 | 200 | \`0x5E\` | \`0x930B51DE\` |

All five \`wisun_915\_\*\` presets currently configure 902.2 MHz. The names refer to the historical NA-1 plan context; they do not mean the literal center frequency is 915 MHz.

The higher-rate deviation and bandwidth values are source presets, not independently validated optimal Wi-SUN parameters.

**### Sidewalk-named raw FSK**

\| Preset | Hz | mod | Rate | Dev | RX BW | Sync |

\|---|---:|---:|---:|---:|---:|---:|

\| \`sidewalk_915_fsk_50k\` | 915000000 | 0 FSK | 50000 | 100 | \`0x52\` | \`0x930B51DE\` |

\| \`sidewalk_915_fsk_250k\` | 915000000 | 0 FSK | 250000 | 200 | \`0x5A\` | \`0x930B51DE\` |

These are raw FSK settings only. Sidewalk LR/LoRa requires a different radio path and is not implemented by these CC presets.

**### Wireless M-Bus-named raw settings**

\| Preset | Hz | mod | Rate | Dev | RX BW | Sync |

\|---|---:|---:|---:|---:|---:|---:|

\| \`wireless_mbus_s_868\` | 868300000 | 1 GFSK | 32768 | 75 | \`0x52\` | \`0x543D0000\` |

\| \`wireless_mbus_t_868\` | 868950000 | 1 GFSK | 100000 | 200 | \`0x57\` | \`0x543D0000\` |

\| \`wireless_mbus_c_868\` | 868950000 | 1 GFSK | 100000 | 180 | \`0x57\` | \`0x543D0000\` |

These dictionaries configure raw proprietary packets. They do not implement Wireless M-Bus framing, coding, timing, application behavior, or interoperability.

**### Recommended order**

1\. \`wisun_915_fsk_50k\` positive control

2\. \`gfsk_902_50k\`

3\. \`wisun_915_fsk_100k\`

4\. \`wisun_915_fsk_150k\`

5\. \`wisun_915_fsk_200k\`

6\. \`wisun_915_fsk_300k\`

7\. \`gfsk_915_50k\`

8\. \`sidewalk_915_fsk_50k\`

9\. \`sidewalk_915_fsk_250k\`

10\. \`gfsk_868_50k\`

11\. \`gfsk_868_100k\`

12\. \`wireless_mbus_s_868\`

13\. \`wireless_mbus_t_868\`

14\. \`wireless_mbus_c_868\`

This moves from the known-good frequency to same-frequency variants, then 915 MHz, then 868 MHz, minimizing frequency/path and rate changes.

**---**

**## 9. Do-not-test list**

Do not execute these in the next manual campaign:

\| Deferred family | Exact current presets | Reason |

\|---|---|---|

\| OOK | \`ook_433_4k8\`, \`ook_433_2k4\`, \`ook_868_4k8\` | Current API warns that OOK loads dedicated RF-core patches and locks mode/frequency until a power-cycle. Lifecycle review and isolated recovery procedure are required. |

\| MIOTY | \`mioty_868_tsunb\` | Source explicitly labels native support pending. 396-baud TS-UNB may require a custom CPE patch; raw preset is incomplete as validation of MIOTY. |

\| 169 MHz | \`wireless_mbus_n_169_2k4\`, \`wireless_mbus_n_169_4k8\` | Current CatSniffer silicon/front-end path is not established as physically supporting the requested 169.45 MHz operation. |

\| 433 MHz | \`gfsk_433_50k\`, \`gfsk_433_10k\`, \`fsk_433_50k\`, plus 433 OOK/MSK/4FSK/4GFSK | CC source contains a 433 configuration, but installed front-end, matching, antenna path, and generated-configuration compatibility have not been established. |

\| MSK | \`msk_433_50k\`, \`msk_868_50k\` | TI command semantics for source \`mod_type=4\`, required format fields, and patching need audit. API acceptance would not establish MSK emission. |

\| 4FSK | \`4fsk_433_50k\`, \`4fsk_868_50k\` | \`mod_type=5\` command encoding and required format/patch behavior remain unresolved. |

\| 4GFSK | \`4gfsk_433_50k\`, \`4gfsk_868_50k\` | \`mod_type=6\` command encoding and required format/patch behavior remain unresolved. |

No commands for these modes are included intentionally.

**---**

**## 10. Older failed flow versus current working flow**

“Old failed campaign” below means the later 0/70 and accumulated 0/240 procedure—not the historical F29 70/70 claim.

\| Variable | Later failed campaign | Current working procedure | Assessment |

\|---|---|---|---|

\| Radio objects | Fresh TX/RX objects per helper invocation | Fresh TX/RX objects per run | Same; already controlled |

\| CC reset | No reset in later 0/70/0/240 | 3A reset; 3C–4A no reset and passed | “Reset every run” strongly weakened |

\| F29 reset | Historical F29 attempted both resets | Not part of later failure helper | Provenance distinction; F29 used unsafe arithmetic |

\| Shell ports | Later commands used explicit COM35/COM87 in documented transitions | Explicit COM35/COM87 | Same in relevant later campaign |

\| COM arithmetic | Historical F29 derived Shell=Bridge+2 and swallowed exceptions | Prohibited | Potentially behavior-changing historically, but does not explain later no-reset 0/70 by itself |

\| Band command | Later failure campaign explicitly transitioned logical band by frequency group | Current 3D–4A passed with \`Band: 0\`; Mode A sends none | Already experimentally controlled for close-range reference |

\| \`set_phy()\` | \`PHY.PROPRIETARY_GFSK, 0\` | Same | Same |

\| Channel argument | \`0\` | \`0\` | Same |

\| Preset lookup | \`PROP_PRESETS[preset]\` | Same current dictionary | Same host-requested values |

\| \`configure_prop()\` | Applied to both endpoints | Applied to both endpoints | Same |

\| TX power | \`tx.set_power(0)\` and per-call 0 dBm | Same | Same |

\| RX-before-TX | Yes | Yes | Same |

\| Negative window | Read for 3 seconds before TX | Current formal procedure sleeps 0.3 seconds without reading | Visible procedural difference, but 3E reproduced the legacy window and passed 10/10 |

\| TX count | Exactly 10 | Exactly 10 | Same |

\| TX spacing | Approximately 0.1 seconds | Approximately 0.1 seconds | Same |

\| Post-TX observation | Legacy \`read_packets(timeout=5.0)\` | 1-second wait, then 2-second read | Difference already controlled by 3E |

\| Packet parsing | \`Packet\`, CRC-good, marker subsequence | Same, with fields logged individually | Functionally equivalent criterion |

\| \`RxStreamError\` | Counted during negative/positive reads | Logged on RX and a separate TX observation window | Better evidence retention; no error was seen in successful controls |

\| Stats | Not consistently retained in old helper | Before/after stats retained | Diagnostic improvement, not an RF variable |

\| Cleanup | Old helper swallowed stop/disconnect exceptions | Current procedure preserves cleanup errors | Procedural/evidence-quality improvement |

\| Retry | Later helper did not automatically retry completed 0/10 results | No automatic retry | Same for completed runs |

\| F29 retry | Historical F29 retried once after any exception | Current campaign does not | Historical difference; not evidence of the later failure cause |

\| Firmware/binaries | Exact installed CC binary UNKNOWN | Exact installed CC binary still UNKNOWN | Provenance-unknown and potentially behavior-changing |

\| Prior PHY/session state | Mixed in accumulated campaign | Fresh objects and explicit reconfiguration every run | Potentially behavior-changing interaction remains possible, but not established |

\| Physical RF path | Unmeasured | Still unmeasured; peer delivery proves a usable path during each PASS | Provenance/electrical unknown |

The recent working procedure looks different chiefly because it:

\- records more evidence;

\- does not swallow cleanup errors;

\- uses a shorter current observation sequence;

\- avoids Shell arithmetic;

\- can run without a reset or band command;

\- explicitly logs asynchronous and statistics data.

However:

\- Checkpoint 3E reproduced the old 3-second negative/5-second positive sequence and passed 10/10.

\- Checkpoints 3C–4A passed without reset.

\- Checkpoints 3D–4A passed while reporting \`Band: 0\`.

\- Therefore none of those visible differences, in simple isolation, explains the historical 0/70 or 0/240 results.

The exact installed CC1352 binary, physical RF state, prior sequence state, and historical hardware/environment remain unresolved.

**---**

**## 11. Per-preset manual template**

First define \`Invoke-FeralManualPreset\` from Section 7.

For each run, change only these three values:

\`\`\`powershell

$PRESET = 'gfsk_902_50k'

$MARKER = 'A5010001'

$LABEL  = 'campaign-gfsk-902-50k-run-01'

Invoke-FeralManualPreset \`

    -Preset $PRESET \`

    -MarkerHex $MARKER \`

    -Label $LABEL

\`\`\`

Allowed changes:

\`\`\`text

PRESET

MARKER

LABEL

\`\`\`

Keep fixed unless a separately reviewed preset requirement says otherwise:

\`\`\`text

TX unit = CatSniffer #2

TX Bridge = COM33

RX unit = CatSniffer #3

RX Bridge = COM88

PHY = PROPRIETARY_GFSK

channel argument = 0

TX power = 0 dBm

TX count = 10

spacing = 0.1 s

pre-TX wait = 0.3 s

post-TX wait = 1.0 s

RX observation = 2.0 s

TX async observation = 0.5 s

fresh Radio objects

RX before TX

no automatic retry

\`\`\`

Before invoking a preset, independently print its dictionary:

\`\`\`powershell

$PRESET = 'gfsk_902_50k'

python -c "import sys; from feralrf import PROP_PRESETS; name=sys.argv[1]; print(name, PROP_PRESETS[name])" $PRESET

\`\`\`

Compare that literal output to the planned configuration before transmitting.

**---**

**## 12. Control insertion strategy**

Use \`wisun_915_fsk_50k\` / \`DEADBEEF\` as the sentinel.

Recommended schedule:

1\. Run the known-good control before the campaign.

2\. Run it after every three new presets that pass.

3\. Run it immediately after any \`FAIL\`, \`PARTIAL\`, \`BLOCKED\`, or unexpected asynchronous error.

4\. Run it after any reset experiment.

5\. Run it after any \`band1\`/\`band2\` experiment.

6\. Run it after a power-cycle before interpreting another preset.

7\. Run it at campaign end.

Interpretation:

\`\`\`text

new preset FAIL

\+ known-good control PASS

→ evidence favors a preset/configuration-specific problem,

  but does not prove the precise cause

\`\`\`

\`\`\`text

new preset FAIL

\+ known-good control FAIL

→ do not interpret the new preset;

  device/campaign/environment state may have changed

\`\`\`

If the sentinel fails:

\- stop the campaign;

\- preserve the first result;

\- do not automatically reset or change band;

\- record the state;

\- treat later preset interpretation as inconclusive until a separately authorized recovery action is chosen.

Do not automatically rerun the failed preset after a passing sentinel. A rerun should be a distinct, authorized repeatability checkpoint.

**---**

**## 13. Acceptance criteria**

**### PASS-RF**

For a formal ten-packet run:

\`\`\`text

10/10 peer Packet objects

AND crc_ok == True

AND expected marker occurs in packet.data

\`\`\`

A one-packet sanity run can be recorded as \`SANITY PASS 1/1\`, but it is not equivalent to the formal ten-packet criterion.

**### PARTIAL**

\`\`\`text

1–9/10 CRC-good peer packets containing the expected marker

\`\`\`

Report the literal count.

**### FAIL**

\`\`\`text

0/10 CRC-good marker matches

AND setup/control/API procedure otherwise completed normally

\`\`\`

This is failure of peer delivery. It does not prove zero RF emission.

**### BLOCKED**

The intended comparison is invalid because of an infrastructure or control problem, for example:

\- wrong or unverified ports;

\- import from the wrong FeralRF source;

\- initialization/configuration failure;

\- RX could not start;

\- cleanup from a prior run was incomplete;

\- required device disappeared;

\- fatal exception prevented the intended procedure.

**### INCONCLUSIVE**

The test ran, but another result prevents attribution, for example:

\- the new preset failed and the known-good control also failed;

\- stale/duplicate marker ambiguity;

\- state changed unintentionally during the run;

\- unexpected preparation mode;

\- asynchronous errors or transport loss make completeness uncertain;

\- the initial or final control condition is unknown.

A successful TX API return proves only that the command completed without a reported synchronous error. It is not an OTA PASS.

For presets named:

\`\`\`text

wisun\_\*

sidewalk\_\*

wireless_mbus\_\*

\`\`\`

\`PASS-RF\` means:

\`\`\`text

raw PHY/preset OTA packet delivery validated

\`\`\`

It does not mean:

\`\`\`text

Wi-SUN interoperability

Sidewalk interoperability

Wireless M-Bus interoperability

protocol compliance

spectral compliance

\`\`\`

**---**

**## 14. Manual result template**

\`\`\`text

SUB-GHZ MANUAL PRESET RESULT

Date/time:

Test label:

TX physical unit:

RX physical unit:

TX Bridge:

RX Bridge:

TX Shell:

RX Shell:

Preparation mode: A / B / C / D

Reset performed:

Band command performed:

Initial TX Shell status:

Initial RX Shell status:

Preset:

Exact configuration:

  frequency_hz:

  mod_type:

  symbol_rate:

  deviation:

  rx_bw:

  sync_word:

  format_conf:

Requested TX power:

Marker:

PHY:

Channel argument:

TX requests:

TX API completions:

TX exceptions:

Peer Packet objects:

CRC-good packets:

CRC-good marker hits:

RxStreamError count:

Other RX objects:

RX stats before:

  rx_ok:

  rx_crc_err:

  rx_drop:

  rx_overflow:

RX stats after:

  rx_ok:

  rx_crc_err:

  rx_drop:

  rx_overflow:

TX stats before:

TX stats after:

Reported packet metadata:

  channels:

  RSSI values:

  LQI values:

  device timestamps:

Post-run Shell:

  CatSniffer #2 uart_overrun:

  CatSniffer #2 ring_dropped:

  CatSniffer #3 uart_overrun:

  CatSniffer #3 ring_dropped:

RX stop outcome:

TX disconnect outcome:

RX disconnect outcome:

Result:

  PASS-RF / PARTIAL / FAIL / BLOCKED / INCONCLUSIVE

Known-good control required afterward:

Known-good control result:

Notes:

What this proves:

What this does not prove:

\`\`\`

Treat channel, RSSI, and LQI as unverified metadata until their current anomalies are resolved.

**---**

**## 15. Final operator checklist**

**### Before test**

\- [ ] CatSniffer #2 identified explicitly as \`COM33\` Bridge / \`COM35\` Shell

\- [ ] CatSniffer #3 identified explicitly as \`COM88\` Bridge / \`COM87\` Shell

\- [ ] No COM arithmetic used

\- [ ] Correct repository directory selected

\- [ ] \`PYTHONPATH\` points to \`.\python\`

\- [ ] \`feralrf.\_\_file\_\_\` verified

\- [ ] Preparation Mode A/B/C/D selected and recorded

\- [ ] Initial Shell status recorded

\- [ ] Exact preset dictionary printed and checked

\- [ ] Unique marker selected and recorded

\- [ ] No deferred preset selected

**### RX**

\- [ ] Fresh \`Radio(COM88)\` object

\- [ ] \`DeviceInfo\` recorded

\- [ ] \`PHY.PROPRIETARY_GFSK\`, channel 0 selected

\- [ ] Exact preset configured

\- [ ] Pre-stats recorded

\- [ ] RX started before TX

**### TX**

\- [ ] Fresh \`Radio(COM33)\` object

\- [ ] \`DeviceInfo\` recorded

\- [ ] Same PHY, channel, and preset as RX

\- [ ] Requested power recorded as 0 dBm

\- [ ] Correct marker used

\- [ ] Exactly 10 calls for formal validation

\- [ ] No automatic retry

**### After test**

\- [ ] Every TX completion/exception recorded

\- [ ] Every \`Packet\` and \`RxStreamError\` recorded

\- [ ] CRC-good marker count recorded literally

\- [ ] Final stats recorded

\- [ ] RX stopped cleanly

\- [ ] TX and RX disconnected cleanly

\- [ ] Post-run Shell/loss counters recorded if required

\- [ ] Known-good control rerun when required

\- [ ] Final logical/power state restored or explicitly recorded

\- [ ] No protocol-interoperability claim made from raw delivery

No hardware action, serial access, RF execution, source modification, commit, or push was performed while preparing this runbook.