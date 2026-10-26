# Envolvente rev246 y escritura de registros paralelos

Se reconstruye independientemente la función candidata ADSR `0x01912128`.
Sus referencias, callbacks y constructor pertenecen a la vecindad identificada
por la cadena `Adsr.cpp` del firmware del usuario. No se publica su código.

## Por qué se usa otro modelo de simulador

GNU GDB 13.1 contiene una cola `STORE` para diferir escrituras de registros hasta
completar un paquete paralelo, pero algunas operaciones ALU/min/max y cargas
usan `SET_DREG` inmediato. El propio `bfin-sim.h` advierte que quedan instrucciones
por convertir. En nuestros probes independientes, una escritura y una lectura
del mismo registro en slots paralelos exponen el valor nuevo en lugar del anterior.

Esto causaba una copia LUT desplazada, pérdidas de nivel en la transición de
sustain y salidas de release fijas en vez de una caída. Se conserva el resultado
stock como evidencia: **no se incorpora esa salida defectuosa al instrumento**.
La [corrección local](../research/blackfin/gnu-13.1-dreg-writeback.patch) dirige las
escrituras D-reg paralelas a la cola existente. No modifica instrucciones originales.

Cinco probes escritos por nosotros comprueban suma, mínimo, carga/almacenamiento,
carga más almacenamiento de mitad alta y puesta a cero. Los cinco satisfacen la
lectura anterior al writeback con el modelo modificado, y muestran la discrepancia
del modelo stock. La copia LUT original pasa a coincidir con la tabla fuente sin
desplazamiento. FM/Q15 y 1.572.864 muestras Pd de los probes previos no cambian.
[Informe de comparación](firmware/dreg-model-comparison.json).

No es una validación general del simulador, de todas las combinaciones de registros,
del pipeline, del tiempo de ejecución ni del hardware Blackfin. La corrección se
limita a los hazards D-reg documentados. El simulador stock y sus fixtures se conservan.

## Qué se compara

Las tablas float en `0x018c13c4` y `0x018c15c4` se convierten con la función original
`0x01aa6fd0`. Se incluyen sus **256 observaciones numéricas Q31**; no son instrucciones
ni una imagen de firmware. El estado observado ocupa 24 bytes y contiene etapa,
contador de bloques, nivel y campos de disparo/ganancia.

- 4.096 bloques de estados/extremos/entradas deterministas: 524.288 muestras,
  coeficientes, volumen/ganancia, estado final, finalización y estado de dither.
- 16 envolventes almacenadas × 1.024 bloques: 2.097.152 muestras y 16.384 estados.
  Se conserva el estado entre bloques y se invocan nota `0x0191211a`, release
  `0x019120d0` y parada rápida `0x0191209c`. Los resets de seed están explícitos
  en el harness; no se infiere la seed del aparato.
- Las primeras cuatro palabras ADSR de los presets proceden de
  [phase-envelopes.json](../research/blackfin/phase-envelopes.json). Las otras cuatro
  se conservan como datos de investigación; su significado/ruta no se convierte.

[Metadatos completos y hashes](firmware/adsr246-validation.json).

## Reproducción

Usa una copia separada del código GNU GDB 13.1 para aplicar la corrección, con un
build separado del usado para los oráculos stock:

```sh
patch -d /ruta/gdb-13.1-copia -p1 < research/blackfin/gnu-13.1-dreg-writeback.patch
# Configurar/compilar el simulador bfin en su build separado.
# Si el build ya está configurado, reconstruir ambas capas:
make -C /ruta/build-corregido/sim/bfin libsim.a
make -C /ruta/build-corregido/sim bfin/run

python scripts/verify-envelope246.py \
  --ldr /ruta/OP1_vdk.ldr --assembler /ruta/bfin-elf-as \
  --linker /ruta/bfin-elf-ld --stock-simulator /ruta/stock/sim/bfin/run \
  --simulator /ruta/build-corregido/sim/bfin/run \
  --testutils /ruta/gdb-13.1/sim/testsuite/bfin \
  --output /ruta/nueva-carpeta-privada-fuera-del-repositorio
```

La función, imagen SDRAM y ELF solo se generan en esa carpeta privada. El script
exige el hash del LDR inventariado, prueba ambos modelos, limita los ciclos y
compara los cinco archivos de fixtures byte a byte. No ejecuta arranque ni flashea.

Con los probes privados creados antes por `verify-reference-dsp.py` usando stock:

```sh
python scripts/compare-dreg-model.py --probes /ruta/probes-stock \
  --clock-probes /ruta/probes-voz-generados-por-verify-phase-contract \
  --simulator /ruta/build-corregido/sim/bfin/run --output /ruta/nuevo-informe.json
```

El plugin no necesita GNU, REA ni firmware al ejecutarse. Integra código C++ propio
basado en estas observaciones. Sus adaptadores, normalización, mezcla y flujo de
velocidad/dither no reproducen toda la cadena de audio original. No se afirma
identidad sonora con la máquina. FM y los efectos originales siguen pendientes.

La corrección GNU es GPL-3.0-or-later, separada del código del instrumento.
[Créditos](../CREDITS.md) · [Licencia GPL](../licenses/GPL-3.0.txt).
