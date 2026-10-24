#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ORIG_DLL="pthreadVCE3.dll"
ORIG_BAK="pthreadVCE3_o.dll"

GD_DIR="${1:-}"
if [ -z "$GD_DIR" ]; then
    CANDIDATES=(
        "$HOME/.steam/steam/steamapps/common/Geometry Dash"
        "$HOME/.local/share/Steam/steamapps/common/Geometry Dash"
        "/opt/GeometryDash"
    )
    for dir in "${CANDIDATES[@]}"; do
        if [ -f "$dir/GeometryDash.exe" ]; then
            GD_DIR="$dir"
            break
        fi
    done
fi

if [ -z "$GD_DIR" ] || [ ! -f "$GD_DIR/GeometryDash.exe" ]; then
    echo "Error: Geometry Dash directory not found."
    echo "Usage: $0 /path/to/Geometry/Dash"
    exit 1
fi

echo "[Titanium] Geometry Dash: $GD_DIR"

# Restore previous pthreadVC3 proxy if it exists (from old install)
if [ -f "$GD_DIR/pthreadVC3_o.dll" ] && [ ! -f "$GD_DIR/pthreadVC3.dll.bak" ]; then
    cp "$GD_DIR/pthreadVC3.dll" "$GD_DIR/pthreadVC3.dll.bak"
    mv "$GD_DIR/pthreadVC3_o.dll" "$GD_DIR/pthreadVC3.dll"
    echo "[Titanium] Restored original pthreadVC3.dll (old install cleanup)"
fi

# Backup original pthreadVCE3.dll
if [ -f "$GD_DIR/$ORIG_DLL" ] && [ ! -f "$GD_DIR/$ORIG_BAK" ]; then
    echo "[Titanium] Backing up $ORIG_DLL -> $ORIG_BAK"
    cp "$GD_DIR/$ORIG_DLL" "$GD_DIR/$ORIG_BAK"
elif [ ! -f "$GD_DIR/$ORIG_BAK" ]; then
    echo "Error: $ORIG_DLL not found in GD directory!"
    exit 1
fi

# Generate .def
echo "[Titanium] Generating export definitions..."
BUILD_DIR="$PROJECT_DIR/build-proton"
mkdir -p "$BUILD_DIR"
"$PROJECT_DIR/scripts/gendef.sh" "$GD_DIR/$ORIG_BAK" "$BUILD_DIR/titaniumproxy.def" "pthreadVCE3_o"

# Compile proxy DLL
echo "[Titanium] Compiling titaniumproxy.dll..."
x86_64-w64-mingw32-g++ \
    -std=c++23 \
    -shared \
    -o "$BUILD_DIR/titaniumproxy.dll" \
    "$PROJECT_DIR/loader/src/dllmain.cpp" \
    "$PROJECT_DIR/loader/src/hookengine.cpp" \
    "$PROJECT_DIR/loader/src/patternscan.cpp" \
    "$PROJECT_DIR/loader/src/modloader.cpp" \
    "$PROJECT_DIR/loader/src/gdinit.cpp" \
    "$PROJECT_DIR/loader/src/loader.cpp" \
    "$PROJECT_DIR/loader/src/ui/modmenu.cpp" \
    "$PROJECT_DIR/loader/src/log.cpp" \
    -I"$PROJECT_DIR/loader/include" \
    -I"$PROJECT_DIR/sdk/include" \
    -lpsapi \
    -static -static-libgcc -static-libstdc++ \
    "$BUILD_DIR/titaniumproxy.def" \
    -Wl,--enable-stdcall-fixup

echo "[Titanium] Compilation OK."

# Install - rename our DLL to the original DLL name
cp "$BUILD_DIR/titaniumproxy.dll" "$GD_DIR/$ORIG_DLL"

echo "[Titanium] Installed $ORIG_DLL (proxy)."
echo ""
echo "[Titanium] === DONE ==="
echo "  Launch GD normally from Steam."
echo "  Logs will be in <pfx>/drive_c/users/steamuser/Temp/titaniumloader.log"
echo "  Find them: find ~/.steam/steam/steamapps/compatdata/322170 -name titaniumloader.log"
echo ""
