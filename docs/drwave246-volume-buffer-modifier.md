# Rev246: modificación del buffer de volumen antes de Dr. Wave

Se reconstruye la rama de volumen de `0x019cce68..0x019cd01c`, junto a las
combinaciones de volumen/pitch/desactivación de sus cuatro canales. La rama de
volumen transforma **128 muestras signed16 ya preparadas en `0xff801d50`** y
actualiza cuatro palabras de suavizado compartidas en `0x01dd5b3c`.
`Source/ControlVolume246.h` permanece aislado del plugin.

No se atribuye a esta rutina el llenado inicial de ese buffer, la salida ADSR,
la mezcla de voz ni la identidad comercial de un modulador completo. Tampoco
resuelve el escritor original del pitch `0xff801950` desde `0xff8012d4`, cuyas
referencias candidatas siguen atravesando L1 parcialmente cifrada.

## Contrato acotado

Los selectores signed16 y las fuentes son los mismos del
[modificador de pitch](drwave246-pitch-buffer-modifier.md): destino por `>> 11`,
objetivo por `>> 12`, cuatro fuentes de 32 bits en `0x01dff13c`.

| Destino | Objetivo | Ruta cubierta |
| --- | --- | --- |
| 2 | Cualquiera | Desactivado; conserva buffers y palabra de suavizado. |
| 3 | 0 | Volumen: actualiza su suavizado y transforma el buffer signed16. |
| 3 | 1 | Pitch: transformación Q31 previamente contrastada; conserva su palabra de suavizado. |

Con `kind != 0`, estas rutas conservan ambos buffers y el suavizado. El ensayo
usa `kind` 0 y 2. El adaptador valida todos los canales antes de mutar y rechaza
otros destinos/objetivos; esas guardas son propias, no emulan asserts.

Por canal de volumen, en orden de índice:

```text
scaled = saturate16(signed16(source.low16) * 256)
smoothing[channel] = saturate16(
    multiplyQ15_nearest_even(smoothing[channel], 28672) +
    multiplyQ15_nearest_even(scaled, 4096))
for each existing amplitude sample:
    sample = saturate16(multiplyQ15_nearest_even(sample, smoothing[channel]) * 2)
```

Las dos multiplicaciones del suavizado se redondean **por separado** antes de
sumar. Se conserva el resultado por canal entre llamadas. Los canales se aplican
en serie, con redondeo y saturación tras cada uno; no se reemplazan por una única
ganancia equivalente. Las palabras altas de las fuentes no intervienen. El
retorno original vale uno cuando se aplica cualquier canal de volumen o pitch,
incluso si su fuente es cero, y cero en las rutas inactivas cubiertas.

## Corrección necesaria del modelo GNU

Esta ruta contiene un paquete que escribe mitades distintas del mismo D-reg.
El simulador stock y el primer parche D-register encolaban dos imágenes completas
del registro, perdiendo la primera mitad al aplicar la segunda carga. Esto
eliminaba la contribución de la fuente al suavizado; no se porta esa salida.

Tres paquetes propios en `research/blackfin/parallel-halfword.s` comprueban
ALU alta + carga baja, ALU baja + carga alta y dos cargas a mitades distintas.
También observan el valor anterior desde un almacenamiento paralelo. El segundo
parche GNU, separado y GPL-3.0-or-later,
`research/blackfin/gnu-13.1-halfword-writeback.patch`, combina las mitades en la
cola pendiente sin modificar los registros visibles para los otros slots.
Se aplica **después** del parche D-register existente, únicamente a las cargas
halfword de LDSTpmod/dspLDST. No modifica instrucciones originales ni pretende
validar el pipeline completo o el hardware.

Los cinco probes previos conservan sus resultados. Se reproducen **sin
`--record`** los oráculos anteriores de pitch, selector ENV/Dr. Wave, activación
VoiceCtl/Dr. Wave y ADSR (2.621.440 muestras), sin cambiar fixtures.
[Comparación de modelos](firmware/halfword-model-comparison.json).
El verificador nuevo rechaza automáticamente el simulador que pierde mitades.
Los binarios del modelo anterior se conservan separadamente en el entorno privado.

## Evidencia y reproducción

**65.536 muestras de volumen, 65.536 palabras de pitch y 61.568 muestras
posteriores de Dr. Wave**. Son 512 registros: 256 estados independientes y cuatro
secuencias de 64 llamadas. El suavizado se recarga únicamente al comenzar un
estado/secuencia; los dos buffers de entrada siguen siendo explícitos en cada
llamada. No se multiplican las salidas Dr. Wave por el volumen para inventar una
mezcla original.

Se cubren las **81 combinaciones** de canales desactivado/volumen/pitch,
selección con bits inferiores variables, fuentes y suavizados signed16,
saturación de ambos signos y muestras límite. Hay 459 callbacks aplicados y 53
inactivos; el render posterior conserva 240 actualizaciones, 241 lecturas y 31
retornos inactivos. Se comparan el suavizado completo con sus vecinos, ambos
buffers/guardas, fuentes, controles, objeto, pool de voces, ondas, cachés,
incrementos, configuración, puntero, flags, contador y bancos compartidos.

Dos ejecuciones privadas finales coinciden byte a byte. Pasan **33 suites de
núcleo y 35 con JUCE en Linux**, y ASan/UBSan del comparador (LeakSanitizer
desactivado por ptrace). Windows/macOS quedan para CI; no se ejecuta Wine aquí.
REA 6.8.0 aporta inspección estática con `inspect-with-objdump`; el oráculo
numérico es GNU BF524 con **ambos** parches documentados. No se valida DAW,
hardware, latencia, temporización ni ausencia de dropouts.

Para preparar herramientas nuevas, añade `--halfword-writeback` al
[setup Blackfin](new-account-dsp-setup.md). La opción es explícita; los setups
anteriores siguen usando solo el primer parche. En este entorno se recompiló
el simulador existente, se verificó la aplicación del segundo parche y sus
probes; no se repitió toda la descarga/compilación de GNU desde cero.

```sh
python3 scripts/setup-blackfin.py --output /privado/blackfin-volumen --halfword-writeback
source /privado/blackfin-volumen/blackfin-env.sh
python3 scripts/verify-control-volume246.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/volumen-nuevo
```

La salida debe ser nueva y externa al checkout; `--record` solo regenera los
dos fixtures numéricos de este ensayo. [Evidencia y hashes](firmware/control-volume246-validation.json).
Firmware, instrucciones, desensamblados y ELF permanecen privados. Siguen
pendientes el productor inicial de buffers, ruteo por voz, cadencia y salida
antes de integrar Dr. Wave; el próximo contrato legible puede buscarse en la
preparación de las cuatro fuentes o el destino instrumento de este callback.
