# Rev246: callback de controles tipo 2 con destino ENV

Se contrasta el dispatch por argumento **`kind`** dentro del callback
`0x019cce68..0x019cd01c`: instrumento/volumen/pitch para `kind == 0` y ENV para
`kind == 1`, en contexto explícito Dr. Wave/tipo interno 2. La API propia es
`Source/DrWave246ControlDispatch.h`; compone las operaciones contrastadas y
conserva el orden de los canales activos. No reconstruye el dispatcher MIDI
ni el scheduler de voces.

## Contrato conjunto

Las cuatro entradas conservan los selectores signed16 y las fuentes low16 ya
contrastadas. Se cubren cinco categorías por canal:

| Categoría | Destino `>> 11` | Objetivo `>> 12` | `kind` que aplica |
| --- | --- | --- | --- |
| Desactivado | 2 | Cualquiera | Ninguno. |
| Instrumento | 0 | 0–7 | 0. |
| Volumen | 3 | 0 | 0. |
| Pitch | 3 | 1 | 0. |
| ENV | 1 | 0–4 | 1. |

Se prueban las **625 combinaciones ordenadas** en **cada** valor de `kind` 0 y 1.
Una ruta válida dirigida al otro `kind` se omite: no modifica su destino ni
cuenta como aplicada. También se prueba `kind == 2`, con estas rutas inactivas.
El retorno original vale uno si se procesa algún canal aplicable y cero si no
se procesa ninguno, aunque haya canales dirigidos al otro tipo de imagen.

La implementación valida todos los selectores antes de mutar y enmascara en
una copia de controles las rutas que el `kind` no aplica. Para `kind == 0` reutiliza
[las rutas intercaladas](drwave246-mixed-control-routing.md); para `kind == 1`,
[los parámetros y selector auxiliar ADSR](drwave246-envelope-mode-control.md).
Los rechazos propios no emulan asserts originales. El contexto presupone tipo
interno 2, bandera de tipo original cero y pool de seis envolventes inicializado.
Otros tipos, construcción perezosa y errores originales quedan excluidos.

La imagen de trabajo en `0xff801724` es compartida por dirección, pero el
harness la recarga según el `kind`: controles de síntesis dentro de sus rangos
o parámetros ADSR válidos. **No se recupera aquí quién prepara esa imagen ni
cuándo cambia su función en el aparato.** Las imágenes de controles y destinos
son separadas; no se contrasta aliasing.

## Dos renderers, elegidos explícitamente por el ensayo

El llamador propio ejecuta **6.024 decodificaciones originales** y **1.506
callbacks completos sin modificar instrucciones**. Incluye 1.250 casos
independientes —625 por cada `kind`— y cuatro secuencias de 64 llamadas que
intercalan los valores 0, 1 y 2. En las secuencias se conservan los pools,
ondas/cachés, suavizado de volumen y RNG/dither. Parámetros de trabajo, buffers,
controles, volumen ADSR y eventos de voz siguen siendo entradas explícitas.

Después del callback, el harness invoca el render Dr. Wave para los registros
con `kind != 1` y el render ADSR original `0x01912128` para `kind == 1`, sobre
una voz elegida explícitamente. Esa elección y ese orden pertenecen al harness,
no a un llamador original recuperado. Los eventos explícitos de construcción,
reset/release de objetos Dr. Wave se comparan también cuando no se renderizan.

Los resultados son:

- **1.158 callbacks aplicados / 348 inactivos**.
- **763 renders Dr. Wave**: 413 actualizaciones, 303 lecturas y 47 inactivos;
  **91.648 muestras activas** coinciden.
- **743 renders ADSR / 95.104 posiciones**: **86.656 muestras escritas** y
  **8.448 posiciones preservadas** por inactividad. No se cuentan estas últimas
  como señal generada.
- **192.768 palabras de pitch y 192.768 muestras de volumen** comparadas,
  modificadas únicamente por las rutas aplicables del callback.

Además del audio se comparan los seis estados ADSR completos antes/después del
render, el pool Dr. Wave completo, palabras pendientes/ganancias, fuentes,
mensajes, parámetros y guardas, rangos originales generados, tipos/selección,
banderas, suavizados/vecinos, ondas, cachés, incrementos, configuración,
puntero, flags, contador y bancos. Se cubren los nueve estados de render ADSR.

Se capturan las 128 palabras del scratch de dither en `0xff800164`, su guarda,
RNG `0xff802bc8` y valor anterior `0x01b60534` con vecinos. En registros ADSR
coinciden con la secuencia independiente; en los demás se preservan. El flag de
pool inicializado vale explícitamente uno. Ambos selectores posibles del getter
de tipo apuntan explícitamente a 2; no se ejecutan callbacks adicionales tipo 4.

La salida ADSR se captura en un buffer propio del harness. **No se la identifica
con el productor original del buffer de volumen `0xff801d50`**, ni se mezcla
con Dr. Wave para inventar la salida de voz. El render Dr. Wave contrastado
consume la primera palabra de pitch; las 128 se comparan aparte como memoria.
Siguen pendientes los productores/relevos y la cadencia originales.

## Validación y reproducción

Dos ejecuciones privadas finales coinciden byte a byte. Pasan **38 suites de
núcleo y 40 con JUCE en Linux**, y ASan/UBSan del nuevo comparador (LeakSanitizer
desactivado por ptrace). Windows/macOS quedan pendientes de CI; Wine no se
utiliza aquí. REA 6.8.0 aporta inspección estática con `inspect-with-objdump`;
GNU BF524 con ambos parches documentados ejecuta el oráculo y comprueba los
probes halfword. No se valida hardware, Ableton, latencia o dropouts.

```sh
python3 scripts/verify-drwave246-control-dispatch.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/dispatch-nuevo
cmake --build build-dsp --target drwave246_control_dispatch_tests
ctest --test-dir build-dsp -R '^drwave246_control_dispatch$' --output-on-failure
```

Salida nueva y externa al checkout; `--record` regenera solo los dos fixtures
nuevos. El linker sitúa las capturas fuera del SDRAM original y el simulador
usa 128 MiB por su tamaño; no es una afirmación de memoria del aparato.
[Evidencia y hashes](firmware/drwave246-control-dispatch-validation.json).
Firmware, instrucciones, desensamblados y ELF permanecen privados.

El siguiente límite es localizar un llamador legible que explique la preparación
y selección de la imagen de trabajo. Si conduce a L1 cifrada se registra el
bloqueo, sin deducir una cadencia. Productor inicial de pitch, productor de
volumen, relevo de campos ADSR pendientes, asignador y mezcla/salida siguen
pendientes antes de integrar Dr. Wave; este ensayo no completa el instrumento.
