# Windows

Titanium runs natively on Windows. It works the same way as on Linux/Proton: a
**proxy DLL** replaces `pthreadVCE3.dll` in the Geometry Dash folder, forwards all
of that library's exports to the original (renamed `pthreadVCE3_o.dll`), and
initializes the loader on a separate thread.

The loader code is the same as in the Proton build (it is a Windows DLL either
way). Only the build and install tooling differs, and it lives in
[`scripts/windows/`](../scripts/windows). The Linux scripts and the native Linux
launcher are not affected.

## Requirements

- Windows 10 or 11, 64-bit
- Geometry Dash installed through Steam (hook offsets target **GD 2.206 x64**)
- **MinGW-w64 GCC** (GCC 13 or newer, because the code is built as C++23)

### Installing MinGW-w64 with MSYS2

1. Install [MSYS2](https://www.msys2.org/) to the default location (`C:\msys64`).
2. Open the **MSYS2 MINGW64** shell and run:

   ```bash
   pacman -S mingw-w64-x86_64-gcc
   ```

   This also installs `objdump`, which the installer uses to read the original
   DLL's exports.

The install script looks for the compiler on `PATH` first, then in
`C:\msys64\mingw64\bin` and `C:\msys64\ucrt64\bin`. If you installed MSYS2
somewhere else, add its `mingw64\bin` folder to `PATH`.

## Install

Close Geometry Dash, then from the repository root in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1
```

`-ExecutionPolicy Bypass` applies to this one command only. Windows blocks
unsigned local scripts by default, and this avoids changing your system policy.

The script finds Geometry Dash through the Steam registry entry and the library
folders listed in `libraryfolders.vdf`, so games installed on other drives are
found too. To point it at a specific folder:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -GdPath "D:\Games\Geometry Dash"
```

The script, in order:

1. **Backs up the original**: `pthreadVCE3.dll` becomes `pthreadVCE3_o.dll` (only
   once; later runs never overwrite the backup).
2. **Generates the `.def`** from the original DLL's exports
   (`scripts\windows\gendef.ps1`).
3. **Compiles** `build-windows\titaniumproxy.dll` with MinGW.
4. **Installs** it by copying it into the game folder as `pthreadVCE3.dll`.
5. **Creates the mods folder** at `%USERPROFILE%\.titaniumloader\mods`.

Then launch Geometry Dash from Steam as usual.

## Install into a separate copy (keep Steam unmodded)

If you want your Steam install to stay completely clean, let the script copy the
game to a second folder and install Titanium only there:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -CopyTo "C:\GD"
```

The script:

1. Finds your Steam install of Geometry Dash (or uses `-GdPath` as the source).
2. Copies the whole game to `C:\GD` with `robocopy`. This can take a few
   minutes. If `C:\GD` already contains `GeometryDash.exe`, the copy is skipped
   and only Titanium is (re)installed there.
3. Backs up, builds and installs the proxy DLL **inside `C:\GD` only**.
4. Writes `steam_appid.txt` (containing `322170`) next to the copy's
   `GeometryDash.exe` if it is missing, so Steam's API can recognise the game
   when it is not run from Steam's library folder.

The Steam folder is never written to. The script refuses to run if the target is
the Steam folder itself, is inside it, or contains it, and it refuses a target
that is non-empty but is not already a Geometry Dash copy.

**To play the modded game**, keep Steam running and start
`C:\GD\GeometryDash.exe` yourself (a desktop shortcut to it works well). Starting
Geometry Dash from the Steam library launches the unmodded game, as before.

To update the copy after rebuilding Titanium, run the same command again. To
remove everything, delete the `C:\GD` folder. Your mods in
`%USERPROFILE%\.titaniumloader` are shared by every install, since both use the
same mods folder.

> Note: this relies on Geometry Dash starting outside Steam's library folder
> while Steam is running. If the game refuses to start from the copy, use the
> normal in-place install instead.

## Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\uninstall.ps1
```

This moves `pthreadVCE3_o.dll` back over `pthreadVCE3.dll`. Your mods folder is
left untouched. Verifying the game files in Steam also replaces the proxy with
the original library.

For a separate copy made with `-CopyTo`, either delete the folder, or restore it
in place with `uninstall.ps1 -GdPath "C:\GD"`.

## Mods

Mods go in a **flat** folder (no subdirectories):

```
%USERPROFILE%\.titaniumloader\mods\
├── example_mod.dll
└── mod.json
```

Build mods with the static flags shown in [Building](./building.md#building-mods),
otherwise loading fails with error 126. See [Writing Mods](./modding.md) for the
SDK and `mod.json`.

To build the loader library and the example mod without touching the game
(useful when developing mods), use CMake from the MSYS2 MINGW64 shell:

```bash
cmake -G "MinGW Makefiles" -B build-windows-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows-cmake
```

This produces `loader/libtitaniumloader.dll` and `mods/example/libexample_mod.dll`
inside the build folder. It does not build or install the proxy DLL; use
`install.ps1` for that.

## Logs

The log is written to `%TEMP%\titaniumloader.log`. Follow it live with:

```powershell
Get-Content "$env:TEMP\titaniumloader.log" -Wait
```

What to look for is described in [Debugging](./debugging.md#key-lines-to-look-for).

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `running scripts is disabled on this system` | Run the script through `powershell -ExecutionPolicy Bypass -File ...` as shown above. |
| `MinGW-w64 g++ not found` | Install MinGW-w64 (see above) or add its `bin` folder to `PATH`. |
| `Geometry Dash directory not found` | Pass the folder explicitly with `-GdPath`. It must contain `GeometryDash.exe`. |
| `could not write pthreadVCE3.dll` | Geometry Dash is still running. Close it and run the script again. |
| `-CopyTo must be a separate folder` | The target is the Steam install, inside it, or contains it. Pick a folder elsewhere, such as `C:\GD`. |
| `already exists, is not empty and has no GeometryDash.exe` | `-CopyTo` points at an unrelated folder. Use a new or empty folder. |
| The copy starts but asks for or restarts through Steam | Make sure Steam is running and signed in to the account that owns the game. |
| Game does not start after install | Run `uninstall.ps1`, then verify game files in Steam, then install again. |
| No `titaniumloader.log` appears | Check that `pthreadVCE3_o.dll` sits next to `pthreadVCE3.dll` in the game folder. Antivirus software can also quarantine unsigned proxy DLLs, so check its history. |
| Hook or pattern-scan errors in the log | Offsets are specific to the GD version. See [Debugging](./debugging.md#hook-does-not-work--crash-on-hook). |
