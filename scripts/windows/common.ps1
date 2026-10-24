# Shared helpers for the Titanium Windows scripts.
# Dot-source this file (. "$PSScriptRoot\common.ps1"); do not run it directly.

# Finds a MinGW-w64 tool. Tries each name on PATH first, then the default
# MSYS2 install locations. Returns the full path, or $null if nothing matches.
function Find-MinGWTool {
    param([Parameter(Mandatory = $true)][string[]]$Names)

    foreach ($name in $Names) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) { return $cmd.Source }
    }

    $dirs = @('C:\msys64\mingw64\bin', 'C:\msys64\ucrt64\bin')
    foreach ($dir in $dirs) {
        foreach ($name in $Names) {
            $candidate = Join-Path $dir "$name.exe"
            if (Test-Path $candidate) { return $candidate }
        }
    }

    return $null
}

# Returns every Steam library root we can discover (install dir plus any extra
# libraries listed in libraryfolders.vdf).
function Get-SteamLibraryRoots {
    $roots = New-Object System.Collections.Generic.List[string]
    $steam = $null

    $keys = @(
        'HKCU:\Software\Valve\Steam',
        'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam',
        'HKLM:\SOFTWARE\Valve\Steam'
    )
    foreach ($key in $keys) {
        $props = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
        if ($props) {
            foreach ($prop in @('SteamPath', 'InstallPath')) {
                if ($props.$prop) { $steam = [string]$props.$prop; break }
            }
        }
        if ($steam) { break }
    }

    if ($steam) {
        $steam = $steam -replace '/', '\'
        $roots.Add($steam)

        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            foreach ($line in Get-Content $vdf) {
                if ($line -match '^\s*"path"\s+"([^"]+)"') {
                    $roots.Add(($Matches[1] -replace '\\\\', '\'))
                }
            }
        }
    }

    $pf86 = ${env:ProgramFiles(x86)}
    if ($pf86) { $roots.Add((Join-Path $pf86 'Steam')) }
    if ($env:ProgramFiles) { $roots.Add((Join-Path $env:ProgramFiles 'Steam')) }

    return $roots | Select-Object -Unique
}

# Returns the Geometry Dash install directory (the folder that contains
# GeometryDash.exe), or $null. If -Hint is given it is the only place checked.
function Find-GeometryDash {
    param([string]$Hint = '')

    if ($Hint) {
        if (Test-Path (Join-Path $Hint 'GeometryDash.exe')) {
            return (Resolve-Path $Hint).Path
        }
        return $null
    }

    foreach ($root in (Get-SteamLibraryRoots)) {
        $dir = Join-Path $root 'steamapps\common\Geometry Dash'
        if (Test-Path (Join-Path $dir 'GeometryDash.exe')) { return $dir }
    }

    return $null
}

# Reads the export table of a PE file (.dll) directly, with no external tools.
# Returns an object with:
#   Machine        PE machine type (0x8664 = x64, 0x14C = x86)
#   HasExportTable $true if the DLL has an export directory at all
#   FunctionCount  number of exported functions (named or by ordinal)
#   Names          the exported names, in the order stored in the file
function Get-PeExports {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
        throw "$Path is not a Windows executable or DLL (no MZ header)."
    }

    $peOffset = [int64][BitConverter]::ToUInt32($bytes, 0x3C)
    if (($peOffset + 24) -gt $bytes.Length -or
        $bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or
        $bytes[$peOffset + 2] -ne 0 -or $bytes[$peOffset + 3] -ne 0) {
        throw "$Path is not a valid PE file (no PE signature)."
    }

    $coff = $peOffset + 4
    $machine = [int][BitConverter]::ToUInt16($bytes, $coff)
    $sectionCount = [int][BitConverter]::ToUInt16($bytes, $coff + 2)
    $optionalSize = [int][BitConverter]::ToUInt16($bytes, $coff + 16)
    $optional = $coff + 20
    $magic = [int][BitConverter]::ToUInt16($bytes, $optional)

    # The export directory is the first entry of the optional header's data
    # directory, which starts at a different offset for PE32 and PE32+.
    if ($magic -eq 0x20B) { $dirOffset = $optional + 112 }
    elseif ($magic -eq 0x10B) { $dirOffset = $optional + 96 }
    else { throw ("{0} has an unknown optional header type (0x{1:X})." -f $Path, $magic) }

    $result = [pscustomobject]@{
        Machine        = $machine
        HasExportTable = $false
        FunctionCount  = 0
        Names          = @()
    }

    $exportRva = [int64][BitConverter]::ToUInt32($bytes, $dirOffset)
    if ($exportRva -eq 0) { return $result }

    # Section table, used to turn RVAs into file offsets.
    $sectionTable = $optional + $optionalSize
    $sections = @()
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $s = $sectionTable + 40 * $i
        $virtualSize = [int64][BitConverter]::ToUInt32($bytes, $s + 8)
        $rawSize = [int64][BitConverter]::ToUInt32($bytes, $s + 16)
        $sections += [pscustomobject]@{
            Va   = [int64][BitConverter]::ToUInt32($bytes, $s + 12)
            Span = [Math]::Max($virtualSize, $rawSize)
            Raw  = [int64][BitConverter]::ToUInt32($bytes, $s + 20)
        }
    }

    $toOffset = {
        param([int64]$rva)
        foreach ($sec in $sections) {
            if ($rva -ge $sec.Va -and $rva -lt ($sec.Va + $sec.Span)) {
                return [int]($rva - $sec.Va + $sec.Raw)
            }
        }
        return -1
    }

    $exportOffset = & $toOffset $exportRva
    if ($exportOffset -lt 0 -or ($exportOffset + 40) -gt $bytes.Length) {
        throw "$Path has a damaged export table."
    }

    $result.HasExportTable = $true
    $result.FunctionCount = [int][BitConverter]::ToUInt32($bytes, $exportOffset + 20)
    $nameCount = [int][BitConverter]::ToUInt32($bytes, $exportOffset + 24)
    $namesOffset = & $toOffset ([int64][BitConverter]::ToUInt32($bytes, $exportOffset + 32))

    $names = New-Object System.Collections.Generic.List[string]
    if ($nameCount -gt 0 -and $namesOffset -ge 0) {
        for ($i = 0; $i -lt $nameCount; $i++) {
            $nameRva = [int64][BitConverter]::ToUInt32($bytes, $namesOffset + 4 * $i)
            $start = & $toOffset $nameRva
            if ($start -lt 0) { continue }
            $end = $start
            while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
            $names.Add([System.Text.Encoding]::ASCII.GetString($bytes, $start, $end - $start))
        }
    }

    $result.Names = $names.ToArray()
    return $result
}
