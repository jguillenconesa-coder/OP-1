# Rev246: instrumento, volumen y pitch en una misma llamada

Se contrasta el callback original **`0x019cce68..0x019cd01c`** con destinos de
instrumento, volumen y pitch **intercalados**, en el contexto explícito de
Dr. Wave/tipo interno 2. `Source/DrWave246MixedControl.h` compone las operaciones
independientes ya verificadas y mantiene su orden de canal. Sigue aislado del
plugin; no cambia IDs MIDI/automatización, sesiones, presets, UI o instaladores.

## Contrato conjunto acotado

Cada uno de los cuatro canales puede elegir:

| Categoría | Destino `>> 11` | Objetivo `>> 12` | Memoria mutable |
| --- | --- | --- | --- |
| Desactivado | 2 | Cualquiera | Ninguna. |
| Instrumento | 0 | 0–7 | Primeras cuatro palabras de parámetros; 4–7 se redirigen con modulación reducida. |
| Volumen | 3 | 0 | Buffer signed16 y palabra de suavizado de ese canal. |
| Pitch | 3 | 1 | Buffer de 128 palabras Q31. |

El ensayo recorre las **256 combinaciones ordenadas** de estas cuatro categorías
con `kind == 0`, incluida la llamada con todos los canales desactivados. También
prueba `kind == 2`, que conserva parámetros, buffers y suavizado en este alcance.
El retorno original vale uno si se aplica algún canal, aunque su fuente sea
neutra o produzca cero, y cero cuando no se aplica ninguno.

Se conservan los contratos de cada operación:

- [Instrumento](drwave246-instrument-control-routing.md): rangos originales del
  tipo 2, wrap de los tres primeros controles, clamp del cuarto, reducción de
  cuatro bits antes de multiplicar para selectores 4–7 y ocho palabras de imagen.
- [Volumen/pitch](drwave246-volume-buffer-modifier.md): cuatro suavizados
  persistentes, redondeos Q15 separados, transformaciones en serie y saturación
  entre canales; la rama de pitch conserva su aritmética Q31.

No se agrupan canales en una ganancia o desviación equivalente. El adaptador
valida todos los destinos/objetivos y los cuatro parámetros iniciales antes de
modificar cualquier imagen. Las guardas propias rechazan, sin mutar estado, una
ruta inválida situada después de otras válidas. No emulan asserts originales.
Los controles y las imágenes de destino son buffers separados en este contrato;
no se contrasta aliasing entre ellos.

Queda fuera **destino ENV 1**, que ya tiene ensayos separados de parámetros y
selector auxiliar. No se atribuye a esta API el dispatcher genérico, otros tipos
de módulo ni la totalidad del callback con todos sus modos/errores.

## Ejecución y comparación

Se reutiliza el llamador propio del ensayo de instrumento, cambiando sus entradas
explícitas. **Las instrucciones originales del callback se ejecutan completas
sin parches**, una vez por registro; no se reemplazan por cuatro llamadas a
fragmentos originales. El código C++ reutiliza las operaciones contrastadas por
canal y se compara contra esa ejecución conjunta independiente.

Hay **512 registros**: 256 estados independientes que cubren toda la matriz con
`kind == 0`, más cuatro secuencias de 64 llamadas que conservan pool, ondas,
cachés y suavizado de volumen. Los parámetros iniciales y ambos buffers se
suministran de nuevo por registro. Se ejecutan antes **2.048 decodificaciones
originales** hacia las fuentes 1–4 de la tabla de controles.

El render Dr. Wave original consume directamente los parámetros y el pitch
resultantes. Se comparan **61.568 muestras activas**, **65.536 palabras de pitch**
y **65.536 muestras de volumen**, junto a parámetros completos, rangos generados,
selección/tipos/banderas, fuentes, mensajes, suavizados y vecinos, pool completo
de seis voces, ondas, cachés, configuración, incrementos, puntero, flags,
contador, guardas y bancos compartidos.

Se cubren los ocho selectores de instrumento, las 16 máscaras activas y de cero
a cuatro canales activos. Hay **487 callbacks aplicados y 25 inactivos**; el
render conserva 240 actualizaciones, 241 lecturas y 31 retornos inactivos.
El contrato de render contrastado consume la primera palabra de pitch; las
128 se comparan separadamente como salida del callback. No se multiplica el
audio Dr. Wave por el buffer de volumen para inventar la
mezcla/salida original.

Dos reproducciones privadas finales coinciden byte a byte. Pasan **37 suites de
núcleo y 39 con JUCE en Linux**, y ASan/UBSan del nuevo comparador (LeakSanitizer
desactivado por ptrace). Windows/macOS quedan para CI; Wine no se ejecuta aquí.
REA 6.8.0 aporta inspección estática del callback con `inspect-with-objdump`;
el oráculo numérico usa GNU BF524 con ambos parches documentados y comprueba
el probe de cargas halfword. No se valida hardware, Ableton, temporización,
latencia ni ausencia de dropouts.

```sh
python3 scripts/verify-drwave246-control-mixed.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/controles-mixtos-nuevo
cmake --build build-dsp --target drwave246_control_mixed_tests
ctest --test-dir build-dsp -R '^drwave246_control_mixed$' --output-on-failure
```

Salida nueva y externa al checkout; `--record` solo regenera los dos fixtures
numéricos de este ensayo. Se reutiliza `drwave-control-instrument.s.in` porque
el llamador sirve para ambas matrices; no se modifica ese ensayo anterior.
[Evidencia y hashes](firmware/drwave246-control-mixed-validation.json).
Firmware, instrucciones, desensamblados y ELF permanecen privados.

Este avance une rutas de un callback bajo un contexto explícito; no completa
el llenado original del pitch bloqueado en L1, el productor del volumen, relevo
ADSR por voz, asignador, reloj, mezcla/salida ni integración del instrumento.
El siguiente contrato puede acotar la convivencia con destino ENV y los modos
0/1 del mismo callback, manteniendo explícitos el pool y las imágenes de trabajo.
