# Run only on an isolated Windows CI runner; this installs and removes Pocket One.
param([string]$BuildDir = "build")
$ErrorActionPreference = "Stop"
$SourceDir = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$VersionEntries = @(Select-String -Path (Join-Path (Resolve-Path $BuildDir).Path "CMakeCache.txt") -Pattern '^CMAKE_PROJECT_VERSION:STATIC=([0-9]+\.[0-9]+\.[0-9]+)$')
if ($VersionEntries.Count -ne 1) { throw "Expected one configured CMake project version. Reconfigure $BuildDir first." }
$Version = $VersionEntries[0].Matches[0].Groups[1].Value
$Setup = Join-Path $SourceDir "dist/Pocket-One-$Version-Windows-x64-Setup.exe"
if (!(Test-Path $Setup)) { throw "Installer for configured version $Version is missing: $Setup. Build it first." }
$Install = Join-Path $env:ProgramW6432 "Pocket Instruments/Pocket One"
$Plugin = Join-Path $env:CommonProgramW6432 "VST3/Pocket One.vst3/Contents/x86_64-win/Pocket One.vst3"
$Registry = "HKLM:/Software/Microsoft/Windows/CurrentVersion/Uninstall/PocketOne"
if ((Test-Path $Install) -or (Test-Path (Split-Path (Split-Path (Split-Path $Plugin))))) {
    throw "Refusing to run over an existing Pocket One installation. Use an isolated runner."
}
$Process = Start-Process $Setup -ArgumentList "/S" -Wait -PassThru
if ($Process.ExitCode -ne 0) { throw "Installer failed: $($Process.ExitCode)" }
$Artifacts = Join-Path (Resolve-Path $BuildDir).Path "PocketOne_artefacts/Release"
foreach ($Pair in @(
    @((Join-Path $Install "Pocket One.exe"), (Join-Path $Artifacts "Standalone/Pocket One.exe")),
    @($Plugin, (Join-Path $Artifacts "VST3/Pocket One.vst3/Contents/x86_64-win/Pocket One.vst3"))
)) {
    if (!(Test-Path $Pair[0])) { throw "Installed file missing: $($Pair[0])" }
    if ((Get-FileHash $Pair[0]).Hash -ne (Get-FileHash $Pair[1]).Hash) { throw "Installed file differs from build product" }
}
if (!(Test-Path $Registry)) { throw "Uninstall registration missing" }
$Process = Start-Process (Join-Path $Install "Uninstall.exe") -ArgumentList @("/S", "_?=$Install") -Wait -PassThru
if ($Process.ExitCode -ne 0) { throw "Uninstaller failed: $($Process.ExitCode)" }
if ((Test-Path $Plugin) -or (Test-Path (Join-Path $Install "Pocket One.exe")) -or (Test-Path $Registry)) {
    throw "Uninstaller left a product binary or registration behind"
}
# _?= keeps the uninstaller in place for a synchronous CI test; remove it afterwards.
Remove-Item (Join-Path $Install "Uninstall.exe") -ErrorAction SilentlyContinue
Remove-Item $Install -ErrorAction SilentlyContinue
Write-Output "PASS: installer file hashes, registration and uninstall"
