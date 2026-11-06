# Preparación, evidencia, cleanup y recovery comunes

Estas reglas aplican a todos los procedimientos. Los COM son variables del banco, no parte del contrato.

## Entorno y working directory

Los comandos de este Runbook parten de `FeralRF/python`:

```powershell
$FeralRFRoot = 'C:\path\to\FeralRF'
Set-Location (Join-Path $FeralRFRoot 'python')
$env:PYTHONPATH = (Resolve-Path '.').Path
python --version
python -c "import feralrf; print(feralrf.__file__)"
```

Sustituya `$FeralRFRoot` por la ruta local del checkout. Todos los comandos posteriores asumen que el working directory es `FeralRF/python`, y la ruta impresa por `feralrf.__file__` debe pertenecer al checkout previsto. El paquete exige Python `>=3.9`; sus dependencias normales están declaradas en [`python/pyproject.toml`](../../../python/pyproject.toml). El uso directo mediante `PYTHONPATH` evita depender de otro editable instalado. No instale la dependencia opcional KillerBee como parte de una campaña ordinaria.

Antes de ejecutar registre:

```powershell
git branch --show-current
git rev-parse HEAD
git status --short
```

## Puertos, identidades y roles

- Cat-Bridge opera a 921600 baud y transporta el protocolo FeralRF al CC1352.
- Cat-Shell opera a 115200 baud y controla funciones RP2040 como `identify`, `status`, `band1`, `band2`, `boot` y `exit`.
- Asocie cada Bridge y Shell a una placa física. `COM88`→`COM87` demuestra que Bridge+2 no es una regla.
- Designe explícitamente `TX`/`RX` o `DUT`/`peer`. Los ordinales históricos “#2/#3” cambiaron entre campañas.
- `Band: 0/1` es estado lógico RP2040, no lectura eléctrica ni prueba del switch o la ruta de antena.

Variables de ejemplo, que deben sustituirse:

```powershell
$TxBridge = 'COM33'; $TxShell = 'COM35'
$RxBridge = 'COM88'; $RxShell = 'COM87'
```

<a id="op-01"></a>
## OP-01 — Identificar placas, Bridge y Shell

**Estado:** `CURRENT_PARTIAL`
**Objetivo:** enumerar puertos y asociar interfaces sin aritmética.
**Capacidad relacionada:** [sesión, transporte y configuración](../../CURRENT_VALIDATION_MATRIX.md#51-sesión-transporte-y-configuración).
**Demuestra:** identidad host-visible de un Shell y asociación operacional con una placa.
**No demuestra:** identidad inequívoca del binario CC1352, revisión física, switch o RF.

**Hardware/software:** una o dos CatSniffer; PowerShell, WMI/CIM y `pyserial` del entorno FeralRF.
**Precondiciones:** monitores serie cerrados; para máxima certeza conecte una placa a la vez.
**Entrada:** `Get-CimInstance` y consulta Shell; **working directory:** cualquiera para CIM, `FeralRF/python` para Python.

**Comando:**

```powershell
Get-CimInstance Win32_SerialPort |
    Sort-Object DeviceID |
    Select-Object DeviceID, Name, PNPDeviceID |
    Format-List

python -c "import serial,time; s=serial.Serial('COM_SHELL',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
```

**Parámetros/procedimiento:** sustituya `COM_SHELL`; consulte uno por uno, etiquete la placa y relacione el Bridge mediante PNP/location o desconexión controlada.
**Observación esperada:** identificación/versión y `Mode: ..., Band: ..., FW: ...`.
**Aceptación:** `PASS` sólo si cada par queda asociado sin inferencia; `PARTIAL` si se identifican Shell pero el Bridge queda ambiguo; `BLOCKED` si el puerto está ocupado o no responde.
**Cleanup/recovery:** cierre `serial.Serial`; ante `Access is denied`, cierre el dueño del puerto y reenumere.
**Evidencia:** inventario completo, PNP ID, etiqueta física, respuesta Shell, fecha y operador.
**Limitaciones:** la enumeración puede cambiar tras reconectar.
**Historia:** [`User-Manual/05`, OP-01](<../../User-Manual/05 - Manual práctico de uso de FeralRF.md>).

<a id="op-02"></a>
## OP-02 — Inicializar y reconectar

**Estado:** `CURRENT_VALIDATED`
**Objetivo:** comprobar cinco aperturas/cierres independientes del Bridge.
**Capacidad relacionada:** [conexión y reapertura](../../CURRENT_VALIDATION_MATRIX.md#51-sesión-transporte-y-configuración).
**Demuestra:** apertura, `RADIO_INIT`/`GET_INFO`, desconexión y liberación básica del puerto entre procesos.
**No demuestra:** RF, pérdida USB, sesiones largas ni identidad source→binario.

**Hardware/software:** una CatSniffer, Bridge identificado, API `Radio.init()`/`disconnect()`.
**Precondiciones:** OP-01 terminado; equipo idle; ningún proceso sobre el COM.
**Entrada/working directory:** API pública, `FeralRF/python`.

```powershell
1..5 | ForEach-Object {
    Write-Host "`n===== CICLO $_ / 5 ====="
    python -c "from feralrf import Radio; r=Radio(port='COM_BRIDGE'); print(r.init()); r.disconnect()"
    if ($LASTEXITCODE -ne 0) { throw "Fallo en ciclo $_" }
}
```

**Parámetros/procedimiento:** sustituya `COM_BRIDGE`; no abra procesos concurrentes.
**Observación esperada:** cinco `DeviceInfo(...)` sin timeout ni puerto ocupado.
**Aceptación:** `PASS` = 5/5 y puerto liberado; `FAIL` = un ciclo llega a init y falla su criterio; `BLOCKED` = puerto/estado impide iniciar.
**Cleanup/recovery:** cada proceso desconecta; ante fallo detenga el bucle, libere COM y use OP-03 sólo si sigue sin responder.
**Evidencia:** cinco salidas y exit codes, COM, versión reportada y estado git.
**Limitaciones:** `1.0.0` y `FERALRF1` no identifican una placa o commit.
**Historia:** EV-03 y [`User-Manual/05`, OP-02](<../../User-Manual/05 - Manual práctico de uso de FeralRF.md>).

<a id="op-03"></a>
## OP-03 — Reset manual y recuperación

**Estado:** `CURRENT_PARTIAL`
**Objetivo:** recuperar el CC1352 mediante el Shell explícito y comprobar el Bridge.
**Capacidad relacionada:** [recovery manual y `reset_device()`](../../CURRENT_VALIDATION_MATRIX.md#51-sesión-transporte-y-configuración).
**Demuestra:** ruta host-visible `boot`→`exit` y reinicialización posterior.
**No demuestra:** power-cycle, forma eléctrica del reset, hash del firmware ni recuperación universal.

**Hardware/software:** una placa con par Bridge/Shell verificado, `pyserial` y API `Radio`.
**Precondiciones:** todos los procesos cerrados; último fallo preservado; Shell dado explícitamente. No use `Radio.reset_device()` si el mapping no fue verificado.
**Entrada/working directory:** Shell y API, `FeralRF/python`.

```powershell
python -c "import serial,time; s=serial.Serial('COM_SHELL',115200,timeout=1,write_timeout=1); s.write(b'boot\r\n'); s.flush(); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.write(b'exit\r\n'); s.flush(); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close(); time.sleep(3.5)"
python -c "from feralrf import Radio; r=Radio(port='COM_BRIDGE'); print('INIT:',r.init()); print('STATS:',r.get_stats()); r.disconnect()"
```

**Parámetros/procedimiento:** sustituya ambos COM; cierre Shell antes de abrir Bridge.
**Observación esperada:** `BOOT`, `PASSTHROUGH`, luego `DeviceInfo` y stats.
**Aceptación:** `PASS` sólo para la recuperación host-visible; mantenga el procedimiento `CURRENT_PARTIAL` porque depende del mapping/estado y no es power-cycle.
**Cleanup/recovery:** cierre ambos puertos. Si init falla, no repita indefinidamente: conserve evidencia, retire toda alimentación cuando el procedimiento que originó el estado lo exija, espere ≥15 s, reenumere y ejecute OP-02 una vez.
**Evidencia:** Shell/Bridge explícitos, transcript, esperas, init/stats y estado anterior/posterior.
**Limitaciones:** `reset_device()` implementa esta idea usando Bridge+2; sólo es seguro si esa relación se verificó para la unidad.
**Historia:** EV-04 y [`User-Manual/05`, OP-03](<../../User-Manual/05 - Manual práctico de uso de FeralRF.md>).

## Inicialización, banda y orden de operación

Después de OP-01 y antes de abrir los Bridges:

- IEEE 2,4 GHz: envíe `band1`, luego `status`, a cada Shell explícito.
- Sub-GHz: envíe `band2`, luego `status`, a cada Shell explícito.
- Cierre los Shell; abra los Bridges; configure PHY/preset; inicie RX antes de TX.

Ejemplo, sustituyendo comando y puerto:

```powershell
python -c "import serial,time; s=serial.Serial('COM_SHELL',115200,timeout=1); s.reset_input_buffer(); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
```

## Markers y evidencia OTA

- Use un marker distinto por ejecución y por dirección.
- Para estimar delivery por solicitud, incluya un contador único por TX. Diez recepciones del mismo marker no constituyen PDR por secuencia.
- Un control negativo con `ota_rx_probe.py --min-hits 0` sólo es válido si el resumen dice literalmente `marker_hits=0`; el exit 0 no exige ausencia.
- Preserve bytes completos, `crc_ok`, conteos, ventana, roles, orden temporal y cleanup. RSSI/LQI/timestamp no son instrumentación calibrada.

| Observación | Sí prueba | No prueba |
|---|---|---|
| ACK correlacionado | aceptación/control | ejecución física o peer |
| `scheduled=N` | solicitud aceptada/programada | N emisiones |
| marker CRC-good en peer | entrega host-visible | frecuencia, potencia, modulación o protocolo nombrado |
| `TX_STOP ACK` | stop aceptado | instante físico de cese |
| `RX_STOP ACK` | cleanup host-visible | ausencia universal de estado residual |

## Cleanup y recovery general

1. Espere la terminación y exit code de cada proceso; no use Ctrl+C como ruta ordinaria.
2. Exija el ACK de stop que corresponda y cierre todos los objetos/COM.
3. Si el stop agota timeout, registre `BLOCKED` para cleanup aunque hubiera evidencia funcional positiva.
4. Preserve stdout/stderr antes de cambiar el estado.
5. Libere los COM. Si el procedimiento lo exige, retire USB y alimentación externa de la placa durante ≥15 s.
6. Reenumere con OP-01, ejecute OP-02 y como máximo un control conocido. Si vuelve a bloquearse, detenga la campaña.

## OOK: estado aislado y peligroso

`mod_type=2` carga patches genOOK. El backend actual requiere reset completo del CC1352, power-cycle o reflash antes de usar con seguridad otro modo; `RADIO_INIT` no descarga esos patches. La única recuperación demostrada recientemente fue el power-cycle físico.

`Radio.reset_device()` envía `boot`/`exit`, pero deriva Shell como Bridge+2: no es universalmente seguro. No use `--auto-reset` de `smoke_prop_phase1.py` como garantía de recovery OOK. OP-16 permanece `BLOCKED`; OOK debe ser la última operación de una campaña aislada, con power-cycle disponible antes y después.

## Evidencia mínima

Use el [registro canónico](README.md#registro-canonico-de-ejecucion). Como mínimo preserve comando exacto, cwd, timestamp, commit/dirty, Python, binarios si se conocen, puertos y roles, salida completa, marker/conteos, exit code, cleanup y archivos generados. Escriba `UNKNOWN` en vez de inferir identidad.
