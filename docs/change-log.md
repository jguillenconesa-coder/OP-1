# 0.3.0

- Replaced the flat sound menu with a searchable bank/type/variant/favorite browser and unique-sample view, source metadata, explicit load and audition.
- Added persistent independent synth/drum sound slots with edited parameter settings, stable bank IDs, duplicate import detection and multi-file sample import.
- Connected contextual tape tricks, split/join, lift/drop, clipboard-to-sampler, slide and undo. Recorded take boundaries use preallocated storage.
- Added master EQ/drive/echo pages, record/input/click levels, metronome, host-tempo bar jumps, monitored input and sampler capture.
- COM renders the tape mix through master processing or exports raw tracks. Expanded validation of catalog, slots, input, DSP, OLED and realtime heap behavior.

# 0.2.2

- Rebuilt the editor around the supplied hardware layout, raised key caps, mode icons, T1–T4, sound slots, transport and tap tempo.
- Added original OTTO-inspired animated OLED screens driven by bounded audio telemetry; 30 fps UI rendering.
- Added persistent user ZIP banks, root notes, safe import and a reproducible bank generator. Verified the separately delivered 79-preset user bank.
- Removed the keyboard mutex from the audio callback, cached parameter pointers and DSP coefficients, and avoided unnecessary string-buffer clearing and per-sample tape modulo.
- Added 36 realtime workloads with tracked callback heap checks and timing reports. Four Linux/six Wine Windows suites and installer/uninstaller checks pass. Native Ableton validation remains pending.

# 0.2.1

- Fixed Windows ordinal 345 loader failures by embedding Common Controls v6 activation manifests in EXE and VST3. Added a regression check that rejects the original 0.2.0 executable.
- Added 25 original factory presets, three eight-pad drum kits and four melodic samples (28 CC0 WAVs).
- Fixed native NSIS packaging paths/stubs. Native CI passed Windows/Linux/macOS, including Windows app startup and installer/uninstaller checks.

# 0.2.0

- Added sampler and kick/snare/hat synthesis engines.
- Added ADSR, LFO, delay feedback/time and drive parameters with dedicated encoder pages and screen graphics.
- Added four-track loop tape, overdub, per-track gain, playback/reverse speed, four-channel export and complete session save/load.
- Embedded sample/tape audio in project state and disarmed tape recording on restore.
- Added factory sounds, octave switching, MIDI sustain and pitch wheel handling, sequencer pitch writing and host PPQ/seek sync.
- Moved OUTPUT to a smoothed master stage covering tape playback.
- Added Windows NSIS installer, native MSVC CI packaging/install/uninstall checks and a one-command Windows build entry point. Generated the real Windows x64 installer using clang-cl/Microsoft CRT/SDK and verified all four Windows test targets plus installer/uninstaller behaviour under Wine.
- Expanded to three test suites and refreshed real UI screenshots.
