#!/usr/bin/env bash
# Verify that the pinned Filament SDK works end-to-end in this environment by
# compiling and running tests/native/filament_smoke.cpp directly against it
# (independent of the C ABI bridge).
#
# The Filament release SDK is built with libc++, so this script requires a
# clang toolchain with libc++ available. On Debian/Ubuntu: apt install clang
# libc++-dev. For user-local toolchains, set CXX (and, if needed,
# FILAMENTSHARP_LIBCXX_INCLUDE / FILAMENTSHARP_LIBCXX_LIB) in
# scripts/env.local.sh — see env.local.sh.example.
#
# Usage: verify-sdk.sh [backend ...]   (default: noop vk)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

if [ ! -f "$FILAMENT_SDK_DIR/lib/x86_64/libfilament.a" ]; then
    echo "verify-sdk: Filament SDK not found at $FILAMENT_SDK_DIR — run scripts/fetch-filament.sh first" >&2
    exit 1
fi

BACKENDS=("$@")
if [ ${#BACKENDS[@]} -eq 0 ]; then
    BACKENDS=(noop vk)
fi

LIBCXX_FLAGS=()
if [ -n "${FILAMENTSHARP_LIBCXX_INCLUDE:-}" ]; then
    LIBCXX_FLAGS+=("-I$FILAMENTSHARP_LIBCXX_INCLUDE")
fi
if [ -n "${FILAMENTSHARP_LIBCXX_LIB:-}" ]; then
    LIBCXX_FLAGS+=("-L$FILAMENTSHARP_LIBCXX_LIB")
fi

OUT="$ROOT/external/verify-sdk"
mkdir -p "$OUT"

echo "verify-sdk: compiling filament_smoke with $CXX"
# shellcheck disable=SC2086
"$CXX" -std=c++17 -O1 -stdlib=libc++ "${LIBCXX_FLAGS[@]}" \
    -I "$FILAMENT_SDK_DIR/include" \
    "$ROOT/tests/native/filament_smoke.cpp" -o "$OUT/filament_smoke" \
    -L "$FILAMENT_SDK_DIR/lib/x86_64" \
    -lfilament -lbackend -lbluegl -lbluevk -lwebgpu_dawn -lfilabridge -lfilaflat \
    -lsmol-v -lutils -lgeometry -lshaders -libl -lzstd -lX11 -lpthread -ldl

FAILED=0
for backend in "${BACKENDS[@]}"; do
    echo "verify-sdk: running smoke test (backend=$backend)"
    if ! "$OUT/filament_smoke" "$backend"; then
        echo "verify-sdk: backend $backend FAILED" >&2
        FAILED=1
    fi
done
if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
echo "verify-sdk: all requested backends verified"
