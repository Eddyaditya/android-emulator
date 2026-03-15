#ifndef ROOT_MANAGER_H
#define ROOT_MANAGER_H

#include "adb_manager.h"
#include <string>
#include <functional>

// Represents the current state of root access on the emulator
enum class RootStatus {
    Unknown,        // Status not yet determined
    NotAvailable,   // su binary absent or not supported
    Available,      // su binary present but not yet activated
    Active,         // Root shell confirmed working
    SafeMode,       // Root intentionally disabled (safe mode)
    Error           // An error occurred during root setup
};

// Manages root access configuration and verification for the Android emulator.
// Relies on AdbManager for all communication with the device.
class RootManager {
public:
    explicit RootManager(AdbManager& adb);
    ~RootManager();

    // Attempt to obtain root by running "adb root"; falls back to su binary check
    bool enableRoot();

    // Revert to non-root ADB daemon ("adb unroot")
    bool disableRoot();

    // Toggle safe mode (disables 3rd-party app loading and root su usage)
    bool enableSafeMode();
    bool disableSafeMode();

    // Verify whether root is currently active
    bool verifyRoot();

    // Status
    RootStatus getStatus() const;
    std::string getStatusString() const;
    std::string getLastError() const;

    // Print a security warning to the log callback / stdout
    void printSecurityWarning() const;

    // Optional log callback
    void setLogCallback(std::function<void(const std::string&)> callback);

private:
    AdbManager& adb;
    RootStatus status;
    std::string lastError;
    std::function<void(const std::string&)> logCallback;

    void log(const std::string& message) const;
    void setStatus(RootStatus newStatus);

    // Check whether the su binary exists at the expected path on the device
    bool checkSuBinary();
};

#endif // ROOT_MANAGER_H
