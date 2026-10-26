# Phase 246: banco adicional de estudios del núcleo

[Descargar Pocket-One-Phase-246-Core-Studies.zip](../downloads/private-banks/Pocket-One-Phase-246-Core-Studies.zip)
· [SHA256](../downloads/private-banks/Phase-Core-Studies-SHA256SUMS.txt)

En Pocket One **0.7.0**, abre **BROWSE → IMPORT BANK** y elige el ZIP.
Después filtra por **Phase 246 core studies (experimental)**. Se añaden
**16 presets y 13 WAV únicos**; los bancos anteriores se conservan. Para
reinstalarlo desde cero, selecciona ese banco y usa **PURGE BANK**.
Las descargas de Tiny-one requieren iniciar sesión con acceso al repositorio.

Son **instrumentos muestreados de un núcleo Pd experimental**, generados con
la implementación independiente contrastada con instrucciones originales en
el simulador GNU Blackfin. Se utilizan las cuatro palabras de control guardadas
en los presets Phase del firmware 246 suministrado. REA y nuestro lector SQLite
coinciden en los 16 conjuntos de valores; esto verifica su extracción, no la
conversión que hace el aparato antes del render.

## Contenido

bass phase, bubble, classic lfo, hold me, jangle, kitt, lil critter, pass band,
pure, reflection, roar bass, rownd bass, schain bass, spring pad, valentines,
werkin it. Todos llevan la variante **Core study**.

classic lfo/spring pad, kitt/rownd bass y lil critter/pass band tienen los mismos
cuatro valores crudos. Comparten físicamente el WAV; en la máquina pueden sonar
distintos por sus envolventes, efectos y LFO. Las 16 entradas conservan sus
nombres de procedencia, sin fingir 16 fuentes de audio diferentes.

## Qué se escucha y qué falta

- Cada WAV contiene seis segundos mono PCM16 a 44,1 kHz, generado desde reset,
  con afinación de prueba MIDI 60 (A4 = 440 Hz), tabla seno fuente contrastada y
  cuatro controles constantes. Un fundido de 10 ms cierra el final del archivo.
- Se reproduce en **SAMPLER**, con **CLEAN SAMPLE**, sin delay, drive ni LFO
  añadidos por el banco. El sampler conserva su propia envolvente de amplitud.
  Son one-shots; LOOP queda desactivado para evitar un salto al reiniciar el WAV.
- Las otras notas transponen la muestra. Esto **no equivale a ejecutar el
  sintetizador Phase nativo a cada nota**: el algoritmo puede cambiar de timbre
  con la frecuencia de una forma que el remuestreo no reproduce.
- Todavía no se aplican la ruta/escalado original de los controles, ADSR, FX,
  LFO, selección de octava, preparación de frecuencia ni mezcla completa de voz.
  La afinación MIDI usada es una convención externa del renderizador de prueba.
- La discrepancia observada en la copia paralela de la LUT por el simulador
  sigue pendiente de contraste con el procesador real. Se carga explícitamente
  la tabla fuente verificada, sin presentar esto como fidelidad física completa.

Los datos de procedencia viajan en `bank.json`. Los WAV son renders de estudio,
no grabaciones de un OP-1 ni un banco de presets originales completos. Los
presets propios de Pocket One y los aportados por el usuario siguen disponibles
como material adicional.

## Verificación y reproducción

Se compararon **294.912 muestras adicionales y 2.304 estados finales** entre
el núcleo independiente y el render original simulado: 16 conjuntos de controles,
44,1/48/96 kHz, nota fija, glide y vibrato, con estado conservado entre bloques.
No hubo diferencias. El renderizador offline también coincide en **98.304
muestras** de afinación fija, cubriendo los 16 presets y las tres frecuencias de
muestreo. Los comienzos de los 16 archivos del banco coinciden con las salidas
originales de esos casos; el fundido final es una edición propia declarada.

La importación y reproducción de las 16 entradas, la salida finita/no silenciosa
y la restauración del sonido activo en otra instancia sin el banco instalado
pasan en las pruebas del plugin. Dos ejecuciones del generador producen un ZIP con bytes idénticos.
El núcleo y el renderizador se comprueban en Linux y en Windows bajo Wine.
[Resultados y límites](firmware/phase-core-studies-validation.json).

Con el código actual de `main`, el firmware suministrado y una compilación nativa:

```sh
cmake -S . -B build-research -DPOCKET_BUILD_PLUGIN=OFF
cmake --build build-research --target pd_reference_render reference_dsp_tests
ctest --test-dir build-research --output-on-failure -R 'reference_dsp|pd_renderer'
python scripts/build-phase-studies.py --firmware /ruta/op1_246.op1 \
  --renderer build-research/pd_reference_render --output Mi-banco-de-estudio.zip
```

En Windows usa la ruta del `.exe` en la configuración elegida, por ejemplo
`build-research/Release/pd_reference_render.exe`. El generador verifica el
SHA256 del firmware y la base SQLite; no descarga archivos ni ejecuta firmware.
El programa offline escribe únicamente PCM calculado por nuestro C++.
Para regenerar los oráculos de instrucciones originales se usa el
[procedimiento privado de simulación](../Tests/ReferenceVectors/README.md).

El banco funciona con el instalador **0.7.0 existente**. Esta ampliación no
cambia el audio de producción, la latencia declarada, los IDs MIDI ni los
instaladores ya publicados.
