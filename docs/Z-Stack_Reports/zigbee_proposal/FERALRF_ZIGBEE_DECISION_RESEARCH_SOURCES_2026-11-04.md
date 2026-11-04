# Registro complementario de fuentes — decisiones pendientes Zigbee/FeralRF

**Fecha de consulta web:** 2026-11-04  
**Naturaleza:** investigación documental; no es evidencia experimental.  
**Baseline FeralRF reportado por auditoría:** `59816be3460884481f2827eacb630d54cfff6ae8`; SimpleLink `8.30.01.01` submódulo `5b31d0a4903351e544546e23ef3330eaa4291ceb`. No se comprobó localmente el repositorio Windows.

La columna «fuente primaria» distingue normas, documentación oficial y código del testimonio de auditoría. **Las páginas de las normas pueden requerir licencia/acceso y no se redistribuyen aquí.** No copiar la especificación al repositorio o Vault; citar documento, sección y versión.

## A. Documentación interna utilizada (evidencia secundaria del checkout)

| ID | Ruta relativa desde FeralRF | Qué aporta | Límite |
|---|---|---|---|
| INT-01 | `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_PROPOSAL.md`, revisión 1.1, §§6–18 | Contratos `CaptureContext`, R-ID-01, subsets, planes de pruebas, allowlist vacía, security gates | Propuesta, no implementación/validación |
| INT-02 | `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_ARCHITECTURE_REVIEW.md`, 2026-11-04, §§4–17 | Recorrido RX, revisión del modelo, criterio TX accepted ≠ completed | Auditoría estática por Codex |
| INT-03 | `docs/Z-Stack_Reports/FERALRF_NATIVE_ZIGBEE_INTEGRATION_SOURCES.md`, revisión 1.1, IDs `FW-*`, `SDK-*`, `TI-*`, `DISC-*` | Rutas, símbolos, versiones, discrepancias | No reemplaza acceso directo al checkout |
| INT-04 | `docs/Z-Stack_Reports/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md`, §§ de licencias/decoders | Posibilidades de reuso, dependencia OSAL y licencias | No es dictamen legal |
| INT-05 | `docs/FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md` | Baseline RF/Python, contrato y colas | Puede ser histórico frente a HEAD futuro |

Rutas del checkout a comprobar antes de copiar código, con el SDK fijado: `source/ti/zstack/stack/zcl/zcl.c` (cabecera de licencia y `zclParseHdr()`); `source/ti/zstack/stack/zdo/zd_object.c` (cabecera de licencia, `ZDO_Parse*`); `source/ti/zstack/stack/nwk/nwk_util.h`, `aps.h` (cabeceras restrictivas); `source/ti/zstack/stack/nwk/aps_mede.h`, `aps_util.h`; `python/pyproject.toml` y las licencias del repositorio raíz.

## B. Normas y documentos técnicos primarios

| ID | Documento, edición, sección | URL | Evidencia y cautela |
|---|---|---|---|
| NORM-01 | *Zigbee Specification*, doc. `05-3474-22`, **Revision 22 1.0**, **19-abr-2017**, §§1.5.1 [B1], 2.2 (APS), 2.4 (ZDP), 3.3 (NWK), **Annex D §§D.1.1–D.1.2** | https://zigbeealliance.org/wp-content/uploads/2021/10/docs-05-3474-22-0csg-zigbee-specification-1.pdf | El texto web indexado señala IEEE **802.15.4-2015** como norma MAC/PHY asumida por R22. Su descarga directa puede redirigir; conservar una copia obtenida conforme a términos antes de declarar verificación local completa. |
| NORM-02 | *Zigbee Cluster Library Specification*, doc. `07-5123-07`, **Revision 7**, **feb-2018**; capítulo 2 §§2.4, 2.5, 2.6; tabla 2-3; §2.5.5.1.2, §§2.6.2.11–2.6.2.12 | https://www.zigbeealliance.org/wp-content/uploads/2021/10/07-5123-07-ZigbeeClusterLibrary_Revision_7-1.pdf | PDF oficial indexado y portada inspeccionada: IDs de comandos foundation; regla `Write Attributes Response` y prefijo `0xFF` inválido para strings cortas; sujeto a copyright CSA. |
| NORM-03 | IEEE **802.15.4-2015** (IEEE SA; publicado 22-abr-2016), catálogo oficial | https://standards.ieee.org/ieee/802.15.4/5788/ | Confirma edición y condición de superseded; el texto completo requiere acceso legal. No sustituir automáticamente por 2024. |
| NORM-04 | CSA *Alliance Specification Downloads*: Core R22.1/R23.x y ZCL R7/R8 | https://csa-iot.org/developer-resource/specifications-download-request/ | R22 y R22.1 no son el mismo documento; catálogo/download sujeto a licencia. |
| NORM-05 | *ZigBee Specification* `05-3474-21`, R21, 05-ago-2015, tabla 2.44 (contexto histórico) | https://zigbeealliance.org/wp-content/uploads/2019/11/docs-05-3474-21-0csg-zigbee-specification.pdf | Identifica `0x0104` Common (HA). No es sustituto normativo de R22. |
| NORM-06 | *Lighting & Occupancy Device Specification* doc. `15-0014-05`, **24-feb-2016**, §5 | https://www.zigbeealliance.org/wp-content/uploads/2019/11/docs-15-0014-05-0plo-Lighting-OccupancyDevice-Specification-V1.0.pdf | Evidencia primaria histórica de uso del Profile ID `0x0104` en dispositivos de iluminación/ocupación; sujeto a términos. |
| NORM-07 | TI SimpleLink Academy *Zigbee ZCL: Keepalive/Poll Control Cluster and Groups/Scenes*, documentos indicados | https://dev.ti.com/tirex/content/simplelink_academy_cc13xx_cc26xxsdk_5_30_00_00/modules/zigbee/zigbee_06_zcl/zigbee_06_zcl.html | TI identifica explícitamente docs `05-3474-22`, `07-5123-07` y `15-0014-05`. Es índice técnico, no norma. |
| NORM-08 | NXP *Application level concepts — Application profiles* (documentación fabricante, revisiones Zigbee 3.0) | https://mcuxpresso.nxp.com/mcuxsdk/25.09.00/html/middleware/wireless/zigbee/Docs/JNUG3130/topics/application_level_concepts.html | Confirma significado/aplicación de `0x0104` como perfil histórico HA/ZLL/ZLO, no prueba por sí sola todas las reglas R22. |
| NORM-09 | Silicon Labs *Zigbee Application Framework Developer Guide* §Profile ID | https://docs.silabs.com/zigbee/9.0.0/zigbee-app-framework-dev-guide-sdk-7x/05-zcl-concepts-and-definitions | Fabricante independiente indica `0x0104` como ID de aplicación Zigbee 3.0; comparador documental secundario. |
| NORM-10 | TI *Z-Stack 8.30 Release Notes* | https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/release_notes_zigbee_8_30_00.html | Declara Zigbee PRO 2017 R22 y ZCL7. No prueba existencia de decoder en FeralRF. |
| NORM-11 | TI *Z-Stack Overview*, SDK 7.40, sección security (versión distinta; corroboración) | https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/7.41.00.17/exports/docs/zigbee/html/zigbee/z-stack-overview.html | Declara NWK security obligatoria en Zigbee 3.0/Z-Stack; no extrapolar particularidades de versión sin comprobar SDK 8.30. |
| NORM-12 | Silicon Labs *Standard Security*, documentación Zigbee | https://docs.silabs.com/zigbee/latest/zigbee-security/03-standard-security | NWK protege tráfico de red habitual; implica que el parser sin claves no verá normalmente ZDO/ZCL en claro. |

## C. Licencias, compatibilidad y políticas oficiales

| ID | Fuente | URL | Conclusión limitada |
|---|---|---|---|
| LEG-01 | **TI SimpleLink 8.30.01.01 manifest**, §TI Z-Stack table, §TI Z-Stack Licenses | https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html | El propio manifest distingue **TI Text File License** (binary/limited source) y **BSD-3-Clause** (source). **No hay licencia libre homogénea del repositorio Z-Stack**. |
| LEG-02 | Mismo manifest, *TI Text File License* de Z-Stack | misma URL LEG-01 | El texto contempla derechos condicionados a **TI Devices**, conservación de avisos, prohibición de reverse engineering de binarios; restricciones para derivados/source/object. No es una licencia permisiva general para host Python. |
| LEG-03 | Mismo manifest, *TI Z-Stack BSD-3-Clause* | misma URL LEG-01 | Permite uso/modificación/redistribución en las condiciones de licencia (notices, disclaimer y no endorsements), sólo para los archivos que efectivamente tengan esa licencia. |
| LEG-04 | CSA *Terms and Conditions* (Specification/license, propiedad intelectual, no sublicencia) | https://csa-iot.org/terms-conditions/ | Ver/descargar la norma para uso interno según condiciones; **no** permiso para publicarla/adaptarla como documento; patentes/otras IP y membresía separadas. |
| LEG-05 | CSA *Specification Downloads*, declaración de copyright | https://csa-iot.org/developer-resource/specifications-download-request/ | Descarga sin cargo **no** significa dominio público; no redistribuir PDFs ni copiar tablas largas al repositorio. |
| LEG-06 | CSA *Trademark/Logo Usage Guidelines* (uso de Zigbee Certified) | https://csa-iot.org/wp-content/uploads/2021/12/CSA-Trademark-and-Logo-Usage-Guidelines-05.06.2021.pdf | No anunciar Zigbee Certified ni usar logos sin certificación/autorización. |
| LEG-07 | FSF *Various Licenses* / GPL compatibility | https://www.gnu.org/licenses/license-list.html | BSD-3-Clause es licencia permisiva compatible GPL; **GPLv2-only y GPLv3 no son compatibles para combinar código** (según orientación FSF). |
| LEG-08 | FSF *GPL FAQ* `#v2v3Compatibility` | https://www.gnu.org/licenses/gpl-faq.en.html#v2v3Compatibility | Diferenciar uso de un comparador por proceso separado de incorporar su código; no hay inmunidad automática. |
| LEG-09 | Scapy upstream `pyproject.toml` | https://github.com/secdev/scapy/blob/master/pyproject.toml | Declara **GPL-2.0-only**; no importarlo automáticamente en un módulo de FeralRF GPL-3.0 sin análisis especializado. |
| LEG-10 | FeralRF `python/pyproject.toml:5-11` (según INT-03, `FW-H07`) | checkout local, sin URL verificada | Auditoría declara `GPL-3.0` para el paquete Python. Revisar LICENSE raíz, otros módulos y atribuciones antes de distribuir. |

## D. Limitaciones del presente registro

- **No hubo inspección directa** del submódulo de TI en el equipo Windows; conclusiones de cabeceras de archivo provienen de las auditorías existentes. El manifest oficial sí se consultó directamente.
- El PDF R22 está indexado por búsqueda web y contiene texto pertinente con la referencia explícita IEEE 802.15.4-2015, pero la URL histórica presentó redirecciones al intentar capturar una página. Para reproducibilidad, obtener una copia lícita, verificar portada, sección D.1 y referencias B1, conservar hash y versión en el registro privado autorizado.
- El PDF ZCL7 se indexó y se inspeccionó su portada; no se verificaron exhaustivamente todos los esquemas de los seis mensajes ZDO ni todos los comandos ZCL.
- **Ninguna** fuente aquí demuestra ejecución de un decoder Zigbee en CatSniffer.
- **Esta investigación no sustituye asesoría legal**, especialmente para redistribución pública, patentes, marcas o incorporación de código GPL incompatible.
