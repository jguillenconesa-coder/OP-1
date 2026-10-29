# VoiceCtl rev246: productor de los 2400 coeficientes de afinación

Se contrasta la cola SDRAM **`0x01a85454..0x01a854ca`** (fin exclusivo), que
genera las 2400 palabras de `0x027fc720`. La reconstrucción C++ es
`Source/Voice246TuningTable.h`. Un nuevo oráculo conecta setter de índice,
tabla generada, activación VoiceCtl y render Dr. Wave: **61.568 muestras
adicionales sin sobrescribir coeficientes por caso**.

Es un contrato aislado: no cambia el plugin, instalador, IDs MIDI/automatización,
sesiones, presets ni interfaz. No equivale al arranque del firmware completo.

## Alcance y operaciones

La rutina contenedora comienza en `0x01a8541c`. El ensayo omite su prefijo de
setup de módulo/eventos y reinicio de listas/controles. Un adaptador propio
prepara el layout de frame y entra en la cola en su dirección original; su
epílogo restaura ese frame. No se parchean ni reubican las instrucciones de la
cola ni de los helpers matemáticos llamados.

Las tres constantes binary64 en `0x01909764` son **1200.0, 2.0 y 2.0**.
La cadena contrastada para cada entrada equivale a:

```text
exponent = binary64(index - 1200) / 1200.0
value64 = pow(2.0, exponent) / 2.0
value32 = binary32(value64)
coefficient = original_float32_to_Q31(value32)
```

Se conserva la conversión intermedia a binary32 antes del helper Q31
`0x01aa6fd0`, ya contrastado independientemente. El C++ reutiliza ese conversor
y `std::pow` binary64 bajo el redondeo habitual. Se verifica el dominio finito
de 2400 entradas, no toda la biblioteca matemática Blackfin. La tabla se prepara
fuera del callback de audio; no se introduce `pow` en el callback del instrumento.

| Índice | Coeficiente Q31 |
| --- | --- |
| 0 | `0x20000000` |
| 1200 | `0x40000000` |
| 2399 | `0x7fed1400` |

Todas las entradas crecen estrictamente. Ahora se establece la procedencia de
`0x40000000` en la entrada 1200, antes suministrado como dato de prueba. Esto no
valida la calibración física del reloj, los controles restaurados de cualquier
sesión ni posibles modificaciones posteriores de la tabla en el arranque completo.

## Oráculos y evidencia

El productor se ejecuta con tres precargas: cero, `0xa5a5a5a5` y `0xffffffff`.
Coinciden **7200 palabras de tabla**; se conservan guardas antes/después y los
16 bytes del índice y vecinos. Las 2400 entradas finales son independientes
del relleno previo. El antiguo constructor `0x01a84388` solo escribe cero: no
debe confundirse con este productor posterior.

El oráculo combinado reutiliza las 512 activaciones con índices acotados del
ensayo anterior. Genera la tabla con la cola original una vez y elimina la
escritura de coeficientes por registro. El viejo campo de override contiene
**`0xdeadbeef`** para detectar su reutilización accidental.

Se comparan índice/vecinos, coeficiente y palabras adyacentes, la tabla final
completa, registros, pool antes/después, audio, ondas, cachés, configuración,
incrementos y bancos intactos. Se mantienen 427 activaciones aceptadas / 85
rechazadas, 79 actualizaciones de onda, 402 lecturas y 31 retornos inactivos.
Estos últimos no se cuentan como audio. Los tres oráculos anteriores del
comparador se conservan y siguen pasando.

Dos reproducciones privadas nuevas coinciden byte a byte en ambos probes y
tres fixtures. Pasan **29 suites de núcleo y 31 con JUCE** en Linux, más
**ASan/UBSan** del productor y de la activación ampliada. LeakSanitizer se
desactiva por el límite de ptrace. Windows/macOS quedan pendientes de CI;
no se han probado hardware/DAW/ASIO ni temporización física.

REA 6.8.0 aporta desensamblado de la cola mediante GNU objdump y símbolos
sintéticos en un ELF privado, con salida completa y digest observado sin cambios.
El contraste numérico usa GNU BF524 con el parche D-register documentado.
Solo se distribuyen fixtures numéricos y llamadores propios; firmware,
instrucciones, desensamblados y ELF permanecen privados.

## Dependencias restantes y reproducción

El productor de esta tabla deja de estar pendiente. Siguen separados el setup
completo, otras rutas de pitch, el escritor original del buffer, asignación,
cadencia, ADSR y salida. El harness aún mantiene el registro de pitch en el
buffer del render: no se deducen glide, modulación ni conexión con el control
de bend desde esta tabla. Cluster aún requiere sus propias comprobaciones de
consumo, normalización y relevo L1; Dr. Wave sigue siendo el bloque activo.

```sh
python3 scripts/verify-voice246-tuning-table.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/tabla-nueva
cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target voice246_tuning_table_tests drwave246_activation_tests
ctest --test-dir build-dsp -R '^(voice246_tuning_table|drwave246_activation)$' --output-on-failure
```

La salida debe ser nueva y externa al checkout. `--record` regenera solo los
tres fixtures nuevos. [Evidencia y hashes](firmware/voice246-tuning-table-validation.json).

La [repreparación posterior por lista/modo](drwave246-pitch-refresh.md) ya
contrasta otra ruta VoiceCtl con esta tabla, callbacks ADSR y RNG compartido,
en un contexto de módulos Core auxiliares ausentes. El relevo al buffer sigue pendiente.
