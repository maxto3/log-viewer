#Requires -Version 7.0
<#
.SYNOPSIS
    Configures and builds the project with CMake + Ninja + MSVC 2022.

.DESCRIPTION
    Locates the Visual Studio installation, imports the MSVC x64 environment
    (vcvars64.bat) and then drives the CMake preset "windows-msvc-qt6-<config>".

.EXAMPLE
    .\scripts\build.ps1
.EXAMPLE
    .\scripts\build.ps1 -Config Debug -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [string]$QtRoot = '',
    [switch]$Clean,
    [switch]$SkipConfigure
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$preset = if ($Config -eq 'Debug') { 'windows-msvc-qt6-debug' } else { 'windows-msvc-qt6-release' }
$buildDir = Join-Path $repoRoot "build\$preset"

function Find-VcVars64 {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($installPath) {
            $candidate = Join-Path $installPath 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path $candidate) { return $candidate }
        }
    }
    $fallbacks = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
    )
    foreach ($f in $fallbacks) { if (Test-Path $f) { return $f } }
    throw 'vcvars64.bat not found. Install Visual Studio with the C++ desktop workload.'
}

$vcvars = Find-VcVars64
Write-Host "Using MSVC environment: $vcvars"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

$configureArgs = "--preset $preset"
if (-not [string]::IsNullOrWhiteSpace($QtRoot)) {
    $configureArgs += " -DCMAKE_PREFIX_PATH=$QtRoot"
}

$steps = @()
if (-not $SkipConfigure) { $steps += "cmake $configureArgs" }
$steps += "cmake --build --preset $preset"
$commandLine = "call `"$vcvars`" >nul && " + ($steps -join ' && ')

Write-Host "> $commandLine"
& cmd /c $commandLine
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

$exe = Join-Path $buildDir 'bin\log-viewer.exe'
if (Test-Path $exe) {
    Write-Host "Built: $exe"
} else {
    Write-Warning "Build succeeded but $exe was not found."
}
