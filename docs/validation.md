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
