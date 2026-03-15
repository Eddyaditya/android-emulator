#ifndef GAPPS_CONFIG_H
#define GAPPS_CONFIG_H

#include <string>

// Supported Open GApps package variants (ordered by size, smallest first)
enum class GAppsVariant {
    Pico,   // Smallest: core Play Services + Play Store only
    Nano,   // Adds Gmail, Maps (lite), TTS
    Micro   // Adds more Google apps (Calendar, Clock, etc.)
};

// GApps installation configuration
class GAppsConfig {
public:
    GAppsVariant variant;           // Which GApps package to install
    std::string androidVersion;     // Android version string, e.g. "9.0"
    std::string architecture;       // Target architecture, e.g. "x86_64"
    std::string downloadDir;        // Directory where downloaded packages are cached
    std::string mountPoint;         // Temporary mount point for the Android image
    std::string imagePath;          // Path to the Android x86 disk image
    bool enablePlayStore;           // Whether to configure Play Store on first boot
    bool enableChrome;              // Whether to pre-install Chrome
    std::string chromeHomepage;     // Default Chrome homepage URL
    bool enableSync;                // Whether to enable Chrome sync on first run

    GAppsConfig();

    // Convert variant enum to the Open GApps package-name string
    std::string getVariantName() const;

    // Build the expected Open GApps zip filename for this configuration
    std::string getPackageFilename() const;

    // Build the Open GApps download URL for this configuration
    std::string getDownloadUrl() const;

    // Return the full path to the locally cached package file
    std::string getCachedPackagePath() const;

    // Validate the configuration (paths set, version non-empty, etc.)
    bool validate() const;
};

#endif // GAPPS_CONFIG_H
