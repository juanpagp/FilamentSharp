#!/usr/bin/env bash
# Publishes every sample as a self-contained Native AOT executable, bundles
# the native bridge and assets next to it, and smoke-runs each one.
#
# BasicRendering runs its smoke headlessly (always). WindowedRendering's smoke
# needs a reachable X server: it runs when DISPLAY is set and is reported as
# skipped otherwise (exit 0 with a message, not a failure).
#
# Output: external/aot-publish/<SampleName>/  (Git-ignored)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

OUT_BASE="${FILAMENTSHARP_AOT_OUTPUT_DIR:-$ROOT/external/aot-publish}"

if [ ! -f "$FILAMENTSHARP_NATIVE_DIR/libFilamentSharp.Native.so" ]; then
    echo "publish-aot: native bridge not found - run scripts/build-native.sh first" >&2
    exit 1
fi
if [ ! -f "$ROOT/assets/models/triangle.glb" ]; then
    echo "publish-aot: test model missing - run scripts/gen-test-gltf.py first" >&2
    exit 1
fi

bundle() {
    # bundle <sample-dir> <out-dir>: copy the bridge, materials, and glTF model
    # next to the published executable (the samples locate them by walking up).
    local out="$2"
    mkdir -p "$out/external/native-install/lib" \
             "$out/external/native-install/materials" \
             "$out/assets/models"
    cp "$FILAMENTSHARP_NATIVE_DIR/libFilamentSharp.Native.so" "$out/external/native-install/lib/"
    cp "$ROOT/external/native-install/materials/"*.filamat \
       "$out/external/native-install/materials/" 2>/dev/null || true
    cp "$ROOT/assets/models/triangle.glb" "$out/assets/models/"
}

echo "publish-aot: publishing BasicRendering (cross-links a native executable; needs a C linker)"
OUT_DIR="$OUT_BASE/BasicRendering"
rm -rf "$OUT_DIR"   # idempotent re-runs (a stale layout must not break MSB3191)
dotnet publish "$ROOT/samples/BasicRendering" \
    -c Release -r linux-x64 --self-contained true /p:PublishAot=true \
    -o "$OUT_DIR"
bundle samples/BasicRendering "$OUT_DIR"
echo "publish-aot: BasicRendering smoke run (headless)"
"$OUT_DIR/BasicRendering" "$OUT_DIR"
echo "publish-aot: done -> $OUT_DIR/BasicRendering"

echo ""
echo "publish-aot: publishing WindowedRendering"
OUT_DIR="$OUT_BASE/WindowedRendering"
rm -rf "$OUT_DIR"
dotnet publish "$ROOT/samples/WindowedRendering" \
    -c Release -r linux-x64 --self-contained true /p:PublishAot=true \
    -o "$OUT_DIR"
bundle samples/WindowedRendering "$OUT_DIR"
if [ -n "${DISPLAY:-}" ]; then
    echo "publish-aot: WindowedRendering smoke run (DISPLAY=$DISPLAY)"
    "$OUT_DIR/WindowedRendering" --frames 5 --width 128 --height 96
    echo "publish-aot: done -> $OUT_DIR/WindowedRendering"
else
    # Recorded honestly: the binary was built and linked as AOT, the runtime
    # half needs a display. Manual run instructions: docs/windowed-rendering.md.
    echo "publish-aot: WindowedRendering AOT build OK; runtime smoke SKIPPED"
    echo "publish-aot:   (no DISPLAY in this environment). Run it yourself:"
    echo "publish-aot:   DISPLAY=:0 $OUT_DIR/WindowedRendering --frames 240"
fi
