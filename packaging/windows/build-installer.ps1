param([string]$BuildDir = "build", [string]$Configuration = "Release")
$ErrorActionPreference = "Stop"
$SourceDir = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Artifacts = Join-Path (Resolve-Path $BuildDir).Path "PocketOne_artefacts/$Configuration"
foreach ($item in @("VST3/Pocket One.vst3/Contents/x86_64-win/Pocket One.vst3", "Standalone/Pocket One.exe")) {
    if (!(Test-Path (Join-Path $Artifacts $item))) { throw "Missing Windows build product: $item" }
}
$Nsis = (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
if (!$Nsis) { $Nsis = "${env:ProgramFiles(x86)}/NSIS/makensis.exe" }
if (!(Test-Path $Nsis)) { throw "Install NSIS 3 (for example: choco install nsis)." }
$OutputDir = Join-Path $SourceDir "dist"
New-Item -ItemType Directory -Force $OutputDir | Out-Null
& $Nsis "/DARTIFACT_DIR=$Artifacts" "/DSOURCE_DIR=$SourceDir" "/DOUTPUT_DIR=$OutputDir" (Join-Path $PSScriptRoot "PocketOne.nsi")
if ($LASTEXITCODE -ne 0) { throw "NSIS failed with exit code $LASTEXITCODE" }
