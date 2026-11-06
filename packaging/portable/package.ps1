# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the portable folder and ZIP for a phase, then proves the ZIP runs from a
# fresh extract with no developer tools on PATH. Run through tools\package.cmd.
param([Parameter(Mandatory = $true)][string]$Phase)
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$exe = Join-Path $root 'build\release\bin\LightweightDAW.exe'
if (-not (Test-Path $exe)) { throw "Release build not found. Run tools\build.cmd release first." }

# --- Only DLLs that ship with Windows are allowed until a phase adds audited ones.
$allowedDlls = @('KERNEL32.dll', 'USER32.dll', 'GDI32.dll', 'SHELL32.dll', 'ole32.dll', 'OLEAUT32.dll', 'ADVAPI32.dll',
                 'd3d11.dll', 'dxgi.dll', 'd3dcompiler_47.dll', 'dwmapi.dll', 'IMM32.dll', 'dbghelp.dll', 'PSAPI.dll', 'SHCORE.dll', 'AVRT.dll', 'sndfile.dll')
$dependencies = & dumpbin /dependents $exe | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '\.dll$' }
foreach ($dll in $dependencies) {
    if ($dll -match '^Qt') { throw "Forbidden Qt dependency: $dll" }
    if ($allowedDlls -notcontains $dll) { throw "Unreviewed DLL dependency: $dll. Audit it, then add it here." }
}
Write-Output "DLL dependencies (Windows system DLLs, plus sndfile.dll): $($dependencies -join ', ')"

# --- Assemble the portable folder.
$outDir = Join-Path $root "builds\phase-$Phase"
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
New-Item -ItemType Directory -Force (Join-Path $outDir 'licenses') | Out-Null
Copy-Item $exe $outDir
Copy-Item (Join-Path $root 'build\release\bin\LightweightDAW_audiotest.exe') $outDir
Copy-Item (Join-Path $root 'LICENSE') $outDir
Copy-Item (Join-Path $root 'LICENSE-EXCEPTION.md') $outDir
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') $outDir
Copy-Item (Join-Path $root 'packaging\portable\README_FIRST.txt') $outDir
Copy-Item (Join-Path $root '.deps\imgui-src\LICENSE.txt') (Join-Path $outDir 'licenses\imgui-LICENSE.txt')
Copy-Item (Join-Path $root 'build\release\bin\sndfile.dll') $outDir
Copy-Item (Join-Path $root '.deps\sndfile-src\COPYING') (Join-Path $outDir 'licenses\libsndfile-COPYING.txt')
Copy-Item (Join-Path $root '.deps\samplerate-src\COPYING') (Join-Path $outDir 'licenses\libsamplerate-COPYING.txt')

# --- ZIP it beside the folder.
$zip = Join-Path $root "builds\LightweightDAW-phase-$Phase-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $outDir '*') -DestinationPath $zip

# --- Extract and run with no developer PATH, data kept in the temp folder.
$scratch = Join-Path $env:TEMP "ldaw-package-check-$PID"
Expand-Archive -Path $zip -DestinationPath $scratch
$savedPath = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
try {
    $runArgs = @('--smoke-seconds', '4', '--data-dir', "`"$scratch\data`"")
    if ($env:LDAW_PACKAGE_WARP) { $runArgs += '--warp' } # CI runners have no GPU.
    $process = Start-Process -FilePath (Join-Path $scratch 'LightweightDAW.exe') -ArgumentList $runArgs -PassThru
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Extracted build did not exit.' }
    if ($process.ExitCode -ne 0) { throw "Extracted build exited with $($process.ExitCode)." }
} finally {
    $env:PATH = $savedPath
}
Remove-Item -Recurse -Force $scratch
Write-Output 'Extracted ZIP ran and exited cleanly with no developer PATH.'

$exeHash = (Get-FileHash (Join-Path $outDir 'LightweightDAW.exe') -Algorithm SHA256).Hash.ToLower()
$zipHash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
Write-Output "EXE SHA-256: $exeHash"
Write-Output "ZIP SHA-256: $zipHash"
Write-Output "ZIP: $zip"
