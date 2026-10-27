# Dr. Wave rev246: lectura de ondas y cinco osciladores

Se reconstruye una **ruta aislada del render original**: cinco fases, desviación
de afinación, lectura y mezcla de ondas explícitas, reset y construcción de seis
objetos. El C++ coincide con **241.920 muestras**, 2.048 pools completos y el
estado compartido de las instrucciones originales bajo GNU BF524.

**La generación y actualización de las ondas sigue pendiente.** Esta prueba
suministra seis buffers sintéticos propios; no recupera las ondas originales ni
añade Dr. Wave al selector del plugin. Los presets continúan aplazados.

## Contrato observado

| Elemento | Observación contrastada |
| --- | --- |
| Render `0x0197c870` | Procesa bloques mono de 128 muestras signed16. |
| Byte en `0x01cc1865` | Si es cero, retorna sin escribir salida, fases ni contexto de lectura. El adaptador conserva ese comportamiento, sin limpiar el buffer. |
| Contador en `0x01ab8bac` | Su resto módulo seis designa el slot que toma la ruta de actualización de onda. Se contrasta aquí el dominio no negativo signed16, 0–32767. No es la selección de sonido del usuario. |
| Slot del objeto | En esta etapa solo se ejecutan los otros cinco slots; la rama del slot de actualización se rechaza antes de mutar datos. |
| Objeto de 36 bytes | Vtable en offset 0; palabra preservada en 4; cinco fases en 8–24; slot en 28; campo auxiliar en 32. |
| Ondas en `0x01cea08c` | Seis buffers separados por 1.024 bytes; cada lectura usa una palabra de 32 bits. Las seis ondas del harness son sintéticas. |
| Pitch en `0xff801950` | Esta ruta lee **solo la primera palabra** del buffer de 128 incrementos. Las otras 127 se varían como entradas independientes en las pruebas. |
| Contexto en `0x01ceb88c` | Cinco incrementos compartidos, recalculados por llamada activa. No se convierten en cinco estados independientes por voz. |
| Semilla de recíproco | Ocho uint16 en `0x018d5100`, copiados a `0x01cc187c` cuando el flag de `0x01b60eec` es cero. Se compara la copia completa y su inicialización diferida. |
| Puntero en `0xff906a94` | Se actualiza al buffer del slot del objeto. No cambia cuando el byte de habilitación es cero. |

El índice de lectura usa la fase **anterior** al incremento. Cada fase vuelve al
dominio de 24 bits con una máscara `0x00ffffff`. La lectura de palabra se desplaza
cuatro posiciones, se suman los cinco resultados y se conserva la mitad baja de
16 bits: no se sustituye el wrap de salida por saturación ni por interpolación.

El control naranja se contrasta en `[0,32767]`. Su cuadrado produce una desviación
pequeña; dos osciladores multiplican sucesivamente por el factor y otros dos por
su recíproco aproximado. Se conservan las dos iteraciones de refinamiento y los
truncamientos enteros. En este dominio el índice de la tabla del recíproco es
siempre cero. El pitch base usa desplazamiento **con signo**, también para las
entradas negativas de estrés. Los otros tres controles no afectan esta rama;
su papel en la generación de onda todavía no se reconstruye.

El reset `0x0197a826` pone a cero las cinco fases y el campo 32, conservando
vtable, palabra 4 y slot. La factoría `0x0197ada8` construye uno de seis objetos
en `0xffb00000 + slot × 36`, con vtable `0x0190fb2c`, y devuelve su dirección.
El release `0x0197a838` no modifica el objeto; esto no elimina la dependencia
externa del ADSR común.

## Pruebas y límites

- **1.024 registros independientes** y **16 secuencias de 64 registros**,
  intercalando seis slots y conservando su contexto compartido.
- **1.890 bloques renderizados** y **158 retornos inactivos**; comparación de
  muestras, pool de 216 bytes, cinco incrementos, flag y ocho semillas del
  recíproco, puntero compartido y guardas del buffer de salida.
- **293 construcciones**, **121 resets** y **187 releases** originales. Se
  comprueban también los objetos vecinos y el puntero devuelto por la factoría.
- Pitch cero, positivo y negativo, límites de palabra, cambios de control,
  fases próximas al wrap y los 256 índices de lectura iniciales. Las ondas
  incluyen impulsos, alternancias, palabras con signo y valores deterministas.
  Se verifica que sus 6.144 bytes finales no cambian.
- Una segunda ejecución privada reproduce byte a byte los dos fixtures. Pasan
  las 20 suites de núcleo Linux, la prueba nueva compilada para Windows con el
  toolchain MSVC y ejecutada bajo Wine, y ASan/UBSan para la prueba nueva.

Los guards de índices, fases iniciales, control naranja, contador y caché inválida
son protecciones del adaptador C++, no ramas atribuidas al firmware. Una caché
marcada como inicializada debe contener las ocho semillas observadas. La ruta de
actualización de onda, inicializadores de sus tablas, cadencia externa, preparación
de pitch, asignación de voces, ADSR y mezcla/salida siguen pendientes.

La comprobación usa GNU/Binutils y el modelo BF524 con el parche documentado de
escritura diferida de registros D. No se ha arrancado el firmware completo ni
validado temporización física, Ableton o ASIO. No se ha usado REA para descifrar L1
ni se ha copiado la síntesis del proyecto comunitario WHEEL. El código, las
instrucciones y el ELF originales permanecen privados.

[Evidencia numérica y hashes](firmware/drwave246-reader-validation.json) ·
[Plan activo](firmware-dsp-workplan.md).

## Reproducción privada

```bash
python3 scripts/verify-drwave246-reader.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/drwave-reader-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target drwave246_tests
ctest --test-dir build-dsp -R '^drwave246$' --output-on-failure
```

El directorio de salida debe ser nuevo y estar fuera del checkout. `--record`
permite regenerar los fixtures numéricos para revisión, sin publicar imágenes
de firmware ni instrucciones originales.
