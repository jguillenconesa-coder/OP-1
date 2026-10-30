# Rev246: consulta del último identificador de voz válido

## Rutina y contrato observado

La entrada SDRAM **`0x01a863a8`** abarca **120 bytes**, hasta
**`0x01a86420`** exclusivo, donde comienza el siguiente prólogo. Incluye la
ruta de error al final; el retorno sin error está en `0x01a86406`. Los límites
proceden del flujo legible; el símbolo del ELF de investigación es sintético.

No necesita argumentos ABI explícitos. Lee el contador en **`0x01dff134`** y,
si no es cero, recorre en orden las entradas de **20 bytes** desde
**`0x01dfe734`**. En cada entrada lee solo el identificador **signed32 en offset
16**: la primera dirección de ese campo es `0x01dfe744`.

Cada identificador entre **0 y 5**, extremos incluidos, se escribe como palabra
completa en **`0x01dfe4d4`**. Los negativos, incluidos -1 y -2, y los valores
mayores que 5 se descartan. No se truncan a byte o halfword. Tampoco se consulta
el byte de offset 8, palabra de nota, velocidad ni demás contenido de la entrada.

Al terminar, lee la palabra persistente, comprueba que sea signed32 0–5 y la
devuelve en R0. Por tanto:

| Lista y valor previo | Resultado de la ruta contrastada |
| --- | --- |
| Hay identificadores válidos | El último de ellos según el orden de la lista reemplaza el persistente y se devuelve. |
| Lista vacía o sin identificadores válidos; persistente previo 0–5 | Conserva y devuelve ese valor anterior. |
| Persistente previo fuera de rango, pero la lista contiene un válido | La escritura durante el recorrido repara el valor previo; retorna el último válido. |

No elige el máximo identificador, no limpia la palabra al empezar y no modifica
lista, contador, notas o flags. No se presenta como asignador completo, prioridad
de ejecución, evento musical ni modo comercial. Que el recorrido sea hacia
delante no demuestra quién produce u ordena la lista.

El contador se lee al entrar y de nuevo durante el recorrido; el ensayo mantiene
lista y contador estables dentro de cada llamada. El adaptador recibe una
instantánea y no emula mutaciones concurrentes de esa memoria.

## Error excluido y adaptador

Si tras el recorrido la palabra persistente sigue fuera de 0–5 —por ejemplo,
lista vacía y valor anterior -1— salta a **`0x01a86408`** y llama al helper
**`0x01919aa8`**. Hay continuación: si ese helper devuelve, se vuelve a leer
la palabra persistente antes de retornar. Sus efectos y ese retorno no se
ejecutan ni se suponen en el oráculo numérico. No se fuerza el flag del helper
para eludirlo, no se parchean instrucciones y no se ejecuta L1.

El verificador comprueba **antes de simular** que todas las llamadas terminen
con una palabra válida. La ausencia de un identificador válido y un persistente
inválido se conserva como condición excluida, sin inventar un retorno original.

`Source/Voice246LastVoiceQuery.h` recibe 128 registros raw, contador y palabra
persistente, y devuelve `std::optional<uint32_t>`. Un valor presente es el
identificador 0–5 contrastado. `nullopt` es rechazo del adaptador para la
condición excluida o contador mayor que 128; **no es un valor observado del
firmware**. La validación de capacidad es propia: esta rutina original no
comprueba el límite 128 antes de recorrer la memoria. Un valor persistente
previo inválido no se rechaza antes de examinar una lista capaz de repararlo.

## Ensayo y comparación

Son **1.024 consultas originales completas**: 512 casos independientes y ocho
secuencias de 64 llamadas. Las listas y otros globales seleccionados son entradas
explícitas por llamada. La palabra persistente y las regiones retenidas solo se
recargan al iniciar el caso o la secuencia; después se conserva el resultado
de la consulta anterior.

Se cubren todas las longitudes **0–128**, los seis identificadores válidos,
trece patrones descartados signed32 (negativos, 6/7, bits altos con mitades bajas
aparentemente válidas), duplicados, válidos al principio/interior/final y entradas
válidas fuera del contador que deben ignorarse. Hay **32 pares de listas
invertidas**, con los mismos registros completos en orden contrario: una lista
termina en 5 y la inversa en 0. El byte 8 a cero no impide leer un identificador
válido en este contrato.

Resultados observados:

- **67 listas vacías**, **733 con identificador válido** y **291 retornos al
  valor persistente previo**; los retornos de fallback incluyen las listas vacías.
- **3.715 lecturas válidas** y **58.099 descartadas** dentro del contador.
- **199 valores previos reparados**, 616 cambios del valor persistente y 109
  listas con identificadores válidos duplicados.
- 1.322 lecturas válidas pertenecen a entradas cuyo byte 8 es cero.

Se compara R0 y una imagen completa de **3.740 bytes**: lista de 128 entradas,
contador y vecinos, palabra persistente y sus 60 bytes vecinos, registros
VoiceCtl, pool ADSR y vecinos, RNG y vecino, parámetros de ocho sonidos,
selección y discriminador con vecinos. Solo se permite la escritura de la
palabra persistente. El word `0x01dfe72c` es memoria vecina real de la lista,
no un slot extra. Los patrones compartidos son estrés raw; no se presentan
como voces inicializadas ni se ejecutan sus callbacks.

El llamador también varía los valores de entrada en R0/R1/R2. No se convierten
en parámetros semánticos de la rutina. La pila, registros de trabajo y estado
DAG/loops de Blackfin no se modelan como una imagen completa de máquina en C++.

## Herramientas, reproducción y coordinación

El oráculo usa GNU BF524 con ambos parches de writeback documentados y un probe
halfword previo. REA 6.8.0 / GNU objdump conserva privadamente el desensamblado
del símbolo sintético completo. REA no realiza descifrado ni validación numérica.
Coincidir con este modelo no demuestra hardware, temporización, DAW o ASIO.

Dos ejecuciones privadas nuevas coinciden byte a byte. Pasan **47 suites de
núcleo en Linux**, el nuevo comparador compilado para **Windows bajo Wine** y
**ASan/UBSan** (LeakSanitizer desactivado por la limitación de ptrace). El oráculo
previo de setter/constructor reproduce sus fixtures sin regenerarlos con estas
herramientas. CI nativo Windows/macOS queda pendiente. No se añaden mediciones
de latencia o dropouts.

Prepara herramientas y LDR según [la guía entre cuentas](new-account-dsp-setup.md).
La salida debe ser nueva, privada y externa al checkout:

```sh
python3 scripts/verify-voice246-last-voice-query.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/last-voice-nuevo
cmake --build build-dsp --target voice246_last_voice_query_tests
ctest --test-dir build-dsp -R '^voice246_last_voice_query$' --output-on-failure
```

`--record` solo regenera los dos fixtures nuevos. Formato propio:
`Tests/ReferenceVectors/voice246-last-voice-query-format.md`. Evidencia y hashes:
`docs/firmware/voice246-last-voice-query-validation.json`.

La rama se creó desde **`origin/main` en `ceb1bb6`** y se actualizó a **`bf854e5`**
tras fusionarse la PR #22 de la coordinadora. Su cadena
(`collab/drwave-adjusted-note-chain`, `7fbd5c0`) queda fuera del nuevo ensayo.
El nuevo harness no ejecuta setter, constructor, activación, retirada, render,
dispatcher MIDI, asignador o scheduler. Las suites existentes se conservan como
regresión. No cambian MIDI/automatización, sesiones, presets ni UI.
Planes globales, cobertura y README común de fixtures permanecen intactos.

Código propio y datos numéricos tienen la procedencia documentada del rev246
aportado por el propietario. Implementación, harness, comparador y verificador
son AGPL-3.0-only; las herramientas GNU y REA tienen las atribuciones de
[CREDITS](../CREDITS.md). Firmware, instrucciones originales, desensamblados y ELF
permanecen privados y no se publican como artefactos.
