#Requires -Version 7.0
<#
.SYNOPSIS
    Runs the unit tests through CTest.

.DESCRIPTION
    ctest.exe ships with Visual Studio (not with the standalone CMake package),
    so it is located next to the Visual Studio installation unless it is already
    available on PATH.

.EXAMPLE
    .\scripts\test.ps1
.EXAMPLE
    .\scripts\test.ps1 -Filter tst_formats
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [string]$Filter = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$preset = if ($Config -eq 'Debug') { 'windows-msvc-qt6-debug' } else { 'windows-msvc-qt6-release' }

function Find-CTest {
    $command = Get-Command ctest -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products '*' -property installationPath 2>$null
        if ($installPath) {
            $candidate = Join-Path $installPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
            if (Test-Path $candidate) { return $candidate }
        }
    }
    $fallbacks = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe',
        'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
    )
    foreach ($f in $fallbacks) { if (Test-Path $f) { return $f } }
    throw 'ctest.exe not found. Install Visual Studio with the C++ desktop workload.'
}

$ctest = Find-CTest
$arguments = @('--preset', $preset, '--output-on-failure')
if (-not [string]::IsNullOrWhiteSpace($Filter)) { $arguments += @('-R', $Filter) }

Write-Host "Running: $ctest $($arguments -join ' ')"
& $ctest @arguments
if ($LASTEXITCODE -ne 0) { throw "Tests failed with exit code $LASTEXITCODE" }
