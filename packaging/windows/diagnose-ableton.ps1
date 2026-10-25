# Read-only installation check. Run with Windows PowerShell; no administrator needed.
$ErrorActionPreference = "Stop"
$Root = Join-Path $env:CommonProgramW6432 'VST3\Pocket One.vst3'
$Module = Join-Path $Root 'Contents\x86_64-win\Pocket One.vst3'
Write-Host 'Pocket One / Ableton Live installation check'
Write-Host "Windows: $([Environment]::OSVersion.VersionString)"
Write-Host "64-bit OS: $([Environment]::Is64BitOperatingSystem)"
Write-Host "VST3 bundle: $Root"
if (!(Test-Path -LiteralPath $Module -PathType Leaf)) {
    Write-Host 'FAIL: the x64 VST3 module is missing. Run Setup.exe and select VST3 instrument.'
    Write-Host 'For a portable install, copy the entire Pocket One.vst3 folder into the system VST3 folder.'
    exit 1
}
$Bytes = [System.IO.File]::ReadAllBytes($Module)
if ($Bytes.Length -lt 64) { throw 'Invalid module: file is too short.' }
$Offset = [BitConverter]::ToInt32($Bytes, 60)
if ($Offset -lt 0 -or $Offset -gt $Bytes.Length - 6) { throw 'Invalid PE header offset.' }
if ([BitConverter]::ToUInt32($Bytes, $Offset) -ne 0x4550 -or [BitConverter]::ToUInt16($Bytes, $Offset + 4) -ne 0x8664) {
    throw 'The installed module is not a Windows x64 PE module.'
}
Write-Host 'PASS: Windows x64 module is installed in the standard VST3 bundle location.'
Get-FileHash -LiteralPath $Module -Algorithm SHA256 | Format-List
Write-Host 'In Live Preferences/Settings > Plug-ins, enable VST3 system folders.'
Write-Host 'Hold Alt while clicking Rescan for a full scan. Search for Pocket One under VST3 > Pocket Instruments.'
Write-Host 'VST3 requires Live 10.1 or later. This check does not validate loading inside Live.'
$Logs = Join-Path $env:APPDATA 'Ableton'
if (Test-Path -LiteralPath $Logs) {
    Get-ChildItem -LiteralPath $Logs -Filter Log.txt -Recurse -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 3 | ForEach-Object {
            Write-Host "Relevant entries in $($_.FullName):"
            Select-String -LiteralPath $_.FullName -Pattern 'Pocket One|PocketOne' | Select-Object -Last 15
        }
}
