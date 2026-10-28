# Dr. Wave rev246: factoría Core, disparo y release de seis voces

Se contrastan **Core factory `0x01967960`, note-on `0x01967854` y release
`0x019678f0`** para Dr. Wave. Su reconstrucción C++ coincide con los seis objetos
antes de renderizar y con **61.568 muestras adicionales** del render completo
original, conservando ondas y cachés compartidas entre eventos.

Es un paso de integración de contratos, todavía **aislado del plugin**. Orden de
voces, contador y pitch son entradas explícitas del ensayo. No identifica el reloj
original ni completa el asignador, la calibración de afinación, ADSR o salida.

## Reglas observadas

| Ruta | Comportamiento |
| --- | --- |
| Factoría Core, tipo interno **2** | Construye seis objetos de 36 bytes y escribe seis punteros en `0x01aaad18`: `0xffb00000 + voz × 36`. El constructor de voz es `0x0197ada8`. |
| Note-on con sonido cargado igual al seleccionado | Despacha `+0x1c` de la vtable. El callback `0x0197a826` borra las cinco fases de lectura y el acumulador de inversión; conserva tag, campo 4 y slot. Core devuelve 1. |
| Note-on con sonidos distintos | Core devuelve 0 sin llamar al reset. Los IDs comparados son índices de sonido, no IDs de motor. |
| Controles de nota | Core pasa las ocho palabras del sonido cargado, desde `0xff8013c4 + sonido × 16`; el callback de reset de Dr. Wave no las utiliza. |
| Release | Core despacha `+0x2c` sin exigir coincidencia con el sonido seleccionado. El callback de Dr. Wave `0x0197a838` no modifica el objeto. |
| Render posterior | El ensayo llama al render completo con las primeras cuatro palabras del sonido cargado, pitch y contador explícitos. Las seis voces comparten ondas/cachés/configuración como en los oráculos anteriores. |

**El release del motor no representa el release de la envolvente común.** No se
debe apagar Dr. Wave únicamente en función de este callback ni inferir que el
instrumento original deja de aplicar ADSR. Esa conexión sigue pendiente.

El reset de nota tampoco reconstruye la onda compartida por sí mismo. En el
ensayo hay **149 disparos aceptados seguidos de lectura sin actualización**, 33
seguidos de actualización y 12 con render inactivo. Así se comprueba el reinicio
de fase junto a ondas ya existentes, sin imponer una actualización extra al
recibir una nota.

`Source/DrWave246Routing.h` especializa estas rutas. Sus guards de índices
inválidos son protecciones propias y no emulan los asserts del original. El bool
del adaptador de release solo indica si aceptó el índice de voz; no se atribuye
un retorno al callback void del firmware.

## Oráculo combinado

- **256 casos independientes** y **cuatro secuencias de 64 llamadas**, con orden
  de voz ascendente/descendente, cambios de sonido y cuatro mandos por sonido.
- **51 factorías Core dentro de los registros**, además de una inicial: seis
  punteros y objetos contrastados. No se confunden con una asignación MIDI.
- **194 note-on aceptados**, **86 rechazados**, **122 releases**, incluidos
  **39 con sonido seleccionado distinto**.
- **79 actualizaciones**, **402 lecturas sin actualizar** y **31 retornos
  inactivos**. Solo se cuentan las 61.568 muestras de las llamadas activas.
- Se usan los seis slots, ocho sonidos, seis variantes espectrales y tres
  transformaciones. Los contadores incluyen palabras con bit alto y cruces de
  límites bajo un orden externo explícito.
- Comparación del pool de 216 bytes **antes** del render; después, salida con
  guardas, pool, seis ondas, configuración, cachés, incrementos, punteros, flags,
  contador y 128 bytes de controles fuente. Se conservan los bancos generados.

Los objetos sintéticos cargados reciben explícitamente la vtable válida de
Dr. Wave para poder ejecutar los despachos indirectos originales. Los demás
campos proceden del fixture anterior; esta preparación no recupera el asignador.
Las rutinas Core, sus callbacks y el render se ejecutan sin modificar sus
instrucciones. El seno se prepara explícitamente como en los oráculos anteriores.

Dos reproducciones privadas coinciden byte a byte. Pasan las **25 suites Linux**,
la prueba nueva compilada para Windows/MSVC y ejecutada bajo Wine, y ASan/UBSan.
LeakSanitizer se desactiva por el límite de ptrace del sandbox. El modelo es GNU
BF524 con el parche documentado de escritura diferida de registros D; no valida
temporización física, Ableton ni ASIO.

## Apoyo de REA y distinción de IDs

REA **6.8.0** consulta en lectura `synth_types`, columnas `id` y `type`, con límite
32. Devuelve diez filas sin truncamiento: `drwave` tiene **ID de base de datos 3**.
Ese ID es distinto del **tipo interno DSP 2**, contrastado mediante la factoría.
Los slots de voz 0–5 y de sonido 0–7 son otros dos dominios separados.

No se consultan filas de presets ni se publica la base. REA aporta la observación
de identidad; el contraste de instrucciones usa Binutils y el simulador GNU.
No se atribuye a REA descifrado de L1 ni recuperación del scheduler.
[Resumen de procedencia REA](firmware/rea-drwave-routing-support.json).

## Reproducción y siguiente dependencia

```bash
python3 scripts/verify-drwave246-routing.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/drwave-routing-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target drwave246_routing_tests
ctest --test-dir build-dsp -R '^drwave246_routing$' --output-on-failure
```

Los nuevos fixtures contienen overrides de eventos/controles y observaciones
numéricas; reutilizan la entrada del render anterior. Firmware, imágenes,
instrucciones y ELF permanecen privados. No cambia el instalador ni los IDs del
plugin, y presets/bancos siguen aplazados.

Sigue pendiente el [proveedor del contador en L1](drwave246-counter.md), además
de preparación/calibración de pitch, asignación y ruteo ADSR/salida. El siguiente
paso de activación ya tiene una [ampliación VoiceCtl contrastada](drwave246-note-activation.md),
manteniendo explícita la afinación y sin completar las dependencias restantes.
[Evidencias y hashes](firmware/drwave246-routing-validation.json) ·
[Plan activo](firmware-dsp-workplan.md).
