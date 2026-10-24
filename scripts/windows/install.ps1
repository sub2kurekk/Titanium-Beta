# Builds the Titanium proxy DLL and installs it into Geometry Dash on Windows.
# Windows counterpart of scripts/install-proton.sh.
#
# Usage (from the repository root):
#   powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -GdPath "D:\Games\Geometry Dash"
#   powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -CopyTo "C:\GD"
#
# By default Titanium is installed into the Steam copy of Geometry Dash.
# With -CopyTo, the game is copied to that folder first and Titanium is
# installed only into the copy, so the Steam install stays untouched.
# -GdPath selects which install to use (or copy from) instead of auto-detecting.
#
# Requires MinGW-w64 (see docs/windows.md).

[CmdletBinding()]
param(
    [string]$GdPath = '',
    [string]$CopyTo = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$OrigDll = 'pthreadVCE3.dll'
$OrigBak = 'pthreadVCE3_o.dll'

$gd = Find-GeometryDash -Hint $GdPath
if (-not $gd) {
    throw "Error: Geometry Dash directory not found (looked for GeometryDash.exe).`nUsage: install.ps1 -GdPath `"C:\path\to\Geometry Dash`""
}
Write-Host "[Titanium] Geometry Dash: $gd"

$gpp = Find-MinGWTool -Names @('x86_64-w64-mingw32-g++', 'g++')
if (-not $gpp) {
    throw "Error: MinGW-w64 g++ not found. Install it and add it to PATH (see docs/windows.md)."
}
Write-Host "[Titanium] Compiler: $gpp"

# Optional: install into a separate copy of the game so the Steam install is
# never modified.
if ($CopyTo) {
    $sourceDir = [System.IO.Path]::GetFullPath($gd).TrimEnd('\')
    $targetDir = [System.IO.Path]::GetFullPath($CopyTo).TrimEnd('\')
    $ignoreCase = [System.StringComparison]::OrdinalIgnoreCase

    # The two folders must be separate: not the same, and neither inside the other.
    if ($targetDir.Equals($sourceDir, $ignoreCase) -or
        $targetDir.StartsWith("$sourceDir\", $ignoreCase) -or
        $sourceDir.StartsWith("$targetDir\", $ignoreCase)) {
        throw "Error: -CopyTo must be a separate folder that is not inside the Geometry Dash install (and does not contain it).`n  Game:   $sourceDir`n  CopyTo: $targetDir"
    }

    if (Test-Path (Join-Path $sourceDir 'pthreadVCE3_o.dll')) {
        Write-Warning "The Steam install already has a Titanium proxy (pthreadVCE3_o.dll). The copy will be set up fresh, but run uninstall.ps1 if you want the Steam install back to normal."
    }

    if (Test-Path (Join-Path $targetDir 'GeometryDash.exe')) {
        Write-Host "[Titanium] $targetDir already contains Geometry Dash, skipping the copy."
    }
    else {
        # Refuse to dump a game into some unrelated, non-empty folder.
        if ((Test-Path -LiteralPath $targetDir) -and
            (Get-ChildItem -LiteralPath $targetDir -Force | Select-Object -First 1)) {
            throw "Error: $targetDir already exists, is not empty and has no GeometryDash.exe. Choose a new or empty folder."
        }

        Write-Host "[Titanium] Copying Geometry Dash to $targetDir (this can take a few minutes)..."
        # No trailing backslashes in the paths: robocopy mishandles `"...\"`.
        & robocopy $sourceDir $targetDir /E /R:2 /W:2 /NP /NFL /NDL /NJH /NJS
        # robocopy exit codes 0-7 mean success; 8 and above mean failure.
        if ($LASTEXITCODE -ge 8) {
            throw "Error: copying the game failed (robocopy exit code $LASTEXITCODE)."
        }
        $global:LASTEXITCODE = 0
    }

    # Steam's API looks for this file when the game is not started from Steam's
    # own library folder. 322170 is Geometry Dash's app id.
    $appIdFile = Join-Path $targetDir 'steam_appid.txt'
    if (-not (Test-Path $appIdFile)) {
        [System.IO.File]::WriteAllText($appIdFile, '322170', (New-Object System.Text.ASCIIEncoding))
    }

    $gd = $targetDir
    Write-Host "[Titanium] Installing into the copy: $gd (the Steam install is left untouched)"
}

# Back up the original pthreadVCE3.dll (only once).
$origPath = Join-Path $gd $OrigDll
$bakPath = Join-Path $gd $OrigBak
if ((Test-Path $origPath) -and -not (Test-Path $bakPath)) {
    Write-Host "[Titanium] Backing up $OrigDll -> $OrigBak"
    Copy-Item $origPath $bakPath
}
elseif (-not (Test-Path $bakPath)) {
    throw "Error: $OrigDll not found in the Geometry Dash directory!"
}

# Generate the .def from the original DLL's exports.
$buildDir = Join-Path $ProjectDir 'build-windows'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$defPath = Join-Path $buildDir 'titaniumproxy.def'
$dllPath = Join-Path $buildDir 'titaniumproxy.dll'

Write-Host "[Titanium] Generating export definitions..."
& "$PSScriptRoot\gendef.ps1" -DllPath $bakPath -Output $defPath -ForwardName 'pthreadVCE3_o'

# Compile the proxy DLL. Relative paths keep this safe when the repository
# lives in a folder with spaces in its name.
Write-Host "[Titanium] Compiling titaniumproxy.dll..."
Push-Location $ProjectDir
try {
    $compileArgs = @(
        '-std=c++23', '-shared',
        '-o', 'build-windows/titaniumproxy.dll',
        'loader/src/dllmain.cpp',
        'loader/src/hookengine.cpp',
        'loader/src/patternscan.cpp',
        'loader/src/modloader.cpp',
        'loader/src/gdinit.cpp',
        'loader/src/loader.cpp',
        'loader/src/ui/modmenu.cpp',
        'loader/src/log.cpp',
        '-Iloader/include', '-Isdk/include',
        '-lpsapi',
        '-static', '-static-libgcc', '-static-libstdc++',
        'build-windows/titaniumproxy.def',
        '-Wl,--enable-stdcall-fixup'
    )
    & $gpp @compileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Error: compilation failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Pop-Location
}
Write-Host "[Titanium] Compilation OK."

# Install: our DLL takes the original DLL's name.
try {
    Copy-Item $dllPath $origPath -Force
}
catch {
    throw "Error: could not write $origPath. Close Geometry Dash and try again.`n$($_.Exception.Message)"
}
Write-Host "[Titanium] Installed $OrigDll (proxy)."

# The loader creates only the last folder of the mods path, so make sure the
# parent exists too.
$modsDir = Join-Path $env:USERPROFILE '.titaniumloader\mods'
New-Item -ItemType Directory -Force -Path $modsDir | Out-Null

Write-Host ""
Write-Host "[Titanium] === DONE ==="
if ($CopyTo) {
    Write-Host "  Start the modded game from: $(Join-Path $gd 'GeometryDash.exe')"
    Write-Host "  (Steam must be running. Launching from Steam still starts the unmodded game.)"
}
else {
    Write-Host "  Launch Geometry Dash normally from Steam."
}
Write-Host "  Mods folder: $modsDir"
Write-Host "  Log file:    $(Join-Path $env:TEMP 'titaniumloader.log')"
Write-Host ""
