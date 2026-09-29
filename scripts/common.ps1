#Requires -Version 7.0
<#
.SYNOPSIS
    Shared helpers for the Windows scripts (build.ps1, package.ps1).

.DESCRIPTION
    Dot-source this file from the other scripts:
        . (Join-Path $PSScriptRoot 'common.ps1')
#>

# Returns the project version from CMakeLists.txt; that file is the single
# source of truth (the VERSION field of the project() call).
function Get-LogViewerVersion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$RepoRoot
    )

    $cmakeLists = Join-Path $RepoRoot 'CMakeLists.txt'
    $match = Select-String -Path $cmakeLists -Pattern '^\s*VERSION\s+(\d+\.\d+\.\d+)' |
        Select-Object -First 1
    if (-not $match) {
        throw "Could not read the project version from $cmakeLists (expected a 'VERSION x.y.z' line)."
    }
    return $match.Matches[0].Groups[1].Value
}
