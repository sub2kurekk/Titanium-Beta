# Removes the Titanium proxy DLL and restores the original pthreadVCE3.dll.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\windows\uninstall.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\windows\uninstall.ps1 -GdPath "D:\Games\Geometry Dash"

[CmdletBinding()]
param(
    [string]$GdPath = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$gd = Find-GeometryDash -Hint $GdPath
if (-not $gd) {
    throw "Error: Geometry Dash directory not found (looked for GeometryDash.exe).`nUsage: uninstall.ps1 -GdPath `"C:\path\to\Geometry Dash`""
}

$origPath = Join-Path $gd 'pthreadVCE3.dll'
$bakPath = Join-Path $gd 'pthreadVCE3_o.dll'

if (-not (Test-Path $bakPath)) {
    throw "Error: $bakPath not found, so there is nothing to restore. If the game is broken, verify its files in Steam."
}

try {
    Move-Item $bakPath $origPath -Force
}
catch {
    throw "Error: could not restore the original DLL. Close Geometry Dash and try again.`n$($_.Exception.Message)"
}

Write-Host "[Titanium] Restored the original pthreadVCE3.dll in $gd"
Write-Host "[Titanium] Your mods in $(Join-Path $env:USERPROFILE '.titaniumloader') were left untouched."
