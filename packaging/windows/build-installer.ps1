param([string]$BuildDir = "build", [string]$Configuration = "Release")
$ErrorActionPreference = "Stop"
$SourceDir = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Log = Join-Path (Resolve-Path $BuildDir).Path "installer-build.log"
try {
    $Artifacts = Join-Path (Resolve-Path $BuildDir).Path "PocketOne_artefacts/$Configuration"
    foreach ($Item in @("VST3/Pocket One.vst3/Contents/x86_64-win/Pocket One.vst3", "Standalone/Pocket One.exe")) {
        if (!(Test-Path (Join-Path $Artifacts $Item))) { throw "Missing Windows build product: $Item" }
    }
    $Command = Get-Command makensis.exe -ErrorAction SilentlyContinue
    $Nsis = if ($Command) { $Command.Source } else { "${env:ProgramFiles(x86)}/NSIS/makensis.exe" }
    if (!(Test-Path $Nsis)) { throw "Install NSIS 3.11+ (for example: choco install nsis)." }
    $OutputDir = Join-Path $SourceDir "dist"
    New-Item -ItemType Directory -Force $OutputDir | Out-Null
    & $Nsis "/DPATH_SEP=\" "/DARTIFACT_DIR=$Artifacts" "/DSOURCE_DIR=$SourceDir" "/DOUTPUT_DIR=$OutputDir" (Join-Path $PSScriptRoot "PocketOne.nsi") 2>&1 | Tee-Object -FilePath $Log
    $CompilerExit = $LASTEXITCODE
    if ($CompilerExit -ne 0) { throw "NSIS failed with exit code $CompilerExit" }
} catch {
    $_ | Out-String | Add-Content $Log
    if ($env:GITHUB_ACTIONS -and (Test-Path $Log)) {
        $Detail = ((Get-Content $Log -Tail 35) -join " | ").Replace("%", "%25").Replace("`r", "%0D").Replace("`n", "%0A")
        Write-Host "::error title=NSIS packaging failed::$Detail"
    }
    throw
} finally {
    if ($env:GITHUB_STEP_SUMMARY -and (Test-Path $Log)) {
        "### Windows installer build output" | Add-Content $env:GITHUB_STEP_SUMMARY
        '```text' | Add-Content $env:GITHUB_STEP_SUMMARY
        Get-Content $Log -Tail 70 | Add-Content $env:GITHUB_STEP_SUMMARY
        '```' | Add-Content $env:GITHUB_STEP_SUMMARY
    }
}
