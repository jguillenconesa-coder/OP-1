# VoiceCtl rev246: repreparación de pitch, ADSR y seis voces Dr. Wave

Se contrasta la rutina completa **`0x01a854cc..0x01a856bc`** (fin exclusivo)
con la tabla de afinación generada, seis objetos ADSR y Dr. Wave. La
reconstrucción es `Source/DrWave246PitchRefresh.h`. Son **512 llamadas** con
**61.568 muestras adicionales** de render posterior. El código sigue aislado
del plugin; no cambia MIDI/automatización, sesiones, presets ni interfaz.

## Contexto contrastado

La rutina lee la palabra de modo en offset 8 de los parámetros comunes del
sonido seleccionado: **`0x01b60ef0 + seleccionado*16`**. Ese bloque es distinto
de los controles de Core en `0xff8013c4`. El harness carga imágenes explícitas
y da valores distintos a las palabras de modo seleccionadas en ambos bloques,
para comprobar su procedencia. Ambas imágenes se conservan. El modo es la palabra signed16 desplazada doce bits
con signo. Se ejercitan todos los nibbles resultantes **-8..7**; los negativos
son estrés del contrato, no modos comerciales identificados.

La lista comienza en `0x01dfe734`: hasta 128 entradas de nota de 16 bytes más
un índice signed32 de voz. El contador está en `0x01dff134`. El pool ADSR de
seis objetos de 24 bytes está en `0xff8000d4`, con su flag de inicialización
explícitamente a uno. Los seis punteros de módulos auxiliares de Core en
`0x01aaad6c` se suministran **nulos**: el despacho auxiliar `0x019673dc` no
ejecuta otro instrumento en este contrato. Punteros, flag y vecinos se verifican.

## Ramas contrastadas

| Condición | Comportamiento |
| --- | --- |
| Modo 4 o lista vacía | Conserva objetos, registros y RNG. No consulta una nota ni un coeficiente. |
| Modo 0 | Recorre la lista. Omite índices de voz negativos; para cada voz 0–5 llama al note-on ADSR, escribe su ganancia baja y solicita la activación VoiceCtl ya contrastada. El `-2` de esta lista se omite, sin el broadcast de la otra rutina de actualización. |
| Modo 3 con lista no vacía | Usa la última nota y prepara las voces 0–5, independientemente del índice de voz almacenado en esa entrada. |
| Otros modos no nulos salvo 4 | Usa la última nota y prepara solo la voz 0. |

El note-on ADSR `0x0191211a` escribe etapa 1 y borra contador/nivel. Conserva
las palabras pendientes y la ganancia, salvo la escritura adicional de ganancia
en modo 0. **Se ejecuta antes de conocer la aceptación Core**: también sucede
si los sonidos cargado/seleccionado difieren y Core rechaza el disparo.

En modo 0, la ganancia usa el **índice de nota**, mediante la expresión binary32
`1 - noteIndex/256` y el helper Q15 original. Para el dominio 0–127 equivale a
`min(32767, 32768 - noteIndex*128)`. Solo se escribe su mitad baja; la alta se
conserva. Pitch, velocidad y actividad se preparan después de la aceptación,
como en el contrato de activación anterior. Entradas repetidas de la misma voz
se procesan en el orden de la lista.

En los modos de una/seis voces, el pitch base combina la última nota o su
pitch explícito con la entrada de afinación generada. Cada voz recibe primero
note-on ADSR y el despacho auxiliar vacío. Core acepta solo si coinciden los
sonidos. Tras cada aceptación avanza el RNG compartido `0xff802bc8`:

```text
seed = (seed * 1664525 + 1013904223) modulo 2^32
q = floor(signed16(seed >> 16) / 512)
deltaWord = signed32((uint32(q) * voice) << 16)
pitch = saturate32(basePitch + multiplyQ31(basePitch, deltaWord))
```

Se conserva el orden de truncamiento, wrap y saturación. **La voz 0 consume
RNG aunque su desviación sea cero.** Un rechazo Core conserva RNG, pitch,
velocidad, actividad y el objeto de Dr. Wave; no deshace el note-on ADSR previo.
La semilla inicial es explícita del ensayo. No se reconstruye su origen ni se
ejecuta el render ADSR que también utiliza estado aleatorio compartido.

El bool y los guards del adaptador C++ son propios. Los índices de sonido,
tamaño/puntero de lista, notas e índice de afinación se acotan antes de mutar.
No se emulan asserts/excepciones originales ni módulos auxiliares presentes.

## Oráculo, cobertura y límites

Se usan 256 casos independientes y cuatro secuencias de 64 llamadas con estado
persistente. La lista, los parámetros comunes/Core y los eventos de render se
suministran explícitamente por registro. La semilla y los seis ADSR solo se
recargan al iniciar un caso independiente o una secuencia. La tabla de afinación
se genera una vez por el productor original ya contrastado, sin overrides.

Se cubren todas las longitudes 0–128 y las 128 notas en activaciones aceptadas,
pitch explícito/extremos, voces intercaladas y repetidas y los 16 nibbles de modo.
Se observan **1850 eventos Core aceptados / 369 rechazados**, 2219 callbacks
ADSR, 1619 escrituras de ganancia y 507 avances del RNG. Tras cada refresh
el harness solicita el render de **una** voz: 79 actualizaciones de onda, 402
lecturas y 31 retornos inactivos. Las 61.568 muestras solo cuentan las activas.
Esto no mezcla seis voces ni recupera el scheduler.

Se comparan los seis ADSR con guardas, RNG y vecino, lista completa/contador,
parámetros comunes y controles Core, registros/vecinos, pool Dr. Wave antes y
después del render, audio, ondas, cachés, incrementos, configuración, bancos y
tabla de afinación. El comparador conserva los cuatro oráculos anteriores.

Dos reproducciones privadas nuevas coinciden byte a byte. Pasan **29 suites
de núcleo y 31 con JUCE** en Linux, y el comparador ampliado bajo **ASan/UBSan**.
LeakSanitizer se desactiva por el límite de ptrace del entorno. Windows/macOS
quedan pendientes de CI; no se prueba hardware/DAW/ASIO ni temporización.
REA 6.8.0/GNU objdump aporta desensamblado estático completo en un ELF privado
con símbolos sintéticos; el oráculo usa GNU BF524 con el parche D-register.

**El harness aún mantiene el registro de pitch en el buffer del render.** No
se completa el escritor original, asignador, cadencia, relevo/render ADSR,
salida ni integración del instrumento. Los llamadores externos del refresh no
se ejecutan aquí; no se atribuye una cadencia a la rutina. Firmware,
instrucciones, desensamblados y ELF permanecen privados.

```sh
python3 scripts/verify-drwave246-pitch-refresh.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/refresh-nuevo
cmake --build build-dsp --target drwave246_activation_tests
ctest --test-dir build-dsp -R '^drwave246_activation$' --output-on-failure
```

Salida privada nueva y externa al checkout; `--record` solo regenera los dos
fixtures nuevos. [Evidencia y hashes](firmware/drwave246-pitch-refresh-validation.json).
