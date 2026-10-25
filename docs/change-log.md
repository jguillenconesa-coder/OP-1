# 0.2.0

- Added sampler and kick/snare/hat synthesis engines.
- Added ADSR, LFO, delay feedback/time and drive parameters with dedicated encoder pages and screen graphics.
- Added four-track loop tape, overdub, per-track gain, playback/reverse speed, four-channel export and complete session save/load.
- Embedded sample/tape audio in project state and disarmed tape recording on restore.
- Added factory sounds, octave switching, MIDI sustain and pitch wheel handling, sequencer pitch writing and host PPQ/seek sync.
- Moved OUTPUT to a smoothed master stage covering tape playback.
- Added Windows NSIS installer, native MSVC CI packaging/install/uninstall checks and a one-command Windows build entry point. Generated the real Windows x64 installer using clang-cl/Microsoft CRT/SDK and verified all four Windows test targets plus installer/uninstaller behaviour under Wine.
- Expanded to three test suites and refreshed real UI screenshots.
