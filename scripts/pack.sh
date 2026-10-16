#!/usr/bin/env bash
# Pack the managed binding (with the native bridge from
# external/native-install) into a versioned .nupkg under
# artifacts/package/release — the local feed consumed by downstream
# repositories (e.g. PAL) via a relative nuget.config source.
#
# Prerequisites: scripts/build-native.sh has run (the native .so is packed
# into runtimes/linux-x64/native/).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

if [ ! -f "$FILAMENTSHARP_NATIVE_DIR/libFilamentSharp.Native.so" ]; then
    echo "pack: native bridge not found at $FILAMENTSHARP_NATIVE_DIR" >&2
    echo "  - run scripts/build-native.sh first" >&2
    exit 1
fi

OUT_DIR="$ROOT/artifacts/package/release"
echo "pack: creating FilamentSharp.nupkg in $OUT_DIR"
dotnet pack "$ROOT/src/FilamentSharp/FilamentSharp.csproj" \
    -c "${FILAMENTSHARP_MANAGED_CONFIG:-Release}" -o "$OUT_DIR"
echo "pack: done -> $OUT_DIR/FilamentSharp.1.0.0.nupkg"
