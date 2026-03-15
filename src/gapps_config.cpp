#include "gapps_config.h"

#include <iostream>
#include <sys/stat.h>

GAppsConfig::GAppsConfig()
    : variant(GAppsVariant::Pico)
    , androidVersion("9.0")
    , architecture("x86_64")
#ifdef _WIN32
    , downloadDir(std::string(getenv("TEMP") ? getenv("TEMP") : "C:\\Temp") + "\\gapps_cache")
    , mountPoint(std::string(getenv("TEMP") ? getenv("TEMP") : "C:\\Temp") + "\\gapps_mount")
#else
    , downloadDir("/tmp/gapps_cache")
    , mountPoint("/tmp/gapps_mount")
#endif
    , imagePath("")
    , enablePlayStore(true)
    , enableChrome(true)
    , chromeHomepage("https://www.google.com")
    , enableSync(false)
{}

std::string GAppsConfig::getVariantName() const {
    switch (variant) {
        case GAppsVariant::Pico:  return "pico";
        case GAppsVariant::Nano:  return "nano";
        case GAppsVariant::Micro: return "micro";
        default:                  return "pico";
    }
}

std::string GAppsConfig::getPackageFilename() const {
    // Open GApps naming convention:
    //   open_gapps-<arch>-<android_ver>-<variant>-<date>.zip
    // We use a stable "latest" placeholder that the install script resolves.
    return "open_gapps-" + architecture + "-" + androidVersion
           + "-" + getVariantName() + ".zip";
}

std::string GAppsConfig::getDownloadUrl() const {
    // Official Open GApps GitHub releases API (latest release for this
    // architecture / Android version / variant combination).
    return "https://github.com/opengapps/" + architecture
           + "/releases/latest/download/"
           + getPackageFilename();
}

std::string GAppsConfig::getCachedPackagePath() const {
    return downloadDir + "/" + getPackageFilename();
}

bool GAppsConfig::validate() const {
    if (androidVersion.empty()) {
        std::cerr << "[GAppsConfig] Error: androidVersion is not set" << std::endl;
        return false;
    }

    if (architecture.empty()) {
        std::cerr << "[GAppsConfig] Error: architecture is not set" << std::endl;
        return false;
    }

    if (downloadDir.empty()) {
        std::cerr << "[GAppsConfig] Error: downloadDir is not set" << std::endl;
        return false;
    }

    if (imagePath.empty()) {
        std::cerr << "[GAppsConfig] Error: imagePath is not set" << std::endl;
        return false;
    }

    struct stat st;
    if (stat(imagePath.c_str(), &st) != 0) {
        std::cerr << "[GAppsConfig] Error: image file not found: " << imagePath << std::endl;
        return false;
    }

    return true;
}
