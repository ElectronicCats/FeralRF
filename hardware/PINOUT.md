# Pinout eléctrico CatSniffer para FeralRF

## Alcance y evidencia

Las conexiones siguientes se comprobaron contra los pads y nets de PCB en los tags oficiales CatSniffer v3.1, v3.2 y v3.3. El PCB local de FeralRF es idéntico por Git blob al PCB v3.1. Las señales indicadas tienen el mismo mapeo en los tres tags revisados. Esto describe archivos de diseño; no confirma el ensamblaje ni el comportamiento de una placa física. Ver [HARDWARE_REVISIONS.md](HARDWARE_REVISIONS.md) y [HARDWARE_SOURCES.md](HARDWARE_SOURCES.md).

## UART: RP2040 a CC1352

| Dirección de la señal | Lado RP2040 | Lado CC1352 | Net de PCB |
|---|---|---|---|
| TX del RP2040 a RX del CC1352 | GPIO0, UART TX | DIO12, UART RX | `/TXD` |
| TX del CC1352 a RX del RP2040 | GPIO1, UART RX | DIO13, UART TX | `/RXD` |

Los nombres de señal se expresan desde el punto de vista de cada procesador. FeralRF configura TX del CC1352 en DIO13 y RX en DIO12, a 921600 baud. `UART_HW_FLOW_CONTROL` vale `0`; por ello `HostIF_init()` no configura CTS ni RTS. Referencias: [config.h](../firmware/cc1352/include/config.h), [host_if.c:HostIF_init](../firmware/cc1352/src/host_if.c).

## RTS/CTS no están disponibles como cables de control de flujo documentados

| Pin CC1352 | Conexión declarada en los PCB revisados | Consecuencia |
|---|---|---|
| DIO14 | Sin conexión | No hay ruta de placa para la señal de control de flujo documentada anteriormente |
| DIO15 | `/BOOT`, también conectado a RP2040 GPIO2 | Es un net de boot en el diseño, no un cable CTS/RTS verificado |

No habilitar hardware flow control con el mapeo anterior del PINOUT. FeralRF lo mantiene deshabilitado. El firmware RP2040 no forma parte de las fuentes autorizadas para esta revisión, así que el manejo de `/BOOT` por ese firmware queda sin verificar.

## Nets de reset y boot

| Señal/net | Pads/nets declarados por los tags revisados | Lo que se conoce |
|---|---|---|
| `/RESET_CC` | CC1352 `RESET_N`; RP2040 GPIO3 y GPIO15 asignados al mismo net en los datos de pads del PCB | La asignación pad/net está confirmada en los archivos de diseño. La razón de compartir el net, su relación con el esquemático y la configuración del firmware siguen sin resolverse. No inferir un comportamiento de reset seguro a partir de este mapeo. |
| `/BOOT` | CC1352 DIO15 y RP2040 GPIO2 | La conexión está declarada; el comportamiento de entrada a boot depende del firmware RP2040/CC1352 y no se verificó aquí. |

La afirmación anterior de que solo RP2040 GPIO15 controla reset es incompleta. La tabla anterior que asignaba GPIO3 a CTS contradice los nets del PCB. Antes de depender de cualquiera de esos pines, resolver el net compartido GPIO3/GPIO15 contra el esquemático, las placas ensambladas y el firmware RP2040.

## Control del switch externo de antena

| Señal | RP2040 GPIO | Pin del switch en tags v3.1/v3.2/v3.3 | Net PCB |
|---|---:|---|---|
| CTF1 | GPIO8 | U2 `CRF1` | `/CTF1` |
| CTF2 | GPIO9 | U2 `CRF2` | `/CTF2` |
| CTF3 | GPIO10 | U2 `CRF3` | `/CTF3` |

Son números GPIO del RP2040, no DIO del CC1352. Los tres tags oficiales instancian `RFSW8006QTR7` en U2. Su datasheet describe selección one-hot: CRF1 selecciona RF1, CRF2 selecciona RF2 y CRF3 selecciona RF3. Ver la [página de producto Qorvo RFSW8006Q](https://www.qorvo.com/products/p/RFSW8006Q) y una [copia del datasheet del fabricante](https://datasheet.octopart.com/RFSW8006QTR7-Qorvo-datasheet-181149857.pdf). No inferir qué puerto RF corresponde a una banda sin trazar toda la ruta y verificar la placa física.

El diseño SKY13317-373LF posterior al tag conecta CTF1 a V1/RF1, CTF3 a V2/RF2 y CTF2 a V3/RF3. Ese mapeo no pertenece al tag v3.3; ver [HARDWARE_REVISIONS.md](HARDWARE_REVISIONS.md). Los umbrales de control y estados indefinidos del reemplazo son distintos; consultar el [datasheet Skyworks](https://www.skyworksinc.com/-/media/SkyWorks/Documents/Products/301-400/SKY13317-373LF_200914L.pdf).

## GPIO del RF Driver del CC1352 en FeralRF actual

| Símbolo FeralRF | CC1352 DIO | Conexión al U2 en tags PCB revisados |
|---|---:|---|
| `CONFIG_RF_24GHZ` | DIO28 | Sin conexión |
| `CONFIG_RF_HIGH_PA` | DIO29 | Sin conexión |
| `CONFIG_RF_SUB1GHZ` | DIO30 | Sin conexión |

`rfDriverCallbackAntennaSwitching()` usa esos pines y el mux RF Core GPO durante setup y power-down. En los diseños CatSniffer revisados, el callback no controla directamente U2. Esto no demuestra que el RP2040 no pueda controlar el switch. No cambiar estas asignaciones sin definir y verificar la arquitectura de control prevista. Referencias: [ti_drivers_config.h](../firmware/cc1352/syscfg/ti_drivers_config.h), [ti_drivers_config.c](../firmware/cc1352/syscfg/ti_drivers_config.c), [main_rtos.c](../firmware/cc1352/src/main_rtos.c).

## Otras conexiones declaradas por el diseño

Los pads RF del paquete CC1352 en el PCB revisado se nombran `RF_P_2_4GHZ` (pad 1), `RF_N_2_4GHZ` (pad 2), `RF_P_SUB_1GHZ` (pad 3) y `RF_N_SUB_1GHZ` (pad 4). Esto corrige los números de pin indicados en el PINOUT anterior. Las rutas pasan por circuitería RF de la placa; los nombres de estos pads no identifican por sí mismos la ruta completa hasta la antena.

El PCB asigna RP2040 GPIO26/27/28 a nets llamados LED1/LED2/LED3. No se repite como hecho confirmado físicamente la afirmación anterior de polaridad active-low. Los nets de depuración CC1352 aparecen etiquetados cJTAG/JTAG en el PCB; consultar el esquemático de cada release para detalles de conectores en vez de asumir un pinout genérico.

## Límites de firmware y revisión

El código FeralRF revisado configura el UART y RF Driver del CC1352. No incluye el firmware RP2040 que atiende USB, boot/reset o GPIO8-10. Reportes históricos mencionan control RP2040 de CTF, pero no aportan el código fuente ni la identidad del binario instalado correspondientes. Ninguno de los mapeos anteriores demuestra niveles de voltaje, estado del switch ni operación RF exitosa en una revisión física identificada.
