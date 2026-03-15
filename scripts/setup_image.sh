#!/usr/bin/env bash
# setup_image.sh – Prepare an Android x86 QEMU disk image (Linux / macOS)
#
# Usage:
#   chmod +x scripts/setup_image.sh
#   ./scripts/setup_image.sh [--iso <path_to_iso>]
#
# The script will:
#   1. Check that qemu-system-x86_64 and qemu-img are installed.
#   2. Download the Android-x86 ISO if it is not already present.
#   3. Create a writable QEMU disk image (android-x86.img) in the project root.
#
# Set ANDROID_X86_ISO_URL to override the default download URL.

set -euo pipefail

# ── Configuration ────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

ISO_FILENAME="android-x86_64-9.0-r2.iso"
ISO_URL="${ANDROID_X86_ISO_URL:-https://sourceforge.net/projects/android-x86/files/Release%209.0/${ISO_FILENAME}/download}"
ISO_PATH="${PROJECT_ROOT}/${ISO_FILENAME}"

IMAGE_PATH="${PROJECT_ROOT}/android-x86.img"
IMAGE_SIZE="16G"

# ── Helpers ───────────────────────────────────────────────────────────────────
log()  { echo "[setup_image] $*"; }
warn() { echo "[setup_image] WARNING: $*" >&2; }
die()  { echo "[setup_image] ERROR: $*" >&2; exit 1; }

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --iso)
            ISO_PATH="$2"; shift 2 ;;
        --image-size)
            IMAGE_SIZE="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [--iso <path_to_iso>] [--image-size <size>]"
            echo ""
            echo "  --iso          Path to an existing Android-x86 ISO (skips download)"
            echo "  --image-size   Size for the new disk image (default: 16G)"
            exit 0 ;;
        *)
            die "Unknown argument: $1" ;;
    esac
done

# ── Check dependencies ────────────────────────────────────────────────────────
log "Checking dependencies..."

if ! command -v qemu-system-x86_64 &>/dev/null; then
    die "qemu-system-x86_64 not found. Install QEMU first.
  Ubuntu/Debian: sudo apt-get install -y qemu-system-x86
  Fedora:        sudo dnf install -y qemu-system-x86
  macOS:         brew install qemu"
fi

if ! command -v qemu-img &>/dev/null; then
    die "qemu-img not found. It is usually bundled with QEMU."
fi

log "  qemu-system-x86_64: $(command -v qemu-system-x86_64)"
log "  qemu-img:           $(command -v qemu-img)"

# ── Download ISO (if needed) ──────────────────────────────────────────────────
if [[ -f "$ISO_PATH" ]]; then
    log "ISO already present: $ISO_PATH"
else
    log "Downloading Android-x86 ISO..."
    log "  URL:  $ISO_URL"
    log "  Dest: $ISO_PATH"

    if command -v wget &>/dev/null; then
        wget --show-progress -O "$ISO_PATH" "$ISO_URL"
    elif command -v curl &>/dev/null; then
        curl -L --progress-bar -o "$ISO_PATH" "$ISO_URL"
    else
        die "Neither wget nor curl is available. Install one and retry."
    fi
    log "ISO downloaded: $ISO_PATH"
fi

# ── Create writable disk image ────────────────────────────────────────────────
if [[ -f "$IMAGE_PATH" ]]; then
    log "Disk image already exists: $IMAGE_PATH"
else
    log "Creating ${IMAGE_SIZE} QEMU disk image at $IMAGE_PATH..."
    qemu-img create -f qcow2 "$IMAGE_PATH" "$IMAGE_SIZE"
    log "Disk image created."
fi

# ── Summary ───────────────────────────────────────────────────────────────────
cat <<EOF

────────────────────────────────────────────────────────────
  Setup complete!

  ISO image:  $ISO_PATH
  Disk image: $IMAGE_PATH

  Next steps:
    1. Boot from the ISO to install Android to the disk image:

       qemu-system-x86_64 \\
           -m 2048 -smp 2 \\
           -cdrom "$ISO_PATH" \\
           -hda  "$IMAGE_PATH" \\
           -boot d

    2. In the Android-x86 installer, choose "Installation" and
       select the QEMU disk as the target.

    3. After installation, rename or copy android-x86.img to the
       project root and run:  ./build/edu_emulator

  See INSTALLATION.md for hardware-acceleration setup (KVM/HAXM).
────────────────────────────────────────────────────────────
EOF
