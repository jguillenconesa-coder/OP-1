# VoiceCtl rev246: constructor aislado del argumento de nota

## Alcance y entradas

Se investiga exclusivamente el constructor SDRAM **`0x01a85014..0x01a85058`**
(fin exclusivo, 68 bytes). Es una rutina completa sin llamadas externas. Recibe
un puntero al argumento en R0 y dos palabras raw de 32 bits en R1 y R2. Aquí
se describen como nota recibida y velocidad recibida por su uso posterior en
VoiceCtl; no se deduce un mensaje MIDI, canal, status o modo comercial.

La reconstrucción independiente está en `Source/Voice246NoteArgument.h`. No
modifica Engine, parámetros del host, MIDI/automatización, sesiones ni UI.
El contrato es little-endian y conserva el argumento como 16 bytes explícitos.

## Escrituras demostradas

| Offset | Tamaño escrito | Valor |
| --- | --- | --- |
| 0 | 32 bits | Nota recibida + 53, más el ajuste condicional indicado abajo. |
| 4 | 32 bits | Copia íntegra de R2, incluidos bits altos. |
| 8 | Un byte | 1. |
| 9–11 | Ninguno | Conserva los tres bytes anteriores del destino. |
| 12 | 32 bits | 0. |

No inicializa los bytes 9–11, no limita la nota a 0–127 y no convierte la
velocidad a 7, 8 o 16 bits en este constructor. No aplica saturación. Las
sumas y el producto efectivo por doce conservan el resultado módulo 2^32.
Los patrones raw con bit de signo se conservan como tales; no se convierten
mediante una aritmética signed C++ que pueda producir comportamiento indefinido.

## Dependencias de memoria

Siempre lee la palabra completa **`0x01de4c40`** y la compara con **1**. No basta
con que su byte o mitad baja valgan uno. Esta palabra es distinta del modo
VoiceCtl situado en `0xff801038`.

- Si vale exactamente 1: escribe `nota + 53`. No lee el slot seleccionado ni
  la tabla de ajustes. Se ejercitan índices seleccionados fuera de rango,
  incluidos los que generarían una dirección fuera del RAM mapeado si se usaran.
- En otro caso: lee la palabra seleccionada en **`0xff8016d4`**, después la
  palabra en **`0x01dfe4e0 + slot * 4`**, y añade doce veces ese valor a la nota
  ya desplazada. El producto por doce procede de operaciones de puntero que
  también envuelven a 32 bits.

```text
noteWord = (receivedNote + 53) mod 2^32
if word_at_01de4c40 != 1:
    noteWord = (noteWord + 12 * adjustmentWords[selectedSlot]) mod 2^32
```

El constructor no comprueba el índice ni el valor de tabla. Como contexto
estático separado, la ruta `0x01a8633c` comprueba un índice signed de 0–7 y un
valor signed de -4–4 antes de escribir esa tabla por su ruta sin error. Esa
ruta no se ejecuta aquí y no demuestra valores predeterminados, procedencia de
todas las escrituras ni significado comercial del ajuste. Los valores de tabla
muy grandes del ensayo son estrés de la aritmética del constructor.

El adaptador C++ acepta ocho entradas explícitas y rechaza un índice fuera de
0–7 antes de mutar cuando la tabla es necesaria. En bypass admite cualquier
índice. Su bool es propio; no atribuye un retorno semántico a R0 en el original.
No se emulan lecturas arbitrarias fuera de tabla, alias del destino con globales
ni destinos desalineados. Los destinos originales ensayados están alineados a
cuatro bytes, en las cuatro posiciones módulo 16.

## Oráculo y comparación

El harness propio llama a la rutina original completa en su dirección SDRAM;
no sustituye instrucciones ni ejecuta llamadores o L1. Argumento y globales son
entradas explícitas. Se conserva una zona de destino de 64 bytes y una imagen
compartida de 3.592 bytes con:

- palabra discriminadora, slot y tabla de ocho palabras con vecinos;
- registros VoiceCtl y sus vecinos;
- pool ADSR y memoria vecina, lista completa de notas y contador;
- RNG y vecino.

Las imágenes compartidas son patrones raw para detectar escrituras: no se
presentan como voces inicializadas ni se renderizan. Se compara toda la zona
de destino y toda la imagen compartida después de cada llamada. Los globales
de selección/tabla se recargan por llamada; registros, pool, lista, RNG y zona
de destino se conservan durante las secuencias continuas.

Hay **1.280 llamadas: 1.024 casos independientes y cuatro secuencias de 64**.
Se cubren 160 bypass y 1.120 consultas de tabla, los ocho slots en la rama de
consulta, discriminadores con la mitad/byte bajo igual a uno y bits altos
distintos, límites de 7/8/16/32 bits, los 32 bits individuales y sus complementos
en ambas entradas, ajustes positivos/negativos y wrap de sumas/producto.
El relleno y los vecinos se precargan con patrones no nulos.

El resultado aislado no añade muestras de audio ni completa una ruta de voz.
El modelo es GNU BF524 con los dos parches de writeback documentados; se ejecuta
el probe halfword antes del oráculo. No valida tiempos ni funcionamiento físico.

Tres ejecuciones en directorios privados nuevos producen entradas y resultados
idénticos byte a byte. Pasan **42 suites de núcleo en Linux**, el comparador nuevo
compilado para **Windows bajo Wine** y **ASan/UBSan** (LeakSanitizer desactivado
por la limitación de ptrace del entorno). La reproducción del oráculo existente
de retirada polifónica también coincide, sin regenerar sus fixtures. CI nativo
Windows/macOS queda pendiente; no se ha probado DAW, ASIO ni hardware.

REA 6.8.0 conserva privadamente el desensamblado completo del símbolo sintético
`voice246_note_argument`: salida sin truncar, proceso correcto y digest del ELF
sin cambios. La evidencia pública solo contiene metadatos y hashes.

## Relación con activación y retirada

El resultado tiene el formato de 16 bytes usado por la
[activación VoiceCtl](drwave246-note-activation.md): palabra de nota, palabra de
velocidad, byte en offset 8 con relleno conservado, y pitch explícito cero en
offset 12. En el contrato de activación ya contrastado, ese último cero selecciona
la consulta de tabla. Eso no hace válidos para activación todos los valores raw
de estrés ensayados aquí.

La [retirada polifónica](drwave246-voicectl-poly-release.md) busca por la palabra
de nota; este constructor prepara esa palabra. No se ejecuta su conexión con
la lista ni con activación/retirada en este ensayo. La investigación coordinada
de los selectores 1–4 en `collab/drwave-release-selectors` queda fuera de esta PR.
No se modifica ninguno de sus archivos.

Permanecen pendientes los llamadores/dispatcher, asignador, orden de eventos,
productores de buffers de pitch/volumen, relevo ADSR, reloj y salida. No se infiere
cadencia, interpolación ni identidad comercial para completar esas dependencias.

## Reproducir y atribución

Prepara las herramientas y el LDR privado con
[la guía entre cuentas](new-account-dsp-setup.md), incluyendo
`--halfword-writeback` para el modelo GNU. Cada salida debe ser nueva y externa
al checkout:

```sh
python3 scripts/verify-voice246-note-argument.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/note-argument-nuevo
cmake --build build-dsp --target voice246_note_argument_tests
ctest --test-dir build-dsp -R '^voice246_note_argument$' --output-on-failure
```

`--record` solo regenera los dos fixtures propios. El formato está en
`Tests/ReferenceVectors/voice246-note-argument-format.md`; resultados, hashes y
validación en `docs/firmware/voice246-note-argument-validation.json`.

Implementación, comparador, harness y verificador son código propio AGPL-3.0-only.
La imagen rev246 aportada por el propietario se usa como oráculo privado. GNU
Binutils 2.44/GDB 13.1 y REA 6.8.0 son herramientas de investigación externas,
con las atribuciones y límites documentados en [CREDITS](../CREDITS.md).
REA permite conservar la inspección estática mediante GNU objdump y un símbolo
sintético; no aporta descifrado ni validación numérica. Firmware, instrucciones,
desensamblados y ELF permanecen privados y no se publican como artefactos de CI.
