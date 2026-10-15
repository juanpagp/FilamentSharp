#!/usr/bin/env bash
# Best-effort setup of a headless software Vulkan driver (Mesa lavapipe) for
# machines without a GPU or Vulkan ICD. This enables real Filament rendering
# (Vulkan backend) on headless CI machines.
#
# Mechanism: download the official Debian/Ubuntu `mesa-vulkan-drivers` package
# WITHOUT root (apt-get download), extract it under external/vulkan-runtime,
# and generate an ICD manifest pointing at the extracted lavapipe library.
# scripts/env.sh picks the result up automatically.
#
# Limitations (documented, not hidden):
#   * Debian/Ubuntu only (apt-based distros with the mesa-vulkan-drivers package).
#   * The package must be ABI-compatible with the installed libLLVM/libgallium.
#   * If a real Vulkan ICD is already installed, this script does nothing.
#   * lavapipe is a CPU rasterizer: correct but not fast.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/scripts/env.sh"

if [ -n "${VK_ICD_FILENAMES:-}" ]; then
    echo "vulkan-runtime: VK_ICD_FILENAMES already set ($VK_ICD_FILENAMES); nothing to do."
    exit 0
fi
if ls /usr/share/vulkan/icd.d/*.json >/dev/null 2>&1; then
    echo "vulkan-runtime: system Vulkan ICD(s) present; nothing to do."
    exit 0
fi
if [ -f "$ROOT/external/vulkan-runtime/lvp_icd_local.json" ]; then
    echo "vulkan-runtime: local lavapipe runtime already set up."
    exit 0
fi
if ! command -v apt-get >/dev/null 2>&1; then
    echo "vulkan-runtime: no system Vulkan ICD and no apt-get; cannot set up lavapipe." >&2
    echo "  Install a Vulkan driver (e.g. mesa-vulkan-drivers) and set VK_ICD_FILENAMES." >&2
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
echo "vulkan-runtime: downloading mesa-vulkan-drivers (official Debian package)"
(cd "$WORK" && apt-get download mesa-vulkan-drivers >/dev/null)

DEST="$ROOT/external/vulkan-runtime"
mkdir -p "$DEST"
dpkg -x "$WORK"/mesa-vulkan-drivers_*.deb "$DEST"

LVP_LIB="$DEST/usr/lib/x86_64-linux-gnu/libvulkan_lvp.so"
if [ ! -f "$LVP_LIB" ]; then
    echo "vulkan-runtime: lavapipe library missing after extraction" >&2
    exit 1
fi

cat > "$DEST/lvp_icd_local.json" <<EOF
{
    "ICD": {
        "api_version": "1.4.305",
        "library_path": "$LVP_LIB"
    },
    "file_format_version": "1.0.0"
}
EOF

echo "vulkan-runtime: lavapipe ready ($DEST/lvp_icd_local.json)"
echo "vulkan-runtime: scripts/env.sh will export VK_ICD_FILENAMES automatically."
