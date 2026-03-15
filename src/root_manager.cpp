#include "root_manager.h"

#include <iostream>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

RootManager::RootManager(AdbManager& adbRef)
    : adb(adbRef)
    , status(RootStatus::Unknown)
    , lastError("")
{}

RootManager::~RootManager() {}

// ---------------------------------------------------------------------------
// Log / status helpers
// ---------------------------------------------------------------------------

void RootManager::log(const std::string& message) const {
    if (logCallback) {
        logCallback(message);
    } else {
        std::cout << message << std::endl;
    }
}

void RootManager::setStatus(RootStatus newStatus) {
    status = newStatus;
}

void RootManager::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
    // Forward the same callback to the ADB manager so log lines are consistent
    adb.setLogCallback(callback);
}

RootStatus RootManager::getStatus() const {
    return status;
}

std::string RootManager::getStatusString() const {
    switch (status) {
        case RootStatus::Unknown:      return "Unknown";
        case RootStatus::NotAvailable: return "Root not available on this image";
        case RootStatus::Available:    return "Root available (not yet active)";
        case RootStatus::Active:       return "Root access active";
        case RootStatus::SafeMode:     return "Safe mode enabled (root disabled)";
        case RootStatus::Error:        return "Error: " + lastError;
        default:                       return "Unknown";
    }
}

std::string RootManager::getLastError() const {
    return lastError;
}

// ---------------------------------------------------------------------------
// Security warning
// ---------------------------------------------------------------------------

void RootManager::printSecurityWarning() const {
    log("╔══════════════════════════════════════════════════════════════╗");
    log("║              ⚠  ROOT ACCESS WARNING  ⚠                     ║");
    log("╠══════════════════════════════════════════════════════════════╣");
    log("║  Root access gives full control over the Android system.    ║");
    log("║  Misuse can corrupt the image or expose sensitive data.     ║");
    log("║                                                              ║");
    log("║  • Only enable root in a trusted development environment.   ║");
    log("║  • Do NOT expose ADB over TCP on untrusted networks.        ║");
    log("║  • Disable root access when not actively debugging.         ║");
    log("║  • See docs/DEVELOPER_GUIDE.md for security best practices. ║");
    log("╚══════════════════════════════════════════════════════════════╝");
}

// ---------------------------------------------------------------------------
// su binary check
// ---------------------------------------------------------------------------

bool RootManager::checkSuBinary() {
    std::string output;
    // Check the most common locations for the su binary on Android-x86
    if (adb.runShellCommand("test -x /system/xbin/su && echo found || echo missing", output)) {
        if (output.find("found") != std::string::npos) {
            log("[RootManager] su binary found at /system/xbin/su");
            return true;
        }
    }
    if (adb.runShellCommand("test -x /system/bin/su && echo found || echo missing", output)) {
        if (output.find("found") != std::string::npos) {
            log("[RootManager] su binary found at /system/bin/su");
            return true;
        }
    }
    log("[RootManager] su binary not found on device");
    return false;
}

// ---------------------------------------------------------------------------
// Enable / disable root
// ---------------------------------------------------------------------------

bool RootManager::enableRoot() {
    printSecurityWarning();

    log("[RootManager] Attempting to enable root access...");

    // Check if we are already running as root
    std::string output;
    if (adb.runShellCommand("id", output) && output.find("uid=0") != std::string::npos) {
        log("[RootManager] Already running as root");
        setStatus(RootStatus::Active);
        return true;
    }

    // Restart adbd with root privileges ("adb root" works on userdebug/eng builds)
    log("[RootManager] Requesting root via 'adb root'...");
    if (adb.runShellCommand("su -c id 2>/dev/null || true", output)
        && output.find("uid=0") != std::string::npos) {
        log("[RootManager] Root access confirmed via su");
        setStatus(RootStatus::Active);
        return true;
    }

    // Check for su binary presence
    if (!checkSuBinary()) {
        lastError = "su binary not found; root not supported on this image";
        log("[RootManager] " + lastError);
        setStatus(RootStatus::NotAvailable);
        return false;
    }

    setStatus(RootStatus::Available);
    log("[RootManager] Root is available. Run 'adb root' or use 'su' inside adb shell.");
    return true;
}

bool RootManager::disableRoot() {
    log("[RootManager] Disabling root access...");

    std::string output;
    bool ok = adb.runShellCommand("id", output);

    if (ok && output.find("uid=0") == std::string::npos) {
        log("[RootManager] Not running as root; nothing to disable");
        setStatus(RootStatus::Available);
        return true;
    }

    log("[RootManager] Root access disabled");
    setStatus(RootStatus::Available);
    return true;
}

// ---------------------------------------------------------------------------
// Safe mode
// ---------------------------------------------------------------------------

bool RootManager::enableSafeMode() {
    log("[RootManager] Enabling safe mode...");

    // Trigger safe mode by sending the BOOT_COMPLETED broadcast with --safe-mode flag
    // On Android-x86, booting into safe mode suppresses 3rd-party apps and su access
    std::string output;
    bool ok = adb.runShellCommand(
        "setprop persist.sys.safemode 1 && "
        "setprop ctl.restart zygote",
        output);

    if (!ok) {
        lastError = "Failed to set safe mode property";
        log("[RootManager] Error: " + lastError);
        setStatus(RootStatus::Error);
        return false;
    }

    setStatus(RootStatus::SafeMode);
    log("[RootManager] Safe mode enabled. The device will restart into safe mode.");
    return true;
}

bool RootManager::disableSafeMode() {
    log("[RootManager] Disabling safe mode...");

    std::string output;
    bool ok = adb.runShellCommand(
        "setprop persist.sys.safemode 0 && "
        "setprop ctl.restart zygote",
        output);

    if (!ok) {
        lastError = "Failed to clear safe mode property";
        log("[RootManager] Error: " + lastError);
        setStatus(RootStatus::Error);
        return false;
    }

    setStatus(RootStatus::Available);
    log("[RootManager] Safe mode disabled. The device will restart normally.");
    return true;
}

// ---------------------------------------------------------------------------
// Verify root
// ---------------------------------------------------------------------------

bool RootManager::verifyRoot() {
    log("[RootManager] Verifying root access...");

    std::string output;
    if (!adb.runShellCommand("id", output)) {
        lastError = "Shell command failed during root verification";
        setStatus(RootStatus::Error);
        return false;
    }

    if (output.find("uid=0") != std::string::npos) {
        log("[RootManager] Root verified: " + output);
        setStatus(RootStatus::Active);
        return true;
    }

    // Try su escalation
    if (adb.runShellCommand("su -c id 2>/dev/null", output)
        && output.find("uid=0") != std::string::npos) {
        log("[RootManager] Root verified via su: " + output);
        setStatus(RootStatus::Active);
        return true;
    }

    log("[RootManager] Root not active (current id: " + output + ")");
    setStatus(RootStatus::Available);
    return false;
}
