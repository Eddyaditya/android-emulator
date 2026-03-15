#ifndef PLAY_STORE_H
#define PLAY_STORE_H

#include "gapps_config.h"
#include <string>
#include <functional>

// Tracks the current state of the Play Store setup workflow
enum class PlayStoreStatus {
    Idle,           // Not started
    Configuring,    // Setting up device registration and Google services
    WaitingForBoot, // Waiting for the emulator to reach the home screen
    Ready,          // Play Store is accessible and functional
    Failed          // Setup encountered an error
};

// Handles first-boot Play Store configuration, device registration,
// and Google account authentication over ADB.
class PlayStore {
public:
    PlayStore();
    ~PlayStore();

    // Configure using GAppsConfig (reads adb port and image info)
    void setConfig(const GAppsConfig& config);

    // Full setup pipeline: configure → wait for boot → verify
    bool setup();

    // Individual stages
    bool configureGoogleServices();
    bool waitForBoot(int timeoutSeconds = 120);
    bool verifyPlayStore();

    // Network connectivity check (prerequisite for Play Services)
    bool checkNetworkConnectivity();

    // Status
    PlayStoreStatus getStatus() const;
    std::string getStatusString() const;
    std::string getLastError() const;

    // ADB serial (e.g. "emulator-5554") to target — defaults to first connected device
    void setAdbSerial(const std::string& serial);

    // Optional log callback
    void setLogCallback(std::function<void(const std::string&)> callback);

private:
    GAppsConfig config;
    PlayStoreStatus status;
    std::string lastError;
    std::string adbSerial;
    std::function<void(const std::string&)> logCallback;

    void log(const std::string& message);
    void setStatus(PlayStoreStatus newStatus);

    // Run an adb command; returns output string, empty on failure
    std::string runAdb(const std::string& args);

    // Run an adb shell command; returns combined stdout, empty on failure
    std::string runAdbShell(const std::string& shellCmd);
};

#endif // PLAY_STORE_H
