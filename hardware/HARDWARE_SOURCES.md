# Registro de fuentes y artefactos de hardware

## Baseline de auditoría

La comparación técnica se hizo con objetos Git locales inmutables disponibles al momento de la revisión. CatSniffer HEAD era `6da5050fefde59138fc11d9e2ecf7c0de36f3a22`; el código fuente de FeralRF se inspeccionó en `8bc9f55c57b1a05b7f737ccabfbd64c7ca701048`, en `feature/hardware-validation`. Antes de HW-02A, FeralRF HEAD era `686a87cc4bcebedf6a3122c3ad82c554813478f0`; el único commit intermedio actualizó estos siete documentos y no modificó los archivos de firmware citados. Los working trees estaban limpios al iniciar HW-02A. El remoto `origin` de FeralRF es `https://github.com/ElectronicCats/FeralRF.git`; el upstream CatSniffer es `https://github.com/ElectronicCats/CatSniffer.git`.

Términos de evidencia usados aquí:

- `GIT_IDENTITY`: un blob/commit idéntico demuestra identidad exacta del archivo, no equivalencia completa de hardware.
- `UPSTREAM_DESIGN`: un esquemático/PCB declara una pieza o conexión; no demuestra la población física.
- `FERALRF_SOURCE`: el código fuente o la configuración revisada de FeralRF declara una asignación o comportamiento.
- `RELEASE_CLAIM`: afirmación en metadatos publicados de release, separada del contenido del tag.
- `HISTORICAL_TEST_REPORTED`: un reporte FeralRF registra una observación; no valida una placa identificada por revisión.
- `UNVERIFIED` / `REQUIRES_PHYSICAL_VALIDATION`: las fuentes disponibles no resuelven el asunto.

## Tags oficiales CatSniffer

| Tag | Commit inmutable | Directorio hardware |
|---|---|---|
| v3.1 | `5b99d984933a8b8c36e789853c08d3a2a785bed4` | [hardware en v3.1](https://github.com/ElectronicCats/CatSniffer/tree/5b99d984933a8b8c36e789853c08d3a2a785bed4/hardware) |
| v3.2 | `fbcccd86e84e3fa853bcae02c5938db822e76ac4` | [hardware en v3.2](https://github.com/ElectronicCats/CatSniffer/tree/fbcccd86e84e3fa853bcae02c5938db822e76ac4/hardware) |
| v3.3 | `6da5050fefde59138fc11d9e2ecf7c0de36f3a22` | [hardware en v3.3](https://github.com/ElectronicCats/CatSniffer/tree/6da5050fefde59138fc11d9e2ecf7c0de36f3a22/hardware) |

Los archivos relevantes en cada commit son `hardware/CatSniffer.kicad_sch`, `hardware/CatSniffer.kicad_pcb` y `hardware/CatSniffer.pdf`. No se encontró un archivo BOM independiente dentro del árbol `hardware/` de los tags inspeccionados. Esto no significa que no exista información BOM oficial: GitHub publica assets separados del árbol Git para las releases v3.1, v3.2 y v3.3.

| Release | Assets oficiales relacionados con BOM | Estado de conciliación |
|---|---|---|
| [v3.1](https://github.com/ElectronicCats/CatSniffer/releases/tag/v3.1) | [`BOM-CatSnifferV3.1.csv`](https://github.com/ElectronicCats/CatSniffer/releases/download/v3.1/BOM-CatSnifferV3.1.csv) | No conciliado con KiCad ni con unidades físicas en HW-02/HW-02A. |
| [v3.2](https://github.com/ElectronicCats/CatSniffer/releases/tag/v3.2) | [`BOM-CatSniffer-v3.2.csv`](https://github.com/ElectronicCats/CatSniffer/releases/download/v3.2/BOM-CatSniffer-v3.2.csv) y [`BOM-CatSniffer-Accesorios-v3.2.csv`](https://github.com/ElectronicCats/CatSniffer/releases/download/v3.2/BOM-CatSniffer-Accesorios-v3.2.csv) | No conciliados con KiCad ni con unidades físicas en HW-02/HW-02A. |
| [v3.3](https://github.com/ElectronicCats/CatSniffer/releases/tag/v3.3) | Asset CSV [`CatSniffer.csv`](https://github.com/ElectronicCats/CatSniffer/releases/download/v3.3/CatSniffer.csv); su nombre no basta para establecer por sí solo el alcance del contenido. | No conciliado con KiCad, commits post-tag ni unidades físicas en HW-02/HW-02A. |

La presencia de estos assets demuestra su publicación junto a una release, no la población real de una placa fabricada. Sus contenidos y su correspondencia con las fuentes KiCad permanecen pendientes de conciliación. Los números de parte que siguen se refieren a declaraciones del diseño, no a una lista de componentes ensamblados.

## Historial posterior al tag y metadatos de release

| Commit | Padre | Fecha | Contenido relevante |
|---|---|---|---|
| `08e31d6ef21c7eb41a7c61fc1986f99114e39233` | Tag v3.3 | 2026-02-12 | El valor de MCU del esquemático cambia a `CC1352P74T0RGZR`; el blob PCB sigue siendo el de v3.3 en este commit |
| `17d1215184ff330f2e68fad4630c3b0bb272a951` | 08e31d6 | 2026-09-14 | Reemplaza RFSW8006Q por SKY13317-373LF; cambia esquemático/PCB y agrega assets de librería del switch |
| `234b83cb6e98f041f414e10eda234da6af230dcf` | 17d1215 | 2026-09-14 | Corrección de texto del switch |
| `a06e18568c7738d63ae83c8261b39f512cc815a7` | 234b83c | 2026-09-15 | Tres capacitores del switch cambian de 100 pF a 47 pF |

El texto reportado de la release v3.3 dice `Changed switch for antenna`. La página y el inventario de assets oficiales se verificaron durante HW-02A; no se inspeccionó ni concilió el contenido del CSV. El código fuente local del tag v3.3 todavía declara RFSW8006Q. Estos hechos se mantienen separados y no se usan para cambiar la identidad del tag. Ver [la página de release v3.3](https://github.com/ElectronicCats/CatSniffer/releases/tag/v3.3).

## Inventario de artefactos del directorio hardware de FeralRF

Las identidades de blob son de FeralRF commit `8bc9f55c57b1a05b7f737ccabfbd64c7ca701048`. Un blob coincidente demuestra identidad exacta de archivo, no equivalencia total de revisión.

| Artefacto local | Blob local o coincidencia | Clasificacion |
|---|---|---|
| `hardware/CatSniffer.kicad_pcb` | `e66844eeedfc5e480cddde65fbe1cd0f20ef31ac`; coincide exactamente con CatSniffer v3.1 | `GIT_IDENTITY`: PCB v3.1. Su title block dice v3.2; esa etiqueta no cambia la identidad del blob. |
| `hardware/CatSniffer.kicad_sch` | `ffab96de4a2f10336ec5c0bed13b355bdf7fcc1f`; no coincide con v3.1/v3.2/v3.3 | Esquemático local sin match; title block revision 2.0 no establece una revisión de producto. |
| `hardware/CatSniffer.kicad_pro` | `31f50373f70dcb47c793c7c34be53c94303ae278`; difiere de los tres blobs de proyecto de tag | Configuración de proyecto modificada/sin match; no prueba por sí sola una revisión de hardware. |
| `hardware/CatSniffer.kicad_prl` | `e67f440c1a674bf6388c9c8a0b9db4bab5ef0868`; difiere de los tres tags | Preferencias locales del editor o caché; no es fuente de revisión. |
| `hardware/fp-info-cache` | `573541ac9702dd3969c9bc859d2b91ec1f7e6e56`; ausente en la misma ruta de los tags comparados | Artefacto generado/caché. |
| `hardware/CatSniffer.pdf` | `a5332e83f76ed42b52d29d92fe53e9e16e630518`; mismo blob exacto en v3.1/v3.2/v3.3 | Artefacto upstream compartido; no distingue estas releases. |
| `hardware/CatSniffer.step` | `fb1368e0d93e0bb7c822638f762cce28bdabd546`; difiere del blob STEP v3.2/v3.3 `d9eaa9771faa347120b31420a5ddf6a0fab89c50`; no existe en v3.1 en esa ruta | Archivo mecánico con procedencia exacta sin resolver; no permite inferencias eléctricas. |
| `hardware/CatSniffer-cache.lib`, `hardware/CatSniffer-rescue.dcm`, `hardware/CatSniffer-rescue.lib` | Respectivamente `bcbff4b8f324d9badb397552eb56d190c8a9e06c`, `5dd71cb6097831d9ffdb4ed88535d25d745b496b`, `77c8fcff8028bbb778fd5ebef7bd276e43104ca6` | Coincidencias exactas compartidas por los tres tags; `hardware/CatSniffer-cache.lib` es una librería caché de KiCad. |
| `hardware/fp-lib-table`, `hardware/sym-lib-table` | `1428691a366253380bc7f1b1d86ce4c34a34aa4f`, `e262e1793f59bb98bf1e332adb628c579fef956d` | Coincidencias exactas compartidas por los tres tags. |
| `hardware/CatSniffer.pretty/0900FM15D0039E.kicad_mod` | `2ca0ee5082ce9e4364040be2a16175f47ff3aee9`; coincide con v3.1 y difiere del blob v3.2/v3.3 `65bc59e822af59df0f2cdf5cf2a8696a2287fffe` | Footprint exacto v3.1; variante de revisión en v3.2/v3.3. |
| `hardware/CatSniffer.pretty/calderon.kicad_mod`, `hardware/CatSniffer.pretty/calderon_7.5x7.5.kicad_mod`, `hardware/CatSniffer.pretty/HOLE.kicad_mod` | `db6ef0a30c8379836e8b97ee0955f5eb56954020`, `4ab1d3b166dc31efc95fd31f18051be58c91a607`, `6ba805256ea9c2d00a45ef741f483dafa4305ce5` | Coincidencias exactas compartidas por los tres tags. |
| `hardware/CatSniffer.pretty/RFSW8006QTR7.dcm`, `hardware/CatSniffer.pretty/RFSW8006QTR7.lib`, `hardware/CatSniffer.pretty/RFSW8006QTR7.mod` | `e55b1305faddd5ba53c571e3a355cf7ef8e4609b`, `3065b6ff69eb7ee40a5b1cd912077dfd6b0f4aaa`, `88f04dfe448938d4d2e0b7c837bff9ba9614a741` | Coincidencias exactas compartidas por los tres tags; describen el switch anterior. |
| `hardware/CatSniffer.pretty/SAMTEC_SMA-J-P-H-ST-EM1.STEP`, `hardware/CatSniffer.pretty/SON50P200X200X90-9N.kicad_mod`, `hardware/CatSniffer.pretty/SW409.kicad_mod` | `0cd82630f6d853d54de04c87e61ee4e1c114762d`, `d8899b31e29193750f808c321414ab4fa07724f5`, `98f66f7e9d7643782c0a233fd1555ecd55ce66f5` | Coincidencias exactas compartidas por los tres tags. |
| `hardware/CatSniffer.pretty/TS-1088R-02026.lib`, `hardware/CatSniffer.pretty/TS-1088R-02026.mod`, `hardware/CatSniffer.pretty/TS1088R02026.kicad_mod`, `hardware/CatSniffer.pretty/TS-1088R-02526.STEP` | `8a10787cbcb75f9fde69c6d7c640d54e4916a6f6`, `47eb7b19750a419d906f577b803e7cd84a34c9ea`, `b69eb97248c8ee4c47492e2dca033390c1c3a990`, `5730614aad990577626ae2159f10843ba802cc4e` | Coincidencias exactas compartidas por los tres tags. |
| `hardware/CatSniffer.pretty/W25Q16JVUUIQ.kicad_mod` | `e721fbab6d62a037cecba77769ce48c2b2727aef` | Coincidencia exacta compartida por los tres tags. |
| `hardware/Datasheet/0900FM15D0039.pdf`, `hardware/Datasheet/jti-an100.pdf`, `hardware/Datasheet/LAUNCHXL-CC1352P-2_SCHEMATIC.pdf`, `hardware/Datasheet/SX1262MB2xAS_e499v01b_sch_layout.pdf` | `770a02ced3e0e26b473c7172f14c5644b95ae55d`, `d52b800c46690774f0f96721828564e2ddbb4d1d`, `07706cdc187ad8e30c476e5b9d0b93a4c9600814`, `795edf429559c3b0d6183f222b5b21cc012673b2` | Coincidencias exactas compartidas por los tres tags. Su presencia no demuestra que todas las piezas citadas estén ensambladas. |
| `hardware/Library/3D/0900PC15A0036E--3DModel-STEP-56544.STEP`, `hardware/Library/3D/USB4105-GF-A.stp` | `82f05d9c31a1abbe7c9e1dd776842bb9320d680a`, `3b922889c94fa71b84bd5cf7b263c9f7b2ba5131` | Coincidencias exactas compartidas por los tres tags; assets mecánicos/de librería. |

El `hardware/README.md` anterior también era idéntico a la copia del tag con blob `f5610c31aef7014883f013afa08b824f4d5b7f43`; se reemplazó intencionalmente por el índice documental FeralRF. `hardware/PINOUT.md` es específico de FeralRF y no existe en esa ruta upstream. El conjunto local de archivos de diseño eléctrico no corresponde a una release única verificada: PCB v3.1, esquemático/proyecto sin match y exportaciones compartidas o con procedencia no resuelta.

## Referencias de fabricante

- [Datasheet TI CC1352P](https://www.ti.com/lit/ds/symlink/cc1352p.pdf)
- [Datasheet TI CC1352P7](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf)
- [Página de producto Qorvo RFSW8006Q](https://www.qorvo.com/products/p/RFSW8006Q); la copia disponible del datasheet está enlazada en la referencia [PINOUT](PINOUT.md).
- [Datasheet Skyworks SKY13317-373LF](https://www.skyworksinc.com/-/media/SkyWorks/Documents/Products/301-400/SKY13317-373LF_200914L.pdf)

Estos documentos no hacen afirmaciones de umbral o capacidad de manejo eléctrico del RP2040; por ello no requieren un datasheet RP2040 para las afirmaciones incluidas.

## Referencias del código fuente de FeralRF

Revisado en FeralRF commit `8bc9f55c57b1a05b7f737ccabfbd64c7ca701048`:

- [`firmware/cc1352/include/config.h`](../firmware/cc1352/include/config.h): DIO UART, baud rate y bandera de flow control.
- [`firmware/cc1352/src/host_if.c`](../firmware/cc1352/src/host_if.c), `HostIF_init()`: mux de pines UART y rama de control de flujo.
- [`firmware/cc1352/syscfg/ti_drivers_config.h`](../firmware/cc1352/syscfg/ti_drivers_config.h): constantes GPIO `CONFIG_RF_24GHZ`, `CONFIG_RF_HIGH_PA`, `CONFIG_RF_SUB1GHZ`.
- [`firmware/cc1352/syscfg/ti_drivers_config.c`](../firmware/cc1352/syscfg/ti_drivers_config.c), `rfDriverCallbackAntennaSwitching()`: mux y comportamiento de salida en setup/power-down RF.
- [`firmware/cc1352/syscfg/ti_radio_config.c`](../firmware/cc1352/syscfg/ti_radio_config.c): configuración generada CC1352P7.
- [`firmware/cc1352/CMakeLists.txt`](../firmware/cc1352/CMakeLists.txt) y [`firmware/cc1352/linker/`](../firmware/cc1352/linker/): variantes declaradas y scripts linker.

El firmware RP2040 no está incluido en las fuentes FeralRF autorizadas. Los reportes históricos que describen comportamiento RP2040/CTF se citan solo como reportes, no como evidencia del código fuente.
