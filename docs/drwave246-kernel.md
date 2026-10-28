# Dr. Wave rev246: mezcla espectral y render aislado completo

El núcleo conecta **cuatro controles, seis mezcladores espectrales, inversión de
polaridad, transformaciones y cinco osciladores**. El C++ coincide con **61.568
muestras de audio** de la rutina original completa `0x0197c870`, en 512 llamadas
con estado de seis slots intercalados.

**Sigue siendo un núcleo aislado, sin integrar en el plugin.** El contador externo,
pitch, habilitación y eventos del harness son explícitos. Faltan el scheduler
original, calibración de nota, asignación, ADSR, mezcla y adaptación de salida.
El instalador no cambia y los presets siguen aplazados.

**Ampliación del contador:** el [consumidor de 16 bits](drwave246-counter.md) está
contrastado exhaustivamente, también con el bit alto activo. Otras 61.568 muestras
de render verifican cruces de 32767/32768 y 65535/0, con 79 actualizaciones.
El comparador conserva ambos oráculos: 123.136 muestras de render completo.
La referencia candidata al proveedor en L1 sigue sin resolver su cadencia.

La [ampliación de eventos Core](drwave246-core-routing.md) añade otras 61.568
muestras con seis objetos contrastados antes y después del render, factoría,
disparos aceptados/rechazados y release. El ruteo de la envolvente común sigue
pendiente; este callback de release de motor no modifica la voz.

## Flujo reconstruido

```mermaid
flowchart LR
    A[Cuatro controles raw] --> B[Preparación entera]
    B --> C[Dos familias espectrales]
    C --> D[Inversión de polaridad]
    D --> E[Transformación opcional del blanco]
    E --> F[Retención común del azul]
    F --> G[Cinco osciladores y mezcla]
```

Con el byte de `0x01cc1865` a cero, el render retorna conservando salida y estado.
En una llamada activa, el resto módulo seis del contador de `0x01ab8bac` determina
qué slot actualiza su onda. Los demás leen sus ondas existentes. La actualización
`0x0197c668` y el render se contrastan juntos, sin sustituir el despacho por una
llamada inventada al adaptador del lector.

Se mezclan dos familias adyacentes: primero se reemplaza la onda con el peso
complementario y luego se añade la otra familia. Las palabras de referencia y
ganancia posterior se preservan; se escribe la ganancia de trabajo y solo el byte
de reemplazo/adición de los flags. Cachés, semillas, configuración y cinco
incrementos se comparten entre las seis voces. El adaptador mantiene un único
estado del recíproco compartido por las transformaciones y el lector.

## Seis mezcladores espectrales

| Ruta descriptiva | Callback | Banco base |
| --- | --- | --- |
| Todos los armónicos, ascendente | `0x0197ae28` | `0x01cca08c` |
| Todos, descendente | `0x0197b1bc` | `0x01cc208c` |
| Impares, ascendente | `0x0197b55c` | `0x01cda08c` |
| Impares, descendente | `0x0197b944` | `0x01cd208c` |
| Impares alternos / cuadrado del armónico, ascendente | `0x0197bd20` | `0x01ce608c` |
| Impares alternos / cuadrado, descendente | `0x0197c1b0` | `0x01ce208c` |

Se leen los [bancos generados](drwave246-wavebank.md) y se añaden armónicos desde
el seno con coeficientes fraccionales. Se conserva el orden de productos Q16,
divisiones con signo, desplazamientos, mitades bajas y sumas con wrap. Las
variantes al cuadrado tienen órdenes distintos de multiplicación; no se reducen
a una fórmula flotante. Solo escriben armónicos añadidos para `0 < h < 64`.

Su oráculo separado cubre **768 llamadas / 196.608 palabras**: 128 casos por
callback, 384 independientes y seis secuencias de 64, con 576 reemplazos y 192
adiciones. Compara seis ondas, configuración, puntero y guardas. Los bancos deben
conservar sus 163.840 bytes. El argumento está en `[0,64×65536−1]`, la ganancia de
trabajo en `[0,65536]`, la referencia en `32768` y la ganancia posterior en
`[0,262144]`: entradas de prueba, no nuevos ajustes de mandos o presets.

## Preparación de controles

| Control raw | Operaciones contrastadas |
| --- | --- |
| Azul | Tres secciones de 8192 unidades. `24 × (azul % 8192)` determina familias actual/siguiente y fracción de mezcla. La palabra de retención vale 0 en la primera sección, sube de 0 a 65528 en la segunda y baja de 65536 a 8 en la tercera. |
| Verde | Se separan entero y fracción de `verde × 8`; el cubo de la fracción usa dos productos Q16 sucesivos. Multiplicado por 64 da el argumento espectral. Solo el entero 1 elige variantes ascendentes; los demás, descendentes. |
| Blanco | Entero y fracción de `blanco × 8`. La palabra es `max(0, floor((65536−2×abs(fracción−32768))×66191/65536)−655)`. El entero 0 aplica extremo fijo; 1, repetición; 2/3 omiten esa etapa. |
| Naranja | Desviación de cinco osciladores y avance del acumulador de inversión, con sus dos rutas enteras y truncamientos distintos. |

**La retención del azul siempre sigue a la etapa opcional del blanco.** Los tres
callbacks no son alternativas excluyentes del render. Sus [oráculos individuales](drwave246-wave-transform.md)
se conservan; la prueba nueva verifica el orden real y el estado compartido.

El acumulador de inversión usa 25 bits. En cada actualización de su voz avanza
`4 × floor((2×naranja)²/65536)` módulo `2²⁵`. Su parte alta elige el tramo negado
con wrap de 32 bits: un prefijo en la primera mitad y un sufijo en la segunda.
No avanza en lecturas sin actualización ni con el render deshabilitado.

El lector prepara cinco incrementos desde la primera palabra de pitch. Lee las
fases anteriores al avance, conserva el wrap de 24 bits y produce 128 signed16.
Su cuerpo se comparte con el adaptador anterior: las **241.920 muestras de
regresión del lector** siguen pasando.

## Contraste del render completo

- **256 casos independientes** y **cuatro secuencias de 64 llamadas**.
- **240 actualizaciones**, **241 lecturas activas sin actualización** y **31
  retornos inactivos**. Las 61.568 muestras cuentan solo llamadas activas.
- Se actualizan seis slots y se ejercitan seis variantes espectrales, tres
  transformaciones, tres secciones de azul, cuatro valores internos de verde y
  blanco, ambos tramos de inversión y **12 vueltas del acumulador de 25 bits**.
- Constructor, reset y release originales intercalados; pitch cero/positivo/
  negativo, cambios de controles, cachés reutilizadas y flags cero/no nulos.
- Cada llamada compara salida con guardas, 216 bytes de objetos, seis ondas,
  2.088 bytes compartidos/cachés, cinco incrementos, configuración de 32 bytes,
  puntero, flag del recíproco y contador con sus bytes vecinos.
- Los bancos originales generados coinciden con el C++ y no cambian. El seno se
  copia desde la fuente ya contrastada, sin ejecutar su inicializador completo.
- Dos reproducciones privadas de cada oráculo coinciden byte a byte. Pasan las
  **24 suites Linux**, las dos pruebas nuevas y el lector reutilizado compilados
  para Windows/MSVC y ejecutados bajo Wine, y ASan/UBSan de las pruebas nuevas.
  LeakSanitizer se desactiva por la limitación de ptrace del sandbox.

El dominio contrastado es azul `[0,24575]` y los otros tres `[0,32767]`. La
cobertura ampliada de verde/blanco no identifica sus límites físicos. Los guards
son protecciones del adaptador: fases de lectura de 24 bits, acumulador de
inversión de 25 al actualizar, semillas válidas y configuración dentro del
dominio. El rechazo conserva salida/estado; la deshabilitación tiene precedencia.

Las funciones no reservan memoria dinámica ni bloquean. Eso no valida el futuro
callback del plugin ni garantiza latencia o ausencia de dropouts en Ableton/ASIO.
El modelo es GNU BF524 con el parche documentado de escritura diferida de registros
D, sin arranque del firmware completo ni prueba física. No se distribuyen
firmware, instrucciones originales ni ELF.

[Evidencia espectral](firmware/drwave246-spectral-validation.json) ·
[Evidencia del render completo](firmware/drwave246-kernel-validation.json) ·
[Plan activo](firmware-dsp-workplan.md).

## Reproducción privada

```bash
python3 scripts/verify-drwave246-spectral.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/drwave-spectral-nuevo
python3 scripts/verify-drwave246-kernel.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/drwave-kernel-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target drwave246_spectral_tests drwave246_kernel_tests
ctest --test-dir build-dsp -R 'drwave246' --output-on-failure
```

Los directorios privados deben ser nuevos y estar fuera del checkout. `--record`
regenera únicamente los fixtures numéricos publicables para revisión.
