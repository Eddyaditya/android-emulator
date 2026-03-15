#ifndef QEMU_CONFIG_H
#define QEMU_CONFIG_H

#include <string>

// Configuration class for QEMU emulator settings
class QEMUConfig {
public:
    std::string imagePath;      // Path to Android x86 image (.img or .iso)
    std::string imageFormat;    // Image format: "img" or "iso"
    int memorySizeMB;           // RAM allocated to the guest in MB
    int cpuCount;               // Number of virtual CPUs
    bool hardwareAccelEnabled;  // Whether to attempt hardware acceleration
    bool kvmEnabled;            // KVM acceleration (Linux)
    bool haxmEnabled;           // HAXM acceleration (Windows/macOS)
    int displayWidth;           // Guest display width in pixels
    int displayHeight;          // Guest display height in pixels

    QEMUConfig();

    // Set image path and auto-detect format from file extension
    void setImagePath(const std::string& path);

    // Validate the configuration (image file exists, sane values)
    bool validate() const;

    // Return the QEMU -accel / -enable-kvm argument string for this config
    std::string getAccelerationArg() const;
};

#endif // QEMU_CONFIG_H
