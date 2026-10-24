# Debugging

## Where logs live

The loader logs to `titaniumloader.log` in `%TEMP%` inside the Proton prefix.
On Linux that prefix is:

```
~/.steam/steam/steamapps/compatdata/322170/pfx/drive_c/users/steamuser/AppData/Local/Temp/titaniumloader.log
```

Find the file regardless of the exact Steam path:

```bash
find ~/.steam/steam/steamapps/compatdata/322170 -name titaniumloader.log
```

Follow it live:

```bash
tail -f ~/.local/share/Steam/steamapps/compatdata/322170/pfx/drive_c/users/steamuser/AppData/Local/Temp/titaniumloader.log
```

(Linux native build: logs at `/tmp/titaniumloader.log`.)

## Key lines to look for

| Log line | Meaning |
| --- | --- |
| `[Titanium] GD base: ...` | Pattern scan loaded the GD base |
| `[Titanium] MenuLayer::init hook installed at ...` | Hook active |
| `[TitaniumUI] Mod Library button injected at ...` | The "Mods" button works |
| `[Titanium] Loaded mod: <id>` | Mod loaded + `onLoad` fired |

No `[Titanium] GD base:` → the pattern scan did not find cocos2d (likely a wrong GD
version, or libcocos2d was not loaded at scan time).

## Common problems

### `LoadLibraryA` error 126 for a mod

The mod is not self-contained — `libgcc_s_seh-1.dll` / `libstdc++-6.dll` are
missing from the Proton prefix. **Fix:** build the mod with
`-static -static-libgcc -static-libstdc++` (see
[Building → Why -static](./building.md#why--static)).

### Mod does not load, but the DLL is in the directory

- The mod is in a **subdirectory** — `discoverMods()` scans flat, subdirs are
  ignored. Everything (`*.dll` + `mod.json`) must sit directly in
  `.titaniumloader/mods/`.
- Missing `titaniumModInit` export — check `TITANIUM_MOD()` in the source and verify
  the DLL actually exports the symbol (`x86_64-w64-mingw32-objdump -p mod.dll`).

### GD crashes before MenuLayer::init

Known issue: `CAPIJobRequestUserStats - Server response failed 2`. This is a
Steam API / Proton runtime crash, **not** our bug — it occurs independently of
TitaniumLoader. Effect: the "Mods" button has not yet been visually confirmed
in-game, because GD crashes first.

### Hook does not work / crash on hook

- Check that `resolveAddress()` returned an address (log `[Titanium] GD base:`).
- Make sure you pass the **address**, not a dereference:
  `titanium::hook((void*)addr, ...)` ✓, `titanium::hook((void*)*addr, ...)` ✗.
- Offsets are specific to the GD version. `MenuLayer::init` = `0x333a90` for
  **GD 2.206 x64**. A different version → a different offset.

### The proxy does not load at all

- The `.def` is missing or empty → the proxy does not export the pthread
  symbols and GD fails to start. Regenerate: `scripts/gendef.sh`.
- The original was not backed up correctly as `pthreadVCE3_o.dll`.

## Reinstalling the proxy

If the proxy is in a bad state, rebuild and reinstall:

```bash
./scripts/install-proton.sh
```

The script detects a previous install (`pthreadVC3_o.dll`) and restores the
original before reinstalling.