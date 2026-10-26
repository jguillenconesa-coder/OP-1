# Estudio del OP-1 original, firmware 246

Análisis estático del archivo `op1_246.op1` proporcionado por el usuario. Se usó **REA 6.8.0 por CLI**, su proveedor **Unblob 26.6.4** y las operaciones SQLite de REA con **Node 24.22.1**. No se registró un servidor MCP ni se modificó la configuración del cliente.

## Identidad y resultados observados

- Entrada: **13.039.128 bytes**, SHA256 `c5315218f825f143b415ca554516541898abee843d3a236df0b54c04e1fb13a9`.
- Cuatro bytes iniciales contienen CRC32 little endian `cc08445c`. El CRC del resto coincide. Esto comprueba integridad del contenedor, no firma/autenticidad del fabricante.
- Desde offset 4: LZMA Alone que produce un TAR de **26.368.000 bytes**.
- **107 archivos regulares**: 40 RAW, cuatro SQLite, 61 SVG y dos LDR. Los 107 contenidos extraídos por REA coinciden por SHA256 con la lectura independiente del script del proyecto.
- `OP1_vdk.ldr` contiene la cadena `Rev. 00246; 2022/11/09 16:17:00; 8.1.11.1`. Una cadena identifica lo observado; no certifica la ejecución de ese código.

[Inventario completo de metadatos](firmware/op1-246-inventory.json) · [Resumen de extracción REA](firmware/rea-op1-246-summary.json) · [Índice de evidencia SQLite](firmware/rea-sqlite-evidence-index.json).

## Bases de datos

`content/op1_factory.db` contiene:

| Estructura | Observado |
| --- | --- |
| `synth_presets` | 169 filas: 16 para cada uno de los diez tipos de síntesis y nueve sampler. |
| `drum_presets` | 24 filas: 14 tipo drum con muestras y diez DBox. |
| `synth_types` | cluster, digital, drwave, fm, phase, pulse, dna, string, dsynth, voltage. Sampler aparece en los presets, aunque no en esta tabla de diez motores. |
| `fx_types` | grid, punch, delay, phone, spring, cwo, nitro. |
| `lfo_types` | tremolo, value, random, element, midi, bend, crank. |
| `seq_types` | pattern, endless, tombola, finger, sketch, arpeggio. |

Los nueve sampler se llaman designadrum, dist a log, knorr, leader, m-glide, piano mid, suitcase, punchsub y acidrone. Los 14 recursos de batería se llaman neoprene, gallium, electret, bronze, awa beef, allspice, tantalum, smartphone, red maple, polyimide, ghee, cork, acetate y peek. Los nombres completos del catálogo están en el inventario; no se publican los arrays numéricos de patches.

`content/op1.db` contiene ocho slots synth, ocho drum, dos estados Endless, dos drumseq y dos okseq. Los estados Endless tienen campos `steps`, `playmode`, `timing`, `swing`, `euclidean` y `euclidean_offset`; sus dos listas observadas contienen 64 y cuatro elementos. Los patches drum contienen arrays de **24** posiciones/inicios/finales, volumen, pitch y reproducción. El mezclador guarda cuatro ganancias y cuatro panoramas. `content/tape.db` separa clips por canal/inicio/final y estado de cinta.

Los patches sampler incluyen `base_freq`: por ejemplo, suitcase declara aproximadamente 523,251 Hz. Este dato corresponde a MIDI 72; demuestra por qué asumir siempre MIDI 60 produciría una octava incorrecta. No se deduce de ello la escala de todos los knobs, la respuesta de envolventes ni el algoritmo de efectos.

## Qué cambia en Pocket One

0.4.0 incorpora direcciones de patrón, arpegiador de notas mantenidas y control de longitud/rate/gate/swing. La presencia de campos euclídeos en Endless motivó también una máscara de pulsos y rotación. **Son implementaciones propias**, probadas con fixtures independientes; no son una recuperación del reproductor original. El swing de Pocket One usa 0–45 % de retraso, y no interpreta directamente el valor 50 de la base del OP-1.

Este inventario permite priorizar con hechos:

1. Compatibilidad de sampler con frecuencia raíz original y límites de muestra documentados.
2. Soporte de 24 pads y metadatos por pad; los kits actuales de Pocket One tienen ocho.
3. Mezcla con panoramas por pista y más modos de secuenciación.
4. Investigar por separado cada motor/efecto y contrastarlo con audio de la máquina.

**Todavía no establecido:** instrucciones/funciones DSP de los LDR, escalas físicas de todos los parámetros, sample rate de cada RAW, mapeo completo de efectos/LFO, equivalencia auditiva o firmware ejecutable en un ordenador. No se ha abierto una sesión de descompilación nativa: esta ejecución no dispone de un proveedor validado para estos cargadores. La extracción y los nombres por sí solos no permiten reconstruir FM, Nitro u otros motores fielmente.

## Evidencia y límites de REA

Extracción final: `ev_5401d7e519d36a2a8cf2ab580d38867748a030c32ec665f5b0e0235cf7f24a64`. REA declara cobertura **partial** porque el CRC inicial de cuatro bytes queda como bloque desconocido. El lector independiente identifica/verifica ese CRC. La extracción final no tiene diagnósticos de fallo; eso no significa cobertura del comportamiento DSP. Los resúmenes publicados son índices derivados, no bundles REA completos. Los registros completos y recursos extraídos permanecen en el área de análisis local.

Hubo intentos previos con dependencia 7z ausente y con el ejecutable dinámico 7z sin su módulo disponible dentro del sandbox. Se resolvió usando el ejecutable autónomo oficial **7za**, expuesto con el nombre 7z en un PATH local. No se desactivó el sandbox de Unblob ni se cambiaron sus reglas. La lectura SQLite de REA se realizó sobre copias inmutables; la base de fábrica procede de la extracción REA final.

## Reproducir el inventario

Con Python 3.11 o posterior y una copia local del firmware:

```sh
python scripts/analyze-op1-firmware.py /ruta/op1_246.op1 /ruta/nuevo-informe.json
python Tests/FirmwareInventoryTests.py
```

El script comprueba CRC y límites de memoria/archivo, rechaza rutas inseguras y enlaces, inspecciona las bases en modo lectura y publica solo metadatos en un archivo nuevo. No altera la entrada, extrae recursos a carpetas públicas ni crea un firmware modificado. Las pruebas usan un contenedor sintético propio.

Para REA se necesitan Node compatible, REA 6.8.0, Unblob 26.6.4, `ps`, `prlimit` y sus extractores. Con herramientas ya instaladas, configura rutas locales y elige un destino que no exista:

```sh
export REA_UNBLOB_COMMAND=/ruta/absoluta/unblob
rea extract-firmware /ruta/op1_246.op1 /ruta/nueva-extraccion --format json
rea inspect-sqlite-database /ruta/extraida/content/op1_factory.db --schema-scope database --format json
```

No se incluye el firmware, sus RAW, gráficos o bases completas en los instaladores ni en los archivos fuente publicados. Los motores y recursos propios de Pocket One mantienen sus licencias anteriores.

Referencias: [REA y su guía de firmware](https://github.com/morluto/rea/blob/main/docs/firmware-analysis.md), [op1repacker: documentación del contenedor CRC/LZMA/TAR](https://github.com/op1hacks/op1repacker), manual OP-1 v2.2 proporcionado por el usuario. Las herramientas se usaron para investigar; no se incorporó su código al DSP.

## Continuación del estudio DSP

Después de este inventario se ha preparado un mapa de cabeceras LDR y desensamblado Blackfin independiente. Se han localizado referencias a unidades FM/Pd, sin recuperar todavía sus algoritmos. [Avances, direcciones y límites](dsp-research.md).
