# Cluster: preparación de pitch, velocidad y activación de voz

Se reconstruye la rutina SDRAM **`0x01a84414`**, especializada para Cluster.
Primero despacha el note-on de Core ya contrastado; cuando se acepta, prepara
pitch y velocidad y marca la voz como activa. Se contrasta el render posterior
de Cluster con un incremento mantenido que suministra el harness.

**El coeficiente de afinación sigue siendo una entrada explícita. No se afirma
haber recuperado su tabla original, el proveedor de pitch por muestra ni el
relevo completo entre ADSR y osciladores.** El plugin instalado no cambia.

## Contrato observado

| Entrada o estado | Operación |
| --- | --- |
| Sonido cargado y seleccionado | Si Core rechaza el disparo por no coincidir, no se escriben pitch, velocidad ni flag activo. |
| Palabra en offset 12 del argumento de nota | Si es distinta de cero se usa directamente como palabra signed32 de pitch; si es cero se consulta el índice 0–127 del argumento. No se interpreta como float. |
| Tabla de 128 notas | Fuente SDRAM `0x018c21b0`, copiada a `0x01dfe530` por un fragmento del inicializador original. |
| Afinación | Lee el índice de `0x018c2184`, observado como `1200`, y su palabra en `0x027fc720 + 1200 × 4`. Ese coeficiente lo aporta explícitamente cada caso de prueba. |
| Pitch preparado | Producto Q31 con truncamiento, seguido de desplazamiento saturado de una posición. Se escribe en `0xff8012d4 + voz × 4`. |
| Velocidad | Usa la mitad baja signed16 de la palabra fuente, la desplaza ocho posiciones con saturación signed16 y escribe en `0xff801064 + voz × 2`. Para valores 0–127 resulta `valor × 256`. |
| Actividad | Escribe el byte `1` en `0xff80105c + voz`. |

La API C++ recibe la afinación como argumento obligatorio. Por ejemplo, la tabla
contiene **`21426141`** para el índice 69; con la entrada de prueba `0x40000000`
se observa **`21426140`** después de preparar el pitch. Se conserva ese bit de
truncamiento. Ese ejemplo no identifica la afinación predeterminada del aparato.

El argumento de nota y el objeto ADSR son estructuras distintas: compartir un
offset numérico **12** no establece que ambas palabras tengan el mismo consumidor.
El retorno bool del C++ informa de aceptación del adaptador; la rutina original
no tiene un retorno definido. Los guards de índices/controles inválidos también
pertenecen al adaptador, no a una emulación de los asserts originales.

## Tabla y contraste

El harness ejecuta sin cambiar sus instrucciones el fragmento **`0x01a86700`–
`0x01a8672e`**, reubicado con su bucle PC-relative y un RTS propio añadido al final.
Solo se aísla la copia de 128 palabras; no se ejecuta todo el inicializador.
La salida de esa copia coincide con los 512 bytes de su fuente. El C++ contiene
las observaciones numéricas en `Voice246NoteTable.h`, sin instrucciones originales.

- **512 casos independientes** y **ocho secuencias de 64 registros** con estado
  del pool, registros y RNG conservado entre registros.
- Los 128 índices se ejercitan en disparos aceptados; también hay pitch explícito,
  extremos de afinación, saturación de velocidad y cambios de sonido seleccionado.
- **768 activaciones aceptadas**, **256 bloqueadas** y **384 aceptadas con pitch
  explícito**. Se comprueban los seis slots, todos los objetos y rellenos, pesos,
  display, RNG, registros con guardas y los 16 bytes del argumento fuente.
- **131.072 muestras** del render original, con el pitch preparado mantenido en
  las 128 posiciones del buffer de incrementos. Ese mantenimiento es programación
  explícita del harness y no valida la interpolación, glide o modulación original.
- Solo se seleccionan las normalizaciones conocidas de 1, 4, 5, 6 y 7 osciladores.

Dos reproducciones privadas coinciden byte a byte. Pasan las **19 suites de núcleo
Linux**, la prueba nueva compilada para Windows con el toolchain MSVC y ejecutada
bajo Wine, y ASan/UBSan. LeakSanitizer se desactiva por la limitación de ptrace del
sandbox. Estas pruebas no validan temporización física, Ableton ni ASIO.
[Evidencia, hashes y límites](firmware/cluster246-activation-validation.json).

## Relevo ADSR: bloqueo localizado

En el bloque LDR **`0x206214`**, destino **`0xffa00000`**, 49.076 bytes y flags
**`0xaddb2406`**, hay patrones legibles compatibles con las direcciones del getter
ADSR y su render en `0xffa00a78` y `0xffa00b7a`. Son referencias candidatas, no una
descompilación completa ni prueba del flujo que consume las palabras pendientes.

El formato comunitario documentado cifra los primeros ocho bytes de cada grupo
completo de 24 bytes del payload. Así, los tramos **`0xffa00a80`–`0xffa00a88`** y
**`0xffa00b70`–`0xffa00b78`** están en franjas cifradas junto a esas referencias.
Faltan instrucciones necesarias para establecer argumentos y flujo completos;
el código cifrado no se ejecuta ni se rellena con instrucciones supuestas.

Siguen pendientes ese consumidor ADSR, la tabla/calibración de afinación, el pitch
por muestra, la mezcla/salida y las tres normalizaciones de Cluster. No se añade
una conexión especulativa al plugin. [Plan activo](firmware-dsp-workplan.md).

## Reproducir

```bash
python3 scripts/verify-cluster246-activation.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/cluster-activation-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target cluster246_activation_tests
ctest --test-dir build-dsp -R '^cluster246_activation$' --output-on-failure
```

El directorio debe ser nuevo, privado y estar fuera del checkout. Se publican
harness/C++ propios, fixtures numéricos y metadatos. Firmware, fragmento original,
ELF y payload L1 permanecen privados. No se genera un instalador por este delta.
