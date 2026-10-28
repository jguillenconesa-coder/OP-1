# Dr. Wave rev246: tres transformaciones de la onda de voz

Se reconstruyen tres callbacks SDRAM completos que transforman un buffer de
256 palabras de onda: **retención por grupos, lectura con extremo fijo y lectura
repetida**. El C++ coincide con **131.072 palabras** de las instrucciones
originales, más el estado completo de seis ondas, cachés, flags, configuración y
guardas en 512 llamadas independientes o continuas.

Es un bloque separado de la [generación de las 320 tablas base](drwave246-wavebank.md)
y de la [lectura de cinco osciladores](drwave246-reader.md). El posterior [render
aislado completo](drwave246-kernel.md) conecta controles y mezcla espectral y
contrasta su orden real. Los fixtures de esta página conservan su alcance de
callbacks independientes; todavía no cambia el plugin.

## Contrato contrastado

| Rutina original | Operación reconstruida | Estado modificado |
| --- | --- | --- |
| `0x0197a8a0` | Distribuye la entrada entre grupos de caché usando el recíproco aproximado de la tasa; la última palabra de cada grupo prevalece. Lee esos grupos para formar la salida. | Caché `0x01cc188c`, flag `0x01cc1874` y, cuando hace falta, semillas/flag del recíproco compartido. |
| `0x0197ab74` | Recorre la entrada a la tasa calculada y retiene su última palabra al llegar al extremo 255. | Solo la onda elegida. |
| `0x0197ac88` | Copia la entrada a una caché y la recorre repitiendo sus 256 posiciones. | Caché `0x01cc1c8c` y flag `0x01cc1878`, además de la onda elegida. |

El puntero de configuración está en `0xff906a90` y el de la onda en
`0xff906a94`. La primera rutina lee una palabra signed32 en offset **12** de la
configuración; las otras dos leen el offset **20**. El harness coloca ese objeto
explícitamente en `0x02f00000`: es una dirección de prueba, no una asignación
recuperada del firmware.

La entrada contrastada es una palabra interna **Q16 entre 0 y 65536**, ambos
incluidos. No es el valor MIDI ni una posición de mando normalizada. Las tres
constantes signed64 en `0x018d50b0`, `0x018d50b8` y `0x018d50c0` son `7 × 65536`.
El cálculo observado equivale a:

```text
scaled = word × 7
rateQ16 = (65536 + (scaled & 65535)) << (scaled >> 16)
```

La tasa queda entre 1 y 128 muestras por lectura. Se conserva esa curva lineal
por tramos, sin sustituirla por una exponencial en coma flotante. Para la retención
por grupos, se mantienen la normalización entera, la consulta a las ocho semillas
originales y las dos iteraciones del recíproco, con truncamientos separados.

Usando tasas en unidades de muestra, la lectura con extremo fijo usa el índice
`min(floor(i × rate),255)` y la repetida usa `floor(i × rate) & 255`, partiendo de
una copia de la entrada. La retención escribe y lee `floor(i × reciprocalRate)`.
Se copian palabras completas de 32 bits, sin interpolación, suavizado ni saturación.

Cuando el flag de retención es cero, se escribe `1` y se limpia toda su caché.
La parte que queda sin escribir permanece a cero; con flag no nulo conserva sus
palabras anteriores. La caché de repetición se sobreescribe entera. Los flags no
nulos conservan su valor exacto, en vez de convertirse siempre en `1`.

El flag del recíproco está en `0x01b60eec`; sus ocho semillas, en `0x01cc187c`.
Solo la retención necesita esta dependencia. Su inicialización diferida se
contrasta con el original. El adaptador rechaza una tabla marcada como inicializada
con semillas distintas de las observadas; las otras dos rutas no la consultan.

## Pruebas y límites

- **256 casos independientes** y **cuatro secuencias de 64 llamadas**, mezclando
  los tres callbacks y seis slots. El orden es del harness, no el scheduler original.
- **171 llamadas de retención**, **171 con extremo fijo** y **170 de repetición**.
  Se prueban identidad, tasa máxima y valores a ambos lados de los cambios de
  exponente, además de entradas deterministas dentro del dominio.
- Ondas con palabras de 32 bits, flags cero y no nulos, cachés previas, reutilización
  de voces y reinicio explícito de flags. Se recargan algunas ondas durante las
  secuencias para evitar limitar el contraste a buffers ya constantes.
- Cada llamada compara las seis ondas y sus guardas, 2.080 bytes de región de
  caché con guardas, flag del recíproco, 32 bytes de configuración y dos punteros.
  Las otras voces y la configuración deben conservarse.
- Dos reproducciones privadas coinciden byte a byte. Pasan las 22 suites de
  núcleo Linux, la prueba nueva compilada para Windows con el toolchain MSVC y
  ejecutada bajo Wine, y ASan/UBSan de la prueba nueva.

Los guards de dominio, modo y semillas son protecciones del adaptador, no ramas
añadidas al firmware. El dominio elegido mantiene los índices dentro de las cachés;
no valida el direccionamiento de palabras arbitrarias. Siguen pendientes mezcla
espectral, conversión/mapeo de mandos, despacho de actualización, scheduler, pitch,
ADSR, salida e integración del motor completo.

Se usa GNU BF524 con el parche documentado de escritura diferida de registros D.
La coincidencia bajo ese modelo no prueba temporización en hardware, Ableton,
ASIO ni ausencia de dropouts. No se distribuyen instrucciones, SDRAM ni ELF
originales ni se atribuye a REA un descifrado o decompilación L1.

[Evidencia y hashes](firmware/drwave246-transform-validation.json) ·
[Plan activo](firmware-dsp-workplan.md).

## Reproducción privada

```bash
python3 scripts/verify-drwave246-transform.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/drwave-transform-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target drwave246_transform_tests
ctest --test-dir build-dsp -R '^drwave246_transform$' --output-on-failure
```

El directorio privado debe ser nuevo y estar fuera del checkout. `--record`
permite regenerar los fixtures numéricos publicables para revisión.
