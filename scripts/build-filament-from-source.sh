#!/usr/bin/env bash
# Build Google Filament from source at the pinned tag, as an alternative to the
# prebuilt release SDK (fetch-filament.sh).
#
# STATUS: UNVERIFIED in the reference environment (2 vCPU / 4 GiB RAM makes the
# full build impractical within the session budget; the default workflow uses
# the official prebuilt SDK). This script follows upstream BUILDING.md guidance
# and is provided for developers who need a source build. Verify on your
# machine before relying on it. See docs/building.md.
#
# Produces the same layout as the release SDK (lib/x86_64 + include + bin) in
# external/filament-sdk-src-install so both can be consumed identically via
# FILAMENT_SDK_DIR.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"
# shellcheck disable=SC1090
source "$ROOT/deps/filament.lock"

INSTALL_DIR="${FILAMENT_SRC_INSTALL_DIR:-$ROOT/external/filament-sdk-src-install}"
BUILD_DIR="${FILAMENT_SRC_BUILD_DIR:-$ROOT/external/filament-src-build}"
SRC_DIR="${FILAMENT_SRC_DIR:-$ROOT/external/filament-src}"
JOBS="${FILAMENT_BUILD_JOBS:-$(nproc)}"

if [ ! -d "$SRC_DIR/.git" ]; then
    echo "filament-src: cloning $FILAMENT_VERSION (shallow, with submodules)"
    git clone --depth 1 --branch "$FILAMENT_VERSION" \
        --recurse-submodules --shallow-submodules \
        https://github.com/google/filament "$SRC_DIR"
fi

ACTUAL_COMMIT="$(git -C "$SRC_DIR" rev-parse HEAD)"
if [ "$ACTUAL_COMMIT" != "$FILAMENT_COMMIT" ]; then
    echo "filament-src: commit mismatch (expected $FILAMENT_COMMIT, got $ACTUAL_COMMIT)" >&2
    exit 1
fi

echo "filament-src: configuring with CMake"
cmake -G Ninja -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCMAKE_INSTALL_LIBDIR=lib/x86_64 \
    -DFILAMENT_SUPPORTS_VULKAN=ON \
    -DFILAMENT_SUPPORTS_OPENGL=ON \
    -DFILAMENT_SUPPORTS_METAL=OFF \
    -DFILAMENT_BUILD_SAMPLES=OFF

echo "filament-src: building with -j$JOBS (this can take a long time)"
ninja -C "$BUILD_DIR" -j "$JOBS"
ninja -C "$BUILD_DIR" install

echo "filament-src: installed to $INSTALL_DIR"
echo "filament-src: use it with: FILAMENT_SDK_DIR=$INSTALL_DIR"
