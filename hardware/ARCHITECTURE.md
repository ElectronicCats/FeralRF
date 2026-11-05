# Arquitectura CatSniffer relevante para FeralRF

## Procesadores y componentes de radio

El PCB de los tags revisados contiene un RP2040, un MCU de radio CC1352P, un switch externo de antena y rutas de matching RF. La placa tambien incluye un SX1262 y su propia ruta de conmutacion RF. Estos componentes tienen responsabilidades distintas.

```text
PC / USB
   |
RP2040 bridge -- UART TXD/RXD -- CC1352 con FeralRF
   |                                  |
   | GPIO8/9/10: CTF1/2/3              +-- RF Core / salidas RF del CC1352
   |                                  |
   +------------------------------ switch externo / red RF -- antena
   |
   +-- interfaz y ruta RF separadas del SX1262 (declaradas en el diseno)
```

Es un resumen funcional, no una netlist completa. El CC1352 ejecuta el firmware de radio FeralRF. El RP2040 esta conectado a CTF1-3 y participa en el puente con el host. El firmware RP2040 no esta en el source FeralRF autorizado; por ello, su comportamiento y su relacion con comandos del host no se establecen aqui. El SX1262 es un componente separado del diseno; este documento no atribuye su comportamiento durante ejecucion a la aplicacion CC1352 de FeralRF.

## Rutas de control

Hay dos mecanismos de control distintos:

1. El callback RF Driver de FeralRF configura `CONFIG_RF_24GHZ` (DIO28), `CONFIG_RF_HIGH_PA` (DIO29) y `CONFIG_RF_SUB1GHZ` (DIO30) durante RF setup y power-down. Son salidas GPIO o RF Core GPO administradas por el RF Driver.
2. Los PCB de los tags revisados conectan RP2040 GPIO8/9/10 con `/CTF1`, `/CTF2`, `/CTF3`, que terminan en U2 `CRF1`, `CRF2`, `CRF3`. En esos disenos, los pads CC1352 DIO28-30 no se conectan a U2.

El source estatico establece las asignaciones configuradas y el PCB establece los nets declarados. No demuestra que una imagen RP2040 concreta accione CTF correctamente ni que el mapeo entre puertos RF y bandas sea igual en cada placa ensamblada. El diseno SKY13317 posterior al tag tambien intercambia el mapeo CTF2/CTF3 a RF2/RF3.

## Por que una operacion RF no valida la ruta de antena

Una llamada RF API o el ACK de un comando demuestra ejecucion de software o aceptacion del comando. Un reporte de payload recibido por un peer aporta evidencia de recepcion bajo las condiciones reportadas, pero sin una placa identificada, hashes de firmware y estado medido del switch no demuestra que ruta externa se selecciono. El RF Core del CC1352 puede configurarse para un PHY mientras la ruta de antena controlada por separado por el RP2040 sigue siendo desconocida. La validacion fisica de la ruta requiere identidad de placa y evidencia electrica/RF.

## Limite de las fuentes

Los tags oficiales declaran `CC1352P1F3RGZT`; FeralRF selecciona `CC1352P7` por defecto. Un commit de esquematico upstream posterior cambia el MCU indicado a P7, pero el esquematico no prueba el componente ensamblado. Ver [HARDWARE_REVISIONS.md](HARDWARE_REVISIONS.md), [PINOUT.md](PINOUT.md) y [COMPATIBILITY.md](COMPATIBILITY.md).
