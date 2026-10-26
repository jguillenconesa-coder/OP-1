# Validation of Pocket One 0.2 in the cloud

Native environment: Debian 13 x86_64, GCC 14.2, CMake 4.4.4, JUCE 8.0.6.

## Passed

- Release Linux VST3 and standalone compilation.
- Three nonempty CTest suites: `engine`, `tape`, `plugin_smoke`.
- FM/phase/string at 44.1/48/96 kHz: audio, silence, release, A4 pitch, voice stealing and extreme macro values. Additional sampler loop/release, drum, CC64 sustain and +2-semitone bend tests run at 48 kHz.
- Four-track tape record, overdub, mix, selected-track clear, reverse playback and preservation across sample-rate conversion.
- Plugin sample-offset MIDI, finite stereo output, parameter/pattern state roundtrip, sequencer running state and host beat-position/seek/stop following.
- Sample imported from a generated WAV, original file deleted, and embedded sample successfully restored from plugin state.
- Recorded tape restored from embedded state; master output mutes tape playback. Four-channel 24-bit export is decoded and checked for expected channel count/duration, nonzero selected-track audio and silence in the other tracks.
- State restoration disarms recording to protect stored tracks.
- Actual SYNTH and TAPE editor snapshots rendered by JUCE under Xvfb. The screenshots are not concept mockups. Screenshot outputs are explicitly truncated before rewriting.
- Linux `ldd -r` found no missing dependencies or unresolved symbols.
- Windows PE64 VST3 and standalone cross-compilation with clang-cl 19.1.7, lld-link, Microsoft CRT 14.44 and Windows SDK 10.0.26100, downloaded by checksum-verifying xwin 0.10.0. One SDK header-name alias and five library-name aliases preserve Windows' case-insensitive lookup semantics without changing file contents.
- Four nonempty Windows CTest targets passed under Wine 10: `engine`, `tape`, `plugin_smoke`, `windows_vst3_module`. The last loads the actual VST3 DLL, calls its entry/factory and confirms three exported classes.
- NSIS 3.11 compiled a real amd64 Unicode Windows installer. In an isolated Wine prefix, it installed the standalone and VST3 with SHA256-identical contents and registered version 0.2.0. The installed VST3 loaded successfully. Uninstall removed the product files and registration.
- xwin itself matches the SHA256 published with its official release. No signature/TLS/checksum checks or JUCE platform guards were disabled.

## Publication CI evidence

After the initial publication, GitHub Actions compiled/tested Linux and macOS successfully. The Windows MSVC build and tests passed, but packaging exposed missing amd64 installer stubs in the official Windows NSIS distribution. Main now defaults to the standard Unicode installer container for native Windows packaging, keeping all application/plugin payloads x64; the cloud build explicitly selects its available amd64 stubs. The originally published preview installer was independently verified under Wine. Current CI results are available in the repository's Actions tab.

## Not established

- Native DAW/audio-device compatibility; CI compilation/tests and the Wine results are separate evidence.
- DAW scanning/automation, physical audio listening, interactive UI operation or hardware matching.
- Original OP-1 project compatibility, complete tape editing, production-grade anti-aliasing, or automatic standalone session persistence.

The original cloud setup script was exercised end-to-end and subsequently used for the same native tool/dependency layout. The initial source and verified downloads were published under tag `v0.2.0`; later packaging/CI fixes are on main. Build products are ignored by Git. Saved environment configuration and publication are separate from these current-machine results.

## Correction for the original Windows 0.2.0 packages

A user reported Windows loader error "ordinal 345" on the standalone and failure to discover the plugin in Live. Inspection confirmed that `/manifest:no` in the cross toolchain had dropped JUCE's Common Controls v6 dependency. Wine module loading had not exposed the missing activation context. Version 0.2.1 embeds explicit EXE/DLL manifest resources; regression checks inspect the actual PE resources before module loading. The previous Wine result must not be treated as proof that 0.2.0 started on native Windows.

## Factory bank regression coverage (0.2.1)

The processor smoke suite plays all 25 presets, checks finite/nonzero output, verifies the four embedded melodic WAVs, plays all eight slots in each of the three kits plus GM kick aliases, and restores an embedded kit session before triggering GM snare. Five Windows suites passed under Wine after the manifest and factory bank changes; installer file hashes, VST3 loading and uninstall also passed. The new manifest probe rejects the original published 0.2.0 executable. Native Ableton validation remains pending.

The updated three-suite Linux build also passed, including factory-bank playback and state coverage. Native Windows CI run 40239892035 passed processor/module/manifest tests and standalone startup, then failed at NSIS input path handling; the packaging script now uses the compiler platform’s path separator. This is separate from the original missing Common Controls activation manifest.

## 0.2.2: banco de usuario, OLED y audio

Cuatro suites Linux y seis Windows bajo Wine pasaron. Se comprobaron las once vistas OLED con cambios de telemetría, las rutas y persistencia del importador ZIP, el mapeo de raíz de muestra, la conservación del tempo/patrón al seleccionar sonidos y las 36 cargas de tiempo real. El banco privado del usuario se importó y sus 79 presets se reprodujeron con salida finita/no silenciosa; el kit activo se restauró en otra instancia sin ese banco instalado. El instalador actualizó los archivos con hashes correctos, cargó el VST3 y se desinstaló en el prefijo aislado.

La ejecución sintética Linux dio 0 bloques fuera de plazo en 18.432 callbacks y 0 llamadas de heap monitorizadas. No mide audio físico, round-trip, ni dropouts en Ableton. El CI nativo de 0.2.1, ejecución 40240430390, sí terminó correctamente en Windows, Linux y macOS, incluida la comprobación de arranque de la aplicación Windows y el instalador/desinstalador.

## 0.3.0: navegación y funciones originales

Cuatro suites Linux y seis Windows bajo Wine pasaron, además del instalador/desinstalador y la carga VST3. La cobertura nueva comprueba búsqueda y estado vacío del navegador, favoritos y slots editados al restaurar proyecto, independencia de slots de batería/synth, reimportación sin duplicados, copia/restauración de sonidos, entrada real sintética al sampler sin monitorización accidental, efecto audible del EQ y metronomo. La cinta prueba split/join, lift/drop reutilizable, undo, slide, límites de loop/chop y marcas de tomas. Se rasterizan catorce vistas OLED. Los 79 presets del banco aportado se vuelven a importar y reproducir correctamente.

La prueba de tiempo real incorpora EQ/master FX, metronomo, captura y SysEx. No encontró heap monitorizado en los callbacks; los tiempos/overruns se reportan completos en realtime-metrics-linux.json. El CI nativo de la implementación inicial 0.3.0 (1ee41cc, ejecución 40247180378) terminó correctamente en Windows, Linux y macOS. No sustituye una prueba en el Live del usuario ni valida una entrada física o su controlador.

El CI nativo con el navegador y controles pulidos (b641afe, ejecución 40248233852) también terminó correctamente en Windows, Linux y macOS. Se verificó además el render COM a WAV con dos canales, duración correcta y audio no silencioso. Se corrigió la regeneración de previews para truncar archivos PNG anteriores en vez de concatenarlos.

## 0.4.0: secuenciadores y firmware

Linux: seis suites pasan (`engine`, `tape`, `sequencer`, `firmware_inventory`, `plugin_smoke`, `realtime`). Se comprueban direcciones, aleatoriedad reproducible, swing con duración de pareja constante, PPQ negativo, arpegiador con canales/octavas y límites MIDI, liberación de gate, stop/seek del host, portabilidad de parámetros y valores por defecto de proyectos antiguos. Los slots de sonido no restauran los ajustes del secuenciador. El inventario de firmware se prueba con contenedores sintéticos y rechaza CRC incorrecto, rutas inseguras y enlaces.

Las 36 cargas de callback recorren ahora los siete modos, gate/swing y máscara euclídea junto con las cargas previas. En 18.432 callbacks se observaron cero llamadas de heap monitorizadas y cero muestras de latencia declarada. El peor p99 ocupó el 20,47 % del presupuesto y hubo dos callbacks por encima del plazo en esta VM compartida durante compilación concurrente. El temporizado es diagnóstico y no certifica un DAW/driver real; [datos](realtime-metrics-linux.json).

Firmware 246: REA 6.8.0/Unblob 26.6.4 extrajo 107 archivos regulares que coinciden por SHA256 con la lectura independiente. La cobertura REA sigue marcada parcial por cuatro bytes iniciales; el lector independiente comprueba que son el CRC del contenedor. No se han validado algoritmos DSP ni equivalencia sonora. [Registro y límites](firmware-analysis.md).

Windows x64: ocho suites pasaron bajo Wine, incluyendo carga real del VST3 y comprobación de manifiestos EXE/DLL. El instalador 0.4.0 instaló binarios con SHA256 idénticos al build; el VST3 instalado expuso tres clases. La desinstalación retiró ejecutables, bundle y registro. La suite de inventario Python usa el intérprete del host; las siete suites de ejecutables utilizan los binarios Windows. Esto no es una prueba de Ableton Live ni del dispositivo de audio del usuario.

## 0.5.0: pads y cierre SQLite en Windows

La corrección del cierre de conexiones del lector de firmware pasó el CI nativo en Windows/Linux/macOS: [run 40294659124](https://github.com/jguillenconesa-coder/Tiny-one/actions/runs/40294659124). El fallo previo de 0.4.0 afectaba a la eliminación de temporales del script Python en Windows, no al arranque del VST3. Los logs de pruebas se publican ahora en el resumen de CI.

Las pruebas nuevas ejercitan los 24 pads de cada kit de fábrica y el alias GM de kick, recorte/reversa/ganancia/afinación por pad y liberación gated. Un banco sintético de 24 se importa con ajustes individuales y se restaura sin banco externo; los kits antiguos recuperan su mapa de ocho y valores de pad neutros. El test de interfaz abre KIT EDIT, selecciona el pad 24 y verifica que su deslizador modifica `pad24_gain`. Se generan capturas reales del panel y el OLED representa 24 pads.

Las cargas de tiempo real incluyen notas hasta MIDI 83 y ajustes de los 24 pads, además de los modos de secuenciación y la carga anterior de cinta/FX. Los resultados actuales se guardan en [realtime-metrics-linux.json](realtime-metrics-linux.json); cero llamadas de heap monitorizadas y cero muestras de latencia declarada son límites de esas pruebas, no una garantía de audio sin cortes en un DAW concreto.

Se comprueba además que todos los parámetros del host tengan categoría y nombres únicos, incluidos Output: Volume y Pad 24: Pitch. El cambio conserva IDs, rangos y pasos de 0,01 de los parámetros float. La comparación de parámetros normalizados usa tolerancia numérica para ARM/macOS. El lector de cabeceras LDR se prueba con fixtures sintéticos de payload, fill, checksum y truncamiento; el ensamblador del firmware no se distribuye.

## 0.6.0: fuentes de referencia y ruta limpia

Las pruebas sintéticas del generador verifican CRC/contenedor, frecuencia raíz MIDI, 24 regiones Q31 con END inclusivo y conservación exacta del PCM dentro de los WAV. El banco real contiene 23 entradas y 336 regiones; el smoke test toca todos los pads, comprueba audio finito/no silencioso y restaura el kit sin su banco. La ruta CLEAN SAMPLE se compara desde la primera nota con una senoide PCM de 8 kHz, a velocidad y sample rate originales, con un límite RMSE de 1e-5. La prueba no establece equivalencia de envolventes/efectos del OP-1. Se mantienen los nombres MIDI y los IDs/orden de los parámetros previos.

La ejecución 0.6.0 pasó seis suites Linux y ocho suites Windows bajo Wine, incluida instalación, hashes de binarios, carga del VST3, manifiestos y desinstalación. En 18.432 callbacks de la prueba sintética se observaron cero llamadas de heap monitorizadas, un peor p99 del 15,57 % del presupuesto y seis overruns en la VM compartida. Estos tiempos no son una certificación de ausencia de dropouts en Live. No se ha consultado el último CI nativo del repositorio privado con la conexión disponible.


## 0.7.0: mapeo contextual, purga y oráculos DSP

Pasan **siete suites Linux** y **nueve Windows x64 bajo Wine**. Los casos nuevos
comprueban los cuatro sets desde la interfaz, las identidades de host fijas,
rangos/valores discretos, control con editor cerrado, reapertura y restauración
de página, páginas MIX/TAPE y selección automatizada de pista. Los parámetros
existentes mantienen IDs/orden. No se ha validado MIDI Map en Ableton real.

El importador se prueba reimportando, dañando una muestra y reparándola, contando
los directorios físicos, purgando el banco, comprobando la ausencia de archivos,
audio embebido y referencias, y volviendo a importar dos veces. La purga rechaza
cinta en marcha; se preservan el ZIP fuente y los 25 presets integrados. El test
incluye la purga de un kit de 24 pads y de un banco cuyo audio se ha borrado
del disco mientras seguía cargado en RAM. Las 23 referencias del firmware
se vuelven a importar y reproducir con salida finita, incluyendo sus 336 pads;
la vista del banco de 79 presets también se importa y se renderiza.

La suite `reference_dsp` compara 65.536 campos FM, 65.536 pares Q15, 131.072
muestras Pd estables y **1.048.576 muestras con ramas secundarias/reset**, más
los cuatro campos finales en 8.192 bloques. No hay diferencias con los oráculos
del simulador GNU Blackfin. Un script reproduce los diez archivos de evidencia
byte a byte a partir del LDR inventariado. [Alcance y discrepancia de copia de
la LUT](dsp-research.md): no es validación del aparato ni port de presets.

El instalador 0.7.0 instala binarios con hashes idénticos al build; se carga el
VST3 instalado con sus tres clases y manifiesto Common Controls v6, y la
desinstalación elimina ejecutables, bundle y registro. Windows se comprueba en
Wine, sin afirmar un resultado de CI nativo o de Live 10.2.4.

Tiempo real Linux: 18.432 callbacks / 36 cargas, cero llamadas de heap
monitorizadas y cero muestras de latencia declarada. En la medición final se
observan **cuatro callbacks fuera de plazo** y peor p99 de **21,21 %** del
presupuesto, en esta VM compartida. Las operaciones de archivo se hacen fuera
del callback. Son medidas sintéticas, no una garantía de cero dropouts ni una
medida de ida y vuelta ASIO. [Datos](realtime-metrics-linux.json).
