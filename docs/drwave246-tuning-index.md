# VoiceCtl rev246: índice de afinación y consumo en Dr. Wave

Se reconstruye el setter SDRAM **`0x01a842f0`** en
`Source/Voice246TuningIndex.h`. Se contrasta también su encadenamiento con la
activación VoiceCtl y el render completo de Dr. Wave: **61.568 muestras
adicionales** con coeficientes explícitos colocados en la entrada calculada.
El código sigue aislado del plugin; no cambia el instrumento ni el instalador.

## Qué queda recuperado

La rutina almacena en **`0x018c2184`** la palabra raw de 32 bits equivalente a:

```text
index = ((coarse + 12) * 100 + fine) modulo 2^32
```

No hace clamp, saturación ni comprobación de longitud de tabla. El C++ usa
aritmética unsigned32 para conservar el wrap sin overflow con signo de C++.
**El índice no es un coeficiente Q31 ni una frecuencia.** Un resultado fuera
de 0–2399 no autoriza consultar la tabla; el ensayo de activación solo usa
índices 1–2399. El estrés fuera de dominio ejecuta únicamente el setter.

| coarse | fine | Índice |
| --- | --- | --- |
| -11 | -99 | 1 |
| -1 | 99 | 1199 |
| 0 | 0 | 1200 |
| 1 | -99 | 1201 |
| 11 | 99 | 2399 |

La revisión estática localiza llamadas directas en `0x019adab8`, `0x019adb20`
y `0x019adc28`. Los argumentos del inicializador de controles en `0x019adb68`
sugieren rangos **[-11,11]** y **[-99,99]**, usados como dominio cartesiano
del ensayo. No se ejecutan aquí los objetos UI, sus callbacks ni todo el arranque;
no se deducen unidades físicas ni límites de otros llamadores desde esa revisión.

Esto explica cómo `(0,0)` produce el índice 1200 usado por las pruebas anteriores.
**No establece que esos argumentos sean los valores restaurados en cualquier
sesión ni recupera el coeficiente original de la entrada 1200.**

## Dos oráculos

1. **Setter completo sin modificar:** 65.536 casos de estrés signed32, incluidos
   extremos y wrap positivo/negativo; las **4.577 parejas** del producto
   `[-11,11] × [-99,99]`, que cubren todas las entradas 1–2399; y una secuencia
   continua de 256 llamadas. Se comparan la palabra escrita y los doce bytes
   vecinos preservados. No se afirma un barrido exhaustivo de todas las parejas
   signed32 posibles.
2. **Setter → activación → render Dr. Wave:** 256 casos independientes y cuatro
   secuencias de 64 llamadas. El harness pone a cero una tabla de prueba de
   2400 palabras y, en cada registro, ejecuta el setter original y escribe el
   coeficiente explícito de ese caso en `0x027fc720 + index*4`. La activación
   original lee desde esa tabla. Se comparan índice y vecinos, coeficiente y
   palabras adyacentes por llamada, y las 2400 palabras de la tabla final.

Las 512 activaciones conservan la cobertura previa: 427 aceptadas / 85
rechazadas, tabla de 128 notas, pitch explícito, extremos Q31 y velocidades.
El render posterior mantiene 79 actualizaciones de onda, 402 lecturas sin
actualizar y 31 retornos inactivos. Se comparan audio, registros, pool antes y
después, ondas, cachés, incrementos, configuración y bancos. Los otros dos
oráculos del comparador de activación se conservan y siguen pasando.

La tabla **es estado sintético suministrado por el ensayo**. Su llenado a cero
no se presenta como recuperación del inicializador del firmware y los
coeficientes escritos no son una calibración deducida del índice. El harness
continúa manteniendo el registro de pitch en el buffer fuente del render;
no reconstruye al escritor original, glide ni modulación.

## Validación y dependencias restantes

Dos ejecuciones privadas nuevas coinciden byte a byte en los cuatro fixtures.
Pasan **28 suites de núcleo y 30 con JUCE** en Linux. Los comparadores del setter
y de activación ampliada pasan **ASan/UBSan**; LeakSanitizer se desactiva por el
límite de ptrace del entorno. Windows/macOS quedan pendientes de CI; no hay
pruebas físicas de DAW/ASIO ni temporización de hardware.

REA 6.8.0 captura el desensamblado del setter mediante GNU objdump, delimitado
por símbolos sintéticos en un ELF privado, con salida completa y digest sin
cambios observados. Es apoyo estático; el contraste numérico usa el modelo GNU
BF524 con el parche D-register documentado. No se distribuyen firmware,
instrucciones, desensamblados ni ELF, solo fixtures numéricos y llamadores propios.

Quedan pendientes el productor de los **valores** de la tabla de afinación,
las demás rutas de actualización de pitch y el relevo L1 al buffer del render,
además de cadencia, asignación, ADSR y salida. El índice ya contrastado no elimina
esas dependencias. Presets, bancos e interfaz siguen aplazados.

```sh
python3 scripts/verify-voice246-tuning-index.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/tuning-nuevo
cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target voice246_tuning_index_tests drwave246_activation_tests
ctest --test-dir build-dsp -R '^(voice246_tuning_index|drwave246_activation)$' --output-on-failure
```

La salida privada debe ser nueva y externa al checkout. `--record` regenera
únicamente los cuatro fixtures de este avance.
[Evidencia y hashes](firmware/voice246-tuning-index-validation.json).
