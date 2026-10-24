# Generates a .def file that forwards every export of a DLL to a renamed copy
# of it, plus Titanium's own exports. Windows counterpart of scripts/gendef.sh.
#
# The DLL's export table is read directly (see Get-PeExports in common.ps1), so
# no objdump or other external tool is needed.
#
# Usage: gendef.ps1 -DllPath <original.dll> -Output <file.def> [-ForwardName <name>]

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [Parameter(Mandatory = $true)][string]$Output,
    [string]$ForwardName = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

if (-not (Test-Path $DllPath)) {
    throw "Error: $DllPath not found"
}

if (-not $ForwardName) {
    $ForwardName = [System.IO.Path]::GetFileNameWithoutExtension($DllPath)
}

$exports = Get-PeExports -Path $DllPath

# Titanium builds a 64-bit proxy, so it can only forward to a 64-bit DLL.
if ($exports.Machine -ne 0x8664) {
    throw ("Error: {0} is not a 64-bit (x64) DLL (machine type 0x{1:X}). Titanium builds a 64-bit proxy, so it only supports 64-bit Geometry Dash." -f $DllPath, $exports.Machine)
}

if ($exports.Names.Count -eq 0) {
    throw ("Error: no named exports found in {0} (export table present: {1}, exported functions: {2}). This does not look like the pthreads library the game normally ships. Please report this along with your Geometry Dash version." -f $DllPath, $exports.HasExportTable, $exports.FunctionCount)
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('EXPORTS')

foreach ($name in $exports.Names) {
    $lines.Add("  $name = $ForwardName.$name")
}

foreach ($sym in @('titanium_hook', 'titanium_unhook', 'titanium_resolve_address', 'titanium_log')) {
    $lines.Add("  $sym")
}

# ASCII, no BOM: the linker does not accept UTF-16 or a BOM in .def files.
[System.IO.File]::WriteAllLines($Output, $lines, (New-Object System.Text.ASCIIEncoding))

Write-Host "Generated $Output with $($exports.Names.Count + 4) exports (forwarding to $ForwardName)"
