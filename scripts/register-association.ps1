#Requires -Version 7.0
<#
.SYNOPSIS
    Registers (or removes) the "open .log files with Log Viewer" association.

.DESCRIPTION
    Thin wrapper around the application itself: the registry work is done by
    "log-viewer.exe --register-association" (docs/spec.md REQ-ASSOC-06), which
    always registers the executable it was started from. This script only finds
    that executable - by default the copy next to this script, which is how the
    release zip is laid out (REQ-ASSOC-07) - and forwards the switches, so the
    association stays correct no matter where the folder was unpacked.

    Everything is written below HKEY_CURRENT_USER, so no administrator rights
    are needed and the association only affects the current user. The previous
    value of the .log association is backed up and restored by -Unregister.

    Only ".log" is registered; other extensions are deliberately left alone.

.EXAMPLE
    .\log-viewer\register-association.ps1
.EXAMPLE
    .\scripts\register-association.ps1 -Force
.EXAMPLE
    .\register-association.ps1 -Unregister
.EXAMPLE
    .\scripts\register-association.ps1 -ExePath .\build\windows-msvc-qt6-release\bin\log-viewer.exe
#>
[CmdletBinding()]
param(
    [switch]$Unregister,
    [switch]$Force,
    [string]$ExePath = ''
)

$ErrorActionPreference = 'Stop'

if (-not $ExePath) {
    # First candidate: the released layout, where this script sits next to the
    # executable. The rest cover a checkout (scripts\ in the repository root).
    $candidates = @(
        (Join-Path $PSScriptRoot 'log-viewer.exe'),
        (Join-Path $PSScriptRoot '..\build\windows-msvc-qt6-release\bin\log-viewer.exe'),
        (Join-Path $PSScriptRoot '..\build\windows-msvc-qt6-debug\bin\log-viewer.exe'),
        (Join-Path $PSScriptRoot '..\dist\log-viewer\log-viewer.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { $ExePath = (Resolve-Path $candidate).Path; break }
    }
}
if (-not $ExePath -or -not (Test-Path $ExePath)) {
    throw 'log-viewer.exe not found. Unpack the release archive first or pass -ExePath.'
}

$arguments = @(if ($Unregister) { '--unregister-association' } else { '--register-association' })
if ($Force -and -not $Unregister) { $arguments += '--force' }

& $ExePath @arguments
if ($LASTEXITCODE -ne 0) {
    throw "log-viewer.exe $($arguments -join ' ') failed with exit code $LASTEXITCODE."
}
