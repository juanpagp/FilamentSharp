#!/usr/bin/env bash
# Fetch and verify the pinned Google Filament release SDK.
#
# The pin (version, upstream tag commit, URL, SHA-256, size) lives in
# deps/filament.lock. Upstream does not publish checksums for release assets;
# the SHA-256 below was computed from the first verified download from GitHub
# and pins every subsequent fetch (tamper detection against the recorded
# first-party value).
#
# Environment overrides (see scripts/env.sh):
#   FILAMENT_SDK_DIR  existing Filament install to use instead (skips download)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

# shellcheck disable=SC1090
source "$ROOT/deps/filament.lock"

if [ -f "$FILAMENT_SDK_DIR/lib/x86_64/libfilament.a" ] && [ -x "$FILAMENT_SDK_DIR/bin/matc" ]; then
    echo "filament-sdk: $FILAMENT_SDK_DIR already present (FILAMENT_VERSION=$FILAMENT_VERSION)."
    echo "filament-sdk: set FILAMENT_SDK_DIR to force a different install."
    exit 0
fi

CACHE_DIR="$ROOT/external/cache"
ARCHIVE="$CACHE_DIR/$(basename "$FILAMENT_RELEASE_URL")"
mkdir -p "$CACHE_DIR"

if [ ! -f "$ARCHIVE" ]; then
    echo "filament-sdk: downloading $FILAMENT_RELEASE_URL"
    mkdir -p "$CACHE_DIR"
    curl -fL --retry 3 --retry-delay 2 -o "$ARCHIVE" "$FILAMENT_RELEASE_URL"
else
    echo "filament-sdk: using cached archive $ARCHIVE"
fi

echo "filament-sdk: verifying checksum"
ACTUAL_SHA256="$(sha256sum "$ARCHIVE" | cut -d' ' -f1)"
if [ "$ACTUAL_SHA256" != "$FILAMENT_SDK_SHA256" ]; then
    echo "filament-sdk: CHECKSUM MISMATCH" >&2
    echo "  expected: $FILAMENT_SDK_SHA256" >&2
    echo "  actual:   $ACTUAL_SHA256" >&2
    echo "  The archive has been removed; re-run to retry." >&2
    rm -f "$ARCHIVE"
    exit 1
fi
ACTUAL_BYTES="$(stat -c%s "$ARCHIVE")"
if [ "$ACTUAL_BYTES" != "$FILAMENT_SDK_BYTES" ]; then
    echo "filament-sdk: SIZE MISMATCH (expected $FILAMENT_SDK_BYTES, got $ACTUAL_BYTES)" >&2
    exit 1
fi

echo "filament-sdk: checksum OK, extracting"
STAGING="$FILAMENT_SDK_DIR.staging"
rm -rf "$STAGING"
mkdir -p "$STAGING"
tar -xzf "$ARCHIVE" -C "$STAGING"
rm -rf "$FILAMENT_SDK_DIR"
mv "$STAGING/filament" "$FILAMENT_SDK_DIR"
rmdir "$STAGING"

echo "filament-sdk: Filament $FILAMENT_VERSION (commit $FILAMENT_COMMIT) ready at $FILAMENT_SDK_DIR"
