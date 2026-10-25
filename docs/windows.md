# Windows x64 installation

Close your DAW before installing/updating. Run `Pocket-One-0.2.0-Windows-x64-Setup.exe` and approve Windows' administrator prompt. Select the VST3 instrument, standalone application, or both.

- VST3: `C:\Program Files\Common Files\VST3\Pocket One.vst3` (complete bundle).
- Standalone: `C:\Program Files\Pocket Instruments\Pocket One\Pocket One.exe` and Start menu shortcut.
- Uninstall through Windows Settings > Apps > Pocket One.

Rescan VST3 plugins in your 64-bit DAW, then insert Pocket One on an instrument/MIDI track. The standalone app's Audio/MIDI settings select your devices. The installer targets Windows 10/11 x64, not Windows ARM64/32-bit. The installer is unsigned; no signing certificate is configured. No network downloads are performed by the installer. The MSVC build statically links its C++ runtime. JUCE 8 does not support MinGW; do not use it for the plugin.

To build on Windows: install Visual Studio 2022 (C++ desktop tools), CMake, Git and NSIS 3.11+. From a Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
./packaging/windows/build-installer.ps1 -BuildDir build
```

The GitHub Actions workflow builds, tests and packages an installer on Windows. Its installer smoke test uses an isolated runner, installs silently, verifies installed files, and uninstalls. It does not test a particular DAW or audio interface. A locally cross-compiled installer and a native Windows CI result must be reported separately.

Samples and four-track audio are embedded in the DAW's plugin state (24-bit WAV compressed losslessly for storage). Saving/loading large sessions can briefly block audio; do this while transport is stopped. In standalone, save/load a session with SAVE / OPEN before quitting. Tape EXPORT writes a four-channel WAV containing the raw tracks at normal speed; it does not overwrite your input samples or apply playback speed/mixer levels to the exported tracks.

Current cloud result: real Windows PE64 VST3 and standalone binaries were built using clang-cl 19.1.7 and Microsoft's CRT/SDK. Four Windows test targets and installer/uninstaller checks passed under Wine 10. This is Wine validation, not a native DAW compatibility claim. See `windows-cross-build.md` for the reproducible build route.
