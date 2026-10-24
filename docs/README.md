# TitaniumLoader — Documentation

TitaniumLoader is a mod loader for **Geometry Dash** on Linux (Steam/Proton) and
Windows. It injects into the game as a proxy DLL, scans memory for GD/cocos2d, hooks key
functions, and loads mods from the user's mod directory.

## Table of Contents

| Document | Topic |
| --- | --- |
| [Installation](./installation.md) | How to install the loader into the game (Proton/Steam) |
| [Windows](./windows.md) | Installing, building and troubleshooting on Windows |
| [Building](./building.md) | Compiling the loader and mods (CMake + MinGW) |
| [Architecture](./architecture.md) | Proxy DLL, hook engine, pattern scan, mod loader, UI |
| [Writing Mods](./modding.md) | SDK, mod lifecycle, hooks, logging, `mod.json` |
| [Debugging](./debugging.md) | Where logs live, what to look for, known issues |

## Quick Start

### Windows

```powershell
# 1. Build and install the proxy DLL into the GD directory (needs MinGW-w64)
powershell -ExecutionPolicy Bypass -File scripts\windows\install.ps1

# 2. Launch Geometry Dash normally from Steam

# 3. Watch the logs
Get-Content "$env:TEMP\titaniumloader.log" -Wait
```

### Linux (Steam/Proton)

```bash
# 1. Build and install the proxy DLL into the GD directory
./scripts/install-proton.sh

# 2. Launch Geometry Dash normally from Steam
#    (the loader injects itself automatically)

# 3. Watch the logs
find ~/.steam/steam/steamapps/compatdata/322170 -name titaniumloader.log
```

## Status

See [Debugging](./debugging.md) for known issues.
The loader currently works end-to-end (proxy, pattern scan, hooks, mod loading,
the "Mods" menu button), but a live in-game test is blocked by a Steam API crash
that is independent of TitaniumLoader.