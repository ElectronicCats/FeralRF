# 04 — OTA proprietary: campaña ampliada de presets y verificación del procedimiento
1. Propósito

Este reporte continúa el registro experimental iniciado en el primer informe OTA.

El objetivo de esta segunda etapa fue:

Completar Tier A con 4FSK 868 MHz y GFSK 2.4 GHz.
Comprobar si los resultados negativos podían estar causados por ejecutar TX y RX desde una sola terminal.
Repetir la observación con RX concurrente y timestamps.
Consultar los contadores internos de FeralRF.
Ampliar la cobertura a los presets disponibles de 868, 902/915 y 2440 MHz que no requerían condiciones adicionales conocidas.
Contrastar los resultados actuales con antecedentes históricos de W-MBus, Wi-SUN y Amazon Sidewalk.
Identificar el punto seguro para detener la campaña antes de 433 MHz, 169 MHz, OOK y MIOTY.

Fecha de ejecución: 8 de octubre de 2026.

2. Relación con el primer reporte

El primer reporte había registrado:

gfsk_868_50k, COM33 → COM88: 10 ACK, 0 hits.
gfsk_868_50k, COM88 → COM33: dos corridas, 20 ACK, 0 hits.
gfsk_915_50k, COM33 → COM88: 10 ACK, 0 hits.
Total previo: 40 solicitudes TX aceptadas y cero paquetes observados.
Ambos CatSniffer reportaban Band: 1 después de seleccionar Sub-GHz.
No existía medición eléctrica del selector RF ni confirmación instrumental de emisión.

Este segundo reporte comienza con la prueba 4fsk_868_50k.

3. Equipos y asignación de puertos
Dispositivo	Cat-Bridge	Cat-LoRa	Cat-Shell	Firmware RP2040
CatSniffer #1	COM33	COM34	COM35	v3.1.0.0
CatSniffer #2	COM88	COM86	COM87	v3.1.0.1

Firmware reportado por ambos CC1352P7:

feralrf_cc1352 (custom)

Información devuelta por FeralRF durante las pruebas concurrentes:

DeviceInfo(
    firmware_version='1.0.0',
    capabilities=7,
    serial='464552414c524631'
)

El serial hexadecimal corresponde a:

FERALRF1

Este valor es común y no permite distinguir físicamente las placas.

4. Condiciones generales
Parámetro	Condición
Dirección principal	COM33 TX → COM88 RX
Dirección inversa de control	COM88 TX → COM33 RX
Potencia solicitada	0 dBm
Cantidad por corrida	10 solicitudes TX
Antenas	Multibanda, según confirmación del operador
Separación	Mayor que la condición inicial de 2 cm; distancia exacta no registrada
Helper	Invoke-FeralPresetOta definido en la guía
Reset automático	No utilizado
Flashing	No realizado
Instrumentación RF independiente	No disponible
Criterio positivo	10 ACK, 10 hits, 0 errores
Marcadores	Únicos por preset o control, salvo repeticiones ya documentadas

El helper original permaneció sin modificaciones durante el barrido de presets.

5. Significado de los resultados
acks=10

significa que diez llamadas transmit() regresaron sin excepción. No demuestra diez emisiones físicas completadas.

hits=0

significa que el helper no encontró objetos Packet con:

x.crc_ok and marker in x.data

En todas las salidas de esta segunda campaña faltaron líneas:

RX_ITEM

Esto es consistente con ventanas positivas sin eventos entregados, no sólo con paquetes que fallaran el filtro del marcador.

errors=0

significa que read_packets() no entregó objetos RxStreamError durante la ventana. No demuestra ausencia de errores internos que no se propaguen mediante ese mecanismo.

6. Pruebas 4FSK 868 MHz
6.1 COM33 → COM88

Comando:

Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset 4fsk_868_50k -MarkerHex b20133008802

Salida:

NEG hits= 0 errors= 0
RESULT 4fsk_868_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0
6.2 COM88 → COM33

Comando:

Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset 4fsk_868_50k -MarkerHex b20288003302

Salida:

NEG hits= 0 errors= 0
RESULT 4fsk_868_50k COM88 -> COM33 acks= 10 hits= 0 errors= 0
6.3 Resultado
Dirección	ACK	Hits	Errors
COM33 → COM88	10	0	0
COM88 → COM33	10	0	0
Total	20	0	0

Clasificación:

Control/API: aceptado
Criterio OTA: incumplido
4FSK por aire: no validado
Causa: no localizada
Resultado global: INCONCLUSIVE
7. Cambio controlado a 2.4 GHz

Se forzó la transición:

band2 → band1
7.1 CatSniffer #2 — COM87
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'band2\r\n'); time.sleep(.3); s.write(b'band1\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"

Salida:

SUB-GHz Band

2.4GHz Band

Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=326 bytes
7.2 CatSniffer #1 — COM35
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'band2\r\n'); time.sleep(.3); s.write(b'band1\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"

Salida:

SUB-GHz Band

2.4GHz Band

Mode: 0, Band: 0, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.0, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes

Ambos Shell reportaron Band: 0, consistente con la selección lógica de 2.4 GHz.

Los contadores del puente no cambiaron respecto a las lecturas anteriores.

8. GFSK 2440 MHz con el helper original
8.1 gfsk_2440_50k

Comando:

Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_2440_50k -MarkerHex b30133008803

Salida:

NEG hits= 0 errors= 0
RESULT gfsk_2440_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0

El síntoma observado anteriormente en Sub-GHz también apareció bajo un preset proprietary de 2.4 GHz.

9. Verificación del procedimiento con RX concurrente

Se consideró la hipótesis de que el helper original pudiera perder paquetes porque realiza las diez solicitudes TX antes de volver a consumir la ventana RX positiva.

La secuencia original es:

rx.start_rx()
neg = list(rx.read_packets(timeout=3.0))

for _ in range(10):
    tx.transmit(...)
    time.sleep(0.1)

pos = list(rx.read_packets(timeout=5.0))

Para comprobarlo, se separaron receptor y transmisor en dos terminales.

9.1 Control concurrente COM33 → COM88

Preset:

gfsk_2440_50k

Marcador:

b31133008803

Salida RX:

RX CONNECTED COM88
RX INFO DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')
RX CONFIGURED gfsk_2440_50k
RX READY ? start Terminal 2 now
RX RESULT events= 0 packets= 0 hits= 0 errors= 0
RX STOPPED
RX DISCONNECTED

Salida TX:

TX CONNECTED COM33
TX INFO DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')
TX CONFIGURED gfsk_2440_50k power=0 dBm
TX ACK 1
TX ACK 2
TX ACK 3
TX ACK 4
TX ACK 5
TX ACK 6
TX ACK 7
TX ACK 8
TX ACK 9
TX ACK 10
TX RESULT acks=10
TX DISCONNECTED

El carácter ? sustituyó visualmente a una raya larga en PowerShell. No afecta la ejecución.

Esta primera prueba concurrente produjo cero eventos. Sin timestamps, la transcripción por sí sola no demostraba la superposición temporal exacta.

9.2 Control concurrente inverso con timestamps

Dirección:

COM88 TX → COM33 RX

Marcador:

b31288003303

Salida TX:

2026-10-08T12:34:17.725 TX CONNECTED COM88
2026-10-08T12:34:17.850 TX INFO DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')
2026-10-08T12:34:17.950 TX CONFIGURED gfsk_2440_50k power=0 dBm
2026-10-08T12:34:17.980 TX ACK 1
2026-10-08T12:34:18.114 TX ACK 2
2026-10-08T12:34:18.249 TX ACK 3
2026-10-08T12:34:18.391 TX ACK 4
2026-10-08T12:34:18.525 TX ACK 5
2026-10-08T12:34:18.669 TX ACK 6
2026-10-08T12:34:18.814 TX ACK 7
2026-10-08T12:34:18.958 TX ACK 8
2026-10-08T12:34:19.098 TX ACK 9
2026-10-08T12:34:19.241 TX ACK 10
2026-10-08T12:34:19.342 TX RESULT acks=10
2026-10-08T12:34:19.344 TX DISCONNECTED

Salida RX:

2026-10-08T12:34:13.700 RX CONNECTED COM33
2026-10-08T12:34:13.820 RX INFO DeviceInfo(firmware_version='1.0.0', capabilities=7, serial='464552414c524631')
2026-10-08T12:34:13.885 RX CONFIGURED gfsk_2440_50k
2026-10-08T12:34:13.916 RX READY - start Terminal 2 now
2026-10-08T12:34:43.931 RX RESULT events= 0 packets= 0 hits= 0 errors= 0
2026-10-08T12:34:43.974 RX STOPPED
2026-10-08T12:34:43.976 RX DISCONNECTED
9.3 Comprobación temporal
Evento	Hora
RX listo	12:34:13.916
Primer ACK TX	12:34:17.980
Último ACK TX	12:34:19.241
Fin de RX	12:34:43.931

La primera solicitud TX ocurrió aproximadamente 4.1 segundos después de iniciar la lectura RX. La ventana continuó durante unos 24.7 segundos después del último ACK.

Conclusión:

La ejecución desde una sola terminal y el consumo RX posterior no explican por sí solos el patrón negativo.

La prueba concurrente tampoco demuestra emisión física. Confirma que no se entregó ningún evento al host mientras RX se consumía activamente.

10. Estadísticas internas posteriores

Se consultó get_stats() sin llamar nuevamente a init(), para evitar reiniciar las métricas.

Ambas placas devolvieron:

DeviceStats(
    rx_ok=0,
    rx_crc_err=0,
    rx_drop=0,
    rx_overflow=0,
    ll_kind_unknown=None,
    ll_kind_adv=None,
    ll_kind_scan=None,
    ll_kind_connect=None,
    ll_kind_data=None
)

Interpretación:

Ningún paquete contabilizado como RX correcto.
Ningún paquete contabilizado como error CRC.
Ningún drop registrado.
Ningún overflow registrado.
Sin clasificación Link Layer aplicable en esta ruta.

Esto reduce la sospecha de que los paquetes hayan alcanzado el backend RX y después se hayan perdido por los contadores examinados.

No demuestra ausencia de energía RF, ni cubre todos los descartes posibles de la cadena.

11. Segundo preset proprietary de 2.4 GHz
gfsk_2440_250k

Comando:

Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_2440_250k -MarkerHex b32133008804

Salida:

NEG hits= 0 errors= 0
RESULT gfsk_2440_250k COM33 -> COM88 acks= 10 hits= 0 errors= 0

Los dos presets proprietary de 2.4 GHz produjeron cero recepción:

Preset	ACK	Hits
gfsk_2440_50k	10	0
gfsk_2440_250k	10	0

gfsk_2440_50k también fue verificado en sentido inverso y mediante lectura concurrente.

12. Regreso a Sub-GHz

Se forzó:

band1 → band2
CatSniffer #1
2.4GHz Band

SUB-GHz Band

Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.0, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=0 bytes
CatSniffer #2
2.4GHz Band

SUB-GHz Band

Mode: 0, Band: 1, Radio: LoRa, LoRa: initialized, LoRa Mode: Stream, FW: v3.1.0.1, CC1352 FW: feralrf_cc1352 (custom)
CC1352 loss: uart_overrun=0, ring_dropped=326 bytes

Ambas placas regresaron al estado lógico Band: 1. Los contadores del RP2040 permanecieron sin incremento.

13. Ampliación de presets en 868 MHz
13.1 gfsk_868_100k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_868_100k -MarkerHex b40133008805
NEG hits= 0 errors= 0
RESULT gfsk_868_100k COM33 -> COM88 acks= 10 hits= 0 errors= 0
13.2 msk_868_50k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset msk_868_50k -MarkerHex b41133008806
NEG hits= 0 errors= 0
RESULT msk_868_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0
13.3 4gfsk_868_50k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset 4gfsk_868_50k -MarkerHex b42133008807
NEG hits= 0 errors= 0
RESULT 4gfsk_868_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0
13.4 Consolidación genérica 868 MHz

Incluyendo las pruebas del primer reporte:

Preset	Modulación nominal	Dirección	ACK	Hits
gfsk_868_50k	GFSK	Ambas	30	0
gfsk_868_100k	GFSK	COM33 → COM88	10	0
msk_868_50k	MSK	COM33 → COM88	10	0
4fsk_868_50k	4FSK	Ambas	20	0
4gfsk_868_50k	4GFSK	COM33 → COM88	10	0

La ausencia de paquetes no quedó limitada a un solo valor de tasa o familia de modulación.

14. Wireless M-Bus S/T/C

Estos nombres designan presets de parámetros RF. No implementan EN 13757, cifrado, framing superior ni interoperabilidad completa.

14.1 S
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wireless_mbus_s_868 -MarkerHex b50133008808
NEG hits= 0 errors= 0
RESULT wireless_mbus_s_868 COM33 -> COM88 acks= 10 hits= 0 errors= 0
14.2 T
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wireless_mbus_t_868 -MarkerHex b51133008809
NEG hits= 0 errors= 0
RESULT wireless_mbus_t_868 COM33 -> COM88 acks= 10 hits= 0 errors= 0
14.3 C
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wireless_mbus_c_868 -MarkerHex b5213300880a
NEG hits= 0 errors= 0
RESULT wireless_mbus_c_868 COM33 -> COM88 acks= 10 hits= 0 errors= 0
14.4 Comparación histórica

La documentación registra antecedentes OTA favorables para W-MBus S/T/C, pero carece de suficiente trazabilidad para convertirlos en validación vigente.

En esta campaña:

30 solicitudes TX aceptadas
0 paquetes observados

No se evaluaron los presets W-MBus N de 169 MHz.

15. GFSK 902 MHz

Comando:

Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_902_50k -MarkerHex b6013300880b

Salida:

NEG hits= 0 errors= 0
RESULT gfsk_902_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0

Comparación:

Preset	Frecuencia nominal	ACK	Hits
gfsk_902_50k	902.2 MHz	10	0
gfsk_915_50k	915 MHz	10	0
16. Amazon Sidewalk PHY-only

Los presets Sidewalk están documentados como experimentales y sólo implementan parámetros FSK y bytes raw.

No implementan:

Stack Sidewalk.
Autenticación.
Framing completo.
Asociación o red.
Sidewalk LR/LoRa.

El SX1262 físico no participa en la ruta FeralRF del CC1352P7.

16.1 sidewalk_915_fsk_50k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset sidewalk_915_fsk_50k -MarkerHex b6113300880c
NEG hits= 0 errors= 0
RESULT sidewalk_915_fsk_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0
16.2 sidewalk_915_fsk_250k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset sidewalk_915_fsk_250k -MarkerHex b6213300880d
NEG hits= 0 errors= 0
RESULT sidewalk_915_fsk_250k COM33 -> COM88 acks= 10 hits= 0 errors= 0

La documentación asocia una corrección histórica del ancho de banda del preset de 250 k con el commit:

10fc47a

Resultado actual:

20 solicitudes TX aceptadas
0 paquetes observados
17. Wi-SUN PHY-only

Estos presets son experimentales y representan configuraciones de portadora, tasa y FSK. No implementan Wi-SUN FAN, RPL, seguridad ni certificación.

17.1 50 k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wisun_915_fsk_50k -MarkerHex b7013300880e
NEG hits= 0 errors= 0
RESULT wisun_915_fsk_50k COM33 -> COM88 acks= 10 hits= 0 errors= 0
17.2 100 k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wisun_915_fsk_100k -MarkerHex b7113300880f
NEG hits= 0 errors= 0
RESULT wisun_915_fsk_100k COM33 -> COM88 acks= 10 hits= 0 errors= 0
17.3 150 k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wisun_915_fsk_150k -MarkerHex b72133008810
NEG hits= 0 errors= 0
RESULT wisun_915_fsk_150k COM33 -> COM88 acks= 10 hits= 0 errors= 0
17.4 200 k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wisun_915_fsk_200k -MarkerHex b73133008811
NEG hits= 0 errors= 0
RESULT wisun_915_fsk_200k COM33 -> COM88 acks= 10 hits= 0 errors= 0
17.5 300 k
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset wisun_915_fsk_300k -MarkerHex b74133008812
NEG hits= 0 errors= 0
RESULT wisun_915_fsk_300k COM33 -> COM88 acks= 10 hits= 0 errors= 0
17.6 Consolidación Wi-SUN
Preset	ACK	Hits	Errors
wisun_915_fsk_50k	10	0	0
wisun_915_fsk_100k	10	0	0
wisun_915_fsk_150k	10	0	0
wisun_915_fsk_200k	10	0	0
wisun_915_fsk_300k	10	0	0
Total	50	0	0
18. Discrepancia con el antecedente Wi-SUN/Sidewalk

La matriz histórica reporta:

Wi-SUN/Sidewalk 70/70
Fecha: 2026-05-03

La auditoría documental indica que faltan:

Logs crudos.
Hash del firmware ejecutado.
Identificación completa de las placas.
Condiciones RF suficientes.
Trazabilidad para promoverlo a validación actual.

Campaña presente:

Grupo	Presets	TX aceptados	Paquetes observados
Sidewalk	2	20	0
Wi-SUN	5	50	0
Total	7	70	0

Conclusión:

El antecedente histórico Wi-SUN/Sidewalk 70/70 no se reprodujo en las condiciones actuales.

Esto no demuestra todavía una regresión, porque ambos conjuntos de condiciones no pueden considerarse equivalentes.

19. Cobertura acumulada
19.1 Segunda campaña

Esta segunda etapa añadió:

200 solicitudes TX aceptadas
0 paquetes proprietary observados
0 RxStreamError entregados
19.2 Campaña completa, incluyendo el primer reporte
240 solicitudes TX aceptadas
0 paquetes proprietary observados
0 RxStreamError entregados

Se ejecutaron 19 de los 31 presets únicos.

Grupo	Presets únicos ejecutados
868 genérico	5
W-MBus S/T/C	3
902/915 genérico	2
Proprietary 2.4	2
Sidewalk	2
Wi-SUN	5
Total	19

Las 240 solicitudes incluyen repeticiones bidireccionales y controles concurrentes adicionales.

20. Presets pendientes

Quedan 12 presets:

20.1 Grupo 433 MHz
gfsk_433_50k
gfsk_433_10k
fsk_433_50k
ook_433_4k8
ook_433_2k4
msk_433_50k
4fsk_433_50k
4gfsk_433_50k

Estado documental:

Control implementado.
Comportamiento histórico marginal.
GFSK histórico entre 6 y 10 de 10.
FSK/MSK histórico alrededor de 1 de 10.
OOK 433 histórico 0/10.
Ruta, antena y medición actual no confirmadas.

Decisión actual:

No ejecutar.

Motivo adicional:

El asesor indicó que 433 MHz aparentemente todavía no estaba soportado o que no debía probarse, aunque no especificó la causa.

Esta indicación debe registrarse como restricción del asesor, no como conclusión técnica demostrada.

20.2 OOK 868 MHz
ook_868_4k8

Estado:

Preset existente.
Antecedente histórico favorable en 868 MHz.
Riesgo documentado de que cargar el patch OOK deje la radio bloqueada hasta reset o power cycle.
Recuperación automática insegura por el defecto Bridge+2.
Shell reales conocidos: COM35 y COM87.

Decisión:

Diferido hasta preparar y documentar un procedimiento específico de recuperación.

20.3 W-MBus N 169 MHz
wireless_mbus_n_169_2k4
wireless_mbus_n_169_4k8

Estado:

No cerrado históricamente.
Antenas actuales descritas como multibanda, sin confirmación explícita de 169 MHz.
Ruta RF y compatibilidad del CC1352P7 con esa frecuencia pendientes de la auditoría.
Sin autorización/condiciones experimentales documentadas.

Decisión:

No ejecutar en esta campaña.

20.4 MIOTY
mioty_868_tsunb

Estado:

Preset conservado.
Implementación marcada como incompleta/pending.
Resultado histórico 0/10.
Probable necesidad de custom CPE.
Un preset nominal de 396 baud no equivale a implementar TS-UNB.

Decisión:

No ejecutar como preset funcional actual. Tratar como limitación pendiente.

21. Hallazgos técnicos de la campaña
21.1 El procedimiento básico es correcto

Se verificaron:

Puertos distintos y correctos.
Inicialización de ambos endpoints.
Aplicación del mismo preset en TX y RX.
Potencia explícita de 0 dBm.
RX iniciado antes de TX.
Selector lógico cambiado por banda.
Marcadores únicos.
Control negativo antes de cada corrida.
Prueba en ambas direcciones para presets representativos.
Lectura RX concurrente con timestamps.
Estadísticas posteriores.

No apareció evidencia de un error básico de ejecución por parte del operador.

21.2 La terminal única no explica el síntoma

La prueba concurrente demostró que las diez solicitudes TX ocurrieron dentro de una ventana RX activa de 30 segundos.

Resultado:

events=0
packets=0
hits=0
errors=0

La hipótesis de que el helper perdiera todos los paquetes únicamente por leer después de TX pierde fuerza.

21.3 La comunicación host–dispositivo funciona

Ambas placas:

Abren sus puertos.
Responden a init().
Devuelven DeviceInfo.
Aceptan set_phy().
Aceptan configure_prop().
Aceptan start_rx().
Aceptan transmit().
Responden a stop_rx() y disconnect().

Por tanto, la descripción correcta no es “no existe comunicación”.

La descripción correcta es:

El plano de control host–dispositivo funciona, pero no se observa comunicación OTA proprietary entre los dos CatSniffer.

21.4 Los ACK no confirman RF

Según la arquitectura documentada:

RX_START ACK

puede emitirse antes de que DataTask invoque RadioIF_startRx().

TX_RAW ACK

puede emitirse antes de que RadioIF_transmitRaw() ejecute la operación RF.

configure_prop() puede responder ACK aunque el backend utilizado sea una función sin retorno de estado verificable por el handler.

21.5 El receptor no registró actividad

Después del control concurrente:

rx_ok=0
rx_crc_err=0
rx_drop=0
rx_overflow=0

No existen indicios de paquetes correctos, CRC incorrectos, drops u overflow en esas métricas.

Esto no equivale a una medición de ausencia de RF.

21.6 El síntoma cruza bandas y modulaciones

Se reprodujo con:

868 MHz.
902.2 MHz.
915 MHz.
2440 MHz.
GFSK.
MSK.
4FSK.
4GFSK.
Tasas nominales desde 50 k hasta 300 k.
Dos direcciones en presets representativos.
Lectura posterior y concurrente.

Esto orienta el diagnóstico hacia un elemento compartido, pero no identifica cuál.

22. Hipótesis abiertas
Hipótesis	Evidencia compatible	Evidencia que falta
Configuración proprietary no aplicada correctamente	ACK no confirma resultado del backend	Inspección de RadioIF_setPropConfig() y estructuras RF
RX no inicia físicamente	RX_START es diferido	Estado/resultado real de la operación RF
TX no se ejecuta físicamente	TX ACK precede a RF	TX_DONE o instrumento independiente
Ruta RF externa incorrecta	Band sólo representa estado lógico	Medición CTF/U2 y recorrido del esquemático
Configuración de PA/front-end incorrecta	Cero paquetes en ambas bandas	Análisis de SysConfig, PA y RF switch
Framing/sync/CRC incompatible entre endpoints	Cero eventos y contadores	Comparación TX/RX de longitud, sync, CRC y whitening
Binario instalado diferente del auditado	Sólo se conoce versión 1.0.0	Hash/manifest del firmware cargado
Regresión frente a resultados históricos	Sidewalk/Wi-SUN actual 0/70 frente a 70/70 histórico	Reproducir binario y condiciones históricas
23. Resultado global
Control/API: ampliamente aceptado
TX solicitados: 240
Paquetes proprietary entregados: 0
Errores asíncronos entregados: 0
Presets únicos ejecutados: 19/31
Emisión física: no demostrada
Recepción proprietary física: no demostrada
Selector físico: no medido
Resultado OTA proprietary: criterio incumplido
Causa raíz: no localizada

Clasificación:

INCONCLUSIVE para funcionamiento físico y causa; FAIL del criterio de recepción OTA en todos los presets ejecutados.

24. Decisión de cierre temporal

La campaña experimental se detiene antes de:

433 MHz, por indicación del asesor y condiciones técnicas no confirmadas.
169 MHz, por incertidumbre de silicio, ruta y antena.
OOK, por riesgo de bloqueo y necesidad de recuperación controlada.
MIOTY, por implementación incompleta.

El siguiente paso es reconciliar este reporte con la auditoría final de Codex para determinar:

Si el CC1352P7 y la placa soportan realmente cada banda.
Cómo se aplican los presets en RadioIF.
Cómo se inician RX y TX.
Qué controla el selector RF externo.
Si las dos versiones RP2040 manejan la ruta de forma equivalente.
Qué explicación técnica puede abarcar cero recepción en Sub-GHz y 2.4 GHz.
Qué única observación física o modificación de procedimiento discriminaría mejor las hipótesis.

25. Normalización documental posterior

Esta sección interpreta la evidencia sin modificar los comandos, stdout, timestamps u observaciones literales anteriores. Véanse [[Sub-GHz en CatSniffer V3 - CC1352P7, ruta RF y control de banda]] y [[Seguimiento documental OTA Sub-GHz - ruta RF y control de banda]].

25.1 Aritmética independiente

La segunda campaña contiene 20 corridas de diez retornos exitosos de `transmit()`: 18 con el helper y dos controles concurrentes. Añadidas a las cuatro corridas del primer reporte, el acumulado es:

| magnitud | segunda campaña | acumulado de ambos reportes |
|---|---:|---:|
| corridas | 20 | 24 |
| retornos exitosos de `transmit()` | 200 | 240 |
| presets únicos acumulados | 19 | 19 |
| paquetes proprietary entregados / hits | 0 | 0 |
| `RxStreamError` expuestos | 0 | 0 |

Las repeticiones bidireccionales y concurrentes aumentan corridas y solicitudes, no el número de presets únicos. Los 240 retornos no equivalen a 240 emisiones RF.

25.2 Control concurrente y estadísticas

En el control inverso, RX estuvo listo `4.064 s` antes del primer ACK TX. Entre el primer y último ACK transcurrieron `1.261 s`; después del último ACK permanecieron `24.690 s` de observación RX. Así, las solicitudes TX ocurrieron mientras `read_packets()` estaba activo. Ejecutar TX y RX desde una sola terminal no queda sustentado como explicación única del patrón, y el consumo tardío del helper original pierde fuerza explicativa. Esto no prueba emisiones, llegada a capas internas ni ausencia universal de pérdidas.

Los dos `DeviceStats(rx_ok=0, rx_crc_err=0, rx_drop=0, rx_overflow=0, ...)` indican únicamente que, en el intervalo post-`init()` pertinente, los contadores expuestos no registraron RX correcto, CRC erróneo, drop u overflow. Son consistentes con que ningún paquete proprietary alcanzara esas etapas contadas; no cubren toda la cadena ni prueban ausencia de energía RF.

25.3 Clasificación válida

- **A — diagnóstico OTA GFSK/FSK físicamente plausible:** `gfsk_868_50k`, `gfsk_868_100k`, `gfsk_902_50k`, `gfsk_915_50k`, `gfsk_2440_50k`, `gfsk_2440_250k`, W-MBus S/T/C, Sidewalk FSK y Wi-SUN FSK. Hubo aceptación de control y cero paquetes entregados; TX y RX físicos no quedaron confirmados.
- **B — nombre de modulación no demostrado:** `msk_868_50k`, `4fsk_868_50k` y `4gfsk_868_50k`. El setup `CMD_PROP_RADIO_DIV_SETUP_PA` del SDK auditado reserva los `modType` 4/5/6. Las solicitudes fueron aceptadas, pero no constituyen ensayos físicos demostrados de MSK/4FSK/4GFSK.
- **C — no ejecutar defensiblemente:** los ocho presets 433 y los dos W-MBus N de 169 MHz. El silicio sí cubre 433, pero U4 publicado no; además hubo restricción operativa del asesor. 169.45 MHz queda fuera tanto del silicio CC1352P74 como de U4.
- **D — diferidos:** `ook_868_4k8` por lifecycle/reset y el defecto conocido de selección Shell; OOK 433 añade la limitación del frente publicado; `mioty_868_tsunb` sigue incompleto/pending, con antecedente 0/10 y posible CPE custom.

Los nombres W-MBus, Sidewalk y Wi-SUN siguen describiendo parámetros PHY/raw, no interoperabilidad de sus stacks.

25.4 Reconciliación histórica y cierre

La campaña actual no reprodujo el antecedente Wi-SUN/Sidewalk `70/70`: los mismos cinco nombres Wi-SUN y dos Sidewalk produjeron `0/70` paquetes entregados bajo las condiciones actuales. No se declara regresión porque faltan logs crudos, hash del binario histórico, identidad de placas y condiciones suficientes para demostrar equivalencia. W-MBus S/T/C tuvo claims históricos favorables y proprietary 2.4 GHz antecedentes contradictorios; tampoco se promueven a evidencia vigente.

La expansión ciega de presets queda pausada. El siguiente experimento propuesto usa una sola solicitud `gfsk_868_50k`: observar niveles CTF1/2/3 durante `band1 → band2`, interpretar U2 con la tabla del RFSW8006Q y observar RF en J1 con método apropiado. No se propone continuidad DC a través del switch como prueba de una ruta RF.
