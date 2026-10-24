#!/usr/bin/env bash
set -euo pipefail

DLL_PATH="$1"
OUTPUT="$2"
FORWARD_NAME="${3:-}"

if [ ! -f "$DLL_PATH" ]; then
    echo "Error: $DLL_PATH not found"
    exit 1
fi

if [ -z "$FORWARD_NAME" ]; then
    BASENAME="$(basename "$DLL_PATH")"
    FORWARD_NAME="${BASENAME%.dll}"
fi

echo "EXPORTS" > "$OUTPUT"

x86_64-w64-mingw32-objdump -p "$DLL_PATH" | awk -v fwd="$FORWARD_NAME" '
    /^\[Ordinal\/Name Pointer\] Table/ { in_names = 1; next }
    in_names && /^$/ { in_names = 0 }
    in_names && /^\s+\[/ {
        name = $NF
        gsub(/[ \t\r\n]+$/, "", name)
        if (name != "") {
            printf "  %s = %s.%s\n", name, fwd, name
        }
    }
' >> "$OUTPUT"

echo "  titanium_hook" >> "$OUTPUT"
echo "  titanium_unhook" >> "$OUTPUT"
echo "  titanium_resolve_address" >> "$OUTPUT"
echo "  titanium_log" >> "$OUTPUT"

COUNT=$(grep -c "^  " "$OUTPUT" || true)
echo "Generated $OUTPUT with $COUNT exports (forward to %s)"
