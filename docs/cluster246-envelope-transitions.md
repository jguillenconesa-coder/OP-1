# Dependencia de Cluster: transiciones del ADSR común

Se reconstruyen dos callbacks del ADSR común rev246: inicialización
**`0x019120ba`** y solicitud de reinicio **`0x019120fc`**. El segundo conserva
el nivel actual y solicita la etapa 6: una rampa de un bloque antes del siguiente
ataque. Se comprueban sus estados antes del render y el bloque que produce la
rutina original **`0x01912128`** después de cada evento.

**Es una dependencia de la ruta de voz que estamos investigando para Cluster,
no una validación de la conexión completa entre Cluster, VoiceCtl y el mezclador.**
Los eventos y las dos palabras pendientes son entradas explícitas del harness.
No se activa esta ampliación en el plugin ni se modifican los presets.

## Estado y reglas observadas

| Callback | Campos originales que escribe | Campos que conserva |
| --- | --- | --- |
| Inicialización `0x019120ba` | Etapa, contador, nivel y palabras en offsets 12/16 a cero; mitad baja de ganancia a `32767`. | Mitad alta de la palabra de ganancia, offset 22. |
| Reinicio `0x019120fc` | Etapa `6`, contador `0`, palabra fuente de 32 bits en offset 12, fuente de 16 bits desplazada 16 posiciones en offset 16. | Nivel y palabra de ganancia completos. |
| Note-on `0x0191211a` | Etapa `1`, contador y nivel a cero. | Palabras pendientes y ganancia. |
| Release `0x019120d0` | Etapa `4` o `5`, contador `0x10000000`, según la comparación con sustain. | Nivel, palabras pendientes y ganancia. |
| Parada rápida `0x0191209c` | Etapa `8`, contador a cero. | Nivel, palabras pendientes y ganancia. |

El estado original ocupa **24 bytes**, seis palabras. Los campos `frame`,
`retriggerStep` y `finished` del C++ son contabilidad del adaptador; no forman
parte de esa imagen. Las funciones nuevas reinician esa contabilidad para
llamadas en límites de bloque, sin atribuirla a instrucciones originales.

La etapa 6 resta `floor(nivel_inicial / 128)` en cada muestra y termina en etapa
7. Para los niveles no negativos probados, el residuo es `nivel_inicial % 128`.
En el siguiente render, la etapa 7 pone el nivel a cero y pasa al ataque.
Una segunda solicitud entre ambos bloques vuelve a entrar en etapa 6 conservando
ese residuo. El contador no avanza durante esa rampa de etapa 6.

Esta rampa reduce el nivel previo antes del nuevo ataque en los casos probados.
No se convierte en una garantía acústica de ausencia de clicks ni establece
por sí sola el momento en que debe reiniciarse el oscilador de Cluster.

Las palabras en offsets **12/16 del objeto ADSR no son los controles de preset
5–8**. Se verifica su almacenamiento, sin dar por reconstruidos sus consumidores,
su transformación desde MIDI ni los cuatro controles auxiliares del preset.

## Oráculo y cobertura

- **512 casos independientes**, incluyendo una matriz de seis acciones × nueve
  etapas previas × nueve clases de nivel en los primeros 486 registros. Las
  acciones incluyen continuar, inicializar, note-on, release, parada y reinicio.
- Niveles dirigidos: `0`, `1`, `16`, `127`, `128`, `129`, `2^30−1`, `2^30` y una
  clase aleatoria. La mitad alta de ganancia y los campos pendientes se varían.
- **16 secuencias de 128 bloques**, con controles sintéticos válidos, ataques,
  release, parada y solicitudes de reinicio consecutivas. No proceden de presets.
- **327.680 muestras**, 2.560 pares de imágenes de estado de 24 bytes, retorno de
  finalización y estado completo del RNG/dither. Hay **25 solicitudes desde etapa
  7**: nueve en la matriz independiente y 16 en las secuencias.
- Los 256 coeficientes Q31 del ADSR se generan ejecutando la conversión original
  sobre sus fuentes float, igual que en el oráculo anterior del núcleo.

Dos reproducciones privadas coinciden byte a byte. Pasan las **18 suites de núcleo
Linux**, ASan/UBSan del conjunto nuevo y su ejecutable Windows compilado con el
toolchain MSVC y probado bajo Wine. LeakSanitizer se desactiva por la limitación
de ptrace del sandbox. GNU/Wine no validan reloj físico, Ableton ni ASIO.
[Resultados, hashes y límites](firmware/adsr246-transition-validation.json).

## Qué sigue pendiente

La asignación/cadencia originales y el consumo de las palabras pendientes al
reiniciar una voz; preparación de pitch, ruteo y multiplicación de la envolvente
con Cluster; mezcla, filtrado/decimación de salida; y las tres normalizaciones
cifradas de Cluster. La nueva API conserva los valores opacos en lugar de
atribuirles un comportamiento no comprobado. [Plan activo](firmware-dsp-workplan.md).

## Reproducir

```bash
python3 scripts/verify-envelope246-transition.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/adsr-transiciones-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target envelope246_transition_tests
ctest --test-dir build-dsp -R '^envelope246_transition$' --output-on-failure
```

El directorio de oráculo debe ser nuevo y estar fuera del checkout. El script
comprueba el hash del LDR y compara los vectores numéricos publicados. Los
punteros fuente del reinicio son distintos entre sí y están fuera del estado;
no se afirma cubrir aliasing. Se publican código/harness propios, fixtures
numéricos y metadatos; firmware, instrucciones y ELF permanecen privados.
El ejecutable del plugin no cambia y no se genera un instalador por este delta.
