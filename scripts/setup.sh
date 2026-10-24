#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "[TitaniumLoader] Setting up build environment..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[TitaniumLoader] Configuring with CMake..."
cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Release

echo "[TitaniumLoader] Building..."
cmake --build . -j"$(nproc)"

echo "[TitaniumLoader] Installing to system..."
sudo cmake --install .

echo "[TitaniumLoader] Done!"
echo ""
echo "  Run:  titanium  (launches GD with TitaniumLoader)"
echo "  Logs: tail -f /tmp/titaniumloader.log"
