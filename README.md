# Titanium

Titanium is a mod loader for **Geometry Dash** on Linux (Steam / Proton).

It installs as a proxy DLL, so there is no separate launcher on the Proton path:
Geometry Dash loads Titanium by itself on startup. Titanium then scans memory for
GD / cocos2d, hooks key game functions, adds a **Mods** button to the main menu,
and loads mods from your mod directory.

> **Beta.** The loader works end-to-end (proxy, pattern scan, hooks, mod loading,
> the Mods button), but a live in-game test is currently blocked by a Steam API
> crash that is independent of Titanium. See [Known issues](#known-issues).

## Requirements

- Linux with Steam and **Proton** (Geometry Dash launched through Proton)
- Geometry Dash installed (Steam appid `322170`) — hook offsets target **GD 2.206 x64**
- `x86_64-w64-mingw32-g++` (MinGW cross-compiler, C++23)
- `x86_64-w64-mingw32-objdump` (used to generate the export `.def`)

## Install

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

## Uninstall

Restore the original DLL:

```bash
cd "/path/to/Geometry Dash"
mv pthreadVCE3_o.dll pthreadVCE3.dll
```

## Mods

Mods live inside the Proton prefix, in a **flat** directory (no subfolders):

```
<proton-pfx>/drive_c/users/steamuser/.titaniumloader/mods/
├── example_mod.dll
└── mod.json
```

An example mod is in [`mods/example/`](./mods/example). Mod DLLs must be built
with `-static -static-libgcc -static-libstdc++`, otherwise loading fails with
error 126. See [Writing Mods](./docs/modding.md) for the SDK, lifecycle, hooks and
`mod.json`.

## Logs

```bash
find ~/.steam/steam/steamapps/compatdata/322170 -name titaniumloader.log
```

Follow the file it finds with `tail -f`. A healthy startup logs
`[Titanium] GD base: ...`, then `MenuLayer::init hook installed`, then
`Loaded mod: <id>` for each mod. More in [Debugging](./docs/debugging.md).

## Building without installing

To rebuild only the proxy DLL, or to build the optional Linux-native launcher
(`titanium` + `libtitaniumloader.so`, installed with `./scripts/setup.sh`), see
[Building](./docs/building.md).

## Migrating from the old name

Titanium was previously called Prism. If you installed an older build:

- Re-run `./scripts/install-proton.sh` to replace the old proxy DLL.
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

## Documentation

| Document | Topic |
| --- | --- |
| [Installation](./docs/installation.md) | Installing into the game (Proton/Steam) |
| [Building](./docs/building.md) | Compiling the loader and mods |
| [Architecture](./docs/architecture.md) | Proxy DLL, hook engine, pattern scan, mod loader, UI |
| [Writing Mods](./docs/modding.md) | SDK, mod lifecycle, hooks, logging, `mod.json` |
| [Debugging](./docs/debugging.md) | Logs, what to look for, common problems |
