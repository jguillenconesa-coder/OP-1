# Reproducible Windows cross build in the cloud

The produced `dist/Pocket-One-0.2.2-Windows-x64-Setup.exe` contains native Windows x64 VST3 and standalone binaries, not Linux artifacts or an installer that compiles on the user's machine.

Toolchain: clang-cl/lld 19.1.7, Microsoft's CRT 14.44 and Windows SDK 10.0.26100, JUCE 8.0.6, NSIS 3.11 (amd64 Unicode target). The C++ runtime is statically linked. MinGW is not supported by JUCE 8 and was not used to build this instrument.

```sh
# In the already prepared /workspace/OP-1 cloud checkout:
bash scripts/windows-setup.sh
bash scripts/verify-windows.sh
```

Setup downloads LLVM/NSIS/Wine via signed Debian indexes and uses the official xwin release after comparing its published SHA256. xwin downloads and verifies Microsoft's SDK packages. Existing SDK contents are reused. The helper adds filename symlinks required for Windows-style case-insensitive header/library lookup on Linux; it does not edit headers/libraries or bypass JUCE's platform checks.

Microsoft SDK download access was initially blocked. A supported network-permission grant allowed the download, and the real build then succeeded. Dependencies are retained in the filesystem. The saved network draft also includes `download.visualstudio.microsoft.com`; aka.ms remains supplied by the package-manager preset. Saved configuration and the current turn's network grant are distinct; a clean refresh still needs access to these official download services.

Incremental builds, in a separate shell:

```sh
source scripts/windows-env.sh
cmake --build build-win-msvc --parallel 3
ctest --test-dir build-win-msvc --output-on-failure
```

CMake uses `scripts/run-wine.sh` as its cross-compilation emulator. Windows tests execute under Wine 10; a virtual display is needed for GUI initialization. `verify-windows.sh` manages one when necessary and checks installed file hashes, registry registration, the installed VST3 factory, and uninstall in an isolated prefix. Wine packages are relocated into `/workspace` without changing their executable/DLL contents.

The optional VST3 JSON manifest step is disabled for cross builds because JUCE's host helper is a Linux executable. The plugin itself exports the standard VST3 factory and is explicitly loaded/probed under Wine. Native Windows builds use JUCE's normal manifest step.

Windows activation manifests are distinct from the optional VST3 JSON manifest. The EXE embeds Common Controls v6 at resource ID 1 and the VST3 DLL at ID 2 using explicit RC inputs, because llvm-mt cannot update PE resources in place. The module probe checks those resources before loading; this prevents recurrence of the ordinal 345 loader failure in the original 0.2.0 package.

These results do not establish compatibility with every Windows DAW, audio device or interactive GUI operation. The installer is unsigned; no signing certificate is configured. No repository push, CI run or public release was needed for this build.
