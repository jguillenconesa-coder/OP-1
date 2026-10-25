# Native Windows smoke test: do not treat a passing processor test as app startup.
param([string]$BuildDir = "build")
$ErrorActionPreference = "Stop"
$Exe = (Resolve-Path (Join-Path $BuildDir 'PocketOne_artefacts/Release/Standalone/Pocket One.exe')).Path
$App = Start-Process -FilePath $Exe -PassThru
try {
    Start-Sleep -Seconds 8
    $App.Refresh()
    if ($App.HasExited) {
        $Code = $App.ExitCode
        Write-Host "::error title=Standalone startup failed::Pocket One exited during startup, code $Code"
        throw "Standalone exited during startup: $Code"
    }
    if ($App.MainWindowHandle -eq 0) {
        Write-Host '::error title=Standalone startup failed::Pocket One is running but has no main window'
        throw 'Standalone did not create a main window'
    }
    Write-Host "PASS: standalone stayed running and created a window ($($App.MainWindowTitle))"
} finally {
    if (!$App.HasExited) { Stop-Process -Id $App.Id -ErrorAction SilentlyContinue }
}
