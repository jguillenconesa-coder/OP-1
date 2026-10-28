# VoiceCtl rev246: entrada de pitch bend, consumidor aún pendiente

Se reconstruye el decodificador SDRAM **`0x01a84efa`** en
`Source/Voice246PitchBend.h`. Coincide con **65.536 parejas independientes de
bytes y 256 llamadas continuas** de la rutina original completa. Son controles
y memoria, **no muestras de audio**; no se integra en el plugin ni se cambia su
pitch bend actual, automatización, sesiones, presets o interfaz.

## Contrato contrastado

El argumento original es un objeto con punteros de inicio y fin en offsets 4 y
8. El ensayo suministra exactamente tres bytes de mensaje legibles; la función
lee los bytes en posiciones 1 y 2 como unsigned8. No consume el byte de estado
ni filtra canales: esa responsabilidad pertenece al despachador externo.

Sean `u = byte1 + (byte2 << 7)` y `d = u - 8192`. Se escribe `2*d` para
`u < 8192`, cero en el centro, y `4*d` para `u > 8192`, conservando la mitad
baja de 16 bits. **No se enmascaran los bits altos de los bytes ni se satura.**
Las 16.384 parejas con ambos bytes en 0–127 son el subconjunto MIDI válido.
El resto es estrés del contrato de lectura unsigned8, no mensajes recomendados.

| byte1 | byte2 | Palabra de control interpretada como signed16 |
| --- | --- | --- |
| 0 | 0 | -16384 |
| 127 | 63 | -2 |
| 0 | 64 | 0 |
| 127 | 127 | 32764 |
| 0 | 128 | -32768 (wrap de la escritura) |
| 255 | 255 | -32260 (wrap de la escritura) |

La palabra se escribe en **`0xff80104c`**. Además, la rutina escribe cero en
los bytes **`0xff80104e`** y **`0xff801052`**, y uno en **`0xff801053`**.
Se comparan los 16 bytes del intervalo `[0xff801048,0xff801058)`, incluidos los
vecinos preservados; no se asigna todavía un significado funcional a esos flags.
Se preservan la imagen de 32 bytes del mensaje/objeto con guardas y los seis
registros de pitch de `0xff8012d4`. El decodificador no escribe el buffer del render.

El adaptador devuelve un bool propio y rechaza puntero nulo o menos de tres
bytes antes de mutar. No emula las excepciones originales: esas ramas llaman a
L1 y no se ejecutan en este ensayo. Tampoco presenta cualquier estado MIDI
como un evento aceptado por el sistema completo.

## Relación con Dr. Wave y límites de la investigación

La revisión estática del despachador VoiceCtl `0x01a85c20` muestra una rama
para el nibble de estado `0xe0` que llama al decodificador, precedida por gates
Core y un byte de estado de VoiceCtl. **Ese despachador y sus otros eventos no
se han contrastado dinámicamente en este avance.** Ejecutar el decodificador
con un estado arbitrario solo prueba que él mismo no consulta ese byte.

La búsqueda de materializaciones directas identifica la dirección del control
en **`0xffa009b0..0xffa009b8`**, dentro del bloque L1 con cabecera LDR `0x206214`,
destino `0xffa00000`, 49.076 bytes y flags `0xaddb2406`. Según el perfil
comunitario documentado, **`0xffa009a8..0xffa009b0`** y
**`0xffa009c0..0xffa009c8`** están cifrados. No se decodifican ni ejecutan esas
franjas. La materialización es una referencia candidata, no la recuperación
del consumidor, sus condiciones ni la conversión bend → frecuencia.

La [actualización selectiva/broadcast](drwave246-pitch-update.md) y este registro
son contratos distintos. No se conectan con una fórmula inferida. Siguen
pendientes las demás rutas de pitch, el escritor original del buffer, la
calibración, la cadencia, ADSR y salida antes de integrar Dr. Wave.

## Evidencia y validación

- 65.536 casos independientes: cada pareja unsigned8 aparece una sola vez,
  con imágenes de control y vecinos deterministas y variables.
- Las tres ramas de signo/centro, las 16.384 parejas MIDI válidas y los
  256 valores de estado aparecen en ese barrido. No se afirman todas las
  combinaciones de pareja, estado e imagen inicial.
- Una secuencia adicional de 256 llamadas conserva la imagen de control y los
  seis registros entre llamadas. Cada evento recibe su propio mensaje y guardas.
- Dos directorios privados nuevos producen inputs y salida idénticos byte a
  byte. El comparador C++ verifica toda la memoria capturada por caso.
- Pasan **27 suites de núcleo y 29 con JUCE** en Linux, y **ASan/UBSan** del
  nuevo comparador. LeakSanitizer se desactiva por el límite de ptrace del
  entorno. Windows/macOS quedan pendientes de CI; no se prueba hardware/DAW/ASIO.

Se usa el modelo GNU BF524 con el parche D-register documentado. REA 6.8.0
consulta mediante el proveedor GNU objdump la rutina delimitada por símbolos
sintéticos en un ELF privado, con salida completa y digest observado sin cambios.
REA aporta evidencia estática; no sustituye al oráculo de ejecución ni descifra L1.
Solo se distribuyen fixtures numéricos y un llamador propio; firmware,
instrucciones, desensamblados y ELF permanecen fuera del repositorio.

```sh
python3 scripts/verify-voice246-pitch-bend.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/bend-nuevo
cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target voice246_pitch_bend_tests
ctest --test-dir build-dsp -R '^voice246_pitch_bend$' --output-on-failure
```

La salida privada debe ser nueva y externa al checkout. `--record` regenera
solo los dos fixtures de este decodificador para revisión.
[Evidencia y hashes](firmware/voice246-pitch-bend-validation.json).
