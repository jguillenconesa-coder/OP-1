# Prepared cloud workspace

The selected checkout is `/workspace/OP-1`. Each cloud task is isolated; use this existing checkout without creating a Git worktree.

The host system is read-only Debian 13. `scripts/cloud-setup.sh` installs CMake in `/workspace/.op1-tools`, downloads signed Debian package-index metadata from the official mirror and extracts development packages in `/workspace/op1-system/root`. Runtime shared-library links reuse installed system libraries. JUCE 8.0.6 is pinned in `/workspace/juce-sdk`. Nothing requires credentials, an audio server, or system-wide package installation.

To refresh dependencies, build and test:

```sh
bash scripts/cloud-setup.sh
```

For incremental work, source `scripts/cloud-env.sh`, then use the README's CMake build/test commands. These environment exports must be applied in every new shell used for compilation. No long-running process is required for engine or plugin smoke tests.

To capture the editor, launch `/workspace/op1-system/root/usr/bin/Xvfb :77 -screen 0 1650x900x24 -nolisten tcp`, then run the smoke executable with `DISPLAY=:77` and an absolute PNG path. The virtual display needs restarting in a new task; choose a free display number if 77 is in use. Do not stop someone else's display server. The generated screenshot exercises JUCE's rendering; it is not proof of interactive DAW compatibility.

The cloud machine has no physical audio output for listening. Test the native VST3 in a local DAW before relying on it for recording. Cross-platform CI is provided but must run on GitHub to establish Windows/macOS builds.

## Windows cross build

The prepared Microsoft SDK/CRT, LLVM 19, NSIS 3.11 and Wine 10 are retained under `/workspace/windows-tools`; the host JUCE package is in `/workspace/juce-installed`. Run `bash scripts/windows-setup.sh` to refresh the cross build and installer. Use a separate shell sourced with `scripts/windows-env.sh` for incremental Windows builds; it removes the Linux header/library overlays. `bash scripts/verify-windows.sh` runs four Windows tests and an isolated install/uninstall check, starting/stopping its own virtual display only when one is required. It refuses to overwrite an existing test installation.

The cross-built VST3 omits optional `moduleinfo.json` generation because the host helper cannot inspect a Windows DLL. Its actual factory/module are checked by `windows_module_probe`. Native Windows CI retains JUCE's usual manifest generation. See `windows-cross-build.md`.
