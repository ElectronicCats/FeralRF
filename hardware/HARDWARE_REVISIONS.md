# Revisiones de hardware CatSniffer y cambios upstream posteriores

## Identidades inmutables de las releases

| Release | Commit del tag | Blob del esquemático | Blob del PCB | MCU y switch declarados por el tag |
|---|---|---|---|---|
| v3.1 | `5b99d984933a8b8c36e789853c08d3a2a785bed4` | `a1d26bcb46e7467ab8de59d5eb1c067a1314a9a2` | `e66844eeedfc5e480cddde65fbe1cd0f20ef31ac` | `CC1352P1F3RGZT`; `RFSW8006QTR7` |
| v3.2 | `fbcccd86e84e3fa853bcae02c5938db822e76ac4` | `a1d26bcb46e7467ab8de59d5eb1c067a1314a9a2` | `1ce091e0af5ae7c4399a8f272e2529f1edcd53d3` | `CC1352P1F3RGZT`; `RFSW8006QTR7` |
| v3.3 Git tag | `6da5050fefde59138fc11d9e2ecf7c0de36f3a22` | `4ac3e0b953b55a782a288b6bd6c98e17a3bc9eaf` | `9c160fa8541204def3af55f71d6123e85feebe82` | `CC1352P1F3RGZT`; `RFSW8006QTR7` |

Enlaces: [hardware v3.1](https://github.com/ElectronicCats/CatSniffer/tree/5b99d984933a8b8c36e789853c08d3a2a785bed4/hardware), [hardware v3.2](https://github.com/ElectronicCats/CatSniffer/tree/fbcccd86e84e3fa853bcae02c5938db822e76ac4/hardware), [hardware v3.3](https://github.com/ElectronicCats/CatSniffer/tree/6da5050fefde59138fc11d9e2ecf7c0de36f3a22/hardware).

Los esquemáticos v3.1 y v3.2 son el mismo Git blob; sus PCB tienen blobs distintos. El esquemático y PCB v3.3 tienen blobs distintos a los anteriores. La revisión de pads/nets encontró el mismo mapeo UART, CTF al RP2040 y DIO28-30 del CC sin conexión en los tres tags. No se hizo una comparación de netlist completa normalizada; por eso no se declaran los tags eléctricamente equivalentes en todos sus detalles.

## Comparación enfocada de revisiones

| Interfaz | v3.1 | v3.2 | Tag v3.3 | Hallazgo e impacto para firmware |
|---|---|---|---|---|
| MCU CC1352 | `CC1352P1F3RGZT` en U8 | Igual | Igual | FeralRF usa P7 por defecto. Los archivos de diseño no identifican el MCU instalado. |
| UART | RP GPIO0/1 a CC DIO12/13 | Nets críticos iguales | Nets críticos iguales | Compatible como UART de dos hilos en el código fuente; FeralRF deshabilita flow control. |
| Nets relacionados con RTS/CTS | DIO14 NC; DIO15 `/BOOT`; RP GPIO2 `/BOOT` | Nets críticos iguales | Nets críticos iguales | El mapeo anterior de PINOUT no está respaldado por el PCB. |
| Reset | CC `RESET_N` en `/RESET_CC`; pads RP GPIO3 y GPIO15 asignados al mismo net | Nets críticos iguales | Nets críticos iguales | La causa del net compartido y el comportamiento del firmware siguen sin resolverse. |
| Controles del switch RF | RP GPIO8/9/10 -> CTF1/2/3 -> U2 CRF1/2/3 | Nets críticos iguales | Nets críticos iguales | El switch está asignado eléctricamente al RP2040, no a CC DIO28-30. |
| Switch | U2 `RFSW8006QTR7` | Igual | Igual | El tag v3.3 no incluye el reemplazo SKY13317. |
| PCB general | Blob PCB v3.1 | Blob PCB distinto | Blob PCB distinto | Hay cambios de PCB/serialización; no se declara equivalencia eléctrica total. |

El esquemático de release nombra los pads RF del CC1352 1-4: par diferencial de 2.4 GHz y par diferencial Sub-1 GHz. Esos nets pasan por redes RF de la placa. Esta tabla no infiere el ruteo completo de puertos a bandas ni el rendimiento medido de antena.

## Historial upstream posterior al tag

Estos commits descienden de v3.3; no pertenecen al contenido del tag:

| Commit | Padre / fecha | Cambio | Relevancia para firmware |
|---|---|---|---|
| `08e31d6ef21c7eb41a7c61fc1986f99114e39233` | Tag v3.3 / 2026-02-12 | El valor de MCU en el esquemático cambia a `CC1352P74T0RGZR`; en este commit el blob del PCB sigue siendo el de v3.3 | Indica una intención posterior de diseño, no población física ni una release etiquetada nueva |
| `17d1215184ff330f2e68fad4630c3b0bb272a951` | 08e31d6 / 2026-09-14 | Reemplaza RFSW8006Q por `SKY13317-373LF`; modifica PCB y esquemático; agrega símbolo y footprint | Se reconectan controles: CTF1->V1/RF1, CTF3->V2/RF2, CTF2->V3/RF3. Revisar firmware RP2040 con este diseño |
| `234b83cb6e98f041f414e10eda234da6af230dcf` | 17d1215 / 2026-09-14 | Corrige texto del switch | No se infiere el comportamiento de control por un cambio de texto |
| `a06e18568c7738d63ae83c8261b39f512cc815a7` | 234b83c / 2026-09-15 | Tres capacitores del switch cambian de 100 pF a 47 pF | Cambio de componentes de red RF; no cambia el mapeo GPIO |

El switch Qorvo anterior selecciona RF1, RF2 o RF3 con control one-hot CRF1/2/3. La tabla Skyworks también usa control one-hot, con HIGH de 1.8-5 V y LOW de 0-0.25 V; las otras combinaciones son indefinidas. Ver la [página de producto Qorvo](https://www.qorvo.com/products/p/RFSW8006Q) y el [datasheet Skyworks](https://www.skyworksinc.com/-/media/SkyWorks/Documents/Products/301-400/SKY13317-373LF_200914L.pdf). El mapeo cambiado demuestra un cambio de pin a puerto, pero no un cambio de selección lógica de banda: el ruteo completo y el comportamiento del firmware RP2040 siguen sin verificar.

## Discrepancia de la release v3.3

La descripción oficial de release reportada dice `Changed switch for antenna`. El tag inmutable local v3.3 todavía contiene `RFSW8006QTR7`; el reemplazo aparece en el commit posterior `17d1215184ff330f2e68fad4630c3b0bb272a951`. Los assets de la release v3.3 se registran por separado en [HARDWARE_SOURCES.md](HARDWARE_SOURCES.md) y no se han reconciliado con KiCad ni con unidades físicas. Mantener separados el contenido del tag, el claim de release, los commits posteriores y la población de productos fabricados. No atribuir SKY13317 al tag v3.3 ni identificar el switch de una unidad fabricada con estas fuentes.

## Inventario físico

Inventario informado por el usuario: tres unidades CatSniffer v3.1, una v3.2 y ninguna v3.3. Esto no verifica por separado la población de MCU o switch. Los reportes experimentales históricos no identifican cada unidad con suficiente precisión para atribuir sus resultados a una revisión. No se afirma validación física aquí.
