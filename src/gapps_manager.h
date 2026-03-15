#ifndef GAPPS_MANAGER_H
#define GAPPS_MANAGER_H

#include "gapps_config.h"
#include <string>
#include <functional>

// Describes the current state of the GApps installation workflow
enum class GAppsInstallStatus {
    Idle,           // Not yet started
    Downloading,    // Fetching the GApps zip package
    Downloaded,     // Package downloaded and ready
    Verifying,      // Checking package integrity
    Installing,     // Flashing GApps into the disk image
    Installed,      // GApps successfully installed
    RollingBack,    // Restoring backup after a failed install
    Failed,         // Installation failed (no rollback in progress)
    NotSupported    // Platform or image does not support GApps installation
};

// Manages the full Open GApps installation lifecycle:
//   download → verify → backup → install → verify post-install → (rollback on error)
class GAppsManager {
public:
    GAppsManager();
    ~GAppsManager();

    // Configuration
    void setConfig(const GAppsConfig& config);
    const GAppsConfig& getConfig() const;

    // Full installation pipeline (blocking)
    bool install();

    // Individual pipeline stages (can be called independently)
    bool downloadPackage();
    bool verifyPackage();
    bool installToImage();
    bool verifyInstallation();

    // Rollback — restore image backup made before installation
    bool rollback();

    // Check whether GApps is already installed in the configured image
    bool isInstalled() const;

    // Status
    GAppsInstallStatus getStatus() const;
    std::string getStatusString() const;
    std::string getLastError() const;

    // Optional progress callback: receives human-readable log lines
    void setLogCallback(std::function<void(const std::string&)> callback);

private:
    GAppsConfig config;
    GAppsInstallStatus status;
    std::string lastError;
    std::string backupImagePath;    // Path to the pre-install image backup
    std::function<void(const std::string&)> logCallback;

    void log(const std::string& message);
    void setStatus(GAppsInstallStatus newStatus);

    // Create a backup copy of the image before flashing
    bool createImageBackup();

    // Ensure the download cache directory exists
    bool ensureDownloadDir();

    // Run a shell command; returns true on exit code 0
    bool runCommand(const std::string& command);
};

#endif // GAPPS_MANAGER_H
