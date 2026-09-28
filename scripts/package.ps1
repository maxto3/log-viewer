#Requires -Version 7.0
<#
.SYNOPSIS
    Builds a redistributable folder (portable build) of the application.

.DESCRIPTION
    Builds the requested configuration, then copies the executable into
    <OutDir>\log-viewer and runs windeployqt so that the folder contains the Qt
    runtime, the platform plugins and the translations. The result can be copied
    to another Windows machine (the VC++ runtime is required there; either
    install it or ship vc_redist.x64.exe).

.EXAMPLE
    .\scripts\package.ps1
.EXAMPLE
    .\scripts\package.ps1 -Config Debug -OutDir D:\dist
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [string]$OutDir = 'dist',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$preset = if ($Config -eq 'Debug') { 'windows-msvc-qt6-debug' } else { 'windows-msvc-qt6-release' }
$binDir = Join-Path $repoRoot "build\$preset\bin"
$target = Join-Path $repoRoot (Join-Path $OutDir 'log-viewer')

if (-not $SkipBuild) {
    Write-Host 'Building ...'
    & (Join-Path $PSScriptRoot 'build.ps1') -Config $Config
}

$exe = Join-Path $binDir 'log-viewer.exe'
if (-not (Test-Path $exe)) { throw "Executable not found: $exe" }

if (Test-Path $target) { Remove-Item -Recurse -Force $target }
New-Item -ItemType Directory -Force -Path $target | Out-Null
Copy-Item $exe $target

function Find-WinDeployQt {
    $candidates = @()
    if ($env:LOGVIEWER_QT_ROOT) { $candidates += (Join-Path $env:LOGVIEWER_QT_ROOT 'bin\windeployqt.exe') }
    $candidates += 'D:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe'
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }
    $command = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return $null
}

$windeployqt = Find-WinDeployQt
if (-not $windeployqt) { throw 'windeployqt not found (set LOGVIEWER_QT_ROOT or install Qt).' }

Write-Host "Deploying the Qt runtime with $windeployqt ..."
& $windeployqt --no-opengl-sw --no-system-d3d-compiler (Join-Path $target 'log-viewer.exe') | Out-Null

# Our own translation (windeployqt only ships the Qt catalogues).
$translationSource = Join-Path $binDir 'translations\logviewer_zh_CN.qm'
if (Test-Path $translationSource) {
    $translationTarget = Join-Path $target 'translations'
    New-Item -ItemType Directory -Force -Path $translationTarget | Out-Null
    Copy-Item $translationSource $translationTarget
}

# Documentation shipped next to the executable.
foreach ($doc in 'README.md', 'README.zh_CN.md', 'spec.md', 'design-doc.md') {
    $source = Join-Path $repoRoot $doc
    if (Test-Path $source) { Copy-Item $source $target }
}
Copy-Item (Join-Path $repoRoot 'LICENSE') $target -ErrorAction SilentlyContinue

$size = [math]::Round((Get-ChildItem $target -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
Write-Host "Portable build ready: $target ($size MB)"
Write-Host "Note: the target machine needs the Visual C++ runtime (or run vc_redist.x64.exe)."
