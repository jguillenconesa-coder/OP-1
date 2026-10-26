# Validation log — Pocket One 0.9.0

Current release: 11 Linux suites and 13 Windows suites under Wine passed.
[0.9.0 evidence and binary hashes](releases/v0.9.0-validation.json). Native Windows/Ableton/ASIO validation is pending. Historical records follow.

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


## Investigación posterior a 0.7.0: estudios Phase

Pasan las suites ampliadas `reference_dsp` y `pd_renderer` en Linux y Windows
bajo Wine. Se incorporan 294.912 comparaciones de muestras con incrementos
variables y 2.304 estados finales entre bloques consecutivos. El programa
renderer real coincide en 98.304 muestras de nota fija con los oráculos; no
sobrescribe archivos existentes. Los doce fixtures se reproducen byte a byte
con instrucciones originales aisladas y entradas explícitas.

REA y el lector independiente coinciden en los 16 registros Phase de una
lectura completa de las 169 filas de `synth_presets`. No queda establecida la
ruta original entre estos datos y los controles de render.

El nuevo ZIP contiene 16 presets de estudio y 13 WAV físicos. Se verifica el
formato PCM mono/44100/16-bit, seis segundos, raíz MIDI 60, cierre en cero y
32.768 comparaciones de muestras frente a los oráculos: las primeras 2.048
de cada una de las 16 entradas, que comparten 13 WAV. La suite del plugin importa/reproduce las 16 entradas con audio
finito/no silencioso y restaura el sonido en otra instancia sin el banco. Dos
generaciones del ZIP son idénticas. [Informe](firmware/phase-core-studies-validation.json).

Estos cambios añaden herramientas/datos de investigación y un banco compatible
con el instalador 0.7.0 existente. No modifican el motor de producción ni los
binarios publicados; no se presenta una nueva medición de latencia o dropouts.


## 0.8.0 — native Phase 246 CORE

- Release Linux and Windows x64 builds completed; VST3 identity and all prior
  host parameter IDs/indices preserved. Five new controls follow the four proxies.
- **9 Linux / 11 Windows suites under Wine passed**, including native runtime LUT
  versus verified firmware source table, tuning at 7 host rates, 16 presets at
  3 rates, release, bend, sample-offset MIDI, integer ranges, editor controls,
  closed-editor mapping, automation rerouting, legacy-project defaults,
  project/slot/clipboard roundtrips, existing engines, tape, banks and purge.
- Original isolated range initialization and seven frequency conversions were
  independently reproduced. Another 98,304 original Pd sample outputs at 88200 Hz
  and 768 block states match the reconstruction. Total Pd sample comparisons:
  1,572,864. The three new fixtures reproduce byte-for-byte with the checked-in
  harness/script and operator-supplied original LDR/GNU tools outside the repo.
- 42 callback workloads per platform / 21,504 callbacks each, no monitored heap
  calls, reported latency 0 samples. Linux: 4 deadline overruns, worst p99
  22.42% (native Phase 22.42%). Wine: 4 overruns, worst p99
  43.74% (Phase 43.74%). Timings are VM diagnostics;
  [full metrics and earlier measurement](realtime-audio.md) remain available.
- NSIS amd64 installer 0.8.0 installs binaries with matching SHA256 hashes,
  registers DisplayVersion 0.8.0, embeds Common Controls v6 manifests, loads the
  installed VST3 factory (3 classes), then removes product files and registration
  on uninstall in the isolated Wine prefix. Existing user's Windows is untouched.
- Windows standalone remained running for 30 seconds under Wine, but X11 showed
  only auxiliary/hidden windows; its main GUI is **not confirmed** in this
  headless environment. Baseline 0.7.0 likewise remains alive without a main-size
  window in the same setup. This does not establish a new startup regression or
  prove native startup. Processor/editor construction and rendering are covered
  by smoke tests; native Windows/Ableton/ASIO validation remains pending.
- [Real GUI capture](phase246-native.png), source, installer and portable assets
  accompany the version. No original firmware/code/decryption keys are packaged.

The NCO, fixed-rate adapter, smoothing, output filter, envelope, LFO and FX are our
integration. Original full note preparation, encrypted L1 routines, full preset
behavior, aliasing limits and acoustic fidelity are not validated. This does not
certify zero dropouts or an exact OP-1 replica. [Scope](phase246-native.md).


## 0.9.0 — reconstructed envelope, String tuning and reliable panic

- **11 Linux / 13 Windows suites under Wine passed.** The expanded realtime suite
  was subsequently rebuilt and passed with both native envelope variants (48 workloads).
- **2,621,440 ADSR samples / 20,480 complete states**, 256 converted coefficients,
  all nine observed stages and original note/release/fast-stop callbacks agree
  with the documented deferred D-register simulator model. The stock hazard
  trace is retained separately. Five independently written packet probes check
  the model change; original firmware instructions are unmodified and remain private.
- Existing **1,572,864 Pd samples**, FM field and Q15 probes retain their results
  under that model. The original LUT-copy probe now matches the unshifted source.
- Native envelope timing at seven host rates; all 16 presets with both envelopes
  at three rates; sustain/pedal release, MIDI timestamps, exact block-partition
  invariance with bend/pedal/FX, edited slots/clipboard/session roundtrips and
  0.8.0 project/slot migration. New parameter IDs/indices are appended only.
- String: 36 frequency/bend/rate configurations within the three-cent test bound,
  safe poisoned-buffer reuse and early downward bends. The test's autocorrelation
  estimate is not a hardware/acoustic accuracy claim.
- Explicit FILTER LFO attenuates clean sampler/native Phase test tones; dry CLEAN
  regressions remain passing. CC120/panic silence output histories exactly in
  the test, while ordinary preset changes preserve the master tail. Delay reset
  invalidates history instead of clearing large buffers in the callback.
- 48 workloads / 24,576 callbacks per platform, no monitored callback heap calls.
  Final Linux: 4 deadline overruns, worst p99 14.20% of deadline;
  Wine: 7 overruns, worst p99 25.25%. [Metrics](realtime-audio.md).
- 0.9.0 installer installs matching executable/VST3/source files, registers the
  version, loads its VST3 factory, preserves Common Controls v6 manifests and
  removes files/registration in the isolated Wine prefix. Source/cache exclusions
  and versioned package hashes are checked before publication.

Native Windows startup, Ableton 10.2.4/ASIO, physical dropouts, complete firmware
routing/mixing, FM/FX and acoustic equivalence remain unvalidated. The standalone
main window was not confirmed under headless Wine in the previous baseline;
passing the processor/editor or manifest suites is not proof of native app startup.
No original firmware images/instructions or GNU simulator binary are distributed;
coefficient observations, numerical fixtures and the separately licensed patch are included.
