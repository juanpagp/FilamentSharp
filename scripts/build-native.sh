#!/usr/bin/env bash
# Configure, build, install, and test the native bridge (CMake + Ninja +
# CTest). Output: external/native-install/lib/libFilamentSharp.Native.so
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

BUILD_DIR="${FILAMENTSHARP_NATIVE_BUILD_DIR:-$ROOT/external/native-build}"
INSTALL_DIR="${FILAMENTSHARP_NATIVE_INSTALL_DIR:-$ROOT/external/native-install}"
CMAKE_BIN="${CMAKE_BIN:-cmake}"
NINJA_BIN="${NINJA_BIN:-ninja}"

if [ ! -f "$FILAMENT_SDK_DIR/lib/x86_64/libfilament.a" ]; then
    echo "build-native: Filament SDK not found - run scripts/fetch-filament.sh first" >&2
    exit 1
fi

echo "build-native: configuring with $CMAKE_BIN"
"$CMAKE_BIN" -G Ninja -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DFILAMENT_SDK_DIR="$FILAMENT_SDK_DIR" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

echo "build-native: building"
"$NINJA_BIN" -C "$BUILD_DIR"

echo "build-native: running native tests"
"${CTEST_BIN:-ctest}" --test-dir "$BUILD_DIR" --output-on-failure

echo "build-native: installing to $INSTALL_DIR"
"$CMAKE_BIN" --install "$BUILD_DIR"

echo "build-native: done -> $INSTALL_DIR/lib/libFilamentSharp.Native.so"
