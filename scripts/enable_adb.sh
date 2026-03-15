#!/usr/bin/env bash
# enable_adb.sh – Set up ADB and connect to the EDU Android Emulator
#
# Usage:
#   chmod +x scripts/enable_adb.sh
#   ./scripts/enable_adb.sh [OPTIONS]
#
# Options:
#   --port   <port>   ADB device TCP port (default: 5555)
#   --host   <host>   Emulator host (default: localhost)
#   --root            Request root shell via "adb root" after connecting
#   --logcat          Start logcat after connecting (writes to /tmp/adb_logs/logcat.log)
#   --install <apk>   Install an APK after connecting
#   --shell           Drop into an interactive ADB shell after setup
#   --help            Show this help and exit

set -euo pipefail

# ── Defaults ─────────────────────────────────────────────────────────────────
ADB_HOST="localhost"
ADB_PORT="5555"
REQUEST_ROOT=false
START_LOGCAT=false
INSTALL_APK=""
OPEN_SHELL=false
LOG_DIR="/tmp/adb_logs"

# ── Helpers ───────────────────────────────────────────────────────────────────
log()  { echo "[enable_adb] $*"; }
warn() { echo "[enable_adb] WARNING: $*" >&2; }
die()  { echo "[enable_adb] ERROR: $*" >&2; exit 1; }

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)    ADB_PORT="$2";    shift 2 ;;
        --host)    ADB_HOST="$2";    shift 2 ;;
        --root)    REQUEST_ROOT=true; shift ;;
        --logcat)  START_LOGCAT=true; shift ;;
        --install) INSTALL_APK="$2"; shift 2 ;;
        --shell)   OPEN_SHELL=true;  shift ;;
        --help|-h)
            sed -n '2,/^$/p' "$0"
            exit 0 ;;
        *)
            die "Unknown argument: $1" ;;
    esac
done

# ── Security warning for root ─────────────────────────────────────────────────
if $REQUEST_ROOT; then
    echo ""
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║              ⚠  ROOT ACCESS WARNING  ⚠                     ║"
    echo "╠══════════════════════════════════════════════════════════════╣"
    echo "║  Root access gives full control over the Android system.    ║"
    echo "║  Only enable in a trusted development environment.          ║"
    echo "║  Do NOT expose ADB over TCP on untrusted networks.          ║"
    echo "║  See docs/DEVELOPER_GUIDE.md for security best practices.   ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""
fi

# ── Dependency check ──────────────────────────────────────────────────────────
log "Checking dependencies..."
command -v adb &>/dev/null || die "adb not found. Install Android SDK platform-tools."
log "  adb found: $(command -v adb)"

# ── Start ADB server ──────────────────────────────────────────────────────────
log "Starting ADB server..."
adb start-server
log "ADB server started"

# ── Connect to the emulator ───────────────────────────────────────────────────
TARGET="${ADB_HOST}:${ADB_PORT}"
log "Connecting to emulator at ${TARGET}..."
adb connect "${TARGET}"

# ── Wait for the device to become available ───────────────────────────────────
log "Waiting for device..."
adb -s "${TARGET}" wait-for-device
log "Device ready"

# ── List devices ──────────────────────────────────────────────────────────────
log "Connected devices:"
adb devices -l

# ── Request root ──────────────────────────────────────────────────────────────
if $REQUEST_ROOT; then
    log "Requesting root access (adb root)..."
    adb -s "${TARGET}" root || warn "adb root failed — image may not support userdebug root"
    # Reconnect after adbd restarts as root
    sleep 1
    if ! adb connect "${TARGET}" 2>/dev/null; then
        warn "Reconnect after 'adb root' failed — ADB may still be coming up"
    fi
    adb -s "${TARGET}" wait-for-device
    log "Root shell active"
fi

# ── Logcat ────────────────────────────────────────────────────────────────────
if $START_LOGCAT; then
    mkdir -p "${LOG_DIR}"
    LOGCAT_FILE="${LOG_DIR}/logcat.log"
    log "Starting logcat -> ${LOGCAT_FILE}"
    adb -s "${TARGET}" logcat > "${LOGCAT_FILE}" 2>&1 &
    log "Logcat PID: $!"
fi

# ── APK installation ──────────────────────────────────────────────────────────
if [[ -n "${INSTALL_APK}" ]]; then
    [[ -f "${INSTALL_APK}" ]] || die "APK not found: ${INSTALL_APK}"
    log "Installing APK: ${INSTALL_APK}"
    adb -s "${TARGET}" install -r "${INSTALL_APK}"
    log "APK installed successfully"
fi

# ── Interactive shell ─────────────────────────────────────────────────────────
if $OPEN_SHELL; then
    log "Opening interactive shell (type 'exit' to return)..."
    adb -s "${TARGET}" shell
fi

# ── Summary ───────────────────────────────────────────────────────────────────
cat <<EOF

────────────────────────────────────────────────────────────
  ADB setup complete!

  Target:   ${TARGET}
  Root:     $( $REQUEST_ROOT && echo "enabled" || echo "disabled" )
  Logcat:   $( $START_LOGCAT && echo "${LOG_DIR}/logcat.log" || echo "not started" )

  Useful commands:
    adb -s ${TARGET} shell             # Interactive shell
    adb -s ${TARGET} push <src> <dst>  # Upload file
    adb -s ${TARGET} pull <src> <dst>  # Download file
    adb -s ${TARGET} install <apk>     # Install APK
    adb -s ${TARGET} logcat            # View system logs
    adb -s ${TARGET} shell getprop     # List device properties

  Full guide: docs/ADB_GUIDE.md
────────────────────────────────────────────────────────────
EOF
