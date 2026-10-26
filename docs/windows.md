# Windows x64 installation

Close your DAW before installing/updating. Run `Pocket-One-0.4.0-Windows-x64-Setup.exe` and approve Windows' administrator prompt. Select the VST3 instrument, standalone application, or both.

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

Official Windows NSIS packages use the standard x86 Unicode installer container; it still installs only the x64 VST3/application and uses 64-bit Program Files/registry paths. The cloud's Linux NSIS package also supplies amd64 stubs, selected with `POCKET_AMD64_INSTALLER`. You do not need to obtain extra stubs to build the installer on Windows.

## Ableton Live does not list Pocket One

Pocket One is a **VST3 instrument**, listed as **Pocket One** under **Plug-ins > VST3 > Pocket Instruments**. It will not appear under the name OP-1 or in the VST2 list. Use a MIDI track. VST3 support requires **Live 10.1 or later**; this Windows build requires Windows 10/11 x64.

1. Close Live, run Setup.exe and leave **VST3 instrument (DAW)** selected.
2. Check that this file exists: `C:\Program Files\Common Files\VST3\Pocket One.vst3\Contents\x86_64-win\Pocket One.vst3`. The outer `.vst3` is a folder: keep its full structure. Installing the standalone EXE alone does not install a plugin.
3. Open Live's Preferences/Settings > Plug-ins and enable **Use VST3 Plug-In System Folders**. Custom VST2 folders do not control VST3 discovery.
4. Hold **Alt** while clicking **Rescan** to force a full scan, then search for **Pocket One** in the VST3 category. Allow the scan to finish.
5. If it remains absent, run `powershell -NoProfile -ExecutionPolicy Bypass -File .\packaging\windows\diagnose-ableton.ps1` from the downloaded repository. This read-only script checks the bundle path and x64 module and prints Pocket One entries from available Live logs. It does not clear preferences, delete caches or send logs anywhere.

When reporting a failure, include the Live version, Windows version, whether Setup or portable ZIP was used, and the script output. A successful standalone launch or module factory test does not prove that Live's scanner accepted the plugin. Native Ableton validation is still pending.

## Windows loader error: ordinal 345 (fixed in 0.2.1)

The original 0.2.0 cross-built EXE and VST3 omitted the embedded Common Controls v6 activation manifest. Windows could therefore resolve `COMCTL32.dll` to its legacy version, which lacks ordinal 345 (`TaskDialogIndirect`). This can prevent the standalone from starting and the DAW from scanning the VST3. Installing Visual C++ redistributables or changing VST2 folders does not address this defect.

Close Live and Pocket One, install **0.2.1** over 0.2.0, then hold Alt while clicking Live's Rescan. Both the EXE (resource ID 1) and VST3 DLL (resource ID 2) now embed the dependency. New PE-resource regression checks reject the original 0.2.0 binaries; Wine-only load tests previously missed this problem. No native Ableton compatibility claim is implied by this repair.

For user sound banks and the new hardware layout, see [user-banks.md](user-banks.md) and [hardware-interface.md](hardware-interface.md). For measured callback performance and driver settings see [realtime-audio.md](realtime-audio.md).

Version 0.4.0 reorganizes presets/samples under BROWSE; use IMPORT BANK for existing ZIP banks. See [buttons-and-browser.md](buttons-and-browser.md). IN needs a configured input device; the standalone may initially enable JUCE's "Mute audio input" option, which must be disabled to capture. Host instrument-input routing varies by DAW.
