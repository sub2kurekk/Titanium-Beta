# Titanium

Titanium is a mod loader for **Geometry Dash** on **Linux (Steam / Proton)** and
**Windows**.

It installs as a proxy DLL, so there is no separate launcher on these paths:
Geometry Dash loads Titanium by itself on startup. Titanium then scans memory for
GD / cocos2d, hooks key game functions, adds a **Mods** button to the main menu,
and loads mods from your mod directory.

> **Beta.** The loader works end-to-end (proxy, pattern scan, hooks, mod loading,
> the Mods button), but a live in-game test is currently blocked by a Steam API
> crash that is independent of Titanium. See [Known issues](#known-issues).

Jump to: [Linux install](#linux-steam--proton) · [Windows install](#windows) ·
[Mods](#mods) · [Logs](#logs)

## Linux (Steam / Proton)

### Requirements

- Linux with Steam and **Proton** (Geometry Dash launched through Proton)
- Geometry Dash installed (Steam appid `322170`) — hook offsets target **GD 2.206 x64**
- `x86_64-w64-mingw32-g++` (MinGW cross-compiler, C++23)
- `x86_64-w64-mingw32-objdump` (used to generate the export `.def`)

### Install

```bash
git clone https://github.com/sub2kurekk/Titanium-Beta.git
cd Titanium-Beta
./scripts/install-proton.sh
```

If your Geometry Dash folder is somewhere unusual, pass it explicitly:

```bash
./scripts/install-proton.sh "/path/to/Geometry Dash"
```

Without an argument, the script looks in:

- `~/.steam/steam/steamapps/common/Geometry Dash`
- `~/.local/share/Steam/steamapps/common/Geometry Dash`
- `/opt/GeometryDash`

The script does four things:

1. Backs up the original `pthreadVCE3.dll` as `pthreadVCE3_o.dll` (only once).
2. Generates `build-proton/titaniumproxy.def` from the original DLL's exports.
3. Cross-compiles `titaniumproxy.dll` with MinGW.
4. Copies it into the game folder as `pthreadVCE3.dll`.

Then just launch Geometry Dash from Steam as usual.

### Uninstall

Restore the original DLL:

```bash
cd "/path/to/Geometry Dash"
mv pthreadVCE3_o.dll pthreadVCE3.dll
```

## Windows

Titanium also runs natively on Windows 10/11 (64-bit). It is the same loader as
on Linux; only the build and install scripts differ. Full guide with
troubleshooting: [docs/windows.md](./docs/windows.md).

### Requirements

- Geometry Dash installed through Steam — hook offsets target **GD 2.206 x64**
- **MinGW-w64 GCC 13 or newer**, for example from [MSYS2](https://www.msys2.org/):
  in the *MSYS2 MINGW64* shell run `pacman -S mingw-w64-x86_64-gcc`

The installer finds MSYS2 in `C:\msys64` automatically. For any other location,
add MinGW's `bin` folder to `PATH`.

### Install

Close Geometry Dash, then in PowerShell:

```powershell
git clone https://github.com/sub2kurekk/Titanium-Beta.git
cd Titanium-Beta
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1
```

`-ExecutionPolicy Bypass` applies only to this command; Windows blocks unsigned
local scripts by default.

The script finds Geometry Dash through your Steam install and library folders. If
it cannot, pass the folder explicitly:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -GdPath "D:\Games\Geometry Dash"
```

The script does five things:

1. Backs up the original `pthreadVCE3.dll` as `pthreadVCE3_o.dll` (only once).
2. Generates `build-windows\titaniumproxy.def` from the original DLL's exports.
3. Compiles `titaniumproxy.dll` with MinGW.
4. Copies it into the game folder as `pthreadVCE3.dll`.
5. Creates the mods folder `%USERPROFILE%\.titaniumloader\mods`.

Then launch Geometry Dash from Steam as usual.

#### Keep your Steam install unmodded

To leave the Steam copy untouched and install Titanium into a second copy of the
game instead, add `-CopyTo`:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1 -CopyTo "C:\GD"
```

The game is copied to `C:\GD` and only that copy gets Titanium. Keep Steam
running and start `C:\GD\GeometryDash.exe` to play the modded game; launching from
Steam still gives you the clean one. Details in
[docs/windows.md](./docs/windows.md#install-into-a-separate-copy-keep-steam-unmodded).

### Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\uninstall.ps1
```

This restores the original `pthreadVCE3.dll` and leaves your mods untouched.

## Mods

On **Linux/Proton**, mods live inside the Proton prefix, in a **flat** directory
(no subfolders):

```
<proton-pfx>/drive_c/users/steamuser/.titaniumloader/mods/
├── example_mod.dll
└── mod.json
```

On **Windows**, the same layout lives in your user profile:

```
%USERPROFILE%\.titaniumloader\mods\
├── example_mod.dll
└── mod.json
```

An example mod is in [`mods/example/`](./mods/example). Mod DLLs must be built
with `-static -static-libgcc -static-libstdc++`, otherwise loading fails with
error 126. See [Writing Mods](./docs/modding.md) for the SDK, lifecycle, hooks and
`mod.json`.

## Logs

**Linux/Proton:**

```bash
find ~/.steam/steam/steamapps/compatdata/322170 -name titaniumloader.log
```

Follow the file it finds with `tail -f`.

**Windows:**

```powershell
Get-Content "$env:TEMP\titaniumloader.log" -Wait
```

A healthy startup logs `[Titanium] GD base: ...`, then
`MenuLayer::init hook installed`, then `Loaded mod: <id>` for each mod. More in
[Debugging](./docs/debugging.md).

## Building without installing

To rebuild only the proxy DLL, or to build the optional Linux-native launcher
(`titanium` + `libtitaniumloader.so`, installed with `./scripts/setup.sh`), see
[Building](./docs/building.md). For building on Windows without installing, see
[docs/windows.md](./docs/windows.md#mods).

## Migrating from the old name

Titanium was previously called Prism. If you installed an older build:

- Re-run the install script for your platform to replace the old proxy DLL.
- Move your mods from `.prismloader/mods/` to `.titaniumloader/mods/`.
- **Rebuild your mods** against the current SDK. The SDK headers moved to
  `Titanium/`, macros are now `TITANIUM_*`, exported symbols are now
  `titanium_*`, and the mod entry points are now `titaniumModInit` /
  `titaniumModExit`, so mods built for the old name will not load.

## Known issues

- GD may crash before `MenuLayer::init` with
  `CAPIJobRequestUserStats - Server response failed 2`. This is a Steam API /
  Proton runtime problem, not a Titanium bug, and it is why the Mods button has
  not yet been confirmed visually in-game.
- Hook offsets are specific to the GD version (`MenuLayer::init` = `0x333a90` on
  GD 2.206 x64). A different game version needs different offsets.
- The Windows scripts are new. The proxy DLL they build was verified by
  cross-compiling and inspecting its exports, but the PowerShell scripts have not
  yet been run on a real Windows machine. Please report problems as issues.

## Documentation

| Document | Topic |
| --- | --- |
| [Windows](./docs/windows.md) | Installing and troubleshooting on Windows |
| [Installation](./docs/installation.md) | Installing into the game (Proton/Steam) |
| [Building](./docs/building.md) | Compiling the loader and mods |
| [Architecture](./docs/architecture.md) | Proxy DLL, hook engine, pattern scan, mod loader, UI |
| [Writing Mods](./docs/modding.md) | SDK, mod lifecycle, hooks, logging, `mod.json` |
| [Debugging](./docs/debugging.md) | Logs, what to look for, common problems |
