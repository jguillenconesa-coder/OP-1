# Dr. Wave rev246: actualización de pitch desde VoiceCtl

Se contrasta la rutina SDRAM **`0x01a85cbe`**: **512 actualizaciones** de pitch,
seguidas por **61.568 muestras adicionales** del render completo de Dr. Wave.
La reconstrucción es `Source/Voice246PitchUpdate.h`, común a VoiceCtl y usada
solo en pruebas. No cambia el instrumento, sus IDs MIDI/automatización ni sesiones.

## Contrato observado

| Entrada o estado | Comportamiento contrastado |
| --- | --- |
| Primer argumento | Identidad de nota uint32 que se compara por igualdad, no índice para consultar una tabla. Incluye valores fuera de 0–127. |
| Segundo argumento | Puntero a una palabra signed32 de pitch. Se multiplica por la afinación explícita con producto Q31 truncado y desplazamiento saturado de un bit. La palabra del llamador se sobrescribe, incluso si la lista está vacía. Cero es aquí una entrada aritmética; no selecciona la tabla de notas como en la activación. |
| Afinación | Se lee el índice observado 1200 de `0x018c2184` y la entrada correspondiente en `0x027fc720`. Su valor lo suministra el ensayo; no es una calibración original recuperada. |
| Lista | Comienza en `0x01dfe734`, con entradas de 20 bytes: argumento de nota de 16 bytes y un signed32 de voz. El contador está en `0x01dff134`. El ensayo cubre todas las longitudes 0–128. |
| Voz `-2` | Escribe el pitch afinado en las seis palabras de `0xff8012d4`, aunque la identidad de la entrada no coincida con el primer argumento. |
| Voz `-1` | No escribe registros de pitch. |
| Voz 0–5 | Escribe la palabra de esa voz solo si coincide la identidad de nota. |
| Estado preservado | Lista completa, contador, vecinos, flags activos, velocidad y objetos de Dr. Wave. No llama a Core note-on, no resetea fases y no exige que coincidan los sonidos cargado/seleccionado. |

El bool, el límite de 128 entradas, la comprobación de puntero y los guards de
índice son protecciones del adaptador C++; no se atribuyen al firmware. Un
rechazo del adaptador conserva registros y palabra fuente antes de mutar.
Las entradas de lista son explícitas del harness: este avance no reconstruye
su productor ni asignador, y no identifica las demás rutas de pitch de VoiceCtl.

## Oráculo y regresión

El nuevo llamador reutiliza la preparación de activación original y añade la
llamada completa, sin modificar instrucciones, a `0x01a85cbe`. Carga una lista
explícita y una palabra mutable con guardas, aplica una afinación de prueba
independiente de la usada en la activación y compara el resultado.

Se ejercitan 256 casos independientes y cuatro secuencias de 64 llamadas,
manteniendo objetos, registros y estado compartido entre eventos de cada
secuencia. Cada llamada recibe una lista nueva; no se presenta como evolución
del asignador original. Hay listas vacías, entradas activas/inactivas, broadcasts
con identidad distinta, voces repetidas y las seis voces de destino. Las
palabras fuente y afinación incluyen cero, signos y extremos signed32, incluido
el producto de dos mínimos con saturación positiva.

Después, el harness mantiene el registro resultante en las 128 palabras del
buffer `0xff801950` y solicita un render. **Esa copia sigue siendo código propio
de prueba.** Se conservan 79 actualizaciones de onda, 402 lecturas sin actualizar
y 31 retornos inactivos; no se cuentan estos últimos como audio. Se comparan
audio, pool antes/después, registros y vecinos, palabra fuente y guardas, lista
completa y contador, ondas, cachés, configuración, incrementos y bancos intactos.

El comparador de activación ejecuta los fixtures anteriores y los nuevos sin
duplicar el comparador de render. Dos ejecuciones privadas nuevas producen los
mismos overrides y salida completa. Pasan **26 suites de núcleo y 28 con JUCE**
en Linux, además de **ASan/UBSan** del comparador ampliado. LeakSanitizer se
desactiva por la limitación de ptrace del entorno. Windows/Wine y DAW/ASIO físicos
no se han ejecutado en esta revisión.

Se usa GNU GDB 13.1 con el [parche documentado](envelope246-research.md), construido
desde el tag `gdb-13.1-release` del espejo Sourceware de RTEMS, commit
`4f3e26ac6ee31f7bc4b04abd8bdb944e7f1fc5d2`. Se conserva también el simulador stock.
Antes del nuevo experimento, la reproducción de activación original coincidió
con sus fixtures versionados usando el firmware aportado en esta cuenta.

REA **6.8.0**, mediante `REA_OBJDUMP_COMMAND`, consulta las capacidades del
`objdump` Blackfin construido y el símbolo sintético que delimita la rutina en
un ELF de análisis privado. El proveedor devuelve salida completa y no detecta
cambios de digest del artefacto. REA aporta evidencia de desensamblado; no incluye
el ensamblador/simulador ni verifica por sí mismo el comportamiento DSP. El
símbolo y el ELF son preparaciones propias, no símbolos recuperados del firmware.

## Relevo al buffer: bloqueo localizado

La búsqueda de materializaciones directas encuentra la dirección del registro
de pitch en el tramo legible **`0xffa00a18..0xffa00a20`**, del bloque L1 con
cabecera LDR en `0x206214`, destino `0xffa00000`, 49.076 bytes, flags `0xaddb2406`.
Una ventana legible posterior `0xffa00a88..0xffa00a98` contiene una lectura a
través del puntero guardado en la pila. Son referencias candidatas: las franjas
**`0xffa00a20..0xffa00a28`** y **`0xffa00a80..0xffa00a88`** están cifradas según el
perfil comunitario documentado (primeros ocho bytes de cada grupo de 24).

No se desensamblan esos bytes cifrados como instrucciones ni se ejecuta L1.
Faltan el flujo completo, sus condiciones y el escritor verificado del buffer
antes de afirmar la conexión con el render. La búsqueda directa no excluye
otras referencias indirectas. No se recuperan interpolación, modulación ni
cadencia por deducción a partir de las ventanas legibles.

El siguiente paso es contrastar las demás rutas de actualización de VoiceCtl
y resolver el relevo original al buffer; calibración, contador, ADSR y salida
siguen siendo dependencias separadas. Presets, bancos e interfaz siguen aplazados.

## Reproducción

```sh
python3 scripts/verify-drwave246-pitch-update.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/pitch-nuevo
cmake --build build-dsp --target drwave246_activation_tests
ctest --test-dir build-dsp -R '^drwave246_activation$' --output-on-failure
```

El directorio de salida debe ser nuevo y externo al checkout. Solo `--record`
regenera los dos fixtures numéricos publicables; no modifica los anteriores.
Firmware, imagen, instrucciones, desensamblados y ELF permanecen privados.
[Evidencia y hashes](firmware/drwave246-pitch-update-validation.json).
