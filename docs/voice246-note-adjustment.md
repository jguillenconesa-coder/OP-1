# Rev246: setter de ajustes de nota y consumo por el constructor

## Alcance y límites de la rutina

El setter entra en **`0x01a8633c`**. Su último salto termina en
**`0x01a863a6`** (fin exclusivo de 106 bytes de instrucciones); siguen dos bytes
de alineación y el siguiente prólogo en **`0x01a863a8`**. Estos límites proceden
de la inspección del flujo SDRAM, no de símbolos originales recuperados. Los
símbolos del ELF de investigación son propios.

R0 recibe un **índice signed32** y R1 un **ajuste signed32**. La ruta sin llamada
de error requiere índice **0–7** y ajuste **-4–4**, ambos extremos incluidos.
Escribe la palabra completa, con el patrón signed32 recibido, en:

```text
0x01dfe4e0 + index * 4
```

En esa ruta no hay llamadas externas ni consulta del valor anterior, sonido
seleccionado, discriminador del constructor, pitch, lista o envolventes. No
añade un delta ni limita un valor existente: reemplaza exactamente una entrada.
Se conserva el resto de la tabla y la memoria compartida vigilada. El valor de
R0 observado al volver es **4**, también si se repite un ajuste idéntico. No se
interpreta como un bool o código semántico de éxito.

`Source/Voice246NoteAdjustment.h` reconstruye exclusivamente esa escritura.
Sus rechazos antes de mutar y el retorno bool son **protecciones propias**.
Se usan comparaciones signed32 completas, sin truncar índice/valor a byte o
halfword. No se atribuyen identidad comercial ni valores predeterminados.

## Rutas de error excluidas

El índice negativo o mayor que 7 dirige a **`0x01a86398`**; el ajuste fuera de
-4–4 dirige a **`0x01a86374`**. Ambos llaman al helper **`0x01919aa8`**.
Hay continuación legible después de las llamadas: si el helper devuelve, la
rama de valor alcanza otra escritura de la palabra recibida y la de índice
vuelve a comprobar el valor antes de escribir. **No es una saturación ni se
demuestra un rechazo que conserve memoria para entradas inválidas.**

La inspección del helper muestra comportamiento condicionado por un byte en
`0xff9002f4`, incluida una ruta de retorno y otra con más llamadas. No se ejecuta
el helper, no se fuerza su global para eludirlo ni se emulan sus efectos. Las
ramas inválidas y sus dependencias quedan fuera del oráculo. La ruta válida
ensayada permanece íntegra en SDRAM y no necesita ejecutar L1 cifrada.

## Ensayo separado y cadena acotada

El harness propio tiene dos fases, con capturas antes y después del punto donde
podría llamar al constructor original **`0x01a85014`**:

1. **72 casos aislados:** todas las parejas de ocho índices y nueve ajustes.
   Precarga palabras de tabla con patrones raw de bits altos y comprueba que
   únicamente la entrada elegida cambia. El argumento permanece intacto y no
   se llama al constructor.
2. **576 casos independientes de cadena y cuatro secuencias de 64 llamadas:**
   en cada arranque se precarga una tabla envenenada y se reemplazan sus ocho
   entradas mediante **ocho llamadas originales al setter**. Después se aplica
   el cambio del registro y se llama al constructor. Dentro de una secuencia
   la tabla no se recarga: todos los cambios pasan por el setter.

La precarga es veneno de prueba y guardas, no un override de coeficientes que
consuma el constructor. Los ocho valores iniciales -4–4 son entradas explícitas
al setter, no valores predeterminados inferidos. Tanto el slot cambiado como
el seleccionado para el constructor se suministran por separado.

En total son **904 registros, 5.544 ejecuciones del setter** (4.640 de
inicialización y 904 de cambio) y **832 del constructor**. Hay 376 bypass y
456 consultas de tabla, estas últimas repartidas en 228 consultas al slot recién
escrito y 228 a otro slot. Se cubren las 72 parejas en cada camino del
constructor, 196 escrituras de un valor ya presente y 75 selecciones fuera de
rango ignoradas por el bypass del constructor. El setter conserva su propio
índice válido y es independiente de esa selección.

Se reutiliza **`Source/Voice246NoteArgument.h` sin modificarlo**. La palabra
discriminadora, slot seleccionado, nota y velocidad raw, dirección alineada del
argumento y orden de llamadas siguen siendo entradas explícitas. Los límites de
nota/velocidad, bits individuales y complementos, argumentos desplazados a
cuatro posiciones módulo 16 y wrap de nota verifican el consumo del ajuste.
El orden es propio del harness y no demuestra un llamador o cadencia original.

## Comparación de memoria y modelo

Cada registro captura la zona completa de argumento de **64 bytes**, que contiene
el argumento de 16 bytes y vecinos, y **3.592 bytes compartidos**, tanto tras el
setter como tras el constructor o su omisión. Se comparan tabla completa y
guardas, discriminador y selección con vecinos, registros VoiceCtl, lista y
contador, pool ADSR con memoria vecina y RNG con vecino. Los campos no vigilados
y la pila de llamada no se presentan como una imagen íntegra del aparato.

Se comprueban las escrituras de tamaño correcto y los tres bytes de relleno del
argumento. Los patrones compartidos son estrés raw para detectar cambios, no
voces inicializadas. Solo discriminador y selección se recargan por llamada;
tabla, argumento y demás regiones se conservan dentro de cada secuencia.

El modelo es GNU BF524 con los dos parches de writeback documentados. Se comprueba
el probe halfword antes de cada ejecución. REA 6.8.0 / GNU objdump aporta inspección
estática del símbolo sintético del setter en un ELF privado; no aporta descifrado
ni validación numérica. La comparación numérica corresponde al simulador y al C++.

Dos ejecuciones privadas nuevas producen entradas y salida idénticas byte a
byte. Pasan **44 suites de núcleo Linux**, el nuevo comparador compilado para
**Windows bajo Wine** y **ASan/UBSan** (LeakSanitizer desactivado por la limitación
de ptrace). El oráculo previo del constructor reproduce sus fixtures sin
regenerarlos usando estas herramientas. CI nativo Windows/macOS queda pendiente;
no se han medido hardware, DAW/ASIO, latencia ni dropouts.

## Reproducción y coordinación

Prepara el LDR y las herramientas con [la guía entre cuentas](new-account-dsp-setup.md),
incluyendo `--halfword-writeback`. La salida debe ser privada, nueva y externa
al checkout:

```sh
python3 scripts/verify-voice246-note-adjustment.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/adjustment-nuevo
cmake --build build-dsp --target voice246_note_adjustment_tests
ctest --test-dir build-dsp -R '^voice246_note_adjustment$' --output-on-failure
```

`--record` regenera solo los dos fixtures propios. Formato:
`Tests/ReferenceVectors/voice246-note-adjustment-format.md`. Resultados y hashes:
`docs/firmware/voice246-note-adjustment-validation.json`.

La rama parte de `origin/main` **`c2d1071`**, que incorpora la PR #18. La cadena
constructor → activación/retirada → render ADSR de
`collab/drwave-note-event-chain` pertenece a la cuenta coordinadora y no se
desarrolla aquí. No se llama a activación, retirada, render, dispatcher MIDI,
asignador, scheduler ni L1. No se recuperan productores de buffers ni salida.
El código del instrumento, MIDI/automatización, sesiones y recursos no cambian.

Implementación, comparador, harness y verificador propios son AGPL-3.0-only.
El firmware rev246 aportado por el propietario es un oráculo privado; GNU
Binutils/GDB y REA son herramientas externas, con atribuciones en
[CREDITS](../CREDITS.md). Solo se publican código propio, datos numéricos y
metadatos; firmware, instrucciones originales, desensamblados y ELF quedan
privados. No hay merge, release ni instalador nuevo por este ensayo.
