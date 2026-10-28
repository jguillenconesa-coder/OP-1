# Dr. Wave rev246: generación de las seis tablas base

Se reconstruye la secuencia de arranque que inicializa el estado compartido y
genera **seis bancos, 320 ondas de 256 muestras**. Las instrucciones originales
aisladas y el C++ coinciden en **1.474.560 muestras de tabla**, procedentes de
18 inicializaciones con ganancia predeterminada o explícita de prueba.

Estas tablas se calculan sumando armónicos a partir del seno ya contrastado.
No se copian presets ni muestras de fábrica. La posterior [reconstrucción del
render aislado completo](drwave246-kernel.md) conecta estas tablas a la mezcla
espectral y actualización. Los fixtures de esta página conservan su alcance de
inicialización. El plugin y el instalador siguen sin cambiar.

## Secuencia observada

El harness llama primero al inicializador compartido **`0x0197d774`**. Este pone
a cero los seis buffers de onda runtime y los cinco incrementos compartidos;
establece las tres palabras de configuración que siguen al campo de flags:

| Campo | Dirección | Valor original capturado |
| --- | --- | --- |
| Palabra de trabajo | `0x01cc1868` | 0 |
| Referencia usada al construir tablas | `0x01cc186c` | 32768 |
| Campo de ganancia usado por rutas posteriores | `0x01cc1870` | 262144 |

El primer uint32 de `0x01cc1864`, los flags de caché y sus datos adyacentes se
preservan. Esas palabras se comparan antes de sustituir opcionalmente la referencia
por una entrada de estrés. Los valores alternativos no se presentan como ajustes
originales del aparato ni parámetros de presets.

Después se llama al generador completo **`0x0197cdec`**. Copia la referencia a la
palabra de trabajo y usa un buffer runtime limpio como acumulador de 256 int32.
Tras guardar cada onda, limpia ese acumulador. Al terminar, escribe el byte `1`
en `0x01cc1865`, conservando los otros tres bytes del uint32 de flags.

| Dirección inicial | Ondas | Armónicos acumulados en la onda de índice `i` |
| --- | --- | --- |
| `0x01cc208c` | 64 | Todos los `h` de 1 a 63 con `h > i`. |
| `0x01cca08c` | 64 | Todos los `h` de 1 a 63 con `h <= i`. |
| `0x01cd208c` | 64 | Impares de 1 a 63 con `h > i`. |
| `0x01cda08c` | 64 | Impares de 1 a 63 con `h <= i`. |
| `0x01ce208c` | 32 | Impares de 1 a 31 con `h > i`, signo alterno y coeficiente dividido por `h²`. |
| `0x01ce608c` | 32 | Impares de 1 a 31 con `h <= i`, signo alterno y coeficiente dividido por `h²`. |

En las cuatro primeras, el coeficiente entero es `65536 / h`. En las últimas,
es `+65536 / h²` para `h % 4 == 1` y `-65536 / h²` para `h % 4 == 3`. La división
con signo trunca hacia cero. Se conservan por separado el producto Q16 con la
referencia, el desplazamiento aritmético de tres posiciones y el uso de la mitad
baja signed16 como ganancia del armónico. Cada producto con seno se desplaza
aritméticamente trece posiciones antes de acumularlo; cada onda guarda la mitad
baja de 16 bits. Sustituir esas operaciones por una fórmula en coma flotante o
saturar la salida cambia los resultados.

El seno runtime se carga explícitamente desde las 8.192 mitades bajas de la fuente
previamente verificada en **`0x018b6688`**, al destino **`0x0101e5e8`**. El paso de
índice por armónico es `h × 32` en esa tabla de 8.192 posiciones. Esta copia del
harness no supone que se haya ejecutado el inicializador completo de seno del
aparato ni resuelto su temporización física.

## Contraste y alcance

- Seis casos con los valores predeterminados capturados y un slot de acumulador
  distinto en cada uno, más doce ganancias explícitas: cero, límites cercanos a
  Q16, negativos y extremos signed32.
- Cada caso precarga un patrón en **172.108 bytes**, desde `0x01cc1864` hasta
  `0x01ceb8b0` exclusivo. Se comparan todos esos bytes tras las dos rutinas:
  configuración, flags/cachés preservados, tablas, seis ondas runtime, cinco
  incrementos y memoria de guarda. Se contrasta también el puntero de acumulador.
- El C++ recibe el banco de salida del llamador y no asigna memoria dinámica.
  Es trabajo de **inicialización**, todavía no destinado al callback de audio.
- Dos reproducciones privadas dan los mismos fixtures. Pasan las 21 suites de
  núcleo Linux, la prueba nueva compilada para Windows con el toolchain MSVC y
  ejecutada bajo Wine, y ASan/UBSan de la prueba nueva.

Se verifica la secuencia con acumulador limpio proporcionado por la inicialización
compartida. No se atribuye al C++ la reinicialización original sobre un acumulador
sucio. Falta la ruta que usa azul/verde/blanco para combinar y transformar las
tablas, sus cachés de trabajo y su conexión con los cinco osciladores ya
[contrastados](drwave246-reader.md). También siguen pendientes cadencia, preparación
de pitch, asignación, ADSR, salida e integración del motor completo.

El oráculo usa GNU BF524 con el parche documentado de escritura diferida de
registros D. No certifica hardware Blackfin, Ableton, ASIO ni ausencia de dropouts.
No se ha descifrado L1 ni se ha usado el DSP de WHEEL como sustituto. Las
instrucciones, imagen SDRAM y ELF originales permanecen privados.

[Evidencia y hashes](firmware/drwave246-wavebank-validation.json) ·
[Plan activo](firmware-dsp-workplan.md).

Los oráculos de [transformaciones](drwave246-wave-transform.md) y del [render
aislado completo](drwave246-kernel.md) contrastan después los callbacks, la mezcla
espectral y su despacho interno. Los fixtures de tablas base de esta página
conservan su alcance original; la integración en el plugin sigue pendiente.

## Reproducción privada

```bash
python3 scripts/verify-drwave246-wavebank.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/drwave-wavebank-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target drwave246_wavebank_tests
ctest --test-dir build-dsp -R '^drwave246_wavebank$' --output-on-failure
```

El directorio privado debe ser nuevo y estar fuera del checkout. `--record`
regenera únicamente los fixtures numéricos publicables para revisión.
