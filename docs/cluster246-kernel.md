# Cluster rev246: núcleo aislado con coeficientes explícitos

Se reconstruyen el render mono de **ocho osciladores**, el reset/release y la
actualización de visualización. El C++ coincide con **262.144 muestras y 2.048
estados completos** ejecutados a partir de las instrucciones originales aisladas.
Se comparan también el contexto compartido y la semilla RNG.

**Faltan tres entradas de normalización de L1 y la integración completa.** El motor
no se añade todavía al selector del plugin. Las pruebas iniciales inyectan ocho
coeficientes identificados como sintéticos. La [ampliación de inicialización](cluster246-normalization.md)
añade cinco entradas legibles convertidas por el helper original, 81.920 muestras
con esos valores y el constructor de seis slots. No se han usado presets ni vídeo.

La [rutina de inicialización compartida y sus conversiones](cluster246-shared-init.md)
también están contrastadas bajo un contrato de ocho palabras fuente suministradas.
Esto cierra la lógica observada de esa rutina, sin recuperar los tres valores L1
que siguen cifrados ni completar la integración del instrumento.

La [ampliación de rutas Core y voces intercaladas](cluster246-core-routing.md)
contrasta el disparo condicionado al sonido cargado/seleccionado, la liberación sin
esa condición y otras 131.072 muestras conservando el estado de los seis slots y
su RNG compartido. El scheduler, ADSR y mezclador completos siguen pendientes.

## Contrato reconstruido

| Entrada/estado | Alcance |
| --- | --- |
| Cuatro controles signed16 | Número de osciladores, morph/realimentación y controles de la actualización de desviaciones, según las operaciones observadas. No se atribuyen nombres originales a todos los campos. |
| 128 incrementos de fase uint32 | Entradas del llamador; no se reconstruye aún la preparación original de nota. |
| Tabla seno de 8.192 muestras | Misma fuente previamente contrastada; copiada por el harness a su destino runtime. |
| Ocho coeficientes de normalización signed16 | Pruebas iniciales con entradas sintéticas; ampliación con cinco entradas legibles. Las de 2, 3 y 8 osciladores siguen pendientes. |
| Tabla de amortiguación | Pruebas de estrés con 32 palabras arbitrarias; 24 palabras originales Q31 verificadas separadamente y usadas en las secuencias continuas. |
| Objeto de 108 bytes | Ocho fases/feedbacks, desviaciones y ganancias por oscilador, campos auxiliares y flag de release; se conservan también bytes de relleno. |
| Contexto compartido | Ocho pesos, ocho valores de visualización y semilla LCG de 32 bits; no se transforma en un estado por voz sin validar el ruteo. |

El render aplica productos fraccionales redondeados/saturados, sumas con wrap o
saturación según la ruta, suavizado de ganancias, morph de realimentación,
incrementos dependientes de desviación por oscilador, mezcla saturada y actualización
del RNG. Los callbacks de reset/release y decay de visualización se comprueban
en las mismas secuencias.

El núcleo acepta un dominio de prueba más amplio que los mandos físicos, manteniendo
los índices seguros. Los descriptores originales para los primeros cuatro mandos
son `[3072,17408]`, `[0,32767]`, `[512,24064]` y `[3,1638]`. No se convierten
automáticamente en normalizaciones del host en esta etapa. Las entradas fuera del
dominio seguro se rechazan sin modificar estado, contexto ni buffer de salida;
ese guard es una protección del adaptador C++, no una nueva rama atribuida al firmware.

## Coeficientes y dependencia pendiente

La tabla de amortiguación procede de **24 floats en `0x018befb0`**, transformados
por el helper original **`0x01aa6fd0`** y escritos en `0x0106a954`. El harness
ejecuta esa conversión y contrasta las 24 palabras Q31 con la reconstrucción.

La normalización se inicializa en **`0x019608e0`**, que lee ocho floats de
**`0xff900524`** y los convierte con `0x01aa7014` a la tabla de `0x01baa91c`.
La fuente pertenece al bloque LDR en offset `0x1fe870`, destino `0xff9003f0`,
656 bytes, flags `0xad812406` (**CALLBACK|INDIRECT**). No se dispone de su imagen
descifrada completa validada. El formato documentado deja intactos 16 de cada
24 bytes: la ampliación usa solo cinco floats situados en esas franjas y contrasta
su conversión. Los tres floats en las franjas cifradas siguen desconocidos; no se
rellenan suponiendo una fórmula de normalización. [Detalle](cluster246-normalization.md).

[Contrato numérico y direcciones](../research/blackfin/cluster246-contract.json) ·
[Evidencia y hashes](firmware/cluster246-validation.json).

## Pruebas

- **512 bloques independientes de estrés:** estados y tablas inyectados, límites,
  cambios de signo/saturación, incrementos variables y callbacks.
- **1.536 bloques continuos:** 24 secuencias de 64 bloques con la amortiguación
  original verificada y coeficientes sintéticos de normalización fijos por secuencia.
  Se barren controles, se dispara release y se actualiza el display.
- Cada bloque contrasta 128 muestras, 108 bytes del objeto, 32 del contexto y la
  semilla completa. La cabecera verifica los descriptores y 24 coeficientes.
- Validación de rechazo de controles fuera del dominio seguro, sin mutación.

El modelo de referencia es GNU BF524 con el parche de escritura diferida de
registros D ya documentado. El oráculo no arranca el firmware completo ni prueba
el reloj/allocator original. Se mantienen las etapas de integración y comprobación
física como pendientes explícitos en el [plan DSP](firmware-dsp-workplan.md).

## Reproducción privada

```bash
python3 scripts/verify-cluster246.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/cluster-nuevo
```

El directorio debe ser nuevo y estar fuera del checkout. El harness, los vectores
numéricos y el C++ son independientes; no se distribuyen el firmware, instrucciones
originales, ELF ni los datos L1 cifrados. La entrada usa registros de 812 bytes;
la salida incluye 160 bytes de descriptores/coeficientes y 400 bytes por bloque.

```bash
cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target cluster246_tests
ctest --test-dir build-dsp -R '^cluster246$' --output-on-failure
```
