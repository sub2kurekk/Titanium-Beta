# Writing Mods

A TitaniumLoader mod is a shared library (`.dll`) with one required exported
function (`titaniumModInit`) and an accompanying `mod.json`. The SDK is header-only
(`sdk/include/Titanium/`) — a mod does not link statically against the loader; it
talks to it through the proxy exports at runtime.

## Minimal mod

```cpp
#include <Titanium/Mod.hpp>
#include <Titanium/Log.hpp>

class ExampleMod : public titanium::Mod {
public:
    titanium::ModInfo getInfo() const override {
        return {
            .id          = "example",
            .name        = "Example Mod",
            .version     = "1.0.0",
            .author      = "TitaniumLoader",
            .description = "An example mod for TitaniumLoader",
        };
    }

    void onLoad() override {
        TITANIUM_MOD_LOG(Info, "Example mod loaded!");
    }

    void onUnload() override {
        TITANIUM_MOD_LOG(Info, "Example mod unloaded!");
    }
};

TITANIUM_MOD(ExampleMod)
```

Full working example: [`mods/example/src/main.cpp`](../mods/example/src/main.cpp).

## Mod lifecycle

The `TITANIUM_MOD(modClass)` macro ([`Mod.hpp`](../sdk/include/Titanium/Mod.hpp))
exports:

```cpp
extern "C" {
    titanium::Mod* titaniumModInit();       // new modClass()
    void titaniumModExit(titanium::Mod*);   // delete mod
}
```

The loader calls `titaniumModInit()` on load, then **requires** `onLoad()`.
`onUnload()` is called on removal. `getInfo()` is mandatory (pure virtual) — it
returns a `ModInfo` (id/name/version/author/description, all `string_view`).

## mod.json

Mod metadata, parsed by the loader (minimal JSON, no external libraries):

```json
{
    "id": "example",
    "name": "Example Mod",
    "version": "1.0.0",
    "author": "TitaniumLoader",
    "description": "An example mod for TitaniumLoader",
    "loaderVersion": ">=0.1.0"
}
```

The file **must** sit next to the `.dll` in the same (flat) directory.

## Mod layout (flat!)

Mods are NOT in subdirectories. Everything in one folder:

```
<proton-pfx>/drive_c/users/steamuser/.titaniumloader/mods/
├── example_mod.dll
└── mod.json
```

`discoverMods()` does a flat `*.dll` scan — **subdirs are ignored**. One
`mod.json` describes one DLL.

## Building a mod

CMake:

```cmake
if(MINGW)
    add_library(my_mod SHARED src/main.cpp)
    # Self-contained: statically link the MinGW runtime
    # (otherwise LoadLibraryA throws 126 — no libgcc_s_seh-1.dll in the prefix)
    target_link_options(my_mod PRIVATE
        -static -static-libgcc -static-libstdc++
    )
else()
    add_library(my_mod MODULE src/main.cpp)
endif()
target_include_directories(my_mod PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
```

⚠️ **`-static` is mandatory.** Without it the mod will look for
`libgcc_s_seh-1.dll` / `libstdc++-6.dll`, which are absent from the Proton
prefix → `LoadLibraryA` returns error 126 and the mod fails to load.

## Hooking

The SDK offers two API levels ([`Hook.hpp`](../sdk/include/Titanium/Hook.hpp)):

### Low level: `titanium::hook()`

```cpp
titanium::HookHandle h;
titanium::hook((void*)targetAddr, (void*)myDetour, &h);
// h.trampoline -> pointer to the original
// ...
titanium::unhook(&h);
```

Addresses are resolved via `titanium::resolveAddress("SymbolName")`.

### `TITANIUM_HOOK` macro

Automatic detour with trampoline, registered in a
`__attribute__((constructor))`:

```cpp
TITANIUM_HOOK(void, MenuLayer_init, /* args */) {
    // your code before/after
    // call the original through `MenuLayer_init` (the trampoline)
    return MenuLayer_init(/* args */);
}
```

The macro generates: a detour function, a pointer typedef, a trampoline
variable, and a constructor that calls `resolveAddress(#name)` + `hook`.

> **Important (known bug pattern):** pass the **address** to `titanium::hook()`,
> do not dereference it. `titanium::hook((void*)addr, ...)` is correct;
> `titanium::hook((void*)*addr, ...)` is a bug.

## Logging

[`Log.hpp`](../sdk/include/Titanium/Log.hpp)) — logs go to the loader's log file
(`titaniumloader.log` in the Proton prefix `%TEMP%`):

```cpp
TITANIUM_LOG(Info, "global message");
TITANIUM_MOD_LOG(Info, "message tagged with the mod id");
```

Levels: `Debug`, `Info`, `Warn`, `Error`. Under the hood these call the
`titanium_log` export from the proxy.

## Events

[`Event.hpp`](../sdk/include/Titanium/Event.hpp)) — a simple in-process pub/sub
template:

```cpp
titanium::Event<int> onScore;
onScore.listen([](int s) { /* ... */ });
onScore.trigger(42);
```

`listen`/`remove`/`trigger`. Useful for inter-mod communication within the same
process (does not perturb GD's memory).

## Loader version

[`Version.hpp`](../sdk/include/Titanium/Version.hpp)) — `titanium::LoaderVersion`,
`titanium::Version` with `operator>=`. Check the `loaderVersion` from `mod.json`
against the runtime version if your mod depends on specific offsets (e.g.
`0x333a90` for `MenuLayer::init` — GD 2.206 x64).

## Conventions

- Everything in `namespace titanium`
- C++23, no exceptions, no RTTI
- Windows API instead of `std::filesystem`
- Log via `titanium::log*`, not `std::cout`
- Self-contained DLL (`-static`)