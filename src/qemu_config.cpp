#include "qemu_config.h"

#include <iostream>
#include <sys/stat.h>

QEMUConfig::QEMUConfig()
    : imagePath("")
    , imageFormat("img")
    , memorySizeMB(2048)
    , cpuCount(2)
    , hardwareAccelEnabled(true)
    , kvmEnabled(false)
    , haxmEnabled(false)
    , displayWidth(1080)
    , displayHeight(1920)
{}

void QEMUConfig::setImagePath(const std::string& path) {
    imagePath = path;
    // Auto-detect format from file extension
    if (path.size() >= 4) {
        std::string ext = path.substr(path.size() - 4);
        if (ext == ".iso" || ext == ".ISO") {
            imageFormat = "iso";
        } else {
            imageFormat = "img";
        }
    }
}

bool QEMUConfig::validate() const {
    if (imagePath.empty()) {
        std::cerr << "[QEMUConfig] Error: image path is not configured" << std::endl;
        return false;
    }

    struct stat st;
    if (stat(imagePath.c_str(), &st) != 0) {
        std::cerr << "[QEMUConfig] Error: image file not found: " << imagePath << std::endl;
        return false;
    }

    if (memorySizeMB < 512) {
        std::cerr << "[QEMUConfig] Error: memory must be at least 512 MB" << std::endl;
        return false;
    }

    if (cpuCount < 1) {
        std::cerr << "[QEMUConfig] Error: CPU count must be at least 1" << std::endl;
        return false;
    }

    return true;
}

std::string QEMUConfig::getAccelerationArg() const {
    if (kvmEnabled) {
        return "-enable-kvm";
    }
    if (haxmEnabled) {
        return "-accel hax";
    }
    // Software emulation fallback
    return "-accel tcg";
}
