#Requires -Version 7.0
<#
.SYNOPSIS
    Installs the Qt 6 runtime/development package used by this project.

.DESCRIPTION
    Uses aqtinstall (no Qt account needed) to download the official Qt packages
    into a local directory. Downloads go through a proxy when one is configured,
    which is required on this development machine.

.EXAMPLE
    .\scripts\install-qt.ps1
.EXAMPLE
    .\scripts\install-qt.ps1 -Proxy http://localhost:1081 -QtRoot D:\Qt
#>
[CmdletBinding()]
param(
    [string]$QtRoot = 'D:\Qt',
    [string]$Version = '6.8.3',
    [string]$Arch = 'win64_msvc2022_64',
    [string]$Proxy = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Proxy)) { $Proxy = $env:HTTPS_PROXY }
if (-not [string]::IsNullOrWhiteSpace($Proxy)) {
    $env:HTTP_PROXY = $Proxy
    $env:HTTPS_PROXY = $Proxy
    $env:ALL_PROXY = $Proxy
    Write-Host "Using HTTP proxy: $Proxy"
} else {
    Write-Host 'No proxy configured (set -Proxy or HTTPS_PROXY if downloads fail).'
}

$qtDir = Join-Path $QtRoot (Join-Path $Version ($Arch -replace '^win64_', '') -replace '_64$', '_64')
# Resolve the directory aqtinstall actually creates, e.g. D:\Qt\6.8.3\msvc2022_64
$target = Join-Path $QtRoot "$Version\msvc2022_64"

if (Test-Path (Join-Path $target 'bin\qmake.exe')) {
    Write-Host "Qt already installed: $target"
    exit 0
}

Write-Host 'Installing / upgrading aqtinstall ...'
& python -m pip install --user --upgrade aqtinstall
if ($LASTEXITCODE -ne 0) { throw "pip install aqtinstall failed with exit code $LASTEXITCODE" }

Write-Host "Downloading Qt $Version ($Arch) into $QtRoot ..."
& python -m aqt install-qt windows desktop $Version $Arch -O $QtRoot
if ($LASTEXITCODE -ne 0) { throw "aqt install-qt failed with exit code $LASTEXITCODE" }

if (-not (Test-Path (Join-Path $target 'bin\qmake.exe'))) {
    throw "Qt installation finished but $target\bin\qmake.exe was not found."
}

Write-Host "Qt installed: $target"
