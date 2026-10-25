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

## Not established

- GitHub Actions execution. Native Windows/MSVC CI is configured but has not run; the current cross-build/Wine checks above are separate evidence.
- macOS compilation, DAW scanning/automation, physical audio listening, interactive UI operation or hardware matching.
- Original OP-1 project compatibility, complete tape editing, production-grade anti-aliasing, or automatic standalone session persistence.

The original cloud setup script was exercised end-to-end and subsequently used for the same native tool/dependency layout. Source code remains uncommitted for review. Build products are ignored by Git. Saved environment configuration and publication are separate from these current-machine results.
