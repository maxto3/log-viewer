#Requires -Version 7.0
<#
.SYNOPSIS
    Builds the redistributable Windows package (a single zip file).

.DESCRIPTION
    Builds the requested configuration, copies the executable into a staging
    folder named "log-viewer-<version>" and runs windeployqt so that the folder
    contains the Qt runtime, the platform plugins and the translations. The
    staging folder (plus the documentation and LICENSE) is then compressed into
    <OutDir>\log-viewer-<version>-win64.zip, so <OutDir> only ever contains the
    archive, no subdirectories. The archive is self contained; the target
    machine needs the Visual C++ runtime (either install it or ship
    vc_redist.x64.exe). The version is read from CMakeLists.txt.

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
. (Join-Path $PSScriptRoot 'common.ps1')
$version = Get-LogViewerVersion -RepoRoot $repoRoot
$preset = if ($Config -eq 'Debug') { 'windows-msvc-qt6-debug' } else { 'windows-msvc-qt6-release' }
$binDir = Join-Path $repoRoot "build\$preset\bin"

# The staging folder becomes the single top-level folder inside the archive; it
# lives under build/ (gitignored), so <OutDir> only receives the zip file.
$packageName = "log-viewer-$version"
$stageDir = Join-Path $repoRoot "build\$preset\package\$packageName"
$outDir = Join-Path $repoRoot $OutDir
$zipPath = Join-Path $outDir "$packageName-win64.zip"

if (-not $SkipBuild) {
    Write-Host "Packaging log-viewer $version ($Config) ..."
    & (Join-Path $PSScriptRoot 'build.ps1') -Config $Config
}

$exe = Join-Path $binDir 'log-viewer.exe'
if (-not (Test-Path $exe)) { throw "Executable not found: $exe" }

if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
Copy-Item $exe $stageDir

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
& $windeployqt --no-opengl-sw --no-system-d3d-compiler (Join-Path $stageDir 'log-viewer.exe') | Out-Null

# Our own translation (windeployqt only ships the Qt catalogues).
$translationSource = Join-Path $binDir 'translations\logviewer_zh_CN.qm'
if (Test-Path $translationSource) {
    $translationTarget = Join-Path $stageDir 'translations'
    New-Item -ItemType Directory -Force -Path $translationTarget | Out-Null
    Copy-Item $translationSource $translationTarget
}

# User documentation shipped next to the executable. The internal spec.md and
# design-doc.md are not part of the redistributable package.
foreach ($doc in 'README.md', 'README.zh_CN.md') {
    $source = Join-Path $repoRoot $doc
    if (Test-Path $source) { Copy-Item $source $stageDir }
}
Copy-Item (Join-Path $repoRoot 'LICENSE') $stageDir -ErrorAction SilentlyContinue

# The association helper sits next to the executable it registers, so the
# unpacked folder can be moved anywhere and the script still works
# (REQ-ASSOC-07). It is a thin wrapper around --register-association.
Copy-Item (Join-Path $PSScriptRoot 'register-association.ps1') $stageDir

# Compress the staging folder itself, so the archive has exactly one top-level
# folder ("log-viewer-<version>") with the executable and all dependencies.
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Write-Host "Compressing $zipPath ..."
Compress-Archive -Path $stageDir -DestinationPath $zipPath -CompressionLevel Optimal

# Older versions of this script kept the unpacked folder next to the archive.
$legacyDir = Join-Path $outDir 'log-viewer'
if (Test-Path $legacyDir -PathType Container) {
    Write-Host "Removing the legacy folder $legacyDir"
    Remove-Item -Recurse -Force $legacyDir
}

$size = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host "Package ready: $zipPath ($size MB)"
Write-Host "Archive layout: $packageName\log-viewer.exe with the Qt runtime, plugins, translations, documentation and register-association.ps1."
Write-Host "To associate .log files: $packageName\log-viewer.exe --register-association (or the included script); undo with --unregister-association."
Write-Host "Note: the target machine needs the Visual C++ runtime (or run vc_redist.x64.exe)."
