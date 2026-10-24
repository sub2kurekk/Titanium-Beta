# Building

TitaniumLoader builds in two ways:

- **Proton (Windows DLL)** — MinGW cross-compilation to `titaniumproxy.dll`.
  This is the target path for GD under Proton.
- **Linux native** — CMake builds `libtitaniumloader.so` + the `titanium` launcher
  (optional, for development).

## Proton — proxy DLL

Easiest via the install script (builds + installs):

```bash
./scripts/install-proton.sh
```

To rebuild only the DLL without reinstalling into the game:

```bash
x86_64-w64-mingw32-g++ -std=c++23 -shared \
    -o build-proton/titaniumproxy.dll \
    loader/src/dllmain.cpp loader/src/hookengine.cpp \
    loader/src/patternscan.cpp loader/src/modloader.cpp \
    loader/src/gdinit.cpp loader/src/loader.cpp \
    loader/src/ui/modmenu.cpp loader/src/log.cpp \
    -Iloader/include -Isdk/include -lpsapi \
    -static-libgcc -static-libstdc++ \
    build-proton/titaniumproxy.def \
    -Wl,--enable-stdcall-fixup
```

> The `.def` must exist (`scripts/gendef.sh` generates it from the original
> `pthreadVCE3_o.dll`). Without it the proxy will not export the pthread symbols
> and GD will fail to start.

## Linux native — CMake

```bash
./scripts/setup.sh        # configure + build + sudo install
# or manually:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

After install: run `titanium` (launches GD with the loader); logs go to
`/tmp/titaniumloader.log`.

The MinGW toolchain for CMake lives in `cmake/mingw-w64-x86_64.cmake`.

## Building mods

Mods are separate shared libraries, statically linked against the MinGW runtime
(important — see below):

```cmake
if(MINGW)
    add_library(example_mod SHARED src/main.cpp)
    # Self-contained: statically link the MinGW runtime
    target_link_options(example_mod PRIVATE
        -static -static-libgcc -static-libstdc++
    )
else()
    add_library(example_mod MODULE src/main.cpp)
endif()
target_include_directories(example_mod PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
```

Full example: [`mods/example/`](../mods/example).

### Why `-static`?

Without `libgcc_s_seh-1.dll` / `libstdc++-6.dll` present in the Proton prefix,
`LoadLibraryA` fails with **error 126** ("DLL not found"). Static linking makes
the mod self-contained and load cleanly. **Do not omit this flag.**

## Compile conventions

- **C++23**, no exceptions, no RTTI
- Windows API instead of `std::filesystem` (MinGW compatibility)
- No external dependencies beyond WinAPI / `psapi`