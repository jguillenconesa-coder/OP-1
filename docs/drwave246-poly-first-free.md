# Dr. Wave rev246: llamador polifónico y primera voz libre

Se ejecuta el llamador original **`0x01a84604`** desde su entrada, en la rama
polifónica de **primera voz libre**. La siguiente rutina comienza en
`0x01a84ba0`; no se extraen ni parchean instrucciones del llamador. El alcance
numérico excluye robo de voces, otros modos, módulos auxiliares presentes,
construcción ADSR diferida y caminos de assert.

El avance es que la selección por actividad, inicio ADSR, ganancia por nota,
llamada de activación y **adición a la lista** ocurren dentro del firmware
original. La cadena anterior elegía la voz y sembraba la lista explícitamente.
Aquí la imagen inicial de lista sigue siendo del harness, pero la nueva entrada
la escribe el llamador original, sin añadirla ni corregirla después.

## Contexto y orden observado

| Dependencia | Contrato del ensayo |
| --- | --- |
| Modo VoiceCtl `0xff801038` | Cero. |
| Selector por sonido | Word signed16 en `0x01b60ef0 + seleccionado × 16 + 8`, desplazado 12 bits: cero. Se varían sus bits inferiores. |
| Flag `0x01dfe730` | Se lee **un byte**. Los otros tres bytes vecinos tienen patrones no nulos y se preservan. |
| Nota entrante | Argumento original construido de 16 bytes; nota 0–127 y pitch explícito cero. |
| Lista | Hasta 128 entradas para retornos tempranos; menos de 128 si se va a añadir una nota nueva. |
| Actividad | Seis bytes explícitos por llamada, con al menos uno cero. Valores no nulos 1, 128 y 255 cuentan como ocupados. |
| Core/ADSR | Vtables Dr. Wave y punteros Core válidos; flag ADSR ready uno. |
| Despacho auxiliar `0x019673dc` | Seis punteros en `0x01aaad6c` nulos; puntero secundario `0x01aaac44` nulo. No se ejecutan callbacks auxiliares. |

Primero se comprueba el byte del flag: si es no nulo, retorna sin efectos del
llamador. Con flag cero busca una coincidencia de **palabra completa** de nota
en la lista y retorna si ya existe. Se ensayan entradas con los mismos bits
bajos y bits altos diferentes para detectar comparaciones truncadas.

Sin duplicado y en modo polifónico, recorre actividad desde la voz 0 y usa la
primera cuyo byte sea cero. Puede consultar los contadores ADSR de las voces
ocupadas anteriores; en esta rama no los modifica ni usa su antigüedad para
elegir otra voz. La rama de seis voces ocupadas queda fuera.

Para la voz libre, el llamador original:

1. Obtiene su ADSR por `0x0191264c` y ejecuta note-on `0x0191211a`: etapa 1,
   contador y nivel a cero, preservando palabras pendientes y ganancia anterior.
2. Calcula en binary32 `1 - nota / 256`, lo convierte con el helper Q15 original
   `0x01aa7014` y escribe solo la mitad baja de la ganancia. Para notas 0–127,
   el C++ usa el resultado exacto `32767` para nota cero y `32768 - nota × 128`
   para las demás. La mitad alta se conserva; no se extrapola esta fórmula
   fuera del dominio ensayado.
3. Llama al despacho auxiliar vacío y a VoiceCtl `0x01a84414` con la voz elegida.
4. Añade una entrada de 20 bytes a la lista y aumenta el contador. Copia nota,
   velocidad, byte de metadata, pitch y voz; conserva el relleno 9–11 del destino.

**El rechazo Core por diferencia de sonido no revierte ADSR ni la inserción.**
Se conserva la activación parcial observada: ADSR/gain y lista cambian, mientras
Dr. Wave y los registros de pitch/velocidad/actividad de esa voz se conservan
cuando la activación interna queda bloqueada. No se añade rollback ni se
interpreta este contexto como una política completa de carga del instrumento.

`Source/DrWave246PolyFirstFree.h` implementa ese contrato. Su resultado informa
al adaptador de retorno temprano, voz elegida y aceptación Core; el original no
tiene ese retorno semántico. Los guards de contexto/capacidad/falta de voz libre
son propios y evitan mutaciones fuera del contrato, sin emular asserts o robo
de voces. El helper presupone los modos y la ausencia de auxiliares indicados: esos
globales se preparan en el harness y no son parámetros de su API. No se expone
como un asignador completo del plugin.

## Ensayo y comparación

Los setters originales suministran la tabla de ajustes, seguidos del constructor
original. Se pasa el mismo argumento al llamador polifónico. Después se captura
el estado y se renderiza un ADSR elegido explícitamente por el harness, que en
los casos de inserción coincide con la voz libre preparada. No se escribe la
lista después del llamador.

- **768 registros**: 512 independientes y cuatro streams de 64 llamadas.
- **480 inserciones**: 309 con aceptación Core y **171 con Core bloqueado**;
  todas las notas 0–127 aparecen en inserciones aceptadas y se eligen las seis
  voces. Se ensayan varias voces libres para comprobar que gana la primera.
- **128 retornos por flag y 160 por duplicado**, incluidos duplicados reales
  creados por una inserción previa dentro de cada stream.
- Casos de lista con 127 entradas que pasan a 128, y retornos tempranos con
  128 entradas. La ruta de desbordamiento no se ejecuta.
- **98.304 posiciones ADSR posteriores**: **94.720 escritas y 3.584 conservadas**.
- **4.568 bytes de imagen completa antes/después** del render: lista y contador,
  objetos Dr. Wave, seis ADSR/scratch y guardas, registros VoiceCtl, parámetros,
  RNG/valor anterior, selección/sonido cargado, punteros, afinación, discriminador,
  ajustes y vecinos. Se añaden las tablas/punteros auxiliares con sus vecinos.
- Se compara además la tabla de ajustes con vecinos justo después de los
  setters (48 bytes), argumento construido, otras entradas del harness, salida
  con guardas y retorno ADSR. La tabla de 128 notas se captura antes y después.

Los streams retienen lista, objetos, ADSR/scratch, registros y RNG, con **una
excepción explícita: el harness repone los seis bytes de actividad por llamada**
para garantizar este contexto de primera voz libre. Eso no reconstruye la ruta
original que libera actividad ni un scheduler. Los valores de las entradas
previas de lista son sintéticos y no se presentan como notas musicales válidas.

Los objetos Dr. Wave tienen vtables válidas y otros campos de estrés; no se
presentan como estados renderizables. No se llama a su renderer de audio. El
render ADSR posterior y la secuencia setter/constructor/llamador son orden del
harness; solo los pasos internos del llamador son la conexión original aquí
contrastada. La imagen vigilada no es toda la RAM, pila o CPU del aparato.

`0x01dfe72c` sigue siendo la última palabra de la tabla de notas y
`0xff800164` scratch ADSR, no guardas independientes.

## Validación y reproducción

Dos ejecuciones privadas coinciden byte a byte. Pasan **47 suites de núcleo**
y **49 con JUCE** en Linux y ASan/UBSan del nuevo comparador (`detect_leaks=0`
por el límite ptrace). Windows/macOS quedan pendientes de CI. No se valida
hardware, DAW/ASIO, latencia ni ausencia de dropouts.

REA 6.8.0 inspeccionó privadamente el símbolo sintético del llamador mediante
`inspect-with-objdump` y GNU objdump Blackfin, sin truncamiento ni cambio del
artefacto. La comparación numérica usa GNU BF524 con los dos parches de writeback
y el preflight halfword propio. REA no descifra ni valida muestras.

```bash
python3 scripts/verify-drwave246-poly-first-free.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-halfword-v2 \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/poly-libre-nuevo
cmake --build build-dsp --target drwave246_poly_first_free_tests
ctest --test-dir build-dsp -R '^drwave246_poly_first_free$' --output-on-failure
```

Fixtures: 768 registros de **4.880 bytes de entrada** y **9.500 de resultado**.
La salida añade la tabla de notas de 512 bytes al principio/final. Cada resultado
contiene dos imágenes de 4.568, salida/guardas de 272, retorno ADSR de 4, captura
de ajustes de 48 y cabecera final/argumento de 40 bytes. Se publican números y
código propio; firmware, instrucciones, desensamblados y ELF quedan privados.

La consulta `0x01a863a8` sigue reservada a la otra cuenta y no se ejecuta aquí.
El siguiente tramo legible puede ampliar el llamador a seis voces ocupadas,
delimitando el robo/reinicio por separado. Siguen pendientes otros modos,
módulos auxiliares, productores de buffers, relevo ADSR, reloj, mezcla y salida.
No se cambian MIDI, automatización, sesiones, presets ni interfaz.

[Evidencia](firmware/drwave246-poly-first-free-validation.json) ·
[Cadena anterior](drwave246-adjusted-note-chain.md) ·
[Activación](drwave246-note-activation.md) · [Plan](firmware-dsp-workplan.md).
