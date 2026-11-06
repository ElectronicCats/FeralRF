# Manual práctico de uso de FeralRF

## Propósito, alcance y regla de lectura

Este manual convierte los experimentos preservados en el Vault en procedimientos operativos. No es una referencia general de la API: una operación aparece aquí sólo si corresponde a un experimento, control o recuperación documentados en `05 - Evaluación/` o `06 - Experimentos/`.

Las recetas se reparten así:

- este documento: preparación, identificación, inicialización y recuperación;
- [[06 - Procedimientos IEEE, TX y modos de prueba]]: RX IEEE, exclusión RX/TX, OTA RAW/FRAME, BURST, CONTINUOUS, CW/PRBS y RF-01/RF-03;
- [[07 - Procedimientos Sub-GHz y experimentos condicionados]]: control de presets, recuperación Sub-GHz, RF-02, RF-04 y RF-05.
- [[08 - Validación reciente del manual frente a evidencia histórica]]: disposición de los nueve reportes del operador del 21-oct-2026 y comparación sin sobrescribir la evidencia histórica.

Los comandos que transmiten RF requieren autorización, antena/carga correcta para la banda y cumplimiento regulatorio. `0 dBm` es potencia solicitada, no medida. Las asignaciones `COM33/35/88/87` son ejemplos históricos del banco de octubre de 2026; deben sustituirse si la enumeración actual difiere.

### Estados usados

- **VALIDADO EN LAS CONDICIONES INDICADAS**: el criterio estrecho fue satisfecho históricamente.
- **VALIDACIÓN PARCIAL**: funcionó una parte, pero faltó otro criterio material.
- **EXPERIMENTO BLOQUEADO**: preparación, control o cleanup impidió completar el objetivo.
- **FALLO HISTÓRICO DOCUMENTADO**: la ejecución llegó al criterio y no lo satisfizo.
- **PROCEDIMIENTO RECONSTRUIDO — NO EJECUTADO**: secuencia operativa actual deducida de código/evidencia, sin nueva ejecución.

Cada receta declara además si el comando es **exacto ejecutado**, **equivalente actual** o **reconstruido**. Un equivalente actual no reemplaza el comando histórico como evidencia.

## Mapa de interfaces y orden de configuración

| Capa | Interfaz | Velocidad histórica | Responsabilidad |
|---|---|---:|---|
| RP2040 / Cat-Shell | Shell USB | 115200 | `identify`, `fw_version`, `status`, `boot`, `exit`, `band1`, `band2` |
| RP2040 / Cat-Bridge | Bridge USB | 921600 | transporte FeralRF hacia CC1352P7 |
| CC1352P7 / FeralRF | API `Radio` | por Cat-Bridge | `set_phy()`, `set_channel()`, `configure_prop()`, RX y TX |

`band1` solicita la ruta lógica de 2,4 GHz; `band2`, la ruta lógica Sub-GHz. Ninguno configura el PHY del CC1352P7. A la inversa, `set_phy()` o `configure_prop()` no seleccionan la ruta física del front-end. El orden histórico defendible es: identificar puertos → seleccionar ruta por el Shell explícito → abrir Bridge → configurar PHY/canal o preset → RX antes de TX.

`Band: 0/1` es estado lógico reportado por RP2040, no lectura eléctrica de GPIO/CTF/U2. No derive Cat-Shell como `Cat-Bridge + 2`: `COM88` tuvo Shell `COM87`.

## Preparación común de PowerShell

Para eliminar ambigüedad de rutas, **todos los comandos operativos de estos documentos parten de `FeralRF\python`**, salvo que una receta declare expresamente otra ubicación histórica. En cada terminal nueva:

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF\python'
$env:PYTHONPATH = (Resolve-Path '.').Path
python --version
python -c "import feralrf; print(feralrf.__file__)"
```

La campaña reciente registró Python 3.14.7; el paquete declara Python `>=3.9`. No se exige recrear 3.14.7, pero una repetición debe anotar su versión. No mezcle silenciosamente otro editable con `PYTHONPATH`; la ruta impresa debe terminar en este checkout bajo `FeralRF\python\feralrf`.

Variables recomendadas para sustituir los COM históricos:

```powershell
$TxBridge = 'COM33'
$TxShell  = 'COM35'
$RxBridge = 'COM88'
$RxShell  = 'COM87'
```

Los comandos históricos literales conservan sus COM originales. Cuando una CLI no admita variables dentro de una cadena Python, sustituya el puerto explícitamente y registre el comando resultante.

## Inventario de recetas y patrones de ejecución

| ID | Operación / evidencia | Estado | Patrón | Procedencia del comando |
|---|---|---|---|---|
| OP-01 | Enumerar y asociar interfaces; EV-00/checklist 12-oct | VALIDACIÓN PARCIAL | A | exacto/equivalente actual |
| OP-02 | Inicializar y reconectar cinco procesos; EV-03 | VALIDADO | A | exacto ejecutado |
| OP-03 | Reset manual `boot`→`exit`; EV-04/recuperación 15-oct | VALIDACIÓN PARCIAL | A | reconstruido de orden exacto |
| OP-04 | RX IEEE canal 25; EV-05 y repetición 21-oct | VALIDADO | A | exacto ejecutado |
| OP-05 | TX durante RX, error 0x05 y recuperación; EV-06/21-oct | VALIDADO-control | A | equivalente actual ejecutado |
| OP-06 | OTA RAW bidireccional; OCT-IEEE y 21-oct | VALIDADO con variación 9/10–10/10 | B | equivalente actual ejecutado |
| OP-07 | OTA FRAME; OCT-IEEE | VALIDADO | B | equivalente reconstruido |
| OP-08 | BURST con intervalo positivo; EV-12/OCT-IEEE/21-oct | VALIDACIÓN PARCIAL, 1/5 reproducido | B | equivalente actual ejecutado |
| OP-09 | CONTINUOUS + STOP; EV-12/OCT-IEEE/21-oct | VALIDACIÓN PARCIAL; `RX_STOP` intermitente | B | exacto ejecutado |
| OP-10 | CW/PRBS/`TX_TEST_STOP`; EV-13/21-oct | VALIDACIÓN PARCIAL | A | procedimiento reconstruido y control ACK ejecutado |
| OP-11 | Switching RF-01/RF-03; 20–21-oct | RF-01: switching observado pero BLOCKED; RF-03: BLOCKED antes de Sub-GHz | C | exacto ejecutado |
| OP-12 | Control de preset GFSK 868; EV-11 | VALIDADO-control | A | exacto ejecutado |
| OP-13 | Recuperación OTA Sub-GHz #2→#3; REC-3A…4A | VALIDADO | C | equivalente actual reconstruido |
| OP-14 | RF-02 Sub-GHz inverso #3→#2 | VALIDADO | C | exacto ejecutado |
| OP-15 | Preparación KillerBee RF-04 | EXPERIMENTO BLOQUEADO | A | exacto ejecutado |
| OP-16 | OOK RF-05 | PROCEDIMIENTO RECONSTRUIDO — NO EJECUTADO | C | comando gated no ejecutado |

Patrón A usa un dispositivo y una terminal. Patrón B usa dos procesos independientes: primero se deja corriendo RX y después se inicia TX en otra terminal. Patrón C usa un solo proceso que abre ambas placas; no se abren dos terminales de radio.

---

## OP-01 — Identificar placas, Bridge y Shell

### 1. Objetivo

Enumerar puertos Windows y asociar, para cada placa física, su Cat-Bridge y Cat-Shell sin aritmética de COM.

### 2. Evidencia histórica y estado

**VALIDACIÓN PARCIAL.** EV-00 produjo un mapeo provisional; el checklist del 12-oct confirmó por Shell las placas usadas. La identidad USB completa no quedó preservada en EV-00, por lo que la receta identifica de nuevo en cada sesión.

### 3. Equipo necesario

Una o dos CatSniffer V3 conectadas por USB. No se transmite RF. No hace falta abrir Cat-LoRa.

### 4. Antes de abrir las terminales

Cierre monitores serie. Conecte una sola placa si no puede distinguirlas físicamente; anote su etiqueta y repita con la segunda. Prepare PowerShell como se indicó arriba.

### 5. Disposición de terminales

**Terminal 1 — Inventario y consultas Shell.** Patrón A; una terminal basta.

### 6. Ejecución paso a paso

1. Enumere puertos:

   ```powershell
   Get-CimInstance Win32_SerialPort |
       Sort-Object DeviceID |
       Select-Object DeviceID, Name, PNPDeviceID |
       Format-List
   ```

2. Para el ejemplo histórico, consulte los dos Shell explícitos:

   ```powershell
   python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
   python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
   ```

3. Anote la asociación física. El mapeo reciente fue #2: Bridge `COM33`, Shell `COM35`; #3: Bridge `COM88`, Shell `COM87`.

### 7. Salida esperada

La salida Shell debe incluir identificación/versión y una línea `Mode: ..., Band: ..., FW: ...`. Los datos históricos recientes incluyeron RP2040 `v3.1.0.1`, Git `c0cd5a4 (clean)`. No se exige que un equipo actual repita esos valores.

### 8. Interpretación

Una respuesta prueba que el puerto es Cat-Shell y permite unirlo a la placa etiquetada. No identifica por sí sola cuál de los otros puertos es Bridge si varias placas permanecen conectadas; use PNP/location, desconexión controlada o inicialización OP-02.

### 9. Limpieza

Los comandos cierran `serial.Serial`. No deje miniterm abierto antes de usar el Bridge o el Shell en otra receta.

### 10. Fallos y recuperación

`Access is denied` significa normalmente puerto ocupado. Cierre el proceso dueño y repita. Si la enumeración cambió, no reutilice la tabla histórica ni calcule puertos.

### 11. Límites de la conclusión

No prueba FeralRF, RF, posición eléctrica del switch ni identidad única del firmware CC1352P7.

### 12. Fuentes

[[../../05 - Evaluación/Auditoría técnica de validación FeralRF - EV ejecutadas#EV-00]]; [[../Checklist OTA FeralRF - dos CatSniffer]]; `FeralRF/docs/RF_VALIDATION_RUNBOOK.md` §3.

---

## OP-02 — Comprobar comunicación y reconexión del Bridge

### 1. Objetivo

Abrir, inicializar y cerrar cinco procesos independientes sobre un Cat-Bridge.

### 2. Evidencia histórica y estado

**VALIDADO EN LAS CONDICIONES INDICADAS.** EV-03 terminó 5/5 sin timeout ni puerto ocupado. Esto es PASS-control, no una prueba RF.

### 3. Equipo necesario

Una CatSniffer V3 y su Bridge identificado. Condición histórica: `COM88`, firmware reportado `1.0.0`.

### 4. Antes de abrir las terminales

Complete OP-01 y use el directorio común `FeralRF\python`.

### 5. Disposición de terminales

**Terminal 1 — Bridge / CatSniffer #3.** Patrón A; no abra un segundo proceso sobre el mismo COM.

### 6. Ejecución paso a paso

Comando histórico exacto, ejecutado desde `FeralRF\python`:

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF\python'
1..5 | % { Write-Host "`n===== CICLO $_ / 5 ====="; python -c "from feralrf import Radio; r=Radio(port='COM88'); print(r.init()); r.disconnect()" }
```

### 7. Salida esperada

Se conservaron cinco bloques con `DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')` y sin excepción.

### 8. Interpretación

Cinco aperturas prueban liberación básica del puerto entre procesos. El serial lógico `FERALRF1` fue común a ambos equipos y no distingue placas.

### 9. Limpieza

Cada iteración llama `disconnect()` antes de terminar. Si se modifica el one-liner, conserve un `finally` que desconecte.

### 10. Fallos y recuperación

Ante timeout o puerto ocupado, detenga el bucle, cierre procesos, vuelva a OP-01 y use OP-03 si el Bridge no responde tras liberar el COM.

### 11. Límites de la conclusión

No ensaya pérdida USB, Ctrl+C, la misma instancia de `Radio`, sesiones largas ni RF.

### 12. Fuentes

[[../../05 - Evaluación/EV-03 — Reconexión limpia entre procesos]]; `FeralRF/python/feralrf/radio.py` (inspección estática actual).

---

## OP-03 — Reset manual y recuperación por Cat-Shell

### 1. Objetivo

Recuperar el CC1352P7 mediante `boot` → espera → `exit` en el Shell explícito y verificar después el Bridge.

### 2. Evidencia histórica y estado

**VALIDACIÓN PARCIAL.** EV-04 recuperó manualmente `COM88/COM87`; la campaña del 15-oct repitió la secuencia en ambos equipos. `Radio.reset_device()` quedó bloqueado por derivar `COM90` desde `COM88`.

### 3. Equipo necesario

Una CatSniffer V3; Bridge y Shell ya asociados. Para dos placas, ejecute la receta de una en una. No transmite RF.

### 4. Antes de abrir las terminales

Cierre procesos sobre Bridge/Shell y registre el último fallo. No use `Radio.reset_device()` con el mapeo histórico `COM88`/`COM87`.

### 5. Disposición de terminales

**Terminal 1 — Recovery / Shell de la placa afectada.** Patrón A.

### 6. Ejecución paso a paso

Secuencia operativa reconstruida con el orden exacto recuperado de la campaña del 15-oct (`boot` → 0,5 s → `exit` → 0,3 s → asentamiento ≥3,5 s):

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF\python'
$env:PYTHONPATH = (Resolve-Path '.').Path
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=1,write_timeout=1); print('OPEN',s.name); s.write(b'boot\r\n'); s.flush(); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.write(b'exit\r\n'); s.flush(); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close(); time.sleep(3.5); print('CLOSED')"
python -c "from feralrf import Radio; r=Radio(port='COM88'); print('INIT:',r.init()); print('STATS:',r.get_stats()); r.disconnect()"
```

EV-04 preserva además sus comandos individuales exactos; no se copian aquí como rutina porque incluían una comprobación intermedia del Bridge mientras el equipo estaba en `BOOT`. Los artefactos del 15-oct preservaron `BOOT` y `PASSTHROUGH` juntos después de `exit`.

### 7. Salida esperada

Shell: `BOOT` y luego `PASSTHROUGH`. Bridge final: `DeviceInfo(...)` y stats. La ausencia de respuesta inmediata a `boot` ocurrió históricamente; la respuesta acumulada apareció tras `exit`.

### 8. Interpretación

`BOOT/PASSTHROUGH` y el `init()` posterior prueban la ruta de control y recuperación host-visible. No prueban la forma eléctrica del pulso de reset ni el hash del firmware reiniciado.

### 9. Limpieza

Cierre Shell antes de abrir Bridge. Termine con `disconnect()`. Reenumere si Windows cambió los COM.

### 10. Fallos y recuperación

Si `init()` final falla, no repita indefinidamente: quite toda alimentación de la placa ≥15 s cuando el runbook lo requiera, reconecte, ejecute OP-01 y luego OP-02 una vez.

### 11. Límites de la conclusión

La secuencia no selecciona `band1/band2`, no configura PHY y no acredita RF. La relación Bridge/Shell debe proporcionarse explícitamente.

### 12. Fuentes

[[../../05 - Evaluación/EV-04 — Reset y reinicialización]]; `FeralRF/docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_RECOVERY_RUN.md`, checkpoints 1 y 3A.

## Interpretación común de salida y códigos

| Evidencia | Sí demuestra | No demuestra |
|---|---|---|
| `*_ACK` | aceptación/correlación del comando por control | emisión RF ni entrega al peer |
| marcador en `RX_PACKET` del peer | entrega host-visible bajo esa configuración | frecuencia, potencia o modulación calibradas |
| `crc_ok=true` | CRC reportado válido por firmware | verificación externa independiente |
| `RX_STOP_ACK` | confirmación de cleanup por control | cese físico si había TX |
| `TX_STOP ACK` | comando stop aceptado | instante de cese RF sin observador/instrumento |
| `Band: 1` | estado lógico RP2040 | throw U2 o continuidad de antena |
| exit 0 | criterio implementado por ese script | validez universal de la capacidad |

En scripts de `next_rf_validation`: exit `0` = criterio satisfecho; `2` = FAIL/PARTIAL; `3` = BLOCKED. Lea siempre `RESULT` y consulte `$LASTEXITCODE` inmediatamente después del pipeline.

| Script/familia | Códigos definidos en la versión inspeccionada |
|---|---|
| `ota_rx_probe.py` | 0 umbral satisfecho; 7 umbral no alcanzado; 2 argumentos; 3 conexión; 4 timeout; 5 protocolo; 6 inesperado |
| helpers TX/RX `smoke_*` y `ota_tx_*` | 0 flujo del helper completado; 2 argumentos; 3 conexión; 4 timeout; 5 protocolo; 6 inesperado |
| `manual_preset_runner.py` archivado | 0 `PASS-RF`; 2 cualquier otra clasificación |
| `next_rf_validation` | 0 PASS; 2 FAIL/PARTIAL; 3 BLOCKED, además del evento JSONL `RESULT` |

Un exit 0 de un helper TX puede significar sólo ACK/control. La receta determina si además necesita markers peer. En un control negativo, `--min-hits 0` hace que cualquier conteo no negativo satisfaga el criterio del script: el operador debe verificar expresamente `marker_hits=0`; `RX PROBE PASS hits=N min_hits=0` no afirma ausencia del marcador.

### `$LASTEXITCODE` y pipelines PowerShell

En los comandos documentados, `python ... 2>&1 | Tee-Object ...` deja a `python` como programa nativo de la pipeline. Consulte `$LASTEXITCODE` **en la línea inmediatamente posterior**, antes de ejecutar otro programa nativo. El texto `PASS` o `ACK` no sustituye ese código ni el evento final `RESULT`. No use `$?` como equivalente del código definido por el script y no añada otra pipeline/comando entre Python y la consulta.

## Recuperación general entre recetas

Ante timeout, error async, `RX_STOP` fallido, pérdida de COM o transición fallida:

1. No encadene el siguiente experimento ni reintente automáticamente.
2. Conserve stdout/JSONL y la última etapa completada.
3. Cierre el proceso y confirme que liberó todos los COM.
4. Si el runbook lo exige, quite USB y cualquier alimentación externa de ambas placas durante ≥15 s.
5. Reconecte, ejecute OP-01 y el preflight/OP-02.
6. Para RF-01/RF-03/RF-05, use sólo el control fresco indicado en su receta; un control que vuelve a bloquearse no autoriza continuar.

## Vacíos que impiden recetas seguras adicionales

No se construyó una receta completa para: el baseline de abril (faltan argumentos/logs por fila), F29 70/70 (faltan transcript y binarios instalados) y dos de las transiciones EV-14 narradas (faltan stdout completos). Tampoco se convierte MIOTY, una etiqueta de preset o una API no ensayada en nueva operación. Estos son **tres grupos históricos no reconstruibles con seguridad**, no fallos de FeralRF.

## Operaciones consideradas sin receta independiente

| Evidencia | Decisión de manual |
|---|---|
| EV-10, PHY 0…7 | Ocho comandos exactos demostraron aceptación de control, no RF. OP-04 y OP-12 ya enseñan la configuración en objetivos reales; no se duplica como tutorial genérico de todos los PHY. |
| EV-11, 27 presets | OP-12 conserva un control exacto y OP-13/tabla Sub-GHz las variantes OTA demostradas; no se fabrican 27 recetas equivalentes. |
| EV-14, eventos async/cambios BLE→IEEE | Se conserva como evidencia parcial y gap: tres cambios fueron narrados, sólo una salida completa quedó disponible y no se indujo un error RF concluyente. |
| APR-CTRL/OTA/CRYPTO/EMU | Matrices sin argumentos/logs individuales: permanecen en la matriz histórica, no como comandos reproducibles. |
| F29 70/70 | Claim y script recuperables, pero transcript/binarios instalados ausentes; no se promueve a receta validada. |

