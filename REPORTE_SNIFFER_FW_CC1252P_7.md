# Reporte tecnico: `sniffer_fw_cc1252P_7`

## 1. Resumen ejecutivo

`examples-catsniffer/sniffer_fw_cc1252P_7` es una base probada para sniffing en CC1352P7 con UART a 3 Mbps y una arquitectura robusta de tareas, colas y manejo de PHY/Link Layer.

Para FeralRF, lo mas valioso a copiar no es su protocolo host, sino su **arquitectura interna de pipeline RF** (eventos, colas, estados, manejo de overflow, seleccion de PHY/LL).

## 2. Lo que hace bien (y por que sirve para FeralRF)

1. Arranque estable con tareas separadas por responsabilidad.
- 4 tareas con prioridades claras y stack extra para control: `HOST_IF=1`, `DATA=2`, `USER_IF=3`, `CONTROL=4` en `examples-catsniffer/sniffer_fw_cc1252P_7/source/packet_sniffer_fw.c:61`.
- Crea tareas y eventos en init central en `examples-catsniffer/sniffer_fw_cc1252P_7/source/packet_sniffer_fw.c:73`.

2. Transporte host robusto a alta velocidad.
- UART en binario a `3000000` baud en `examples-catsniffer/sniffer_fw_cc1252P_7/source/host_if.c:73`.
- Proteccion de acceso concurrente con semaforo para write en `examples-catsniffer/sniffer_fw_cc1252P_7/source/host_if.c:56`.

3. Pipeline de datos desacoplado (radio -> procesamiento -> host).
- `dataTask` procesa RX por eventos de RF (`RX_ENTRY_DONE`, `RX_BUFFER_FULL`) en `examples-catsniffer/sniffer_fw_cc1252P_7/source/data_task.c:62`.
- Cola de salida con pool fijo (3 slots) en `examples-catsniffer/sniffer_fw_cc1252P_7/source/packet_queue.c:47`.
- `hostIfTask` solo saca de cola y transmite en `examples-catsniffer/sniffer_fw_cc1252P_7/source/host_if_task.c:57`.

4. Manejo de estados y comandos consistente.
- Maquina de estados (WAIT/INIT/STARTED/STOPPED) en `examples-catsniffer/sniffer_fw_cc1252P_7/source/control_task.c:55`.
- Reglas de validez por estado para comandos de configuracion en `examples-catsniffer/sniffer_fw_cc1252P_7/source/control_task.c:264`.

5. Capa PHY extensible y bien separada.
- Seleccion de PHY por tabla y API RF con function pointers en `examples-catsniffer/sniffer_fw_cc1252P_7/source/phy/phy_manager.c:77`.
- Modelo de tablas para PROPRIETARY, 15.4g, 802.15.4, BLE5, WBMS en `examples-catsniffer/sniffer_fw_cc1252P_7/source/phy/phy_tables.h:69`.

6. Link-layer pluggable.
- Tabla LL con `BLE/WBMS/DEFAULT` en `examples-catsniffer/sniffer_fw_cc1252P_7/source/link_layer/ll_manager.c:48`.
- Cambio de LL segun PHY en `examples-catsniffer/sniffer_fw_cc1252P_7/source/phy/phy_manager.c:117`.

7. Detalles de estabilidad de radio ya resueltos.
- Ajuste XOSC para CC1352P antes de START en `examples-catsniffer/sniffer_fw_cc1252P_7/source/control_task.c:216`.
- Manejo de overflow y reinicio RX en `examples-catsniffer/sniffer_fw_cc1252P_7/source/radio_if.c:144`.
- Cola RF circular y saneo especial para 15.4g en `examples-catsniffer/sniffer_fw_cc1252P_7/source/radio_if_dataqueue.c:57`.

## 3. Lo que NO conviene copiar tal cual

1. Protocolo host legacy (SOF/EOF + FCS suma 8-bit).
- Delimitadores `0x5340/0x4540` en `examples-catsniffer/sniffer_fw_cc1252P_7/source/general_packet.h:50`.
- FCS simple por suma en `examples-catsniffer/sniffer_fw_cc1252P_7/source/command_handler.c:204`.

2. Tu plan y codigo actual ya van por COBS + CRC16.
- COBS/CRC en C: `firmware/cc1352/src/protocol.c:12`.
- COBS/CRC en Python: `python/feralrf/protocol.py:46`.
- Plan Maestro define COBS: `PLAN_MAESTRO.md:98`.

Conclusion: mantener COBS+CRC16 y no migrar a formato legacy.

## 4. Gap real contra FeralRF hoy

1. CC1352 aun esta en blink.
- `firmware/cc1352/src/main.c:43` solo inicializa GPIO y parpadea LED.

2. Faltan piezas de Fase 1 en firmware (UART/dispatcher/radio tasking).
- El estado de sesion lo marca pendiente en `SESSION_STATE.md:39`.

3. Riesgo funcional en API Python actual (importante).
- `Radio._send_command(cmd, payload)` ya envia `cmd` en header de frame: `python/feralrf/radio.py:107`.
- Varios builders incluyen de nuevo el comando dentro del payload, por ejemplo `set_channel` en `python/feralrf/commands.py:19` y `set_phy` en `python/feralrf/commands.py:29`.
- Esto duplicara el CMD en wire-format y rompra compatibilidad con firmware cuando el parser este completo.

## 5. Recomendaciones para nuestro proyecto (prioridad)

1. Adoptar arquitectura de tareas del ejemplo, manteniendo protocolo propio.
- Implementar en CC1352: `control_task`, `data_task`, `host_if_task`, `packet_queue` (modelo de `sniffer_fw_cc1252P_7`).

2. Implementar UART robusta a 3 Mbps con exclusiones y timeout.
- Base tecnica ya validada por el ejemplo en `examples-catsniffer/sniffer_fw_cc1252P_7/source/host_if.c:63`.

3. Implementar RF RX queue circular + eventos de overflow.
- Copiar patron de `radio_if_dataqueue` y `data_task` para no perder paquetes bajo carga.

4. Crear PHY manager tabular desde el inicio (aunque MVP use 1-2 PHY).
- Evita refactor grande cuando agreguemos Zigbee/Sub-1GHz.

5. Corregir primero el contrato Python de payloads.
- Builders deben devolver solo payload (sin CMD), porque CMD ya va en header de `build_frame`.

6. Mantener CCFG actual (ya validado en hardware) y estilo solicitado.
- Ya esta en formato estilo `SET_CCFG_BL_CONFIG_BOOTLOADER_ENABLE` en `firmware/cc1352/ccfg.c:19`.

## 6. Propuesta de siguiente paso inmediato

1. Corregir `python/feralrf/commands.py` para quitar byte de CMD dentro de payload.
2. En paralelo, crear en CC1352 un `command_processor` minimo con 3 comandos:
- `GET_INFO`
- `SET_PHY`
- `RX_START/RX_STOP` (aunque sea stub al inicio)
3. Integrar UART RX/TX con framing COBS+CRC y ACK/ERROR.
4. Probar loop real host->CC1352 con script Python antes de meter BLE LL completo.

## 7. Referencias clave revisadas

- `examples-catsniffer/sniffer_fw_cc1252P_7/source/packet_sniffer_fw.c:61`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/host_if.c:73`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/data_task.c:76`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/packet_queue.c:47`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/control_task.c:55`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/radio_if.c:144`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/radio_if_dataqueue.c:57`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/phy/phy_manager.c:77`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/link_layer/ll_manager.c:48`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/general_packet.h:50`
- `examples-catsniffer/sniffer_fw_cc1252P_7/source/command_handler.c:204`
- `firmware/cc1352/src/main.c:43`
- `firmware/cc1352/src/protocol.c:12`
- `python/feralrf/protocol.py:46`
- `python/feralrf/commands.py:19`
- `python/feralrf/radio.py:107`
- `SESSION_STATE.md:39`
- `firmware/cc1352/ccfg.c:19`
