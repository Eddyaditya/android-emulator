#!/usr/bin/env bash
# install_gapps.sh – Flash Open GApps into an Android-x86 QEMU disk image
#
# Usage (standalone):
#   chmod +x scripts/install_gapps.sh
#   ./scripts/install_gapps.sh [OPTIONS]
#
# Usage (called by GAppsManager):
#   scripts/install_gapps.sh --image <img> --gapps <zip> --mount <dir>
#
# Options:
#   --image  <path>   Path to the Android-x86 QEMU image (qcow2/raw). Required.
#   --gapps  <path>   Path to the Open GApps ZIP package. Required.
#   --mount  <dir>    Temporary mount-point directory. Default: /tmp/gapps_mount
#   --variant <name>  pico | nano | micro. Only used when downloading. Default: pico
#   --android <ver>   Android version string (e.g. 9.0). Default: 9.0
#   --arch   <arch>   x86_64 | x86. Default: x86_64
#   --download        Download the Open GApps package automatically.
#   --no-backup       Skip the image backup step (faster, but no rollback).
#   --help            Show this help and exit.

set -euo pipefail

# ── Defaults ─────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

IMAGE_PATH=""
GAPPS_ZIP=""
MOUNT_DIR="/tmp/gapps_mount"
VARIANT="pico"
ANDROID_VER="9.0"
ARCH="x86_64"
DO_DOWNLOAD=false
SKIP_BACKUP=false

# ── Helpers ───────────────────────────────────────────────────────────────────
log()  { echo "[install_gapps] $*"; }
warn() { echo "[install_gapps] WARNING: $*" >&2; }
die()  { echo "[install_gapps] ERROR: $*" >&2; exit 1; }

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)    IMAGE_PATH="$2"; shift 2 ;;
        --gapps)    GAPPS_ZIP="$2";  shift 2 ;;
        --mount)    MOUNT_DIR="$2";  shift 2 ;;
        --variant)  VARIANT="$2";    shift 2 ;;
        --android)  ANDROID_VER="$2"; shift 2 ;;
        --arch)     ARCH="$2";       shift 2 ;;
        --download) DO_DOWNLOAD=true; shift ;;
        --no-backup) SKIP_BACKUP=true; shift ;;
        --help|-h)
            sed -n '2,/^$/p' "$0"
            exit 0 ;;
        *)
            die "Unknown argument: $1" ;;
    esac
done

# ── Validate required arguments ───────────────────────────────────────────────
[[ -n "$IMAGE_PATH" ]] || die "--image is required"

# ── Dependency check ──────────────────────────────────────────────────────────
log "Checking dependencies..."

check_cmd() {
    command -v "$1" &>/dev/null || die "$1 not found. $2"
}

check_cmd qemu-img  "Install QEMU (sudo apt-get install -y qemu-utils)"

# nbd (Network Block Device) is used to mount qcow2 images
if ! lsmod 2>/dev/null | grep -q nbd && \
   ! modprobe nbd max_part=8 2>/dev/null; then
    warn "nbd kernel module not available — will attempt raw image mount"
fi

# ── Resolve GAPPS_ZIP ────────────────────────────────────────────────────────
if $DO_DOWNLOAD || [[ -z "$GAPPS_ZIP" ]]; then
    PKG_NAME="open_gapps-${ARCH}-${ANDROID_VER}-${VARIANT}.zip"
    CACHE_DIR="${PROJECT_ROOT}/gapps_cache"
    GAPPS_ZIP="${CACHE_DIR}/${PKG_NAME}"

    mkdir -p "$CACHE_DIR"

    if [[ -f "$GAPPS_ZIP" && -s "$GAPPS_ZIP" ]]; then
        log "Using cached package: $GAPPS_ZIP"
    else
        DL_URL="https://github.com/opengapps/${ARCH}/releases/latest/download/${PKG_NAME}"
        log "Downloading Open GApps ${VARIANT} for Android ${ANDROID_VER} (${ARCH})..."
        log "URL: $DL_URL"

        if command -v wget &>/dev/null; then
            wget --show-progress -O "$GAPPS_ZIP" "$DL_URL"
        elif command -v curl &>/dev/null; then
            curl -L --progress-bar -o "$GAPPS_ZIP" "$DL_URL"
        else
            die "Neither wget nor curl found. Install one and retry."
        fi
        log "Download complete: $GAPPS_ZIP"
    fi
fi

[[ -f "$GAPPS_ZIP" ]] || die "GApps ZIP not found: $GAPPS_ZIP"
[[ -s "$GAPPS_ZIP" ]] || die "GApps ZIP is empty: $GAPPS_ZIP"

# ── Validate ZIP magic bytes ──────────────────────────────────────────────────
MAGIC=$(xxd -l 4 "$GAPPS_ZIP" 2>/dev/null | head -1 || od -An -N4 -tx1 "$GAPPS_ZIP" | tr -d ' \n')
if ! echo "$MAGIC" | grep -qi "504b0304\|50 4b 03 04"; then
    die "Package does not appear to be a valid ZIP file: $GAPPS_ZIP"
fi
log "Package validation passed"

# ── Create image backup ───────────────────────────────────────────────────────
BACKUP_PATH="${IMAGE_PATH}.bak"
if ! $SKIP_BACKUP; then
    if [[ -f "$BACKUP_PATH" ]]; then
        log "Backup already exists, skipping: $BACKUP_PATH"
    else
        log "Creating image backup: $BACKUP_PATH"
        cp "$IMAGE_PATH" "$BACKUP_PATH"
        log "Backup created"
    fi
fi

# ── Mount the Android image ───────────────────────────────────────────────────
mkdir -p "$MOUNT_DIR"
NBD_DEV=""

cleanup() {
    log "Cleaning up..."
    if mountpoint -q "$MOUNT_DIR" 2>/dev/null; then
        umount "$MOUNT_DIR" 2>/dev/null || true
    fi
    if [[ -n "$NBD_DEV" ]]; then
        qemu-nbd --disconnect "$NBD_DEV" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Determine image format
IMAGE_FORMAT=$(qemu-img info "$IMAGE_PATH" | grep "file format" | awk '{print $3}')
log "Image format: $IMAGE_FORMAT"

if [[ "$IMAGE_FORMAT" == "qcow2" ]]; then
    # Find a free nbd device using /sys/block to reliably detect in-use devices
    for i in $(seq 0 15); do
        if [[ -e "/sys/block/nbd${i}/size" ]] && \
           [[ "$(cat /sys/block/nbd${i}/size 2>/dev/null)" == "0" ]]; then
            NBD_DEV="/dev/nbd${i}"
            break
        fi
    done
    [[ -n "$NBD_DEV" ]] || die "No free /dev/nbd* device found. Is nbd loaded?"

    log "Connecting image to $NBD_DEV..."
    qemu-nbd --connect="$NBD_DEV" "$IMAGE_PATH"
    sleep 1

    # Find the system partition (usually the largest ext4 partition)
    PARTITION=$(fdisk -l "$NBD_DEV" 2>/dev/null | grep -i "Linux filesystem" | tail -1 | awk '{print $1}')
    if [[ -z "$PARTITION" ]]; then
        PARTITION="${NBD_DEV}p1"
    fi

    log "Mounting partition $PARTITION at $MOUNT_DIR..."
    mount -t ext4 "$PARTITION" "$MOUNT_DIR" || \
        mount "$PARTITION" "$MOUNT_DIR" || \
        die "Failed to mount partition $PARTITION"
else
    # Raw image — attempt direct loop mount
    log "Attempting loop mount for raw image..."
    LOOP_DEV=$(losetup --find --partscan --show "$IMAGE_PATH")
    PARTITION="${LOOP_DEV}p1"
    sleep 1
    mount -t ext4 "$PARTITION" "$MOUNT_DIR" || \
        mount "$PARTITION" "$MOUNT_DIR" || \
        die "Failed to mount partition $PARTITION"
fi

# ── Extract GApps to system partition ────────────────────────────────────────
log "Extracting GApps package to $MOUNT_DIR..."

# Open GApps uses a nested zip structure; we flatten it to /system
TMP_EXTRACT="/tmp/gapps_extract_$$"
mkdir -p "$TMP_EXTRACT"
unzip -q "$GAPPS_ZIP" -d "$TMP_EXTRACT"

SYSTEM_DIR="$TMP_EXTRACT/system"
if [[ -d "$SYSTEM_DIR" ]]; then
    log "Copying GApps system files..."
    # Recursively copy, preserving permissions
    cp -r "$SYSTEM_DIR"/. "$MOUNT_DIR/"
    log "System files copied"
fi

# Extract individual APK sub-zips (Open GApps nests APKs inside the main zip)
while IFS= read -r -d '' INNER_ZIP; do
    [[ -f "$INNER_ZIP" ]] || continue
    log "Extracting $INNER_ZIP..."
    tar -xf "$INNER_ZIP" -C "$MOUNT_DIR/" 2>/dev/null || true
done < <(find "$TMP_EXTRACT" -name "*.tar.lz" -print0 2>/dev/null)

rm -rf "$TMP_EXTRACT"

log "GApps extraction complete"

# ── Sync and unmount ──────────────────────────────────────────────────────────
sync
log "Unmounting $MOUNT_DIR..."
umount "$MOUNT_DIR"

if [[ -n "$NBD_DEV" ]]; then
    qemu-nbd --disconnect "$NBD_DEV"
    NBD_DEV=""
fi

# ── Summary ───────────────────────────────────────────────────────────────────
cat <<EOF

────────────────────────────────────────────────────────────
  GApps installation complete!

  Image:    $IMAGE_PATH
  Variant:  $VARIANT
  Android:  $ANDROID_VER
  Arch:     $ARCH
  Backup:   ${BACKUP_PATH:-"(skipped)"}

  Next steps:
    1. Boot the image with:
         qemu-system-x86_64 -hda "$IMAGE_PATH" -m 2048 -smp 2
    2. On first boot, sign in with your Google account in the
       Play Store app to activate Google Play Services.
    3. Chrome (if included in the variant) will open
       automatically on first launch.

  Rollback:
    If something went wrong, restore from backup:
      cp "${BACKUP_PATH}" "${IMAGE_PATH}"
────────────────────────────────────────────────────────────
EOF
