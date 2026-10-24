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
