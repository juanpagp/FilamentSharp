#!/usr/bin/env bash
# Publishes the BasicRendering sample as a self-contained Native AOT
# executable, bundles the native bridge and assets next to it, and runs it
# once as a smoke test.
#
# Output: external/aot-publish/ (git-ignored)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

OUT_DIR="${FILAMENTSHARP_AOT_OUTPUT_DIR:-$ROOT/external/aot-publish}"

if [ ! -f "$FILAMENTSHARP_NATIVE_DIR/libFilamentSharp.Native.so" ]; then
    echo "publish-aot: native bridge not found - run scripts/build-native.sh first" >&2
    exit 1
fi
if [ ! -f "$ROOT/assets/models/triangle.glb" ]; then
    echo "publish-aot: test model missing - run scripts/gen-test-gltf.py first" >&2
    exit 1
fi

echo "publish-aot: publishing (this cross-links a native executable; needs a C linker)"
dotnet publish "$ROOT/samples/BasicRendering" \
    -c Release -r linux-x64 --self-contained true /p:PublishAot=true \
    -o "$OUT_DIR"

echo "publish-aot: bundling native bridge and assets"
mkdir -p "$OUT_DIR/external/native-install/lib" \
         "$OUT_DIR/external/native-install/materials" \
         "$OUT_DIR/assets/models"
cp "$FILAMENTSHARP_NATIVE_DIR/libFilamentSharp.Native.so" \
   "$OUT_DIR/external/native-install/lib/"
cp "$ROOT/external/native-install/materials/"*.filamat \
   "$OUT_DIR/external/native-install/materials/" 2>/dev/null || true
cp "$ROOT/assets/models/triangle.glb" "$OUT_DIR/assets/models/"

echo "publish-aot: smoke run"
"$OUT_DIR/BasicRendering" "$OUT_DIR"

echo "publish-aot: done -> $OUT_DIR/BasicRendering"
