#Requires -Version 7.0
<#
.SYNOPSIS
    Runs the built application.

.DESCRIPTION
    Everything after the switches is forwarded to log-viewer, so application
    options are written without a "--" separator (PowerShell would parse that
    as a parameter name).

.EXAMPLE
    .\scripts\run.ps1
.EXAMPLE
    .\scripts\run.ps1 --demo
.EXAMPLE
    .\scripts\run.ps1 --lang zh_CN .\test-data\sslocal.2026-09-28.log
.EXAMPLE
    .\scripts\run.ps1 -Config Debug --monitor C:\logs\app.log
.EXAMPLE
    .\scripts\run.ps1 -AppArgs '--version'
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)][string[]]$AppArgs,
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$preset = if ($Config -eq 'Debug') { 'windows-msvc-qt6-debug' } else { 'windows-msvc-qt6-release' }
$exe = Join-Path $repoRoot "build\$preset\bin\log-viewer.exe"

if (-not (Test-Path $exe)) { throw "Executable not found: $exe (run scripts\build.ps1 first)" }

Write-Host "Running $exe $($AppArgs -join ' ')"
& $exe @AppArgs
exit $LASTEXITCODE
