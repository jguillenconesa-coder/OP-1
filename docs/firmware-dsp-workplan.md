# Plan activo: reconstrucción del DSP del firmware

**Prioridad acordada: motores, filtros, envolventes y moduladores originales.**
Los presets, bancos, recetas del manual y la comparación con vídeos pasan a una
etapa posterior. Se conserva la investigación previa como referencia, sin usarla
para dirigir ajustes de sonido durante esta etapa.

## Un componente activo y criterios de cierre

Se trabaja un bloque DSP cada vez. Un bloqueo se documenta con su dependencia
concreta; no se rellena con constantes o algoritmos supuestamente originales.

1. **Contrato:** localizar entradas, controles, memoria, tablas, salidas y callbacks
   en el firmware. Separar datos observados de hipótesis.
2. **Datos:** reproducir las tablas/coeficientes usados por ese bloque, incluyendo
   inicializadores y conversiones. Un buffer rellenado por el harness es una
   entrada de prueba, no un dato recuperado.
3. **Oráculo:** ejecutar las instrucciones originales aisladas con límites,
   barridos, estados independientes y secuencias continuas. Comparar muestras y
   estado completo, incluidos RNG y efectos laterales compartidos.
4. **Reconstrucción:** implementar el bloque C++ independiente y comprobar su
   coincidencia. Repetir el oráculo en otro directorio privado y ejecutar pruebas
   Linux/Windows y sanitizadores aplicables.
5. **Integración:** introducir el bloque en el motor de audio cuando sus
   dependencias estén verificadas. Comprobar asignación de voces, orden/cadencia,
   cambios de controles, tasas, note-off/pánico y ausencia de asignaciones en el
   callback. Conservar IDs de automatización/MIDI y sesiones anteriores.
6. **Distribución:** generar instaladores cuando cambie el instrumento ejecutable.
   Una prueba de núcleo aislado no se presenta como un motor completo instalado.

## Bloque activo: Cluster rev246

| Parte | Estado |
| --- | --- |
| Render de ocho osciladores, realimentación y mezcla | Reconstrucción aislada contrastada con coeficientes explícitos del llamador. |
| Estado de 108 bytes, reset/release, RNG y contexto compartido | 2.048 bloques / 262.144 muestras; estado completo contrastado. |
| Rangos de los cuatro controles | Contrastados con el inicializador original. |
| 24 coeficientes de amortiguación | Conversión original float → Q31 contrastada. |
| Ocho coeficientes de normalización | Pendientes: inicializador lee `0xff900524`, dentro de L1 CALLBACK\|INDIRECT sin imagen descifrada validada. Las pruebas inyectan valores sintéticos identificados. |
| Cadencia, preparación de nota, envolvente/ruteo y salida completos | Pendientes. |
| Integración en el selector del plugin | Pendiente; el núcleo aún no se expone como motor original completo. |

El siguiente paso de **este bloque** es resolver la procedencia/inicialización de
la normalización y contrastar el contrato completo antes de conectarlo al plugin.
[Contrato, alcance y reproducción de Cluster](cluster246-kernel.md).

## Cola de componentes

La [matriz completa](original-op1-coverage.md) conserva el estado individual. El
orden puede ajustarse si una dependencia cifrada bloquea un componente; se registra
el motivo y el siguiente bloque activo, manteniendo un solo frente.

| Área | Trabajo pendiente que debe contrastarse |
| --- | --- |
| Motores | Cluster; después Dr. Wave/FM y demás motores según sus dependencias. Resolver las tablas L1 necesarias; no sustituir un motor por otra síntesis con el mismo nombre. |
| Filtros | Filtros internos de cada motor, ruta Filter/Nitro y etapas de salida/decimación. Cada ruta requiere coeficientes, estado e impulsos/barridos contrastados. |
| Envolventes | Completar cuatro campos ADSR auxiliares, ruteo y modos de ejecución; envolventes específicas de D-synth/D-box/drum. El ADSR común ya contrastado no las completa. |
| Moduladores | Extender Value a rutas pendientes; reconstruir Random/Tremolo y después Element/MIDI/Bend/Crank, con sus destinos, reinicios y fase libre originales. |
| Integración de voz | Preparación de pitch, poly/mono/legato/unison/portamento, asignación y reloj originales, además de mezcla/salida. |

## Aplazado

Identificación adicional y afinado de presets, bancos de samples, recetas de
sonidos, comparación con el vídeo y cambios visuales ajenos a una integración DSP
necesaria. No se añaden presets para presentar avances de esta etapa.

La coincidencia con el modelo GNU documentado prueba los casos aislados bajo ese
modelo. No certifica por sí sola la máquina completa ni temporización/ASIO reales.
