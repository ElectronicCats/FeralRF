# Procedimientos IEEE, TX y modos de prueba

## Prerrequisitos comunes

Complete OP-01 y la preparación de PowerShell de [[05 - Manual práctico de uso de FeralRF]]. El directorio obligatorio es `FeralRF\python`. Para IEEE, seleccione la ruta 2,4 GHz en ambos Shell antes de abrir los Bridges y consulte el estado final:

```powershell
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=1); s.reset_input_buffer(); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=1); s.reset_input_buffer(); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
```

Con RP2040 `v3.1.0.1`, el resultado lógico esperado es `2.4GHz Band` y después `Band: 0`. La secuencia histórica `band2`→`band1` imprimía ambas respuestas acumuladas; no significaba dos rutas activas. Éste sigue siendo un control de software: no mide GPIO, posición física del switch ni frecuencia. Cat-Shell selecciona ruta; FeralRF, por Cat-Bridge, configura PHY/canal.

---

## OP-04 — Recibir IEEE 802.15.4 en canal 25

### 1. Objetivo

Inicializar una CatSniffer, configurar PHY 4/canal 25, observar durante 30 s y detener RX.

### 2. Evidencia histórica y estado

**VALIDADO EN LAS CONDICIONES INDICADAS.** EV-05 ejecutó tres ventanas formales con 41, 43 y 43 paquetes y cleanup correcto. El 21-oct se ejecutó nuevamente en `COM88`: 54 paquetes, primer paquete `crc_ok=True`, `RX_STOP ACK` y exit satisfactorio. Fuente declarada históricamente: tráfico Zigbee continuo; su identidad no se correlacionó independientemente.

### 3. Equipo necesario

Una CatSniffer V3, ruta 2,4 GHz, antena compatible y Bridge histórico `COM88`. Un emisor autorizado en canal 25 era parte de la condición histórica.

### 4. Antes de abrir las terminales

Prepare el entorno, confirme `COM88/COM87`, seleccione `band1` y cierre otros monitores. No hace falta una segunda CatSniffer para esta receta.

### 5. Disposición de terminales

**Terminal 1 — RX / CatSniffer #3, Bridge COM88.** Patrón A; el script termina solo tras la ventana.

### 6. Ejecución paso a paso

Comando histórico exacto, desde `FeralRF\python`, repetido tres veces:

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF\python'
python .\examples\smoke_phy4_ieee154.py --port COM88 --channel 25 --duration 30
```

### 7. Salida esperada

Salida histórica representativa:

```text
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[ OK ] SET_PHY ACK
[ OK ] SET_CHANNEL ACK
[ OK ] RX_START ACK
[ OK ] packets=41
[ OK ] first: ... ch=25 rssi=-85 lqi=52 crc_ok=True len=52
[ OK ] RX_STOP ACK
[ OK ] SMOKE TEST PASS
```

### 8. Interpretación

Los ACK prueban control. `packets>0` y `crc_ok=True` aportan recepción reportada por el DUT. No convierten automáticamente esos bytes en Zigbee ni prueban asociación a una red.

### 9. Limpieza

El script llama `stop_rx()` y `disconnect()`; no pulse Ctrl+C durante los 30 s salvo emergencia. Exit 0 corresponde al PASS del smoke.

### 10. Fallos y recuperación

Si `RX_STOP` agota timeout, conserve salida, no inicie otra prueba y aplique recuperación general. Cero paquetes puede significar ausencia de tráfico; no es por sí solo FAIL del receptor.

### 11. Límites de la conclusión

No se preservaron PCAP/bytes completos, no hubo caracterización de sensibilidad/rendimiento ni atribución externa del emisor.

### 12. Fuentes

[[../../05 - Evaluación/EV-05 — Primera observación RF IEEE]]; `FeralRF/python/examples/smoke_phy4_ieee154.py`.

---

## OP-05 — Verificar exclusión TX durante RX y recuperación

### 1. Objetivo

Solicitar TX mientras RX está activo, comprobar rechazo de estado y recuperar RX con `stop_rx()`.

### 2. Evidencia histórica y estado

**VALIDADO EN LAS CONDICIONES INDICADAS (control).** EV-06 realizó dos ejecuciones válidas: error 0x05 esperado y recuperación. Los intentos previos de quoting/import no cuentan.

### 3. Equipo necesario

Una CatSniffer V3, Bridge `COM88`, ruta 2,4 GHz y antena. El TX debe ser rechazado; no se usa peer.

### 4. Antes de abrir las terminales

Complete OP-02 y deje el equipo idle. Use el símbolo actual `PHY`; el comando histórico preservado usó `Phy`, interfaz de aquella revisión.

### 5. Disposición de terminales

**Terminal 1 — control RX/TX / COM88.** Patrón A.

### 6. Ejecución paso a paso

Equivalente actual reconstruido del comando histórico, completo y con cleanup:

```powershell
@'
from feralrf import PHY, Radio
from feralrf.exceptions import CommandError

r = Radio(port="COM88")
rx_started = False
try:
    print("INIT:", r.init())
    r.set_phy(PHY.IEEE_802_15_4)
    r.set_channel(25)
    r.start_rx(); rx_started = True
    print("RX: STARTED")
    try:
        r.transmit(b"\x01", power_dbm=-20)
        raise RuntimeError("TX aceptado inesperadamente durante RX")
    except CommandError as exc:
        print("TX REJECTED:", str(exc))
        print(f"ERROR CODE: 0x{exc.error_code:02X}")
        if exc.error_code != 0x05:
            raise RuntimeError(
                f"Unexpected TX rejection code: 0x{exc.error_code:02X}"
            )
        print("PASS: TX rejected with expected error code 0x05")
    r.stop_rx(); rx_started = False
    print("RX: STOPPED")
    print("STATS:", r.get_stats())
finally:
    cleanup_error = None
    if rx_started:
        try:
            r.stop_rx()
        except Exception as exc:
            cleanup_error = exc
            print("RX_STOP no confirmado:", repr(exc))
    try:
        r.disconnect()
    finally:
        if cleanup_error is not None:
            raise cleanup_error
'@ | python -
$LASTEXITCODE
```

### 7. Salida esperada

El 21-oct se observó `ERROR CODE: 0x05`, el PASS explícito, `RX: STOPPED` y stats válidos. La excepción de limpieza ya no queda convertida en una impresión exitosa; `disconnect()` se conserva aun cuando el stop falle.

### 8. Interpretación

El rechazo demuestra exclusión de estados en control. Como el TX no fue aceptado, esta receta no acredita emisión RF ni necesita marcador peer.

### 9. Limpieza

`stop_rx()` precede a stats y `disconnect()`; el `finally` intenta stop si la secuencia falla.

### 10. Fallos y recuperación

Si TX se acepta, el script genera `RuntimeError`: deténgase y conserve la salida. Si stop falla, aplique recuperación general y no repita en la misma sesión.

### 11. Límites de la conclusión

No prueba exclusión bajo todos los PHY ni concurrencia de múltiples procesos; prueba el caso IEEE/COM88 documentado.

### 12. Fuentes

[[../../05 - Evaluación/EV-06 — Exclusión RX y TX y recuperación de estado]]; `FeralRF/python/feralrf/radio.py` (API actual).

---

## OP-06 — Intercambiar marcadores IEEE mediante TX_RAW

### 1. Objetivo

Demostrar entrega OTA RAW con marcador único entre dos CatSniffer, separando ACK TX de recepción peer.

### 2. Evidencia histórica y estado

**VALIDADO EN LAS CONDICIONES INDICADAS, CON VARIACIÓN.** OCT-IEEE registró COM33→COM88 20/20 (20 envíos accidentales) y COM88→COM33 10/10. En la validación del manual del 21-oct, la primera corrida COM33→COM88 obtuvo 9/10 en 60 s (FAIL del umbral, pero entrega OTA confirmada) y la segunda 10/10 en 90 s (PASS). No se ha demostrado que ampliar la ventana causara la diferencia.

### 3. Equipo necesario

Dos CatSniffer V3 con antenas 2,4 GHz. El informe OCT-IEEE llamó #1 a `COM33/35` y #2 a `COM88/87`; la campaña del 20-oct llamó #2/#3 a esos pares. Use identidad física/USB y los roles de puerto, no el ordinal cambiante. Potencia solicitada 0 dBm.

### 4. Antes de abrir las terminales

Ejecute OP-01, seleccione `band1` en ambas y confirme que los dos Bridges inicializan. Mantenga el marcador `A10133008801` sólo si no hay otra sesión que pueda emitirlo.

### 5. Disposición de terminales

**Terminal 1 — RX / CatSniffer #3 / COM88.** **Terminal 2 — TX / CatSniffer #2 / COM33.** Patrón B. Prepare el entorno en ambas.

### 6. Ejecución paso a paso

1. Terminal 1, control negativo; espere a que termine y exija cero hits:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex A10133008801 --match-mode contains --min-hits 0 --print-limit 30
   ```

2. Terminal 1, inicie RX positivo y déjelo corriendo:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 90 --marker-hex A10133008801 --match-mode contains --min-hits 10 --print-limit 80
   ```

3. Cuando Terminal 1 muestre `[ OK ] RX_START ACK`, ejecute en Terminal 2:

   ```powershell
   1..10 | ForEach-Object { python .\examples\smoke_tx_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex A10133008801 --tx-timeout 2 }
   ```

4. Espere la terminación natural del RX. Para dirección inversa, cambie RX a `COM33`, TX a `COM88` y marcador a `A10288003301`.

### 7. Salida esperada

RX imprime `RX_START ACK`, líneas de paquetes/marcador, `RX_STOP ACK` y resumen. TX imprime diez veces `TX_RAW ACK`. El control negativo sólo es válido si el resumen dice literalmente `marker_hits=0`: con `--min-hits 0`, el PASS del script no impone cero hits. La validación reciente observó nueve paquetes ambientales sin marcador en ese control. Históricamente el payload recibido fue `a1013300880117b5`: seis bytes de marcador y dos de FCS.

### 8. Interpretación

Diez ACK TX son control. Diez `marker_hits` CRC-good en el peer respaldan entrega OTA. `marker_hits` cuenta coincidencias recibidas; al reutilizar el mismo marcador no establece correspondencia individual ni PDR por solicitud. Los dos bytes extra fueron FCS de la ruta IEEE, no parte del marcador solicitado.

### 9. Limpieza

Ambos scripts terminan solos y desconectan. No cierre Terminal 1 después del primer hit; espere resumen/stop. Consulte exit code si el umbral no se alcanzó.

### 10. Fallos y recuperación

Cero hits con diez ACK no demuestra cero radiación: revise banda, roles, marcador y superposición temporal. Si `RX_STOP` falla, clasifique BLOCKED para cleanup y aplique recuperación.

### 11. Límites de la conclusión

No mide alcance, potencia EIRP, sensibilidad ni interoperabilidad Zigbee/Thread. La repetición actual puede producir otro conteo.

### 12. Fuentes

[[../01 - OTA IEEE - RAW FRAME y BURST]]; [[../EXPERIMENT_REPORT_2026-10-12/historical_command_audit]]; `FeralRF/python/examples/{smoke_tx_phase1.py,lab/ota_rx_probe.py}`.

---

## OP-07 — Intercambiar un marcador mediante TX_FRAME

### 1. Objetivo

Enviar diez `TX_FRAME` y observar el marcador en el peer.

### 2. Evidencia histórica y estado

**VALIDADO EN LAS CONDICIONES INDICADAS.** OCT-IEEE obtuvo 10/10 COM33→COM88. El comando literal original faltó; ésta es una **secuencia equivalente actual reconstruida**.

### 3. Equipo necesario

Dos CatSniffer V3 con configuración y roles de OP-06; canal 25, PHY 4, 0 dBm solicitado.

### 4. Antes de abrir las terminales

Complete el control negativo y RAW de OP-06. Seleccione `band1` en ambos y use un marcador nuevo `A20133008802`.

### 5. Disposición de terminales

**Terminal 1 — RX COM88.** **Terminal 2 — TX COM33.** Patrón B.

### 6. Ejecución paso a paso

1. Terminal 1, control negativo:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex A20133008802 --match-mode contains --min-hits 0 --print-limit 30
   ```

2. Terminal 1, RX positivo; déjelo corriendo:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 45 --marker-hex A20133008802 --match-mode contains --min-hits 10 --print-limit 80
   ```

3. Tras `RX_START ACK`, Terminal 2:

   ```powershell
   python .\examples\lab\ota_tx_frame.py --port COM33 --phy 4 --channel 25 --power 0 --payload-hex A20133008802 --count 10 --interval-ms 250 --tx-timeout 2 --progress-every 1
   ```

### 7. Salida esperada

TX muestra progreso `TX_FRAME sent=n/10` y PASS; RX, diez hits. El histórico recibió `a20133008802f18b`, longitud 8 y `crc_ok=True`.

### 8. Interpretación

El peer prueba entrega de bytes y FCS. `TX_FRAME` no implica framing de pila Zigbee/Thread; la auditoría encontró que el backend delegaba al camino RAW.

### 9. Limpieza

Los helpers detienen RX/desconectan. Espere ambos exits; no use Ctrl+C como terminación normal.

### 10. Fallos y recuperación

Si TX termina antes de `RX_START ACK`, repita sólo después de cerrar ambos procesos y usar marcador nuevo. Un timeout de stop bloquea la continuación.

### 11. Límites de la conclusión

Una dirección y un payload no prueban semántica de protocolo ni interoperabilidad externa.

### 12. Fuentes

[[../01 - OTA IEEE - RAW FRAME y BURST]]; [[../EXPERIMENT_REPORT_2026-10-12/historical_command_audit]]; `FeralRF/python/examples/lab/ota_tx_frame.py`.

---

## OP-08 — Reproducir el control BURST con intervalo positivo

### 1. Objetivo

Solicitar cinco transmisiones BURST separadas 250000 µs y medir hits en un peer.

### 2. Evidencia histórica y estado

**FALLO DEL UMBRAL / VALIDACIÓN PARCIAL, REPRODUCIDO.** El histórico obtuvo ACK pero 1/5; el 21-oct se repitieron `count=5`, 250000 µs y marcador `A30133008803`: TX mostró `scheduled=5`, el peer observó 1 hit CRC-good y terminó FAIL 1/5. No quedó medido el número físico de emisiones.

### 3. Equipo necesario

Dos CatSniffer V3 como OP-06. No use este ensayo para producción ni como generador de conteo exacto.

### 4. Antes de abrir las terminales

Complete RAW/FRAME, mantenga `band1` y reserve marcador `A30133008803`.

### 5. Disposición de terminales

**Terminal 1 — RX COM88.** **Terminal 2 — TX COM33.** Patrón B.

### 6. Ejecución paso a paso

1. Terminal 1, negativo:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex A30133008803 --match-mode contains --min-hits 0 --print-limit 30
   ```

2. Terminal 1, RX positivo:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 12 --marker-hex A30133008803 --match-mode contains --min-hits 5 --print-limit 80
   ```

3. Tras `RX_START ACK`, Terminal 2:

   ```powershell
   python .\examples\lab\ota_tx_burst.py --port COM33 --phy 4 --channel 25 --power 0 --payload-hex A30133008803 --count 5 --interval-us 250000 --tx-timeout 2
   ```

### 7. Salida esperada

TX puede mostrar ACK y `scheduled=5` aunque RX no alcance 5. Tanto el resultado histórico como el reciente fueron un marcador. `scheduled` es el conteo solicitado que el helper imprime tras el ACK inicial; no son cinco completions RF.

### 8. Interpretación

ACK significa que BURST fue aceptado/programado. `1 hit` respalda al menos una entrega, no cinco emisiones ni un intervalo físico de 250 ms.

### 9. Limpieza

El helper TX desconecta tras ACK; RX debe terminar su ventana y confirmar stop. No abra otro BURST mientras RX anterior siga activo.

### 10. Fallos y recuperación

Clasifique `<5` como FAIL del umbral y causa inconclusa. Si cleanup RX falla, registre además BLOCKED y recupere antes de cualquier otro PHY.

### 11. Límites de la conclusión

No permite anunciar repetición exacta, temporización ni conteo de emisiones. Repetir sin instrumentación del scheduler no discrimina causa.

### 12. Fuentes

[[../../05 - Evaluación/EV-12 — TX RAW FRAME BURST CONTINUOUS por aire]]; [[../01 - OTA IEEE - RAW FRAME y BURST]]; `FeralRF/python/examples/lab/ota_tx_burst.py`.

---

## OP-09 — Reproducir CONTINUOUS y observar STOP

### 1. Objetivo

Comparar CONTINUOUS con intervalo positivo y cero, y ejecutar una ventana posterior a `TX_STOP`.

### 2. Evidencia histórica y estado

**VALIDACIÓN PARCIAL.** El 21-oct, 250000 µs produjo 1 hit frente a umbral 5; intervalo 0 produjo 37 hits CRC-good en unos 7 s, con separación media reportada de 1118,7 µs. TX obtuvo `TX_CONTINUOUS ACK` y `TX_STOP ACK` en ambas variantes. La corrida rápida terminó con timeout de `RX_STOP`; una sesión posterior recuperó start/stop y un control de cinco segundos dio cero hits. Los históricos registraron 30 y 99/102 bajo variantes distintas.

### 3. Equipo necesario

Dos CatSniffer V3, PHY 4/canal 25, 0 dBm solicitado y marcadores únicos. Históricos: TX COM33, RX COM88.

### 4. Antes de abrir las terminales

Complete OP-06 y use `band1`. No confunda conteo RX con transmisiones físicas. Prepare ambas terminales.

### 5. Disposición de terminales

**Terminal 1 — RX COM88.** **Terminal 2 — TX COM33.** Patrón B; el TX permanece activo sólo `run-seconds` y manda STOP solo.

### 6. Ejecución paso a paso

1. Terminal 1, RX positivo:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex A40133008804 --match-mode contains --min-hits 5 --print-limit 120
   ```

2. Tras `RX_START ACK`, Terminal 2, comando histórico exacto del procedimiento del 12-oct:

   ```powershell
   python .\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex A40133008804 --interval-us 250000 --run-seconds 3 --tx-timeout 2
   ```

3. Cuando ambos terminen, Terminal 1, control posterior:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex A40133008804 --match-mode contains --min-hits 0 --print-limit 40
   ```

4. Variante histórica intervalo 0, con Terminal 1 primero:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 7 --marker-hex A40233008804 --match-mode contains --min-hits 20 --print-limit 160
   ```

   Después de `RX_START ACK`, Terminal 2:

   ```powershell
   python .\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex A40233008804 --interval-us 0 --run-seconds 1 --tx-timeout 2
   ```

   Al terminar ambos, Terminal 1, control posterior:

   ```powershell
   python .\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex A40233008804 --match-mode contains --min-hits 0 --print-limit 40
   ```

La variante EV-12 con `interval_us=1` obtuvo un marker, frente a 99 en su variante de intervalo cero; no se presenta como workflow separado.

### 7. Salida esperada

TX: `TX_CONTINUOUS ACK`, luego `TX_STOP ACK`. RX con intervalo positivo: un hit; intervalo 0: actividad repetida. Para las ventanas post-stop, compruebe literalmente `marker_hits=0`; el PASS con `--min-hits 0` no basta. La coincidencia intermedia registrada en una corrida de 10 s no tiene relación temporal confirmada con una segunda ejecución TX y no prueba transmisión posterior al STOP.

### 8. Interpretación

El ACK inicial no acredita repetición. Los hits peer acreditan entregas; `TX_STOP ACK` acredita control. Cero hits posteriores sólo cubre esa ventana, no el instante físico exacto de cese.

### 9. Limpieza

El script TX intenta `stop_transmit()` también en `finally`; RX intenta `stop_rx()` y desconecta. No use Ctrl+C como ruta normal.

### 10. Fallos y recuperación

Si TX falla antes de STOP, el `finally` intenta detener. Si no aparece `TX_STOP ACK`, no inicie otro modo; cierre, retire alimentación si procede y ejecute control fresco.

### 11. Límites de la conclusión

No establece correspondencia entre hits y emisiones, precisión temporal ni cese por instrumento. Intervalos positivos mostraron anomalía histórica.

### 12. Fuentes

[[../../05 - Evaluación/EV-12 — TX RAW FRAME BURST CONTINUOUS por aire]]; [[../02 - OTA IEEE - CONTINUOUS y STOP]]; [[../05 - OTA IEEE - CONTINUOUS STOP dirigido]].

---

## OP-10 — Ejecutar controles CW, PRBS15, PRBS32 y STOP

### 1. Objetivo

Recorrer la ruta de control de CW/PRBS y garantizar `tx_test_stop()` tras cada modo.

### 2. Evidencia histórica y estado

**VALIDACIÓN PARCIAL.** EV-13 y la validación reciente obtuvieron ACK para CW, PRBS15, PRBS32 y STOP. No midieron señal, frecuencia, potencia, patrón ni cese. La consulta posterior del 21-oct mostró `Band: 0`, pero no acredita retrospectivamente la banda durante cada modo. El bloque siguiente sigue siendo un **PROCEDIMIENTO RECONSTRUIDO — NO VALIDADO COMO SECUENCIA EXACTA EN HARDWARE** aunque las operaciones de control sí fueron ejecutadas.

### 3. Equipo necesario

Una CatSniffer V3 en COM33, antena/carga apropiada y entorno RF autorizado. La validación física requeriría instrumento; no use otra CatSniffer como sustituto concluyente.

### 4. Antes de abrir las terminales

Empiece desde sesión recuperada. Seleccione ruta adecuada antes de cada familia: `band1` para CW BLE ch37 y `band2` para PRBS Sub-1 GHz 868. El bloque no controla Cat-Shell.

### 5. Disposición de terminales

**Terminal 1 — DUT COM33.** Patrón A. No abra observador ni ejecute este bloque sin haber seleccionado la ruta correspondiente; si quiere ambas familias, ejecute cada sección por separado cambiando banda entre procesos.

### 6. Ejecución paso a paso

Ejemplo actual para **una familia por ejecución**; deje `$Mode='cw'`, o cambie a `prbs15`/`prbs32` después de seleccionar la banda correcta:

```powershell
$Mode = 'cw'
$env:FERALRF_TEST_MODE = $Mode
@'
import os, time
from feralrf import PHY, Radio

mode = os.environ["FERALRF_TEST_MODE"]
r = Radio(port="COM33")
active = False
try:
    print("INFO:", r.init())
    if mode == "cw":
        r.set_phy(PHY.BLE_1M, channel=37)
        r.tx_cw(power_dbm=0)
    else:
        r.set_phy(PHY.SUB_1GHZ_868, channel=0)
        r.tx_prbs(power_dbm=0, pattern=mode)
    active = True
    print(mode, "ACK")
    time.sleep(0.3)
finally:
    if active:
        r.tx_test_stop(); print("TX_TEST_STOP ACK")
    r.disconnect(); print("Disconnected")
'@ | python -
Remove-Item Env:FERALRF_TEST_MODE
```

### 7. Salida esperada

EV-13 conservó `INFO`, `TX_CW ACK` o `TX_PRBS15/32 ACK`, `TX_TEST_STOP ACK`, `Disconnected`. La salida aquí será `cw ACK`/`prbs15 ACK`/`prbs32 ACK`.

### 8. Interpretación

Los ACK prueban sólo la ruta host/protocolo/control. Una espera host de 0,3 s no mide duración RF.

### 9. Limpieza

`tx_test_stop()` está en `finally` y debe imprimirse antes de desconexión. Si el proceso se interrumpe antes del STOP, trate la placa como estado incierto y retire toda alimentación antes de otro PHY.

### 10. Fallos y recuperación

No ejecute el viejo `smoke_f22_tx_test.py`: históricamente derivaba Shell por aritmética y pedía +5 dBm. Si STOP no se confirma, power-cycle completo y preflight.

### 11. Límites de la conclusión

No valida portadora, PRBS, potencia, frecuencia, espectro, cese ni interferencia BLE. PRBS15/32 no es BLE DTM PRBS9.

### 12. Fuentes

[[../../05 - Evaluación/EV-13 — CW PRBS y TX_TEST_STOP por control]]; `FeralRF/python/feralrf/radio.py` (`tx_cw`, `tx_prbs`, `tx_test_stop`).

---

## OP-11 — Reproducir RF-01/RF-03 como experimentos bloqueados

### 1. Objetivo

Preservar los lanzamientos coordinados de switching y el punto exacto donde se bloquearon; no recomendarlos como workflow estable.

### 2. Evidencia histórica y estado

**EXPERIMENTOS BLOQUEADOS, CON NUEVA EVIDENCIA POSITIVA EN RF-01.** El 20-oct RF-01 se bloqueó antes del switching. El 21-oct recorrió 25/25=10/10, 25/20=0/10, 20/20=10/10 y retorno 25/25=9/10; el último `RX_STOP` agotó timeout, por lo que el resultado global sigue `BLOCKED`. RF-03 reciente obtuvo 7/10 en su etapa IEEE inicial y volvió a bloquearse en `RX_STOP` antes de configurar Sub-GHz. No hay evidencia reciente de IEEE↔Sub-GHz.

### 3. Equipo necesario

Dos CatSniffer V3: TX #2 `COM33/35`, RX #3 `COM88/87`; antenas 2,4 GHz y, para el plan RF-03 no alcanzado, Sub-GHz.

### 4. Antes de abrir las terminales

Sólo en una campaña de diagnóstico autorizada. Confirme posibilidad de power-cycle total ≥15 s. Defina log y ejecute preflight:

```powershell
$rfRunDir = Join-Path (Resolve-Path '..\validation_runs') ('manual_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $rfRunDir | Out-Null
python ..\experimental_artifacts\next_rf_validation\preflight.py --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --antenna-confirmed --recovery-confirmed 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '00_preflight.jsonl')
$LASTEXITCODE
```

### 5. Disposición de terminales

**Terminal 1 — script coordinado TX/RX.** Patrón C: el proceso abre ambas placas y ambos Shell. No abra terminal RX/TX aparte.

### 6. Ejecución paso a paso

RF-01 histórico exacto:

```powershell
python ..\experimental_artifacts\next_rf_validation\rf01_ieee_channel_switch.py --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --min-received 9 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '01_rf01_ieee_channel_switch.jsonl')
$LASTEXITCODE
```

Tras su bloqueo, deténgase y recupere; no lance RF-03 en la misma sesión. RF-03 histórico exacto, sólo después de un preflight fresco:

```powershell
python ..\experimental_artifacts\next_rf_validation\rf03_cross_phy_switch.py --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --min-received 9 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '03_rf03_cross_phy_switch.jsonl')
$LASTEXITCODE
```

### 7. Salida esperada

La salida debe interpretarse por etapa. RF-01 reciente logró los cuatro criterios de recepción, pero terminó `BLOCKED` por cleanup; RF-03 reciente terminó `BLOCKED` en IEEE inicial. Exit diseñado para BLOCKED: 3. `last=0x90` identifica paquetes RX asíncronos ignorados durante la espera, no prueba por sí solo qué capa omitió o perdió el ACK.

### 8. Interpretación

Los diez markers prueban el stage IEEE inicial. `0x90` era una respuesta RX inesperada mientras se esperaba STOP. `completed_stages=[]` significa que ni el cambio de canal ni la transición Sub-GHz llegaron a ejecutarse.

### 9. Limpieza

Los scripts desconectaron ambos objetos aun tras el fatal. Conserve JSONL. No confunda `DISCONNECT_OK` con `RX_STOP_ACK`.

### 10. Fallos y recuperación

Quite toda alimentación ≥15 s, reenumere, ejecute preflight y como máximo este control:

```powershell
python ..\experimental_artifacts\next_rf_validation\rf01_ieee_channel_switch.py --only-control --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --min-received 9 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir 'recovery_ieee_control.jsonl')
$LASTEXITCODE
```

Después del primer power-cycle reciente, ese control dio 10/10 y PASS; después de RF-03, el preflight fue PASS pero el control obtuvo 9/10 y volvió a `BLOCKED` pese a un ACK durante cleanup. Un preflight accesible no demuestra recuperación del ciclo RX. Si el control se bloquea, detenga la campaña.

### 11. Límites de la conclusión

La corrida del 21-oct sí respalda switching IEEE 25→20→25 bajo esas condiciones; una sola ejecución no prueba estabilidad. RF-03 no evaluó la transición Sub-GHz. El bloqueo recurrente sigue concentrado en finalización RX host-visible.

### 12. Fuentes

[[../EXPERIMENT_REPORT_2026-10-20/RF-01 — IEEE 802.15.4 Channel Switching]]; [[../EXPERIMENT_REPORT_2026-10-20/RF-03 — Validación de transición IEEE 802.15.4 ↔ Sub-GHz]]; `FeralRF/validation_runs/next_rf_20261020_091040/{01_rf01_ieee_channel_switch,03_rf03_cross_phy_switch}.jsonl`.

Para la comparación completa con el 21-oct, véase [[08 - Validación reciente del manual frente a evidencia histórica]]. RF-04/KillerBee conserva el identificador OP-15 en el documento Sub-GHz por continuidad histórica, pero es una integración IEEE 802.15.4; desde aquí debe seguirse el enlace, no duplicar la receta.

