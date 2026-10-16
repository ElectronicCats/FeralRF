# 03 — OTA proprietary: GFSK 868 y 915 MHz

Reporte de observaciones OTA y revisión conceptual de Sub-GHz en CatSniffer/FeralRF
1. Propósito y alcance

Este reporte consolida la preparación, los procedimientos, los resultados y las preguntas técnicas discutidas durante la evaluación OTA de FeralRF con dos CatSniffer V3.

El objetivo experimental es comprobar si los bytes solicitados por la API Python de FeralRF se transmiten por radio y son recibidos por una segunda CatSniffer.

La sesión se concentró en:

Seleccionar explícitamente la ruta Sub-GHz mediante Cat-Shell.
Probar gfsk_868_50k en ambas direcciones.
Repetir la dirección inversa tras aumentar la separación física.
Probar gfsk_915_50k en una dirección.
Consultar el estado posterior del firmware RP2040.
Diferenciar aceptación de comandos, recepción de paquetes y funcionamiento físico de RF.
Explicar qué hacen CC1352P7, RP2040 y SX1262.
Definir una investigación documental sobre la capacidad Sub-GHz y el recorrido físico de la señal.

Este reporte no demuestra una causa raíz, no registra una corrección de firmware y no constituye validación completa de los presets.

Las fechas y horas exactas de cada ejecución no están disponibles. La fecha de consolidación es 8 de octubre de 2026.

2. Fuentes de referencia

Documentos disponibles:

CatSniffer-FeralRF-plan-de-trabajo-actualizado.md.
FeralRF - Matriz de pruebas(1).md.
Registro de validación FeralRF.md.
Auditoría técnica de validación FeralRF - EV ejecutadas(1).md.
Guía enfocada OTA FeralRF - TX RX con dos CatSniffer.md.
FeralRF - Wiki técnica integral(2).md.
Checklist OTA FeralRF - dos CatSniffer.md.
EV-14 — Eventos RF asíncronos y firma RX.md.

El Vault autoritativo es Understanding-CatSniffer. El Vault anterior CatSniffer-Understanding no debe utilizarse como autoridad para sustituir las notas actuales.

Las notas describen código auditado y pruebas anteriores; eso no acredita automáticamente que el binario cargado corresponda exactamente al código de referencia.

3. Línea base de software y hardware
3.1 Referencias documentales de software
Elemento	Referencia
FeralRF	0178721cbd4f0d0f6f8eba5ae919ca46066d5dea
Paquete Python FeralRF	0.3.0, según la guía
TI SDK	simplelink_cc13xx_cc26xx_sdk_8_30_01_01
Commit del submódulo SDK	5b31d0a4903351e544546e23ef3330eaa4291ceb
CatSniffer-Firmware de referencia de la guía	c0cd5a45e019dbd14ed11d039aacb13f300e5731
CatSniffer-Tools de referencia de la guía	126f13bc0441ad3f37fe0b029160a3c526e4d309

La guía registra una modificación preexistente en Board.md dentro del SDK. No debe atribuirse a esta sesión ni restaurarse automáticamente.

3.2 Equipos y puertos de la sesión
Equipo	Plataforma identificada	Cat-Bridge	Cat-LoRa	Cat-Shell
CatSniffer #1	V3, RP2040 + CC1352P7	COM33	COM34	COM35
CatSniffer #2	V3, RP2040 + CC1352P7	COM88	COM86	COM87

La revisión exacta de PCB no queda demostrada por la identificación genérica “V3”. Debe confirmarse antes de elegir el esquemático definitivo.

Las asignaciones COM corresponden a esta sesión. No representan una identidad permanente de cada placa.

No debe calcularse Cat-Shell como Cat-Bridge + 2:

CatSniffer #1: COM33 → COM35
CatSniffer #2: COM88 → COM87

La segunda placa contradice esa regla.

El identificador FERALRF1 tampoco basta para distinguir físicamente los equipos.

3.3 Firmware RP2040 identificado en el contexto de la sesión
Equipo	Versión reportada	Commit asociado en el contexto
CatSniffer #1	v3.1.0.0	8eaa84c
CatSniffer #2	v3.1.0.1	c0cd5a4

La asociación con esos commits debe conservarse como antecedente. El texto de status confirma versiones, pero no constituye por sí mismo verificación del hash del binario instalado.

4. Modelo de evidencia
Nivel	Significado
A	Evidencia RF física por receptor o instrumento independiente del transmisor. Una segunda CatSniffer cuenta, con independencia limitada por compartir implementación.
B	Comportamiento observado directamente en dispositivo o firmware.
C	Control/API y aceptación de comandos. Debe indicarse cuando corresponde a mocks.
D	Código o documentación estática.
E	Inferencia o hipótesis.
F	Aspecto no evaluado.

El nivel de evidencia se registra separado del resultado: PASS, PARTIAL, FAIL, INCONCLUSIVE o NOT FULLY VALIDATED.

La distinción central es:

Solicitud TX aceptada / ACK
            ≠
Operación RF completada
            ≠
Paquete recibido por otro equipo
5. Antecedentes de evaluación anteriores a estos intentos Sub-GHz

Estos resultados proceden de las notas adjuntas. No se presentan como ejecuciones nuevas de esta sesión.

Evaluación	Antecedente relevante	Límite
EV-05	RX IEEE canal 25: 41, 43 y 43 paquetes en tres ventanas de 30 s.	Recepción local; no demuestra TX propio atribuido.
EV-10	Ocho casos de configuración/control aceptados.	Evidencia C; no validación OTA.
EV-11	27 presets declarados a nivel de control: 18 con stdout individual y nueve resumidos.	Ninguno demuestra OTA por preset en esa campaña.
EV-12 RAW	Marcador deadbeef1519, CRC reportado válido; una coincidencia entre 23 paquetes.	Camino OTA demostrado de forma limitada; no conteo físico exacto de emisiones.
EV-12 FRAME	Marcador corregido a1b2c3d417f1; una coincidencia entre 30 paquetes.	El primer intento utilizó un marcador incorrecto.
EV-12 BURST	Una coincidencia en corridas que solicitaban 40 o cinco transmisiones.	Criterio del observador incumplido; no demuestra que el transmisor emitiera únicamente una vez.
EV-12 CONTINUOUS	Con intervalo cero: 99 coincidencias entre 102 registros. Con intervalos positivos: una coincidencia por corrida.	Actividad repetida para intervalo cero; tasa y conteo físico exactos no establecidos.
EV-12 STOP	ACK, sin ventana física post-STOP suficiente.	Cese RF no demostrado; hubo timeout de RX_STOP y nueve respuestas inesperadas.
EV-13	Control de CW/PRBS y STOP.	Forma de onda y cese físico no medidos.
EV-14	Baseline IEEE, transición mínima BLE→IEEE y tres pruebas FakeSerial.	No se indujo un error RF físico ni se reprodujo la firma buscada.

En EV-14 se documentaron:

Baseline IEEE: ocho paquetes, CRC válido, cero errores asíncronos y cero coincidencias exactas con 8e89be.
Transición literal: diez paquetes BLE y diez IEEE; cero eventos de error y cero coincidencias exactas.
Tres repeticiones declaradas, pero únicamente una transcripción completa de la transición.
Pruebas host: 3 PASSED en 0,25 s.

Estos antecedentes muestran que existió recepción IEEE y alguna evidencia OTA IEEE. No validan automáticamente el backend proprietary Sub-GHz.

Las transcripciones extensas anteriores permanecen en sus notas originales; no están completas en el historial recibido para esta consolidación.

6. Preparación y comandos de referencia
6.1 Directorio y entorno Python

La guía establece:

Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF'
$env:PYTHONPATH = (Resolve-Path '.\python').Path

Get-CimInstance Win32_SerialPort |
    Sort-Object DeviceID |
    Select-Object DeviceID, Name, PNPDeviceID

Los prompts de las ejecuciones compartidas confirman que se trabajó desde:

C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF

No se dispone aquí de la salida literal de la enumeración de puertos ni de la asignación de PYTHONPATH.

6.2 Identificación y preflight definidos en la guía
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"

python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"

Comprobación del Bridge definida en la guía:

python -c "from feralrf import Radio; r=Radio('COM33'); print(r.init()); print(r.get_stats()); r.disconnect()"

python -c "from feralrf import Radio; r=Radio('COM88'); print(r.init()); print(r.get_stats()); r.disconnect()"

Baseline RX IEEE propuesto:

python .\python\examples\smoke_phy4_ieee154.py --port COM88 --channel 25 --duration 10

python .\python\examples\smoke_phy4_ieee154.py --port COM33 --channel 25 --duration 10

Estos comandos se conservan como procedimiento documental. No se inventan salidas ni se consideran todos ejecutados dentro del tramo visible del chat.

6.3 Condiciones de las pruebas proprietary
Condición	Registro
Potencia solicitada	0 dBm
Antenas	Multibanda, según confirmación del operador
Frecuencias	868 y 915 MHz
Autorización	Ambas frecuencias se trataron como autorizadas según el contexto previo
Distancia inicial	Se reportó una condición de aproximadamente 2 cm
Distancia posterior	Mayor separación; sin medición exacta
Orientación posterior	El operador indicó que apuntó la antena directamente
Reset automático	No incluido en el helper
Flashing	No forma parte del procedimiento registrado
Medición GPIO/CTF	No realizada/documentada
Medición RF instrumental	No realizada/documentada

La recomendación de 50 cm–1 m fue una condición propuesta. No debe registrarse como distancia efectivamente medida.

7. Selección explícita de Sub-GHz

La guía documenta:

Comando	Selección	CTF1/CTF2/CTF3 documentados
band1	2.4 GHz	0,1,0
band2	Sub-GHz	0,0,1

También atribuye el control de U2 a GPIO RP2040 8/9/10.

Para forzar una transición hacia Sub-GHz:

python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'band1\r\n'); time.sleep(.3); s.write(b'band2\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"

python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'band1\r\n'); time.sleep(.3); s.write(b'band2\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"

El contexto previo registra que ambos equipos reconocieron la selección y que apareció la confirmación textual:

SUB-GHz Band

No se dispone aquí de las dos transcripciones completas de esa transición.

La transición explícita se justificó por una incertidumbre de inicialización: change_band() puede retornar sin reprogramar salidas cuando considera que la banda solicitada ya está seleccionada.

Límite: una respuesta Shell y el campo Band demuestran comportamiento lógico reportado; no son una medición eléctrica del selector ni una prueba del recorrido RF.

8. Helper utilizado para las pruebas proprietary

El código completo conservado en la guía es:

function Invoke-FeralPresetOta {
  param([string]$TxPort,[string]$RxPort,[string]$Preset,[string]$MarkerHex)
  @'
import sys, time
from feralrf import PHY, PROP_PRESETS, Packet, Radio, RxStreamError
tx_port, rx_port, preset, marker_hex = sys.argv[1:]
marker = bytes.fromhex(marker_hex)
tx, rx = Radio(tx_port), Radio(rx_port)
try:
    tx.connect(); rx.connect()
    for r in (tx, rx):
        r.init(); r.set_phy(PHY.PROPRIETARY_GFSK, 0); r.configure_prop(**PROP_PRESETS[preset])
    tx.set_power(0); rx.start_rx()
    neg = list(rx.read_packets(timeout=3.0))
    neg_packets = [x for x in neg if isinstance(x, Packet)]
    neg_errors = [x for x in neg if isinstance(x, RxStreamError)]
    neg_hits = [x for x in neg_packets if x.crc_ok and marker in x.data]
    print('NEG', 'hits=', len(neg_hits), 'errors=', len(neg_errors))
    for x in neg: print('NEG_ITEM', x)
    acks = 0
    for _ in range(10):
        tx.transmit(marker, power_dbm=0, timeout=2.0); acks += 1; time.sleep(0.1)
    pos = list(rx.read_packets(timeout=5.0))
    packets = [x for x in pos if isinstance(x, Packet)]
    errors = [x for x in pos if isinstance(x, RxStreamError)]
    hits = [x for x in packets if x.crc_ok and marker in x.data]
    print('RESULT', preset, tx_port, '->', rx_port, 'acks=', acks,
          'hits=', len(hits), 'errors=', len(errors))
    for x in pos: print('RX_ITEM', x)
    sys.exit(0 if not neg_hits and not neg_errors and acks == 10 and len(hits) == 10 and not errors else 7)
finally:
    for r in (rx, tx):
        try: r.stop_rx()
        except Exception: pass
        try: r.disconnect()
        except Exception: pass
'@ | python - $TxPort $RxPort $Preset $MarkerHex
}
8.1 Secuencia efectiva del helper
Abre ambos Bridge.
Ejecuta init() en ambos equipos.
Selecciona PHY.PROPRIETARY_GFSK.
Aplica el preset mediante configure_prop().
Solicita potencia de 0 dBm en TX.
Solicita inicio de RX.
Consume una ventana negativa de tres segundos.
Ejecuta diez llamadas TX con pausas de 100 ms.
Consume una ventana positiva de cinco segundos.
Cuenta paquetes con CRC válido que contienen el marcador.
Imprime todos los objetos de ambas ventanas.
Intenta detener RX y desconectar ambos equipos.
8.2 Significado de los contadores
Campo	Qué mide
acks	Llamadas transmit() que retornaron sin excepción.
hits	Objetos Packet con crc_ok=True y marcador presente como subsecuencia.
errors	Objetos RxStreamError entregados por read_packets() en la ventana correspondiente.
NEG_ITEM	Cada objeto de la ventana negativa.
RX_ITEM	Cada objeto de la ventana positiva.

Precisiones:

acks=10 no demuestra diez emisiones completadas.
hits=0 por sí solo no equivale a cero paquetes totales.
En las transcripciones disponibles no aparecen líneas RX_ITEM; bajo este helper, eso es consistente con una lista positiva vacía.
errors=0 no demuestra ausencia de cualquier fallo interno.
El helper no imprime los fallos de cleanup: captura y descarta las excepciones de stop_rx() y disconnect().
El código retorna 7 si no se satisface el criterio completo, pero no se compartió una consulta de $LASTEXITCODE.
8.3 Resultado esperado

Para cada prueba se esperaba:

NEG hits= 0 errors= 0
RESULT <preset> <TX> -> <RX> acks= 10 hits= 10 errors= 0

Además, el helper debería imprimir los objetos recibidos como RX_ITEM.

La guía espera que el marcador aparezca contiguo dentro del PDU y que crc_ok=True. No exige igualdad del paquete completo, porque documenta cabecera y CRC en la representación RX proprietary.

Ese contrato debe contrastarse con el código: no puede deducirse únicamente de los resultados negativos actuales.

9. Primera prueba: GFSK 868 MHz, COM33 → COM88

Comando registrado en el contexto:

Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_868_50k -MarkerHex b10133008801

Resultado esperado:

NEG hits= 0 errors= 0
RESULT gfsk_868_50k COM33 -> COM88 acks= 10 hits= 10 errors= 0

Resultado conservado en el resumen previo:

Observable	Valor
Solicitudes TX aceptadas	10
Coincidencias	0
Errores asíncronos reportados	0

La transcripción literal original de esta primera ejecución no está incluida en el tramo recibido. El análisis previo también indica ausencia de líneas RX_ITEM.

Interpretación: criterio OTA incumplido; emisión y recepción físicas no localizadas.

10. Consulta de estado posterior

Después del primer experimento se solicitaron consultas Shell sin transmisión, flashing ni reset.

10.1 CatSniffer #1 — COM35

Comando ejecutado:

python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"

Salida literal:

Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.0, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
10.2 CatSniffer #2 — COM87

Comando ejecutado:

python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"

Salida literal:

Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=326 bytes
10.3 Comparación conservada en el contexto
Campo	#1 anterior	#1 posterior	#2 anterior	#2 posterior
Band	0	1	0	1
uart_overrun	0	0	0	0
ring_dropped	0	0	326	326

Las lecturas anteriores provienen del contexto resumido; sus salidas completas no están disponibles aquí.

Conclusiones acotadas:

Ambos Shell seguían respondiendo.
Ambos reportaban Band: 1, consistente con la selección lógica Sub-GHz.
Ambos reportaban feralrf_cc1352 (custom).
No se observó incremento de los contadores entre las consultas registradas.
Los 326 bytes no pueden atribuirse a esta prueba ni fecharse con la información disponible.
No hay consulta posterior registrada después de todas las ejecuciones siguientes.

El campo:

Radio: LoRa

no demuestra que el SX1262 ejecutara el TX proprietary de FeralRF. Su significado exacto dentro del estado del firmware RP2040 queda pendiente de rastrear.

11. Dirección inversa: GFSK 868 MHz, COM88 → COM33

Se invirtieron los roles para comprobar si el síntoma dependía de una dirección.

Comando:

Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset gfsk_868_50k -MarkerHex b10288003301

Resultado esperado:

NEG hits= 0 errors= 0
RESULT gfsk_868_50k COM88 -> COM33 acks= 10 hits= 10 errors= 0

El operador indicó:

Primero lo hice a 2cm y luego los volví retirar a una distancia más apartada apuntando la antena directamente, y no obtuve nada.

Las dos ejecuciones compartidas fueron:

PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset gfsk_868_50k -MarkerHex b10288003301
NEG hits= 0 errors= 0
RESULT gfsk_868_50k COM88 -> COM33 acks= 10 hits= 0 errors= 0
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset gfsk_868_50k -MarkerHex b10288003301
NEG hits= 0 errors= 0
RESULT gfsk_868_50k COM88 -> COM33 acks= 10 hits= 0 errors= 0

No aparecieron líneas RX_ITEM.

Según el relato del operador, las ejecuciones corresponden a una condición cercana y otra más separada. No se midió la segunda distancia.

El mismo marcador se reutilizó en ambas repeticiones; no hubo marcador exclusivo por corrida.

Interpretación: invertir roles y aumentar la separación no produjo coincidencias OTA. Esto no demuestra que ambos transmisores estén averiados ni confirma saturación a corta distancia.

12. Prueba exploratoria: GFSK 915 MHz, COM33 → COM88

Se propuso una ejecución a 915 MHz para comprobar si la ausencia de recepción se limitaba al preset de 868 MHz.

No se documentó una nueva transición de banda: se mantuvo la selección Sub-GHz previamente reportada.

Comando y salida literal:

PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_915_50k -MarkerHex b11133008801
NEG hits= 0 errors= 0
RESULT gfsk_915_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0

Resultado esperado:

NEG hits= 0 errors= 0
RESULT gfsk_915_50k COM33 -> COM88 acks= 10 hits= 10 errors= 0

No aparecieron líneas RX_ITEM.

No se ejecutó/documentó la dirección inversa a 915 MHz.

Interpretación: el síntoma también aparece con gfsk_915_50k en COM33 → COM88. No queda restringido al primer preset de 868 MHz.

13. Consolidación de resultados
Corrida	Preset	Dirección	Marcador	Distancia	acks	hits	errors
1	gfsk_868_50k	COM33 → COM88	b10133008801	Sin precisión suficiente	10	0	0
2	gfsk_868_50k	COM88 → COM33	b10288003301	Aproximadamente 2 cm, según relato	10	0	0
3	gfsk_868_50k	COM88 → COM33	b10288003301	Mayor separación, sin medición	10	0	0
4	gfsk_915_50k	COM33 → COM88	b11133008801	No medida en el registro	10	0	0
Total					40	0	0

Las cuatro corridas reportaron controles negativos con:

NEG hits= 0 errors= 0

Ese resultado satisface la ausencia del marcador y de errores entregados en esas ventanas. Un receptor que no recibe nada también puede satisfacerlo: no demuestra sensibilidad ni funcionamiento RX.

Clasificación
Aspecto	Resultado defendible
Control/configuración del procedimiento	Aceptación observada, evidencia C
Retorno de solicitudes TX	40 llamadas aceptadas, evidencia C
Selección lógica Sub-GHz	Reportada por Shell, evidencia B
Criterio de recepción del marcador	Incumplido en cuatro corridas
Paquetes positivos entregados	Ninguno visible en las capturas disponibles
Emisión física	No determinada
Recepción física proprietary	No determinada
Gate GFSK Sub-GHz	INCONCLUSIVE para localizar el fallo; criterio OTA incumplido
Defecto específico de FeralRF	No demostrado

No se calcula una pérdida física de paquetes de 100 %, porque no existe un conteo confirmado de emisiones que permita usarlo como denominador.

14. Limitaciones del procedimiento y posibles causas
14.1 Lectura posterior a las solicitudes TX

El helper solicita RX antes de TX, pero consume la ventana positiva después de las diez llamadas:

tx.set_power(0)
rx.start_rx()
for _ in range(10):
    tx.transmit(marker, power_dbm=0, timeout=2.0)
    acks += 1
    time.sleep(0.1)

pos = list(rx.read_packets(timeout=5.0))

Existe una dependencia de buffering y de la implementación del lector host.

Hipótesis: paquetes recibidos podrían perderse antes de consumirse.

Límite: no se ha inspeccionado aquí si Python mantiene lectura en segundo plano, qué capacidad tienen las colas ni cómo se descartan elementos. La secuencia del helper no demuestra por sí sola pérdida de paquetes.

14.2 ACK de RX_START y TX

La Wiki documenta que:

RX_START puede responder ACK antes de que DataTask invoque el inicio RF.
TX_RAW puede responder ACK antes de la ejecución RF real.

La ventana negativa introduce tiempo antes de TX, pero no sustituye una confirmación de estado RF efectivo.

14.3 Hipótesis abiertas
Hipótesis	Qué falta comprobar
Parámetros proprietary incorrectos o incompletos	Traducción del preset hasta los comandos RF efectivos.
Inicialización/transición RF incorrecta	RF_open, setup, frecuencia, patches, overrides y estado de operaciones.
Ruta física incorrecta	Esquemático, U2, CTF, matching, PA y conector utilizado.
Recepción/framing incompatible	Sync word, longitud, CRC, whitening, colas y parser.
Pérdida antes de entrega al host	Buffer RF, UART, RP2040, USB y colas Python.
Diferencia entre versiones RP2040	Equivalencia de change_band() y estado inicial en ambas versiones.
Firmware instalado diferente del auditado	Trazabilidad de binarios e identificación de placa.

Ninguna es una causa confirmada.

15. Pregunta del compañero y explicación conceptual

El compañero sugirió revisar cómo se realiza la modulación Sub-GHz, porque pensaba que CC1352 no tendría esa capacidad y que podría ejecutarse conjuntamente con RP2040 u otro componente.

La respuesta necesita separar:

Capacidad del silicio.
Implementación del firmware.
Recorrido físico en la placa.
Funcionamiento observado.
15.1 Capacidad del CC1352P7

TI documenta al CC1352P7 como un MCU inalámbrico multibanda Sub-1 GHz/2.4 GHz y lista 2-(G)FSK y 4-(G)FSK entre sus modulaciones. Por tanto, sí dispone de capacidad RF nativa para las modulaciones consideradas. Esto no garantiza cualquier frecuencia o preset arbitrario ni valida la implementación de FeralRF.

Fuentes para conservar:

TI — CC1352P7.
Datasheet CC1352P7.

Las bandas exactas, variantes y condiciones deben verificarse en el datasheet correspondiente. La existencia de un preset en Python no basta para afirmar soporte físico, especialmente para frecuencias pendientes como 169 MHz.

15.2 Cómo participa cada componente
Componente	Papel documentado	Qué no queda probado por estas pruebas
PC/Python	Solicita configuración, TX y RX; interpreta respuestas.	Que las operaciones RF se completen.
RP2040	USB CDC, puente UART, Shell y control de elementos de placa.	Que la ruta eléctrica esté correctamente seleccionada sólo por reportar Band.
CC1352P7 Cortex-M4F	Ejecuta FeralRF y prepara operaciones mediante TI RF Driver.	Que todos los presets se traduzcan correctamente.
RF Core del CC1352P7	Ejecuta las operaciones de radio y utiliza el subsistema RF integrado.	Emisión efectiva de las solicitudes actuales.
SX1262	Radio independiente controlada por RP2040 mediante SPI/GPIO.	Participación en las pruebas FeralRF actuales.

La Wiki documenta que FeralRF se ejecuta en CC1352P7 y no utiliza el SX1262. Eso debe verificarse contra el código local durante la investigación.

El RP2040 no genera por software la modulación GFSK del CC1352P7. Puede ser necesario para comunicación y selección externa de la ruta.

La interpretación plausible del comentario del compañero es:

La operación Sub-GHz de la placa puede depender de varios componentes, aunque el generador de la señal sea el transceptor integrado en CC1352P7.

No se atribuye esa interpretación como una afirmación literal del compañero.

16. Recorrido conceptual y discrepancia pendiente

Recorrido de control documentado:

PC / API Python
→ USB Cat-Bridge
→ RP2040
→ UART
→ CC1352P7 / FeralRF
→ TI RF Driver
→ RF Core

Recorrido RF por confirmar en el esquemático:

Subsistema RF del CC1352P7
→ red RF externa correspondiente
→ selección/conmutación aplicable
→ conector y antena

El orden concreto de matching, PA y conmutación debe extraerse del esquemático; este recorrido conceptual no sustituye una reconstrucción por nets y pines.

La Wiki identifica una discrepancia importante:

FeralRF/SysConfig contiene referencias a DIO28/29/30 para selección RF.
El hardware y firmware CatSniffer atribuyen señales CTF al RP2040.

Esto requiere comprobar ownership, conexiones y configuración real.

No puede afirmarse todavía:

Que ambos procesadores controlen eléctricamente el mismo net.
Que exista contención.
Que los DIO del CC estén conectados a U2.
Que una configuración LaunchPad sea válida para CatSniffer.
Que set_phy() seleccione el camino externo correcto.
17. Cobertura pendiente e intención del usuario

La guía establece:

Tier	Alcance
A	GFSK autorizado 868 o 915 bidireccional; 4FSK 868 cuando corresponda; GFSK 2440.
B	Más tasas/modulaciones y presets RF de W-MBus, Wi-SUN o Sidewalk.
C	OOK, MIOTY, 169/433 y rutas que requieren preparación adicional.

Comandos documentados, sin resultado nuevo en esta sesión:

Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset gfsk_915_50k -MarkerHex b11288003301
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset 4fsk_868_50k -MarkerHex b20133008802
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_2440_50k -MarkerHex b30133008803

Antes de 2440 MHz, la guía define la transición:

python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'band2\r\n'); time.sleep(.3); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"

python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'band2\r\n'); time.sleep(.3); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"

Tras cuatro resultados negativos se recomendó pausar transmisiones Sub-GHz para investigar elementos comunes.

El usuario expresó que prefería probar todos los presets antes de diagnosticar el código. Posteriormente solicitó este reporte y un prompt de investigación sobre la arquitectura Sub-GHz.

Estado actual: la ampliación de pruebas sigue como intención; no se registra como ejecutada ni cancelada. La siguiente actividad solicitada es investigación documental y de código de sólo lectura, con resultados escritos en el Vault.

Un intercambio exitoso bajo un preset llamado W-MBus, Wi-SUN, Sidewalk o MIOTY tampoco demostraría interoperabilidad con esos stacks.

18. Preguntas que debe resolver la investigación
¿Cuál es el modelo exacto de CC y la revisión física de PCB?
¿Qué bandas y modulaciones admite ese componente?
¿Qué partes realizan Cortex-M4F, RF Core y hardware RF?
¿Cuál es la ruta exacta Sub-GHz hasta cada conector?
¿Qué componente controla U2 y CTF1–3?
¿Cómo se relacionan DIO28/29/30 con las conexiones reales?
¿Son equivalentes las implementaciones RP2040 asociadas a 8eaa84c y c0cd5a4?
¿Qué significa Radio: LoRa en status?
¿Cómo traduce FeralRF cada parámetro de configure_prop()?
¿Qué asegura realmente el retorno de start_rx() y transmit()?
¿Dónde se almacenan los paquetes antes de read_packets()?
¿Qué parte de la cadena informa errores y qué fallos pueden quedar sin informar?
¿Qué presets son admisibles para el silicio y la ruta física disponible?
¿Qué observación adicional distinguiría fallo RF de fallo de entrega?
19. Conclusión provisional

Se registraron 40 solicitudes TX aceptadas y cero coincidencias de recepción, repartidas entre cuatro corridas: tres a 868 MHz y una a 915 MHz.

Ambos RP2040 reportaron selección lógica Sub-GHz después de la primera prueba. No se midió el selector ni se confirmó emisión física.

El CC1352P7 dispone de capacidad Sub-GHz nativa. El punto abierto es si la implementación, configuración y recorrido físico de las CatSniffer utilizadas ejercitan correctamente esa capacidad.

El criterio OTA no se cumplió; la causa permanece sin localizar.

## Seguimiento documental

La investigación posterior está consolidada en [[Sub-GHz en CatSniffer V3 - CC1352P7, ruta RF y control de banda]] y resumida para este experimento en [[Seguimiento documental OTA Sub-GHz - ruta RF y control de banda]]. Esos hallazgos confirman capacidad nativa del CC1352P7 y control externo de U2 por el RP2040, pero no convierten estas corridas negativas en una causa raíz ni en validación RF.

**Continuación cronológica:** [[04 - OTA proprietary - campaña ampliada]]. Este reporte conserva su corte original de cuatro corridas, 40 solicitudes aceptadas y cero hits; los acumulados posteriores se mantienen en la segunda campaña y en el seguimiento documental.
