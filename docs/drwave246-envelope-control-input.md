# Rev246: cuatro entradas hacia los primeros cuatro parámetros ADSR

Se contrasta otra rama de **`0x019cce68..0x019cd01c`**, el mismo callback cuyo
[modificador de pitch](drwave246-pitch-buffer-modifier.md) ya está contrastado.
Con destino ENV y `kind == 1`, sus cuatro canales modifican la imagen común de
parámetros en `0xff801724`. Después se ejecuta el render ADSR original
`0x01912128` usando esa imagen. La reconstrucción propia está en
`Source/ControlEnvelope246.h`; permanece aislada del instrumento.

Esto verifica **callback → parámetros → render ADSR** con un llamador explícito.
No recupera el llamador original por voz, el relevo de campos pendientes, el
scheduler ni el envío de la envolvente a la salida de Dr. Wave. No resuelve el
llenado del buffer de pitch `0xff801950` desde `0xff8012d4`, bloqueado por L1
parcialmente cifrada. No se solicitan hardware ni un volcado L1 descifrado.

## Contrato acotado

El callback recibe ocho controles signed16 y cuatro fuentes de 32 bits en
`0x01dff13c`. El desplazamiento aritmético de los controles 4–7 por once bits
selecciona destino: **1** es esta ruta ENV y **2** desactiva el canal. Los
controles 0–3, desplazados aritméticamente por doce bits, seleccionan los
parámetros **0–3**: attack, decay, sustain y release del núcleo común contrastado.
Los bits inferiores de ambos selectores también se varían.

Cada canal ENV, en orden de índice, hace:

```text
half = uint16(source.low16 - 64)       # resta con wrap antes del desplazamiento
modulation = saturate16(signed16(half) * 1024)
combined = saturate16(parameter + multiplyQ15_nearest_even(modulation, 16384))
parameter = clamp(combined, lower, upper)
```

Los límites previamente observados en el inicializador común son `[64,16320]`
para attack, decay y release, y `[0,32767]` para sustain. El helper original
`0x019f69b0` recibe esos descriptores desde `0x01de5710`; se suministran
explícitamente, con guardas, y se comparan preservados. No se vuelve a presentar
ese helper, ya estudiado con Value, como un descubrimiento nuevo.

Varios canales pueden modificar el mismo parámetro **en serie**, aplicando
redondeo y límites después de cada canal. Las mitades altas de las fuentes no
intervienen. Se conservan las cuatro palabras auxiliares de la imagen ADSR.
El retorno original vale uno cuando se procesa algún canal ENV, incluso con
modulación cero, y cero en los casos inactivos cubiertos.

El adaptador rechaza destinos distintos de 1/2 y colores ENV fuera de 0–3 antes
de mutar. Son guardas propias; no emulan asserts. El color auxiliar 4 tiene
callbacks y efectos compartidos adicionales y queda excluido, al igual que
instrumento, volumen y mezcla de destinos en una llamada. Con `kind != 1`, las
rutas aceptadas preservan los parámetros; el oráculo usa `kind` 0 y 1.

## Evidencia

**1.024 llamadas / 131.072 muestras ADSR**: 512 estados independientes y ocho
secuencias de 64 llamadas que conservan estado ADSR y RNG/dither entre bloques.
Los parámetros iniciales, fuentes, volumen, estados, semillas, buffers previos y
orden se suministran explícitamente. No se infiere una cadencia de 128 muestras.

Se cubren las 16 máscaras de canales, los cuatro colores, los nueve estados
ADSR, fuentes límite y aleatorias, palabras altas independientes y 142
actualizaciones repetidas sobre un mismo parámetro. Hay 872 callbacks aplicados
y 152 inactivos; 663 resultados en el límite inferior y 707 en el superior.

El comparador verifica muestras, los 24 bytes completos del estado ADSR,
finalización, RNG y dither compartidos, parámetros completos, controles,
fuentes/vecinos, objeto del callback, descriptores y guardas de estado/salida.
Las instrucciones originales del callback, helper y render se ejecutan sin
parches y en sus direcciones SDRAM originales. Dos reproducciones privadas
finales coinciden byte a byte. También se reproduce sin `--record` el oráculo
de pitch de la PR #7: 65.536 palabras y 61.568 muestras posteriores.

Pasan **31 suites de núcleo y 33 con JUCE en Linux**, y ASan/UBSan del nuevo
comparador (LeakSanitizer desactivado por la restricción de ptrace). Windows y
macOS quedan para CI; no se ha ejecutado aquí el comparador bajo Wine.
No se validan Ableton, hardware, latencia ni ausencia de dropouts.

REA 6.8.0 aporta inspección estática del helper mediante `inspect-with-objdump`
y GNU objdump Blackfin; su evidencia privada termina con exit code 0, sin
truncación ni cambios del artefacto. La validación numérica corresponde al
simulador GNU BF524 con el parche D-register documentado. REA no descifra L1 ni
valida muestras. Firmware, instrucciones, desensamblados y ELF siguen privados.

```sh
python3 scripts/verify-control-envelope246.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/env-nuevo
cmake --build build-dsp --target control_envelope246_tests
ctest --test-dir build-dsp -R '^control_envelope246$' --output-on-failure
```

La salida debe ser nueva y externa al checkout. `--record` regenera únicamente
los dos fixtures numéricos nuevos. [Hashes y evidencia](firmware/control-envelope246-validation.json).
El siguiente límite legible es el color auxiliar 4 y sus efectos compartidos,
si su recorrido puede acotarse sin las franjas L1 cifradas. El relevo por voz,
asignador, llenado inicial de pitch y salida siguen pendientes antes de integrar.
