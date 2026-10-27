# Cluster: normalización parcial y constructor original

Se han localizado **cinco de las ocho entradas** y contrastado su conversión Q15
con el helper original. También se reconstruye el constructor de sus **seis slots
de voz**, con comparación completa de memoria antes y después del reset.

## Qué se pudo leer de L1

La descripción comunitaria del formato indica que, en cada grupo completo de
24 bytes del payload CALLBACK|INDIRECT, se cifran los primeros ocho y los otros
16 permanecen intactos. Esos grupos se cuentan desde el comienzo del payload,
no desde la dirección de la tabla. No se ha obtenido una clave ni descifrado L1.

La tabla comienza en `0xff900524`, offset `0x134` del bloque de 656 bytes que
empieza en `0xff9003f0`. Aplicando ese modelo, cinco palabras están fuera de las
franjas cifradas; las otras tres se mantienen desconocidas. El harness solo copia
las cinco palabras legibles y ejecuta `0x01aa7014`, la conversión que usa el
inicializador original.

| Número de osciladores | Offset dentro del payload | Offset módulo 24 | Float legible | Q15 contrastado |
| ---: | ---: | ---: | ---: | ---: |
| 1 | `0x134` | 20 | 0,5 | 16384 |
| 2 | `0x138` | 0 | Pendiente | Pendiente |
| 3 | `0x13c` | 4 | Pendiente | Pendiente |
| 4 | `0x140` | 8 | 0,125 | 4096 |
| 5 | `0x144` | 12 | 0,10000000149 | 3276 |
| 6 | `0x148` | 16 | 0,08333333582 | 2730 |
| 7 | `0x14c` | 20 | 0,07142857462 | 2340 |
| 8 | `0x150` | 0 | Pendiente | Pendiente |

La pauta de los cinco floats permite formular una hipótesis sobre los restantes,
pero no verifica sus bytes originales. **No se extrapolan los valores faltantes.**
La API `cluster246ObservedNormalization` devuelve un valor opcional y deja sin
valor los casos 2, 3 y 8. Los ceros de los huecos en los vectores de prueba son
marcadores, nunca coeficientes originales ni entradas seleccionadas.

Para estas entradas, el resultado del helper tampoco coincide siempre con
redondear al entero más cercano: por ejemplo, 0,1f produce **3276**, no 3277.
Se conservan los resultados observados de la conversión.

## Constructor y reset

La factoría `0x019607e4` devuelve uno de seis objetos de 108 bytes a partir de
`0xffb00000`. Se contrastan 60 casos, rotando los slots y usando memoria inicial
de distintos patrones y aleatoria. Se comprueban el puntero devuelto, los seis
objetos completos, los bytes de relleno conservados y el reset posterior con RNG.
Los demás slots permanecen iguales. El guard C++ rechaza índices inválidos sin
modificar el pool; no reproduce la ruta assert del firmware.

Esto verifica la construcción de objetos, **no el asignador completo de voces**,
los modos poly/mono ni la cadencia del sistema. Ocho osciladores por voz y seis
slots son cantidades distintas; la polifonía del plugin no se modifica aquí.

## Evidencia adicional

- 60 casos de factoría/reset con comparación completa de memoria y semilla.
- 640 bloques continuos / **81.920 muestras** usando únicamente los cinco
  coeficientes legibles y la amortiguación original ya contrastada. Se barren los
  demás controles, se dispara release y se actualiza la visualización.
- Se vuelven a comprobar las **262.144 muestras** de estrés anteriores; ambos
  conjuntos suman 344.064 muestras, con sus estados/contextos completos.
- Dos ejecuciones independientes del oráculo; 15 suites de núcleo Linux,
  las dos suites Cluster bajo Windows/Wine y ASan/UBSan del conjunto nuevo.

[Evidencia, procedencia y hashes](firmware/cluster246-init-validation.json) ·
[Contrato actualizado](../research/blackfin/cluster246-contract.json).

La clasificación de palabras legibles sigue el formato documentado por los
proyectos de referencia citados en la evidencia; no valida el boot completo ni
reemplaza una imagen L1 descifrada. No se distribuyen los payloads, las instrucciones
originales ni el firmware.

## Reproducir

```bash
python3 scripts/verify-cluster246-init.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/cluster-init-nuevo
```

El directorio debe ser nuevo y privado, fuera del checkout. El test CMake es
`cluster246_init`. Las tres entradas cifradas y la integración completa continúan
pendientes en el [plan DSP](firmware-dsp-workplan.md); no se añaden presets.
