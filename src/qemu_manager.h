#ifndef QEMU_MANAGER_H
#define QEMU_MANAGER_H

#include "qemu_config.h"
#include <string>
#include <vector>
#include <functional>

// Current boot state of the emulator
enum class BootStatus {
    Idle,          // Not yet started
    Initializing,  // Validating config and preparing launch
    Booting,       // QEMU process running, Android loading
    Ready,         // Android reached home screen
    Error,         // An error occurred
    Stopped        // Process was stopped cleanly
};

// Manages the QEMU process lifecycle and Android x86 boot
class QEMUManager {
public:
    QEMUManager();
    ~QEMUManager();

    // Process lifecycle
    bool start();
    void stop();
    void pause();
    void resume();
    bool isRunning() const;

    // Configuration
    void setConfig(const QEMUConfig& config);
    QEMUConfig& getConfig();

    // Hardware acceleration detection and setup
    void configureHardwareAcceleration();
    bool isKVMAvailable() const;
    bool isHAXMAvailable() const;

    // System image support
    bool loadImage(const std::string& imagePath);
    bool validateImage() const;

    // Status and diagnostics
    BootStatus getBootStatus() const;
    std::string getBootStatusString() const;
    std::string getLastError() const;

    // Optional callback invoked for each line of QEMU stdout/stderr output
    void setLogCallback(std::function<void(const std::string&)> callback);

private:
    QEMUConfig config;
    BootStatus bootStatus;
    std::string lastError;
    bool running;
    int processPid;      // PID of the spawned QEMU process (-1 when not running)
    int outputPipeFd;    // Read end of the pipe connected to QEMU stdout/stderr (-1 if closed)
    std::function<void(const std::string&)> logCallback;

    // Build the QEMU command-line argument list from current config
    std::vector<std::string> buildQEMUArgs() const;

    // Update boot status and notify via log callback
    void setBootStatus(BootStatus status);

    // Write a message to the log callback (or stderr if none set)
    void logOutput(const std::string& message);

    void initializeQEMU();
    void cleanupQEMU();
};

#endif // QEMU_MANAGER_H
