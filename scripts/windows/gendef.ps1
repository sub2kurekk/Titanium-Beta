# Generates a .def file that forwards every export of a DLL to a renamed copy
# of it, plus Titanium's own exports. Windows counterpart of scripts/gendef.sh.
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

$objdump = Find-MinGWTool -Names @('x86_64-w64-mingw32-objdump', 'objdump')
if (-not $objdump) {
    throw "Error: objdump not found. Install MinGW-w64 (see docs/windows.md) and make sure it is on PATH."
}

$dump = & $objdump -p $DllPath
if ($LASTEXITCODE -ne 0) {
    throw "Error: objdump failed on $DllPath"
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('EXPORTS')

$inNames = $false
$count = 0
foreach ($line in $dump) {
    if ($line -match '^\[Ordinal/Name Pointer\] Table') {
        $inNames = $true
        continue
    }
    if ($inNames -and $line -match '^\s*$') {
        $inNames = $false
        continue
    }
    if ($inNames -and $line -match '^\s+\[\s*\d+\]\s+(\S+)\s*$') {
        $name = $Matches[1]
        $lines.Add("  $name = $ForwardName.$name")
        $count++
    }
}

if ($count -eq 0) {
    throw "Error: no exports found in $DllPath (is it a 64-bit DLL?)"
}

foreach ($sym in @('titanium_hook', 'titanium_unhook', 'titanium_resolve_address', 'titanium_log')) {
    $lines.Add("  $sym")
}

# ASCII, no BOM: the linker does not accept UTF-16 or a BOM in .def files.
[System.IO.File]::WriteAllLines($Output, $lines, (New-Object System.Text.ASCIIEncoding))

Write-Host "Generated $Output with $($count + 4) exports (forwarding to $ForwardName)"
