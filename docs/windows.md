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

   Only the compiler is needed. The installer reads the original DLL's exports
   itself, so no other tools are required.

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

## Install into a different folder (keep Steam unmodded)

`-GdPath` installs Titanium into exactly the folder you give it, and **nothing
else**. The script never copies the game and never touches your Steam install
when you pass it, so you can keep one clean copy in Steam and one modded copy
elsewhere.

1. Copy the game folder yourself, for example in Explorer: copy
   `...\steamapps\common\Geometry Dash` to `C:\GD`. The copy must contain
   `GeometryDash.exe` and `pthreadVCE3.dll`.
2. Install into that folder only:

   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -GdPath "C:\GD"
   ```

3. Start the modded game with `C:\GD\GeometryDash.exe` (a desktop shortcut works
   well). Launching from Steam still starts your clean, unmodded install.

Everything the script writes goes to the folder you passed, this repository's
`build-windows` folder, and `%USERPROFILE%\.titaniumloader\mods`. Mods are shared
between all modded copies because they use the same mods folder.

If the game does not start from a copy outside Steam's library, make sure Steam
is running. Some games also need a `steam_appid.txt` file next to the exe; for
Geometry Dash its only content is the app id `322170`.

## Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\uninstall.ps1
```

This moves `pthreadVCE3_o.dll` back over `pthreadVCE3.dll`. Your mods folder is
left untouched. Verifying the game files in Steam also replaces the proxy with
the original library.

For a separate copy, either delete its folder or restore it in place:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\uninstall.ps1 -GdPath "C:\GD"
```

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
| `is not a 64-bit (x64) DLL` | Your Geometry Dash is a 32-bit build. Titanium builds a 64-bit proxy and currently supports only 64-bit Geometry Dash (hook offsets target GD 2.206 x64). |
| `no named exports found` | `pthreadVCE3_o.dll` is not the pthreads library the game normally ships. If you ran an earlier broken install, restore the game's original files (Steam: verify game files) and try again, or report the message together with your GD version. |
| `compilation failed` | The compiler's messages are printed just above the error and saved to `build-windows\compile.log`. The first `error:` line is the useful one. Also check the `Compiler target` / `version` lines the script prints: it needs a 64-bit MinGW-w64 GCC (`x86_64-w64-mingw32`), ideally GCC 13 or newer. |
| `this compiler builds for ... but Titanium needs a 64-bit MinGW-w64 compiler` | A different `g++` (for example a 32-bit MinGW) is first on `PATH`. Install `mingw-w64-x86_64-gcc` with MSYS2 and put `C:\msys64\mingw64\bin` first on `PATH`, or remove the other compiler from `PATH`. |
| `Geometry Dash directory not found` | Pass the folder explicitly with `-GdPath`. It must contain `GeometryDash.exe`. |
| `could not write pthreadVCE3.dll` | Geometry Dash is still running. Close it and run the script again. |
| Game does not start after install | Run `uninstall.ps1`, then verify game files in Steam, then install again. |
| No `titaniumloader.log` appears | Check that `pthreadVCE3_o.dll` sits next to `pthreadVCE3.dll` in the game folder. Antivirus software can also quarantine unsigned proxy DLLs, so check its history. |
| Hook or pattern-scan errors in the log | Offsets are specific to the GD version. See [Debugging](./debugging.md#hook-does-not-work--crash-on-hook). |
