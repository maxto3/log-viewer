#Requires -Version 7.0
<#
.SYNOPSIS
    Registers (or removes) the "open .log files with Log Viewer" association.

.DESCRIPTION
    Everything is written below HKEY_CURRENT_USER, so no administrator rights are
    needed and the association only affects the current user. The previous value
    of the .log association is stored under ...\LogViewer.Backup so that
    -Unregister can restore it.

    By default only ".log" is registered (OPEN-11 decision); ".txt" requires the
    explicit -IncludeTxt switch.

.EXAMPLE
    .\scripts\register-association.ps1
.EXAMPLE
    .\scripts\register-association.ps1 -IncludeTxt -Force
.EXAMPLE
    .\scripts\register-association.ps1 -Unregister
#>
[CmdletBinding()]
param(
    [switch]$Unregister,
    [switch]$IncludeTxt,
    [switch]$Force,
    [string]$ExePath = ''
)

$ErrorActionPreference = 'Stop'

$progId = 'LogViewer.log'
$classes = 'HKCU:\Software\Classes'
$backupKey = Join-Path $classes 'LogViewer.Backup'
$extensions = @('.log')
if ($IncludeTxt) { $extensions += '.txt' }

if (-not $ExePath) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $candidates = @(
        (Join-Path $repoRoot 'dist\log-viewer\log-viewer.exe'),
        (Join-Path $repoRoot 'build\windows-msvc-qt6-release\bin\log-viewer.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { $ExePath = (Resolve-Path $candidate).Path; break }
    }
}
if (-not $ExePath -or -not (Test-Path $ExePath)) {
    throw 'log-viewer.exe not found. Build it first or pass -ExePath.'
}

if ($Unregister) {
    foreach ($extension in $extensions) {
        $key = Join-Path $classes $extension
        if (Test-Path $key) {
            $backup = (Get-ItemProperty -Path $backupKey -Name $extension -ErrorAction SilentlyContinue).$extension
            if ($backup) {
                Set-ItemProperty -Path $key -Name '(default)' -Value $backup
                Write-Host "restored $extension -> $backup"
            } elseif ($Force -or (Get-Item $key).Property -contains '(default)') {
                Remove-ItemProperty -Path $key -Name '(default)' -ErrorAction SilentlyContinue
                Write-Host "removed $extension association"
            }
        }
    }
    Remove-Item -Path (Join-Path $classes $progId) -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path (Join-Path $classes 'Applications\log-viewer.exe') -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path $backupKey -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host 'Log Viewer file association removed.'
    exit 0
}

New-Item -ItemType Directory -Force -Path $backupKey | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $classes $progId) | Out-Null

# ProgID: how to open the file, how to show it in "Open with".
$commandKey = Join-Path (Join-Path $classes $progId) 'shell\open\command'
New-Item -ItemType Directory -Force -Path $commandKey | Out-Null
Set-ItemProperty -Path $commandKey -Name '(default)' -Value "`"$ExePath`" `"%1`""
Set-ItemProperty -Path (Join-Path $classes $progId) -Name '(default)' -Value 'Log file'
Set-ItemProperty -Path (Join-Path $classes $progId) -Name 'FriendlyTypeName' -Value 'Log File'

$appKey = Join-Path $classes 'Applications\log-viewer.exe'
$appCommandKey = Join-Path $appKey 'shell\open\command'
New-Item -ItemType Directory -Force -Path $appCommandKey | Out-Null
Set-ItemProperty -Path $appCommandKey -Name '(default)' -Value "`"$ExePath`" `"%1`""
Set-ItemProperty -Path $appKey -Name 'FriendlyAppName' -Value 'Log Viewer'

foreach ($extension in $extensions) {
    $key = Join-Path $classes $extension
    New-Item -ItemType Directory -Force -Path $key | Out-Null
    $current = (Get-ItemProperty -Path $key -Name '(default)' -ErrorAction SilentlyContinue).'(default)'
    if ($current -and $current -ne $progId -and -not $Force) {
        Write-Warning "$extension is currently associated with '$current'. Storing a backup; pass -Force to override."
        Set-ItemProperty -Path $backupKey -Name $extension -Value $current
    } elseif ($current -and $current -ne $progId) {
        Set-ItemProperty -Path $backupKey -Name $extension -Value $current
    }
    Set-ItemProperty -Path $key -Name '(default)' -Value $progId
    Write-Host "associated $extension -> $progId"
}

Write-Host "Log Viewer file association installed for: $($extensions -join ', ')"
Write-Host "Executable: $ExePath"
Write-Host 'Undo with: .\scripts\register-association.ps1 -Unregister'
