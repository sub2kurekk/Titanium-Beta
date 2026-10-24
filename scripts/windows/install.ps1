# Builds the Titanium proxy DLL and installs it into Geometry Dash on Windows.
# Windows counterpart of scripts/install-proton.sh.
#
# Usage (from the repository root):
#   powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -GdPath "D:\Games\Geometry Dash"
#
# Requires MinGW-w64 (see docs/windows.md).

[CmdletBinding()]
param(
    [string]$GdPath = ''
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
if ($GdPath) {
    Write-Host "  Start the game from: $(Join-Path $gd 'GeometryDash.exe')"
}
else {
    Write-Host "  Launch Geometry Dash normally from Steam."
}
Write-Host "  Mods folder: $modsDir"
Write-Host "  Log file:    $(Join-Path $env:TEMP 'titaniumloader.log')"
Write-Host ""
