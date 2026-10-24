# Installation

TitaniumLoader works as a **proxy DLL** — it replaces a library that Geometry Dash
loads normally (`pthreadVCE3.dll`), forwards all of its exports to the original,
and quietly initializes the loader on a separate thread.

## Requirements

- Linux + Steam + **Proton** (Geometry Dash launched through Proton)
- `x86_64-w64-mingw32-g++` (MinGW cross-compiler)
- `x86_64-w64-mingw32-objdump` (for `.def` generation)
- Geometry Dash installed (Steam appid `322170`)

## One-script install

```bash
./scripts/install-proton.sh [/path/to/Geometry/Dash]
```

If you omit the path, the script searches typical locations:

- `~/.steam/steam/steamapps/common/Geometry Dash`
- `~/.local/share/Steam/steamapps/common/Geometry Dash`
- `/opt/GeometryDash`

The script, in order:

1. **Backs up the original** — `pthreadVCE3.dll` → `pthreadVCE3_o.dll` (only
   once; subsequent runs do not overwrite the backup).
2. **Generates the `.def`** from the original DLL's exports
   (`scripts/gendef.sh`).
3. **Compiles** `titaniumproxy.dll` with MinGW (cross-compiled to Windows).
4. **Installs** — copies our proxy as `pthreadVCE3.dll` in the GD directory.

After installation, launch GD normally from Steam. The loader injects itself.

## Uninstall

Remove the proxy and restore the original:

```bash
cd "/path/to/Geometry Dash"
mv pthreadVCE3_o.dll pthreadVCE3.dll
```

The install script also detects a previous install on the next run and restores
the original before reinstalling (see the `pthreadVC3_o.dll` logic in the
script).

## Mods

Mods live in the user directory inside the Proton prefix, using a **flat**
layout (no subdirectories):

```
<proton-pfx>/drive_c/users/steamuser/.titaniumloader/mods/
├── example_mod.dll
└── mod.json
```

More: [Writing Mods → mod.json](./modding.md#modjson).