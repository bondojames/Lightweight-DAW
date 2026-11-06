# SPDX-License-Identifier: GPL-3.0-or-later
# Measures the UI's real CPU, memory and frame rate in each mode on the real GPU.
# Each mode runs alone, because the runs would otherwise disturb each other.
param(
    [string]$Exe = "$PSScriptRoot\..\build\release\bin\LightweightDAW.exe",
    [string]$OutDir = "$PSScriptRoot\..\build\profile",
    [int]$SettleSeconds = 60,
    [int]$ProfileSeconds = 60,
    [string[]]$Modes = @('idle', 'active', 'minimized', 'detached', 'playing')
)
$ErrorActionPreference = 'Stop'
$Modes = $Modes -split ','
New-Item -ItemType Directory -Force $OutDir | Out-Null
foreach ($mode in $Modes) {
    $dataDir = Join-Path $OutDir "data-$mode"
    if (Test-Path $dataDir) { Remove-Item -Recurse -Force $dataDir }
    $json = Join-Path $OutDir "$mode.json"
    if (Test-Path $json) { Remove-Item -Force $json }
    $arguments = @('--profile', $mode, '--settle-seconds', $SettleSeconds, '--profile-seconds', $ProfileSeconds,
                   '--profile-out', "`"$json`"", '--data-dir', "`"$dataDir`"")
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit(($SettleSeconds + $ProfileSeconds + 60) * 1000)) {
        $process.Kill()
        throw "Profile run '$mode' did not finish in time."
    }
    if ($process.ExitCode -ne 0) { throw "Profile run '$mode' exited with $($process.ExitCode)." }
    Get-Content $json
}
