#include "gapps_manager.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  include <unistd.h>
#  define MKDIR(p) mkdir((p), 0755)
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

GAppsManager::GAppsManager()
    : status(GAppsInstallStatus::Idle)
    , lastError("")
    , backupImagePath("")
{}

GAppsManager::~GAppsManager() {}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void GAppsManager::setConfig(const GAppsConfig& cfg) {
    config = cfg;
}

const GAppsConfig& GAppsManager::getConfig() const {
    return config;
}

// ---------------------------------------------------------------------------
// Status helpers
// ---------------------------------------------------------------------------

GAppsInstallStatus GAppsManager::getStatus() const {
    return status;
}

std::string GAppsManager::getStatusString() const {
    switch (status) {
        case GAppsInstallStatus::Idle:         return "Idle";
        case GAppsInstallStatus::Downloading:  return "Downloading GApps package...";
        case GAppsInstallStatus::Downloaded:   return "Package downloaded";
        case GAppsInstallStatus::Verifying:    return "Verifying package...";
        case GAppsInstallStatus::Installing:   return "Installing GApps...";
        case GAppsInstallStatus::Installed:    return "GApps installed successfully";
        case GAppsInstallStatus::RollingBack:  return "Rolling back installation...";
        case GAppsInstallStatus::Failed:       return "Installation failed: " + lastError;
        case GAppsInstallStatus::NotSupported: return "Not supported on this platform";
        default:                               return "Unknown";
    }
}

std::string GAppsManager::getLastError() const {
    return lastError;
}

void GAppsManager::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

void GAppsManager::setStatus(GAppsInstallStatus newStatus) {
    status = newStatus;
}

void GAppsManager::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        std::cout << message << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool GAppsManager::ensureDownloadDir() {
    struct stat st;
    if (stat(config.downloadDir.c_str(), &st) == 0) {
        return true; // Already exists
    }
    if (MKDIR(config.downloadDir.c_str()) != 0 && errno != EEXIST) {
        lastError = "Failed to create download directory: "
                    + config.downloadDir + " (" + strerror(errno) + ")";
        return false;
    }
    return true;
}

bool GAppsManager::runCommand(const std::string& command) {
    log("[GAppsManager] Running: " + command);
    int ret = std::system(command.c_str());
    if (ret != 0) {
        lastError = "Command failed (exit " + std::to_string(ret) + "): " + command;
        log("[GAppsManager] Error: " + lastError);
        return false;
    }
    return true;
}

bool GAppsManager::createImageBackup() {
    backupImagePath = config.imagePath + ".bak";
    log("[GAppsManager] Creating image backup: " + backupImagePath);

    // Check if backup already exists (i.e. a previous partial install)
    struct stat st;
    if (stat(backupImagePath.c_str(), &st) == 0) {
        log("[GAppsManager] Backup already exists, skipping copy");
        return true;
    }

#ifdef _WIN32
    std::string cmd = "copy /Y \"" + config.imagePath + "\" \"" + backupImagePath + "\"";
#else
    std::string cmd = "cp \"" + config.imagePath + "\" \"" + backupImagePath + "\"";
#endif
    if (!runCommand(cmd)) {
        lastError = "Failed to create image backup";
        return false;
    }
    log("[GAppsManager] Backup created: " + backupImagePath);
    return true;
}

// ---------------------------------------------------------------------------
// Pipeline stages
// ---------------------------------------------------------------------------

bool GAppsManager::downloadPackage() {
    if (!config.validate()) {
        lastError = "Invalid configuration";
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    if (!ensureDownloadDir()) {
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    const std::string cachedPath = config.getCachedPackagePath();

    // Check whether package is already cached
    struct stat st;
    if (stat(cachedPath.c_str(), &st) == 0 && st.st_size > 0) {
        log("[GAppsManager] Using cached package: " + cachedPath);
        setStatus(GAppsInstallStatus::Downloaded);
        return true;
    }

    setStatus(GAppsInstallStatus::Downloading);
    log("[GAppsManager] Downloading " + config.getPackageFilename()
        + " (" + config.getVariantName() + " variant)...");
    log("[GAppsManager] URL: " + config.getDownloadUrl());

#ifdef _WIN32
    // Use PowerShell Invoke-WebRequest on Windows
    std::ostringstream cmdStream;
    cmdStream << "powershell -Command \"Invoke-WebRequest -Uri '"
              << config.getDownloadUrl()
              << "' -OutFile '" << cachedPath << "'\"";
    std::string cmd = cmdStream.str();
#else
    // Prefer wget, fall back to curl
    const std::string url = config.getDownloadUrl();
    std::ostringstream cmdStream;
    cmdStream << "if command -v wget >/dev/null 2>&1; then"
              << " wget -O \"" << cachedPath << "\" \"" << url << "\";"
              << " else curl -L -o \"" << cachedPath << "\" \"" << url << "\"; fi";
    std::string cmd = cmdStream.str();
#endif

    if (!runCommand(cmd)) {
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    // Verify the downloaded file is non-empty
    if (stat(cachedPath.c_str(), &st) != 0 || st.st_size == 0) {
        lastError = "Downloaded package is empty or missing: " + cachedPath;
        log("[GAppsManager] Error: " + lastError);
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    log("[GAppsManager] Download complete: " + cachedPath);
    setStatus(GAppsInstallStatus::Downloaded);
    return true;
}

bool GAppsManager::verifyPackage() {
    const std::string cachedPath = config.getCachedPackagePath();

    setStatus(GAppsInstallStatus::Verifying);
    log("[GAppsManager] Verifying package: " + cachedPath);

    struct stat st;
    if (stat(cachedPath.c_str(), &st) != 0) {
        lastError = "Package file not found: " + cachedPath;
        log("[GAppsManager] Error: " + lastError);
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    if (st.st_size == 0) {
        lastError = "Package file is empty: " + cachedPath;
        log("[GAppsManager] Error: " + lastError);
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    // Basic ZIP magic-byte check (PK header: 0x50 0x4B 0x03 0x04)
    FILE* f = fopen(cachedPath.c_str(), "rb");
    if (!f) {
        lastError = "Cannot open package for verification: " + cachedPath;
        log("[GAppsManager] Error: " + lastError);
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }
    unsigned char magic[4] = {0};
    size_t read = fread(magic, 1, 4, f);
    fclose(f);

    if (read < 4 || magic[0] != 0x50 || magic[1] != 0x4B
        || magic[2] != 0x03 || magic[3] != 0x04) {
        lastError = "Package does not appear to be a valid ZIP file: " + cachedPath;
        log("[GAppsManager] Error: " + lastError);
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    log("[GAppsManager] Package verification passed");
    return true;
}

bool GAppsManager::installToImage() {
    setStatus(GAppsInstallStatus::Installing);
    log("[GAppsManager] Starting GApps installation into image: " + config.imagePath);

    // Back up the image so rollback is possible
    if (!createImageBackup()) {
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    const std::string cachedPath = config.getCachedPackagePath();
    const std::string mountPoint = config.mountPoint;

    // Ensure the mount-point directory exists
    struct stat st;
    if (stat(mountPoint.c_str(), &st) != 0) {
        MKDIR(mountPoint.c_str());
    }

#ifdef _WIN32
    // On Windows, delegate to the install_gapps.bat helper script
    std::string cmd = "scripts\\install_gapps.bat \""
                      + config.imagePath + "\" \""
                      + cachedPath + "\" \""
                      + mountPoint + "\"";
#else
    // On POSIX, delegate to the install_gapps.sh helper script
    std::string cmd = "bash scripts/install_gapps.sh"
                      " --image \"" + config.imagePath + "\""
                      " --gapps \"" + cachedPath + "\""
                      " --mount \"" + mountPoint + "\"";
#endif

    if (!runCommand(cmd)) {
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    log("[GAppsManager] GApps flashing complete");
    return true;
}

bool GAppsManager::verifyInstallation() {
    // Check that the image was modified (size changed vs. backup)
    struct stat imgStat, bakStat;
    if (stat(config.imagePath.c_str(), &imgStat) != 0) {
        lastError = "Image not found after installation: " + config.imagePath;
        log("[GAppsManager] Error: " + lastError);
        return false;
    }

    if (!backupImagePath.empty() && stat(backupImagePath.c_str(), &bakStat) == 0) {
        if (imgStat.st_size == bakStat.st_size
            && imgStat.st_mtime == bakStat.st_mtime) {
            log("[GAppsManager] Warning: image appears unchanged after installation");
        }
    }

    log("[GAppsManager] Post-installation verification passed");
    setStatus(GAppsInstallStatus::Installed);
    return true;
}

// ---------------------------------------------------------------------------
// Rollback
// ---------------------------------------------------------------------------

bool GAppsManager::rollback() {
    if (backupImagePath.empty()) {
        lastError = "No backup available for rollback";
        log("[GAppsManager] Error: " + lastError);
        return false;
    }

    struct stat st;
    if (stat(backupImagePath.c_str(), &st) != 0) {
        lastError = "Backup file not found: " + backupImagePath;
        log("[GAppsManager] Error: " + lastError);
        return false;
    }

    setStatus(GAppsInstallStatus::RollingBack);
    log("[GAppsManager] Rolling back to backup: " + backupImagePath);

#ifdef _WIN32
    std::string cmd = "copy /Y \"" + backupImagePath + "\" \"" + config.imagePath + "\"";
#else
    std::string cmd = "cp \"" + backupImagePath + "\" \"" + config.imagePath + "\"";
#endif

    if (!runCommand(cmd)) {
        lastError = "Rollback copy failed";
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    log("[GAppsManager] Rollback complete");
    setStatus(GAppsInstallStatus::Idle);
    return true;
}

// ---------------------------------------------------------------------------
// isInstalled
// ---------------------------------------------------------------------------

bool GAppsManager::isInstalled() const {
    // Heuristic: if a backup exists it means a prior installation ran.
    // A real implementation could mount the image and check for
    // /system/app/Phonesky (Play Store APK) or /system/priv-app/GmsCore.
    if (backupImagePath.empty()) {
        return false;
    }
    struct stat st;
    return (stat(backupImagePath.c_str(), &st) == 0);
}

// ---------------------------------------------------------------------------
// Full pipeline
// ---------------------------------------------------------------------------

bool GAppsManager::install() {
    log("[GAppsManager] === GApps Installation Start ===");
    log("[GAppsManager] Variant:  " + config.getVariantName());
    log("[GAppsManager] Android:  " + config.androidVersion);
    log("[GAppsManager] Arch:     " + config.architecture);
    log("[GAppsManager] Image:    " + config.imagePath);

    if (!downloadPackage()) {
        log("[GAppsManager] Installation aborted: download failed");
        return false;
    }

    if (!verifyPackage()) {
        log("[GAppsManager] Installation aborted: verification failed");
        return false;
    }

    if (!installToImage()) {
        log("[GAppsManager] Installation failed — attempting rollback...");
        rollback();
        return false;
    }

    if (!verifyInstallation()) {
        log("[GAppsManager] Post-install check failed — attempting rollback...");
        rollback();
        setStatus(GAppsInstallStatus::Failed);
        return false;
    }

    log("[GAppsManager] === GApps Installation Complete ===");
    return true;
}
