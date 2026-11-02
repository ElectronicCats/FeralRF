---

title: "05 — OTA IEEE: validación dirigida de TX_STOP con RX continuo"
date: 2026-10-09
project: "CatSniffer - FeralRF"
status: "Cerrado experimentalmente — PARTIAL / anomalía de sesión identificada"
tags:

* FeralRF
* CatSniffer
* OTA
* IEEE-802.15.4
* CONTINUOUS
* TX_STOP
* RX_STOP
* validacion

---

# 05 — OTA IEEE: validación dirigida de `TX_STOP` con RX continuo

## 1. Propósito

Este documento registra la continuación de la evaluación OTA de `TX_CONTINUOUS` y `TX_STOP` con dos CatSniffer V3.

El objetivo específico de esta etapa fue cerrar, con mayor rigor, la pregunta pendiente:

> ¿El comando `TX_STOP` produce un cese físicamente observable de la transmisión `TX_CONTINUOUS`, sin introducir un hueco temporal causado por detener y volver a iniciar el receptor?

La campaña previa ya había demostrado:

* recepción OTA atribuible bajo `TX_CONTINUOUS`;
* repetición física clara con `interval_us=0`;
* aceptación de `TX_STOP` por la API;
* ventanas RX posteriores sin el marcador transmitido.

Sin embargo, aquellas ventanas posteriores se abrían mediante una **nueva invocación RX después del STOP**, por lo que existía un hueco temporal y no era posible afirmar que se hubiera observado directamente la transición TX → STOP.

El reporte previo clasificaba por ello el bloque como `PARTIAL / PASS-RF condicionado` y proponía explícitamente como trabajo posterior evaluar STOP mediante una **ventana RX continua antes y después del comando**, evitando pausas manuales.

---

# 2. Contexto documental previo

## 2.1 Procedimiento original de la guía

La sección `### 4. CONTINUOUS + STOP físico` de:

`Guía enfocada OTA FeralRF - TX RX con dos CatSniffer.md`

prescribía originalmente:

1. control negativo;
2. activar RX;
3. iniciar `TX_CONTINUOUS`;
4. dejar que el helper envíe `TX_STOP`;
5. finalizar esa observación;
6. abrir una nueva ventana RX post-STOP;
7. exigir cero coincidencias.

Para la primera variante:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 30
```

Luego:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex a40133008804 --match-mode contains --min-hits 5 --print-limit 120
```

y mientras RX estaba activo:

```powershell
python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40133008804 --interval-us 250000 --run-seconds 3 --tx-timeout 2
```

Después se abría un nuevo control post-STOP.

La guía también definía una segunda variante con `interval_us=0`, marcador nuevo y RX/TX simultáneos, exigiendo registrar intervalo, duración, timestamps, coincidencias, `ACK STOP` y cese.

## 2.2 Resultado previo relevante

En la campaña del 8 de octubre:

* `interval_us=250000`: al menos una recepción atribuible; resultado `PARTIAL`;
* `interval_us=0`: 30 coincidencias, `PASS-RF` de repetición;
* `TX_STOP`: ACK observado;
* las ventanas RX post-STOP posteriores no encontraron el marcador;
* hubo un timeout transitorio de `RX_STOP`.

En la variante rápida, los 30 paquetes observados abarcaron solamente `32.420 ms` según timestamps del receptor, con separación media observada de aproximadamente `1117.93 µs`; esto caracterizaba eventos recibidos, no la temporización interna real de TX.

El reporte previo concluía:

* `TX_CONTINUOUS interval_us=0`: evidencia física positiva;
* `TX_STOP`: validado a nivel de control;
* ausencia post-STOP: compatible con cese;
* cese físico inmediato: **no validado**;
* `RX_STOP/0x90`: anomalía pendiente de investigación.

---

# 3. Equipos y configuración utilizada

| Elemento                              | Valor                           |
| ------------------------------------- | ------------------------------- |
| CatSniffer TX                         | #1                              |
| Cat-Bridge TX                         | `COM33`                         |
| CatSniffer RX                         | #2                              |
| Cat-Bridge RX                         | `COM88`                         |
| PHY                                   | `4` — IEEE 802.15.4             |
| Canal                                 | `25`                            |
| Potencia solicitada                   | `0 dBm`                         |
| Firmware reportado por ambos CC1352P7 | `1.0.0`                         |
| Capabilities                          | `0x07`                          |
| Serial lógico reportado               | `464552414c524631` / `FERALRF1` |
| Velocidad Bridge                      | `921600` baud                   |
| Reset automático                      | No                              |
| Flashing                              | No                              |
| Catnip                                | No utilizado                    |
| Instrumentación RF externa            | No utilizada                    |

El serial lógico común no permite diferenciar físicamente las dos placas.

---

# 4. Verificación previa de la API utilizada

Antes de construir el helper se inspeccionó:

```powershell
Get-Content .\python\examples\smoke_tx_continuous_phase1.py
```

El helper oficial mostró las llamadas:

```python
radio.transmit_continuous(
    packet,
    interval_us=args.interval_us,
    timeout=args.tx_timeout,
)
```

y:

```python
radio.stop_transmit(timeout=args.tx_timeout)
```

También se verificó:

```powershell
Select-String `
  -Path .\python\feralrf\radio.py `
  -Pattern 'continuous|stop' `
  -Context 3,8
```

La API Python auditada declara como estables:

```text
transmit_continuous
stop_transmit
```

y las firmas observadas fueron:

```python
def transmit_continuous(
    self,
    packet: bytes,
    interval_us: int = 0,
    timeout: float = 5.0,
) -> None:
```

y:

```python
def stop_transmit(self, timeout: float = 5.0) -> None:
```

`transmit_continuous()` envía:

```python
Command.TX_CONTINUOUS
```

mientras `stop_transmit()` envía:

```python
Command.TX_STOP
```

Esto permitió construir los ensayos posteriores sin inventar llamadas de API.

---

# 5. Ensayo nuevo 1 — RX continuo atravesando `TX_STOP`

## 5.1 Objetivo

Eliminar el hueco temporal existente entre:

```text
RX de CONTINUOUS
→ RX_STOP
→ pausa
→ nuevo RX_START post-STOP
```

y sustituirlo por:

```text
RX_START
→ CONTINUOUS
→ TX_STOP
→ RX permanece activo
→ observación post-STOP
→ RX_STOP
```

## 5.2 Marcador

```text
a40333008804
```

## 5.3 Parámetros

```text
TX               = COM33
RX               = COM88
PHY              = 4
channel          = 25
power            = 0 dBm
interval_us      = 0
pre-TX           = 2 s
CONTINUOUS       = 1 s
post-STOP RX     = 5 s
```

## 5.4 Helper ejecutado

```powershell
@'
import time
import threading

from feralrf import PHY, Packet, Radio, RxStreamError

TX_PORT = "COM33"
RX_PORT = "COM88"
PHY_ID = 4
CHANNEL = 25
POWER_DBM = 0

MARKER = bytes.fromhex("a40333008804")

PRE_TX_SECONDS = 2.0
TX_SECONDS = 1.0
POST_STOP_SECONDS = 5.0
TX_TIMEOUT = 2.0
INTERVAL_US = 0

tx = Radio(TX_PORT)
rx = Radio(RX_PORT)

records = []
rx_exception = None
stop_reader = threading.Event()

t0 = time.monotonic()

def reltime():
    return time.monotonic() - t0

def rx_worker():
    global rx_exception
    try:
        for item in rx.read_packets(timeout=None):
            host_t = reltime()
            records.append((host_t, item))

            if isinstance(item, Packet):
                if MARKER in item.data:
                    print(
                        f"[RX HIT] host_t={host_t:.6f}s "
                        f"dev_ts={getattr(item, 'timestamp_us', None)}us "
                        f"rssi={getattr(item, 'rssi', None)} "
                        f"crc_ok={getattr(item, 'crc_ok', None)} "
                        f"len={len(item.data)} "
                        f"data={item.data.hex()}",
                        flush=True,
                    )
            elif isinstance(item, RxStreamError):
                print(
                    f"[RX ERROR] host_t={host_t:.6f}s item={item}",
                    flush=True,
                )
            else:
                print(
                    f"[RX OTHER] host_t={host_t:.6f}s item={item}",
                    flush=True,
                )

            if stop_reader.is_set():
                break

    except Exception as exc:
        rx_exception = exc
        print(
            f"[RX THREAD EXCEPTION] host_t={reltime():.6f}s {exc!r}",
            flush=True,
        )

tx_started = False
rx_started = False

tx_req_t = None
tx_ack_t = None
stop_req_t = None
stop_ack_t = None

try:
    print("FeralRF CONTINUOUS -> STOP continuous-RX test")
    print("=============================================")
    print(
        f"TX={TX_PORT} RX={RX_PORT} phy={PHY_ID} channel={CHANNEL} "
        f"power={POWER_DBM} marker={MARKER.hex()} "
        f"interval_us={INTERVAL_US}"
    )
    print()

    print("[STEP] Init RX")
    rx_info = rx.init()
    print(f"[ OK ] RX INFO {rx_info}")

    print("[STEP] Configure RX")
    rx.set_phy(PHY(PHY_ID), CHANNEL)
    rx.set_channel(CHANNEL)
    rx.start_rx()
    rx_started = True
    print(f"[ OK ] RX_START ACK host_t={reltime():.6f}s")

    reader = threading.Thread(target=rx_worker, daemon=True)
    reader.start()

    print(f"[PHASE] PRE_TX {PRE_TX_SECONDS:.1f}s")
    time.sleep(PRE_TX_SECONDS)

    print("[STEP] Init TX")
    tx_info = tx.init()
    print(f"[ OK ] TX INFO {tx_info}")

    tx.set_phy(PHY(PHY_ID), CHANNEL)
    tx.set_channel(CHANNEL)
    tx.set_power(POWER_DBM)
    print("[ OK ] TX config ACK")

    tx_req_t = reltime()
    print(f"[TX] TX_CONTINUOUS request host_t={tx_req_t:.6f}s")

    tx.transmit_continuous(
        MARKER,
        interval_us=INTERVAL_US,
        timeout=TX_TIMEOUT,
    )
    tx_started = True

    tx_ack_t = reltime()
    print(f"[TX] TX_CONTINUOUS ACK host_t={tx_ack_t:.6f}s")

    print(f"[PHASE] CONTINUOUS {TX_SECONDS:.1f}s")
    time.sleep(TX_SECONDS)

    stop_req_t = reltime()
    print(f"[TX] TX_STOP request host_t={stop_req_t:.6f}s")

    tx.stop_transmit(timeout=TX_TIMEOUT)
    tx_started = False

    stop_ack_t = reltime()
    print(f"[TX] TX_STOP ACK host_t={stop_ack_t:.6f}s")

    print(f"[PHASE] POST_STOP continuous RX {POST_STOP_SECONDS:.1f}s")
    time.sleep(POST_STOP_SECONDS)

    stop_reader.set()

    print("[STEP] RX_STOP")
    rx.stop_rx(timeout=2.0)
    rx_started = False
    print(f"[ OK ] RX_STOP ACK host_t={reltime():.6f}s")

    reader.join(timeout=1.0)

finally:
    stop_reader.set()

    if tx_started:
        try:
            tx.stop_transmit(timeout=TX_TIMEOUT)
        except Exception as exc:
            print(f"[CLEANUP] TX_STOP failed: {exc!r}")

    if rx_started:
        try:
            rx.stop_rx(timeout=2.0)
        except Exception as exc:
            print(f"[CLEANUP] RX_STOP failed: {exc!r}")

    try:
        tx.disconnect()
    except Exception:
        pass

    try:
        rx.disconnect()
    except Exception:
        pass


def is_marker_packet(item):
    return (
        isinstance(item, Packet)
        and getattr(item, "crc_ok", False)
        and MARKER in item.data
    )


negative_hits = []
during_hits = []
post_hits = []
stream_errors = []

for host_t, item in records:
    if isinstance(item, RxStreamError):
        stream_errors.append((host_t, item))

    if not is_marker_packet(item):
        continue

    if tx_req_t is not None and host_t < tx_req_t:
        negative_hits.append((host_t, item))
    elif stop_ack_t is not None and host_t > stop_ack_t:
        post_hits.append((host_t, item))
    else:
        during_hits.append((host_t, item))


print()
print("RESULT")
print("======")
print(f"marker={MARKER.hex()}")
print(f"records_total={len(records)}")
print(f"negative_hits={len(negative_hits)}")
print(f"during_hits={len(during_hits)}")
print(f"post_stop_hits={len(post_hits)}")
print(f"rx_stream_errors={len(stream_errors)}")
print(f"rx_thread_exception={rx_exception!r}")

print()
print("TIMING")
print("======")
print(f"TX_CONTINUOUS request = {tx_req_t}")
print(f"TX_CONTINUOUS ACK     = {tx_ack_t}")
print(f"TX_STOP request       = {stop_req_t}")
print(f"TX_STOP ACK           = {stop_ack_t}")
'@ | python -
```

## 5.5 Resultado observado

Control TX:

```text
TX_CONTINUOUS request host_t=4.428054s
TX_CONTINUOUS ACK     host_t=4.458992s
TX_STOP request       host_t=5.459446s
TX_STOP ACK           host_t=5.496513s
```

Se observaron múltiples paquetes:

```text
data=a40333008804b5e0
crc_ok=True
len=8
```

Los timestamps internos comenzaron en:

```text
87360005 us
```

y terminaron en:

```text
87411439 us
```

Diferencia:

```text
51,434 us = 51.434 ms
```

Sin embargo, Python continuó imprimiendo esos paquetes durante varios segundos.

`RX_STOP` terminó en:

```text
TimeoutError(
  'Response timeout (ignored 17 unexpected response(s), last=0x90)'
)
```

## 5.6 Problema metodológico identificado

El helper clasificaba:

```text
host_t > TX_STOP_ACK
```

como si significara:

```text
paquete RF recibido después de STOP
```

Esto resultó inválido.

Los timestamps internos demostraron que una secuencia registrada por el receptor en solamente ~51 ms tardaba varios segundos en ser entregada por Python.

Por tanto:

```text
tiempo de impresión host
≠
tiempo físico de recepción RF
```

Además, el primer helper permitía que:

```text
read_packets()
```

y:

```text
stop_rx()
```

operaran concurrentemente sobre el mismo objeto `Radio` / puerto.

Esto introducía una posible carrera host-side.

### Clasificación

```text
CONTINUOUS físico: PASS-RF
STOP físico: INCONCLUSIVE
RX_STOP: anomalía observada
Helper v1: metodológicamente insuficiente para ordenar RF respecto a STOP
```

---

# 6. Ensayo nuevo 2 — Marcador RF posterior a STOP

## 6.1 Motivación

Para evitar depender de `host_t`, se diseñó una referencia física en el **mismo reloj del receptor**.

Se utilizaron:

```text
A = a40433008804   CONTINUOUS
B = a40533008805   RAW enviado después de TX_STOP ACK
```

Conceptualmente:

```text
CONTINUOUS A
      ↓
TX_STOP ACK
      ↓
espera 250 ms
      ↓
RAW B
RAW B
RAW B
```

Si COM88 recibía B, su `timestamp_us` constituiría un ancla RF conocida posterior al STOP.

Después sería posible preguntar:

```text
¿existe algún A con timestamp > primer timestamp B?
```

## 6.2 Corrección adicional

El hilo principal fue convertido en el **único consumidor de COM88**.

`RX_STOP` sólo debía ejecutarse después de que:

```python
read_packets(timeout=...)
```

hubiera terminado.

## 6.3 Parámetros

```text
MARKER_A            = a40433008804
MARKER_B            = a40533008805
interval_us         = 0
CONTINUOUS          = 1 s
delay post STOP     = 250 ms
B                    = 3 RAW one-shot
RX total            = 9 s
```

## 6.4 Resultado

Se obtuvieron:

```text
records_total=47
marker_A_hits=43
marker_B_hits=0
rx_stream_errors=0
tx_exception=None
```

Timestamps A:

```text
first_A_dev_ts=357139607
last_A_dev_ts=357186586
A_span_us=46979
```

Es decir:

```text
46.979 ms
```

Ninguno de los tres `B` fue observado físicamente.

El helper produjo correctamente:

```text
[INCONCLUSIVE] CONTINUOUS marker A was observed, but
post-STOP anchor B was not observed.
```

`RX_STOP` volvió a terminar en timeout, aun cuando `read_packets()` había concluido:

```text
TimeoutError(
  'Response timeout (ignored 16 unexpected response(s), last=0x90)'
)
```

### Consecuencia

La hipótesis:

> el timeout de `RX_STOP` era causado únicamente por dos lectores Python concurrentes

queda **debilitada/descartada como explicación completa**.

La carrera existía en el helper v1, pero no explica por sí sola el comportamiento reproducido después.

---

# 7. Ensayo nuevo 3 — Drenado de 6 s + 10 RAW posteriores

## 7.1 Objetivo

Determinar si el marcador B no aparecía simplemente porque Python todavía estaba drenando una gran cola de eventos A.

Se amplió el período entre:

```text
TX_STOP ACK
```

y:

```text
primer RAW B
```

a:

```text
6 segundos
```

y se aumentó B a:

```text
10 solicitudes RAW
```

## 7.2 Marcadores

```text
A = a40633008806
B = a40733008807
```

## 7.3 Helper ejecutado

```powershell
@'
import time
import threading

from feralrf import PHY, Packet, Radio, RxStreamError

TX_PORT = "COM33"
RX_PORT = "COM88"

PHY_ID = 4
CHANNEL = 25
POWER_DBM = 0

MARKER_A = bytes.fromhex("a40633008806")
MARKER_B = bytes.fromhex("a40733008807")

INTERVAL_US = 0

PRE_TX_SECONDS = 2.0
CONTINUOUS_SECONDS = 1.0
DRAIN_AFTER_STOP_SECONDS = 6.0

ANCHOR_COUNT = 10
ANCHOR_SPACING_SECONDS = 0.100

POST_B_SECONDS = 5.0

RX_WINDOW_SECONDS = 20.0

TX_TIMEOUT = 2.0
RX_STOP_TIMEOUT = 2.0

tx = Radio(TX_PORT)
rx = Radio(RX_PORT)

host_t0 = time.monotonic()

records = []
tx_events = {}
tx_exception = None

tx_started = False


def host_time():
    return time.monotonic() - host_t0


def log(msg=""):
    print(msg, flush=True)


def tx_worker():
    global tx_exception, tx_started

    try:
        time.sleep(PRE_TX_SECONDS)

        log()
        log("[TX STEP] Init TX")
        info = tx.init()
        log(f"[ OK ] TX INFO {info}")

        tx.set_phy(PHY(PHY_ID), CHANNEL)
        tx.set_channel(CHANNEL)
        tx.set_power(POWER_DBM)

        log(f"[ OK ] TX config ACK host_t={host_time():.6f}s")

        tx_events["continuous_request_host"] = host_time()

        log(
            f"[TX] TX_CONTINUOUS A request "
            f"host_t={tx_events['continuous_request_host']:.6f}s"
        )

        tx.transmit_continuous(
            MARKER_A,
            interval_us=INTERVAL_US,
            timeout=TX_TIMEOUT,
        )

        tx_started = True

        tx_events["continuous_ack_host"] = host_time()

        log(
            f"[TX] TX_CONTINUOUS A ACK "
            f"host_t={tx_events['continuous_ack_host']:.6f}s"
        )

        time.sleep(CONTINUOUS_SECONDS)

        tx_events["stop_request_host"] = host_time()

        log(
            f"[TX] TX_STOP request "
            f"host_t={tx_events['stop_request_host']:.6f}s"
        )

        tx.stop_transmit(timeout=TX_TIMEOUT)
        tx_started = False

        tx_events["stop_ack_host"] = host_time()

        log(
            f"[TX] TX_STOP ACK "
            f"host_t={tx_events['stop_ack_host']:.6f}s"
        )

        log(
            f"[TX] Drain wait after STOP: "
            f"{DRAIN_AFTER_STOP_SECONDS:.1f}s"
        )

        tx_events["drain_start_host"] = host_time()

        time.sleep(DRAIN_AFTER_STOP_SECONDS)

        tx_events["drain_end_host"] = host_time()

        log(
            f"[TX] Drain wait finished "
            f"host_t={tx_events['drain_end_host']:.6f}s"
        )

        tx_events["anchor_start_host"] = host_time()

        for i in range(ANCHOR_COUNT):
            req_t = host_time()

            log(
                f"[TX] RAW B request "
                f"{i + 1}/{ANCHOR_COUNT} "
                f"host_t={req_t:.6f}s"
            )

            tx.transmit(
                MARKER_B,
                power_dbm=POWER_DBM,
                timeout=TX_TIMEOUT,
            )

            ack_t = host_time()

            log(
                f"[TX] RAW B ACK "
                f"{i + 1}/{ANCHOR_COUNT} "
                f"host_t={ack_t:.6f}s"
            )

            if i + 1 < ANCHOR_COUNT:
                time.sleep(ANCHOR_SPACING_SECONDS)

        tx_events["anchor_done_host"] = host_time()

        log(
            f"[TX] All RAW B requests completed "
            f"host_t={tx_events['anchor_done_host']:.6f}s"
        )

        log(
            f"[TX] Leaving RX active for another "
            f"{POST_B_SECONDS:.1f}s"
        )

    except Exception as exc:
        tx_exception = exc
        log(
            f"[TX EXCEPTION] "
            f"host_t={host_time():.6f}s {exc!r}"
        )

    finally:
        if tx_started:
            try:
                tx.stop_transmit(timeout=TX_TIMEOUT)
                tx_started = False
                log("[TX CLEANUP] TX_STOP completed")
            except Exception as exc:
                log(f"[TX CLEANUP] TX_STOP failed: {exc!r}")

        try:
            tx.disconnect()
        except Exception:
            pass


def packet_ts(item):
    return getattr(item, "timestamp_us", None)


def is_a(item):
    return (
        isinstance(item, Packet)
        and getattr(item, "crc_ok", False)
        and MARKER_A in item.data
    )


def is_b(item):
    return (
        isinstance(item, Packet)
        and getattr(item, "crc_ok", False)
        and MARKER_B in item.data
    )


rx_started = False
worker = None

try:
    log("FeralRF CONTINUOUS -> STOP -> drain -> RAW anchor")
    log("=================================================")

    log(
        f"TX={TX_PORT} RX={RX_PORT} "
        f"phy={PHY_ID} channel={CHANNEL} "
        f"power={POWER_DBM}"
    )

    log(
        f"marker_A={MARKER_A.hex()} "
        f"marker_B={MARKER_B.hex()}"
    )

    log(
        f"continuous={CONTINUOUS_SECONDS}s "
        f"interval_us={INTERVAL_US} "
        f"drain_after_stop={DRAIN_AFTER_STOP_SECONDS}s "
        f"B_count={ANCHOR_COUNT}"
    )

    log()

    log("[RX STEP] Init RX")
    rx_info = rx.init()
    log(f"[ OK ] RX INFO {rx_info}")

    log("[RX STEP] Configure RX")

    rx.set_phy(PHY(PHY_ID), CHANNEL)
    rx.set_channel(CHANNEL)

    rx.start_rx()
    rx_started = True

    log(
        f"[ OK ] RX_START ACK "
        f"host_t={host_time():.6f}s"
    )

    worker = threading.Thread(
        target=tx_worker,
        name="feralrf-tx-worker",
        daemon=True,
    )

    worker.start()

    log(
        f"[RX PHASE] One uninterrupted "
        f"{RX_WINDOW_SECONDS:.1f}s read window"
    )

    for item in rx.read_packets(timeout=RX_WINDOW_SECONDS):
        host_t = host_time()

        records.append((host_t, item))

        if isinstance(item, Packet):

            ts = packet_ts(item)

            if is_a(item):
                log(
                    f"[RX A] "
                    f"host_t={host_t:.6f}s "
                    f"dev_ts={ts}us "
                    f"crc_ok={item.crc_ok} "
                    f"len={len(item.data)} "
                    f"data={item.data.hex()}"
                )

            elif is_b(item):
                log(
                    f"[RX B] "
                    f"host_t={host_t:.6f}s "
                    f"dev_ts={ts}us "
                    f"crc_ok={item.crc_ok} "
                    f"len={len(item.data)} "
                    f"data={item.data.hex()}"
                )

        elif isinstance(item, RxStreamError):

            log(
                f"[RX ERROR] "
                f"host_t={host_t:.6f}s "
                f"item={item}"
            )

        else:

            log(
                f"[RX OTHER] "
                f"host_t={host_t:.6f}s "
                f"item={item}"
            )

    log()
    log("[RX STEP] read_packets() finished")

    if worker is not None:
        worker.join(timeout=3.0)

    log("[RX STEP] RX_STOP")

    try:
        rx.stop_rx(timeout=RX_STOP_TIMEOUT)
        rx_started = False

        log(
            f"[ OK ] RX_STOP ACK "
            f"host_t={host_time():.6f}s"
        )

    except Exception as exc:

        log(
            f"[FAIL] RX_STOP after read window: "
            f"{exc!r}"
        )

finally:

    if worker is not None and worker.is_alive():
        worker.join(timeout=1.0)

    if rx_started:
        try:
            rx.stop_rx(timeout=RX_STOP_TIMEOUT)
            log("[RX CLEANUP] RX_STOP completed")
        except Exception as exc:
            log(
                f"[RX CLEANUP] RX_STOP failed: "
                f"{exc!r}"
            )

    try:
        rx.disconnect()
    except Exception:
        pass


a_packets = []
b_packets = []
stream_errors = []

for host_t, item in records:

    if isinstance(item, RxStreamError):
        stream_errors.append((host_t, item))
        continue

    if is_a(item):
        ts = packet_ts(item)
        if ts is not None:
            a_packets.append((ts, host_t, item))

    elif is_b(item):
        ts = packet_ts(item)
        if ts is not None:
            b_packets.append((ts, host_t, item))


a_packets.sort(key=lambda x: x[0])
b_packets.sort(key=lambda x: x[0])


print()
print("RESULT")
print("======")

print(f"records_total={len(records)}")
print(f"marker_A_hits={len(a_packets)}")
print(f"marker_B_hits={len(b_packets)}")
print(f"rx_stream_errors={len(stream_errors)}")
print(f"tx_exception={tx_exception!r}")


print()
print("HOST CONTROL TIMING")
print("===================")

for key in (
    "continuous_request_host",
    "continuous_ack_host",
    "stop_request_host",
    "stop_ack_host",
    "drain_start_host",
    "drain_end_host",
    "anchor_start_host",
    "anchor_done_host",
):
    print(f"{key}={tx_events.get(key)}")


print()
print("DEVICE TIMESTAMP ANALYSIS")
print("=========================")

first_a = None
last_a = None
first_b = None
last_b = None

if a_packets:
    first_a = a_packets[0][0]
    last_a = a_packets[-1][0]

    print(f"first_A_dev_ts={first_a}")
    print(f"last_A_dev_ts={last_a}")
    print(f"A_span_us={last_a - first_a}")

else:
    print("No marker A packets observed.")


if b_packets:
    first_b = b_packets[0][0]
    last_b = b_packets[-1][0]

    print(f"first_B_dev_ts={first_b}")
    print(f"last_B_dev_ts={last_b}")
    print(f"B_span_us={last_b - first_b}")

else:
    print("No marker B packets observed.")


a_after_first_b = []

if first_b is not None:

    a_after_first_b = [
        (ts, host_t, item)
        for ts, host_t, item in a_packets
        if ts > first_b
    ]

    print()
    print(
        f"A_hits_after_first_B="
        f"{len(a_after_first_b)}"
    )

    if last_a is not None:
        print(
            f"first_B_minus_last_A_us="
            f"{first_b - last_a}"
        )


print()
print("VERDICT")
print("=======")

if not a_packets:

    print(
        "[INCONCLUSIVE] No CONTINUOUS marker A was "
        "physically observed."
    )

elif not b_packets:

    print(
        "[INCONCLUSIVE] CONTINUOUS marker A was observed, "
        "but none of the ten RAW marker B transmissions "
        "was observed after the 6 s drain interval."
    )

    print(
        "The experiment therefore still lacks a physical "
        "post-STOP timestamp anchor."
    )

elif a_after_first_b:

    print(
        "[FAIL / INVESTIGATE] At least one marker A has a "
        "receiver device timestamp later than the first "
        "physically observed marker B."
    )

else:

    print(
        "[PASS-BOUND] CONTINUOUS marker A was observed."
    )

    print(
        "RAW marker B was physically observed after "
        "TX_STOP and after a 6 s drain interval."
    )

    print(
        "No marker A has a receiver timestamp later than "
        "the first marker B."
    )
'@ | python -
```

---

# 8. Resultado del ensayo con drenado — primera corrida

La primera ejecución produjo:

```text
records_total=138
marker_A_hits=131
marker_B_hits=0
rx_stream_errors=0
tx_exception=None
```

Control host:

```text
continuous_request_host=4.426212000020314
continuous_ack_host=4.458252300042659
stop_request_host=5.458839200029615
stop_ack_host=5.489681600010954
drain_start_host=5.489793100045063
drain_end_host=11.490098900045268
anchor_start_host=11.490174400038086
anchor_done_host=12.731158600014169
```

Timestamps del marcador A:

```text
first_A_dev_ts=560981632
last_A_dev_ts=561129279
A_span_us=147647
```

Resultado:

```text
A_span = 147.647 ms
B = 0/10
```

`RX_STOP`:

```text
TimeoutError(
  'Response timeout (ignored 16 unexpected response(s), last=0x90)'
)
```

El veredicto automático fue:

```text
[INCONCLUSIVE] CONTINUOUS marker A was observed, but
none of the ten RAW marker B transmissions was observed
after the 6 s drain interval.
```

---

# 9. Repetición independiente del ensayo con drenado

Se volvió a ejecutar el mismo helper.

## 9.1 Resultado

```text
records_total=141
marker_A_hits=137
marker_B_hits=0
rx_stream_errors=0
tx_exception=None
```

Control host:

```text
continuous_request_host=4.44152579997899
continuous_ack_host=4.474580299982335
stop_request_host=5.474988000001758
stop_ack_host=5.5083030999521725
drain_start_host=5.508403299958445
drain_end_host=11.508704999985639
anchor_start_host=11.508792499953415
anchor_done_host=12.746797600004356
```

Timestamps:

```text
first_A_dev_ts=52560985
last_A_dev_ts=52713092
A_span_us=152107
```

Por tanto:

```text
A_span = 152.107 ms
B = 0/10
```

`RX_STOP` volvió a fallar:

```text
TimeoutError(
  'Response timeout (ignored 17 unexpected response(s), last=0x90)'
)
```

## 9.2 Resultado reproducible

Las dos corridas largas dieron el mismo patrón general:

| Corrida  | A hits | Span A según receptor | RAW B solicitados | B observados | RX_STOP |
| -------- | -----: | --------------------: | ----------------: | -----------: | ------- |
| Drain #1 |    131 |            147.647 ms |                10 |            0 | Timeout |
| Drain #2 |    137 |            152.107 ms |                10 |            0 | Timeout |

Esto hizo poco probable que simplemente aumentar la espera post-STOP resolviera el problema.

---

# 10. Interpretación del buffering observado

Un resultado constante de los ensayos fue la gran diferencia entre:

```text
timestamp_us del receptor
```

y:

```text
momento de entrega/impresión en Python
```

Ejemplo:

```text
137 paquetes A
span de timestamps internos = 152.107 ms
tiempo de entrega host      = muchos segundos
```

Esto demuestra que existe un desacoplamiento importante entre:

```text
recepción/evento registrado por dispositivo
```

y:

```text
consumo de ese evento por el host
```

Por ello, no es válido afirmar:

```text
un paquete fue impreso después del TX_STOP ACK
→ fue recibido por RF después de TX_STOP
```

Los paquetes pueden estar pendientes en una cola/buffer y ser entregados mucho más tarde.

La magnitud exacta y ubicación de esta cola no quedó localizada en esta etapa.

Posibles niveles incluyen:

```text
RF Core / driver
→ firmware CC1352
→ cola/eventos de FeralRF
→ UART
→ RP2040
→ USB CDC
→ serial host
→ parser Python
```

No se atribuye todavía la causa a ninguno de ellos.

---

# 11. Resultado nuevo relacionado con `RX_STOP`

Inicialmente se planteó que el timeout de `RX_STOP` observado en el helper v1 podía deberse a:

```text
read_packets()
```

y:

```text
stop_rx()
```

leyendo simultáneamente el mismo stream.

El helper v2 eliminó esa carrera.

Sin embargo, `RX_STOP` volvió a fallar después de que:

```python
read_packets(timeout=...)
```

había terminado.

El patrón se reprodujo posteriormente varias veces:

```text
Response timeout
(ignored 16/17 unexpected response(s), last=0x90)
```

Por tanto:

> La carrera del primer helper era un defecto metodológico real, pero no explica por sí sola el timeout recurrente de `RX_STOP`.

La semántica exacta del tipo `0x90` no quedó demostrada durante esta campaña y no debe inferirse sin revisar las definiciones de protocolo y el tratamiento de eventos asíncronos.

---

# 12. Control de recuperación posterior a `CONTINUOUS/STOP`

Al observar:

```text
TX_STOP ACK
→ 6 s
→ 10 RAW B ACK
→ 0 B recibidos
```

surgieron al menos dos hipótesis:

## H1 — problema posterior a STOP en TX

```text
COM33
CONTINUOUS
→ STOP
→ estado TX incorrecto
→ RAW devuelve ACK
→ RAW no llega a RF
```

## H2 — problema de estado/cola en RX

```text
COM88
RX durante CONTINUOUS
→ alta actividad / cola
→ estado RX no limpio
→ RAW B podría existir
→ peer no lo entrega
```

El timeout de `RX_STOP` hacía especialmente necesario separar ambas.

Se diseñó entonces un **control de recuperación en una sesión nueva, sin reset ni flashing**.

---

# 13. Intento inicial de control RAW — error procedimental

Primero se propuso:

```powershell
python .\python\examples\smoke_tx_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40833008808 --count 10 --interval-ms 100 --tx-timeout 2
```

El script respondió:

```text
error: unrecognized arguments: --count 10 --interval-ms 100
```

Esto demostró que:

```text
smoke_tx_phase1.py
```

no soporta:

```text
--count
--interval-ms
```

Ese intento **no transmitió** y no debe utilizarse como evidencia RF.

En paralelo se había ejecutado RX sin TX válido:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 15 --marker-hex a40833008808 --match-mode contains --min-hits 10 --print-limit 50
```

Resultado:

```text
packets_total=24
crc_ok=24
marker_hits=0
RX_STOP ACK
```

Dado que no existió TX válido dentro de esa ventana:

```text
marker_hits=0
```

no constituye fallo de RAW.

---

# 14. Control RAW de recuperación corregido

## 14.1 RX

Terminal A:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 20 --marker-hex a40833008808 --match-mode contains --min-hits 10 --print-limit 80
```

## 14.2 TX

Terminal B:

```powershell
1..10 | ForEach-Object {
    Write-Host "TX $_/10"
    python .\python\examples\smoke_tx_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40833008808 --tx-timeout 2
    Start-Sleep -Milliseconds 100
}
```

Cada proceso TX realizó:

```text
Connect + RADIO_INIT + GET_INFO
SET_PHY + SET_CHANNEL + SET_POWER
TX_RAW
TX_RAW ACK
disconnect
```

Las diez solicitudes devolvieron:

```text
[ OK ] TX_RAW ACK
[ OK ] TX SMOKE PASS
```

## 14.3 Resultado OTA

COM88 recibió:

```text
[HIT] ts=226809804us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=228321888us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=229803276us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=231316349us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=232798574us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=234290064us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=235780671us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=237277362us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
[HIT] ts=238772347us ch=25 rssi=-47 crc_ok=True len=8 data=a40833008808356d
```

Resumen:

```text
packets_total=32
crc_ok=32
marker_hits=9
```

`RX_STOP`:

```text
[ OK ] RX_STOP ACK
```

El gate configurado exigía 10:

```text
[FAIL] RX PROBE FAIL hits=9 min_hits=10
```

## 14.4 Clasificación correcta

Formalmente:

```text
9/10 < 10/10
```

por lo que el gate estricto es:

```text
PARTIAL
```

Sin embargo, físicamente:

```text
RAW post-CONTINUOUS/STOP = recuperado OTA
```

porque 9 paquetes inequívocamente atribuibles fueron recibidos con:

```text
crc_ok=True
len=8
RSSI=-47 dBm
payload=a40833008808 + FCS aparente 356d
```

---

# 15. Comparación esperado vs observado

| Prueba                        | Resultado esperado              | Resultado observado                                   | Clasificación                                           |
| ----------------------------- | ------------------------------- | ----------------------------------------------------- | ------------------------------------------------------- |
| `TX_CONTINUOUS interval=0`    | múltiples paquetes OTA          | múltiples paquetes A observados en todas las corridas | `PASS-RF`                                               |
| `TX_STOP` API                 | ACK                             | ACK en todas las corridas                             | `PASS-CONTROL`                                          |
| RX continuo post-STOP         | observar transición sin hueco   | observación contaminada por fuerte buffering host     | `INCONCLUSIVE` para cese inmediato                      |
| Ancla B 250 ms post-STOP      | recibir B y comparar timestamps | 0/3 B                                                 | `INCONCLUSIVE`                                          |
| Ancla B tras 6 s, corrida 1   | recibir al menos un B           | 0/10 B                                                | `FAIL` del criterio de recepción B; causa no localizada |
| Ancla B tras 6 s, corrida 2   | recibir al menos un B           | 0/10 B                                                | resultado reproducido                                   |
| `RX_STOP` tras sesión cargada | ACK                             | timeout con 16–17 respuestas inesperadas, `last=0x90` | anomalía reproducible                                   |
| RAW después, nueva sesión     | recuperación OTA                | 9/10 paquetes recibidos                               | `PARTIAL`, recuperación física demostrada               |
| `RX_STOP` nueva sesión        | ACK                             | ACK                                                   | recuperación demostrada                                 |

---

# 16. Qué se demostró

## 16.1 `TX_CONTINUOUS interval_us=0`

Queda reforzado como físicamente funcional.

Las nuevas corridas produjeron:

```text
43
131
137
```

hits atribuibles en diferentes ejecuciones.

Todos los paquetes mostrados del marcador fueron:

```text
crc_ok=True
len=8
```

La relación observada mantiene el patrón:

```text
6 bytes solicitados
+
2 bytes finales
=
8 bytes recibidos
```

consistentemente con la semántica IEEE previamente documentada.

---

## 16.2 `TX_STOP` a nivel de control

`stop_transmit()` recibió ACK repetidamente.

Por tanto:

```text
TX_STOP command path / API acceptance = validado
```

Esto **no equivale** a demostrar por sí mismo:

```text
cese RF físico exacto
```

---

## 16.3 Cese físico inmediato

No quedó demostrado.

La prueba propuesta precisamente para cerrar esta cuestión descubrió que la entrega host está fuertemente desacoplada del `timestamp_us` del receptor.

Por ello no puede usarse:

```text
host_t de Python
```

como reloj RF.

El intento de crear un ancla RF post-STOP mediante B tampoco consiguió recepción B dentro de la misma sesión cargada.

La clasificación defendible sigue siendo:

```text
TX_STOP físico inmediato:
NOT FULLY VALIDATED / INCONCLUSIVE
```

---

# 17. Anomalía nueva reproducible

Las pruebas dirigidas sí descubrieron un comportamiento reproducible no visible con el procedimiento post-STOP original:

```text
CONTINUOUS A
→ TX_STOP ACK
→ mantener misma sesión
→ solicitar RAW B
→ ACK de RAW B
→ 0 B observados
→ RX_STOP timeout
```

El patrón apareció incluso después de:

```text
6 s
```

de espera antes de solicitar los RAW B.

Esto indica un problema o interacción de **estado/lifecycle/colas dentro de esa sesión**, aunque todavía no localiza el componente responsable.

---

# 18. Recuperación sin reset

El control posterior es importante porque demuestra:

```text
sesión problemática
→ termina
→ nueva sesión RX/TX
→ RAW vuelve a observarse físicamente
→ RX_STOP vuelve a responder
```

No se realizó:

```text
reset
flash
power-cycle
```

entre la sesión problemática y el control RAW.

Resultado:

```text
9/10 RAW recibidos
RX_STOP ACK
```

Por tanto no existe evidencia de que:

```text
CONTINUOUS/STOP deje al dispositivo permanentemente inutilizable
```

La hipótesis de un **poisoning persistente** queda debilitada.

---

# 19. Hipótesis actualizadas

## H1 — estado/lifecycle de TX posterior a CONTINUOUS

Compatible con:

```text
TX_STOP ACK
RAW B ACK
B no observado dentro de la misma sesión
```

Pero no confirmado, porque el receptor también presenta anomalías.

---

## H2 — estado/cola RX posterior a alta actividad

Compatible con:

```text
muchos paquetes pendientes
gran diferencia device timestamp ↔ host delivery
RX_STOP timeout
0x90 repetido
B no observado
```

Actualmente es una hipótesis importante.

No está demostrado si el problema está en:

```text
RF Core
firmware CC1352
DataTask / Output path
UART
RP2040
USB CDC
host serial
Python
```

---

## H3 — `TX_STOP` no detiene realmente TX

No queda demostrada.

La apariencia de múltiples A después del ACK STOP en la consola no basta porque esos paquetes tienen timestamps internos correspondientes a una ventana mucho más corta y pudieron permanecer en cola.

Para demostrar esta hipótesis se necesitaría una referencia temporal RF verdaderamente independiente o una instrumentación interna fiable de completion/cancelación.

---

## H4 — poisoning persistente

Actualmente debilitada.

Evidencia contra ella:

```text
9/10 RAW OTA después
RX_STOP normal después
sin reset
```

---

# 20. Discrepancia temporal importante

En una corrida:

```text
137 paquetes A
```

abarcaron solamente:

```text
152.107 ms
```

según `timestamp_us`.

Sin embargo fueron entregados durante muchos segundos.

Esto significa que el sistema observado tiene una cola suficientemente grande como para invalidar análisis de timing basados únicamente en la impresión host.

Esta conclusión es experimental.

No se conoce todavía:

* profundidad de la cola;
* ubicación exacta;
* política de drop;
* relación entre `0x90` y los eventos pendientes;
* si `read_packets()` consume todos los tipos de respuesta;
* cómo se prioriza el ACK de `RX_STOP` frente a eventos asíncronos.

---

# 21. Sobre `0x90`

Durante las sesiones de alta actividad se observaron errores como:

```text
Response timeout
(ignored 16 unexpected response(s), last=0x90)
```

o:

```text
Response timeout
(ignored 17 unexpected response(s), last=0x90)
```

Este documento **no asigna significado semántico a `0x90`**.

Queda pendiente revisar explícitamente:

```text
python/feralrf/protocol.py
python/feralrf/radio.py
firmware protocol definitions
OutputIF / eventos asíncronos
```

antes de declarar qué tipo de respuesta representa.

---

# 22. Resultado final de esta fase

## `TX_CONTINUOUS`

```text
interval_us=0
→ PASS-RF de repetición
```

La periodicidad física exacta sigue sin validarse.

---

## `TX_STOP`

```text
Control/API:
VALIDATED

Cese RF inmediato:
NOT FULLY VALIDATED
```

Las ventanas post-STOP históricas sin marcador son compatibles con cese, pero la nueva prueba continua descubrió buffering suficiente para impedir una afirmación temporal fuerte basada en el host.

---

## `RAW` posterior dentro de la misma sesión cargada

```text
ACK:
sí

OTA:
0/10, reproducido
```

Clasificación:

```text
anomalía reproducible dentro de la sesión
causa no localizada
```

---

## Recuperación posterior

```text
RAW nueva sesión:
9/10 OTA

RX_STOP:
ACK
```

Clasificación:

```text
PARTIAL del gate 10/10
pero recuperación física demostrada
```

No se observó bloqueo persistente.

---

# 23. Conclusión técnica defendible

La conclusión de esta etapa no es:

> `TX_STOP` está roto.

Tampoco es:

> `TX_STOP` quedó completamente validado.

La conclusión defendible es:

> `TX_STOP` está validado a nivel de control/ACK, pero su cese RF inmediato continúa sin estar completamente validado. La prueba con RX continuo reveló que el camino de entrega RX presenta un buffering significativo: paquetes registrados por el receptor dentro de ventanas de decenas o cientos de milisegundos pueden tardar varios segundos en llegar al host. Esto invalida utilizar el instante de impresión Python como referencia directa de recepción RF.

Además:

> Durante la misma sesión de alta actividad `CONTINUOUS → TX_STOP`, transmisiones RAW posteriores son aceptadas por la API pero no son observadas por el peer, mientras `RX_STOP` puede terminar en timeout consumiendo respuestas inesperadas cuyo último tipo reportado es `0x90`.

Finalmente:

> El sistema recupera operación OTA sin reset cuando se reconstruyen las sesiones: en el control posterior se recibieron 9/10 paquetes RAW con CRC válido y `RX_STOP` volvió a responder correctamente. Por tanto, no existe evidencia de un poisoning permanente del dispositivo; la anomalía parece estar acotada al estado/lifecycle/colas de la sesión posterior a `CONTINUOUS/STOP`.

---

# 24. Estado frente a los resultados esperados

## Esperado originalmente según la guía

```text
CONTINUOUS
→ múltiples RX
→ STOP ACK
→ ausencia del marcador después
```

## Observado históricamente

```text
CONTINUOUS interval=0
→ múltiples RX

STOP ACK
→ sí

nueva ventana post-STOP
→ 0 hits
```

Compatible con STOP, pero con hueco temporal.

## Observado en esta ampliación

```text
RX continuo
→ CONTINUOUS A
→ STOP ACK
→ paquetes A siguen llegando al host desde cola
→ RAW B posterior recibe ACK
→ B no aparece
→ RX_STOP timeout
```

Después:

```text
nueva sesión
→ RAW vuelve a funcionar 9/10
→ RX_STOP ACK
```

Por tanto, el criterio de cese inmediato no se cerró, pero se obtuvo información nueva sobre el lifecycle de la sesión.

---

# 25. Decisión de cierre

No se recomienda seguir repitiendo variaciones temporales OTA de esta misma prueba.

La evidencia experimental disponible ya justifica pasar a **análisis dirigido de implementación**.

El siguiente recorrido prioritario debe ser:

```text
TX_CONTINUOUS
→ command_processor
→ ControlTask_onTxContinuous()
→ estado pending / scheduler
→ RadioIF

TX_STOP
→ handler
→ limpieza de estado continuous/burst
→ cancelación/completion RF
→ estado posterior

TX_RAW
→ ACK
→ ejecución diferida
→ RadioIF_transmitRaw()
```

y en paralelo:

```text
RX_START
→ eventos RX
→ OutputIF / UART
→ Response 0x90
→ read_packets()
→ cola
→ RX_STOP
```

Las preguntas concretas para código son:

1. ¿Qué estado se modifica al iniciar `TX_CONTINUOUS`?
2. ¿Qué variables limpia realmente `TX_STOP`?
3. ¿El ACK de STOP se emite antes de que el RF Core haya cancelado/completado la operación?
4. ¿Puede quedar `continuous pending` después del ACK?
5. ¿Puede ese estado impedir o retrasar un `TX_RAW` posterior?
6. ¿Qué representa exactamente `0x90`?
7. ¿Por qué `_read_response()` encuentra 16–17 respuestas inesperadas antes de agotar el timeout?
8. ¿`read_packets()` está drenando el stream a menor velocidad que la producción de eventos?
9. ¿Dónde se acumulan los paquetes cuya separación `timestamp_us` es ~1.1 ms pero cuya entrega al host tarda ~120 ms por objeto?
10. ¿Por qué una nueva sesión restaura RAW y `RX_STOP` sin reset?

---

# 26. Clasificación final

| Función / propiedad                                            | Estado al cierre                                   |
| -------------------------------------------------------------- | -------------------------------------------------- |
| `TX_CONTINUOUS`, `interval_us=0`, produce múltiples tramas OTA | **VALIDATED / PASS-RF**                            |
| `TX_CONTINUOUS`, temporización física exacta                   | **NOT FULLY VALIDATED**                            |
| `TX_STOP` — aceptación de comando                              | **VALIDATED / PASS-CONTROL**                       |
| `TX_STOP` — cese RF inmediato                                  | **PARTIAL / NOT FULLY VALIDATED**                  |
| Ausencia de marcador en ventanas posteriores independientes    | **OBSERVED / compatible con cese**                 |
| RX continuo alrededor de STOP                                  | **EXECUTED**                                       |
| Uso de `host_t` para ordenar RF                                | **INVALIDATED por buffering observado**            |
| RAW B post-STOP dentro de misma sesión                         | **0/10, reproducido**                              |
| `RX_STOP` tras sesión cargada                                  | **ANOMALÍA REPRODUCIBLE**                          |
| Significado de `0x90`                                          | **OPEN**                                           |
| Recuperación RAW en nueva sesión sin reset                     | **9/10 OTA / PARTIAL pero físicamente demostrada** |
| `RX_STOP` en nueva sesión                                      | **PASS**                                           |
| Poisoning persistente                                          | **NO DEMOSTRADO / debilitado**                     |
| Causa raíz                                                     | **OPEN — trasladar a análisis de código**          |

---

# 27. Fuentes relacionadas

## Procedimiento

`Guía enfocada OTA FeralRF - TX RX con dos CatSniffer.md`

Sección:

```text
### 4. CONTINUOUS + STOP físico
```

La guía prescribe negativo, RX previo, `TX_CONTINUOUS`, `TX_STOP`, control posterior y conservación de timestamps/ACK/cese.

## Evidencia previa

[[02 - OTA IEEE - CONTINUOUS y STOP]]

El reporte previo clasifica:

```text
CONTINUOUS interval=0: PASS-RF de repetición
STOP inmediato: no completamente validado
RX_STOP/0x90: pendiente
```

y propone explícitamente RX continuo antes y después de STOP como trabajo posterior.

## Evidencia nueva

Fuente primaria:

```text
stdout literal de PowerShell compartido durante la sesión del 2026-10-09
```

Incluye:

* helper RX continuo;
* helper con marcador B post-STOP;
* helper con drenado de 6 s;
* repetición independiente;
* errores de `RX_STOP`;
* intento RAW con argumentos inválidos;
* control RAW corregido;
* recuperación 9/10 OTA sin reset.

---

# 28. Resumen ejecutivo

La ampliación experimental cumplió su propósito principal de explorar STOP de forma más rigurosa, aunque no produjo una demostración definitiva del cese RF inmediato.

Se confirmó nuevamente que:

```text
TX_CONTINUOUS interval_us=0
```

produce múltiples tramas OTA válidas.

Se verificó reiteradamente:

```text
TX_STOP ACK
```

pero el nuevo RX continuo mostró una cola suficientemente grande para que el orden de impresión del host no pueda utilizarse como orden RF.

Dos ensayos posteriores intentaron crear un ancla física post-STOP mediante RAW B. En ambos casos los comandos RAW fueron aceptados, pero B no fue observado, mientras que `RX_STOP` terminó repetidamente en timeout con eventos inesperados.

Finalmente se creó una nueva sesión, sin reset ni flashing, y RAW volvió a funcionar físicamente:

```text
9/10 paquetes recibidos
crc_ok=True
RSSI≈-47 dBm
RX_STOP ACK
```

La evidencia favorece actualmente una anomalía **de estado/lifecycle/colas dentro de la sesión cargada de CONTINUOUS/STOP**, no un bloqueo persistente del dispositivo.

La etapa experimental de STOP queda cerrada aquí.

El siguiente paso recomendado es análisis estático y dirigido de:

```text
TX_CONTINUOUS
TX_STOP
TX_RAW
RX_START
RX_STOP
eventos 0x90
colas de OutputIF/UART/Python
```

antes de proponer nuevas pruebas o modificaciones de firmware.
