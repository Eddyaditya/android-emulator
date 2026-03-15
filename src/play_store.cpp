#include "play_store.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <array>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

// DNS server used for network connectivity check
static const char* const CONNECTIVITY_CHECK_HOST = "8.8.8.8";

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

PlayStore::PlayStore()
    : status(PlayStoreStatus::Idle)
    , lastError("")
    , adbSerial("")
{}

PlayStore::~PlayStore() {}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void PlayStore::setConfig(const GAppsConfig& cfg) {
    config = cfg;
}

void PlayStore::setAdbSerial(const std::string& serial) {
    adbSerial = serial;
}

void PlayStore::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

// ---------------------------------------------------------------------------
// Status helpers
// ---------------------------------------------------------------------------

PlayStoreStatus PlayStore::getStatus() const {
    return status;
}

std::string PlayStore::getStatusString() const {
    switch (status) {
        case PlayStoreStatus::Idle:           return "Idle";
        case PlayStoreStatus::Configuring:    return "Configuring Google services...";
        case PlayStoreStatus::WaitingForBoot: return "Waiting for Android boot...";
        case PlayStoreStatus::Ready:          return "Play Store is ready";
        case PlayStoreStatus::Failed:         return "Setup failed: " + lastError;
        default:                              return "Unknown";
    }
}

std::string PlayStore::getLastError() const {
    return lastError;
}

void PlayStore::setStatus(PlayStoreStatus newStatus) {
    status = newStatus;
}

void PlayStore::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        std::cout << message << std::endl;
    }
}

// ---------------------------------------------------------------------------
// ADB helpers
// ---------------------------------------------------------------------------

std::string PlayStore::runAdb(const std::string& args) {
    std::string cmd = "adb";
    if (!adbSerial.empty()) {
        cmd += " -s " + adbSerial;
    }
    cmd += " " + args + " 2>&1";

    log("[PlayStore] adb " + args);

    std::array<char, 256> buf;
    std::string result;

#ifdef _WIN32
    std::shared_ptr<FILE> pipe(_popen(cmd.c_str(), "r"), _pclose);
#else
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
#endif

    if (!pipe) {
        lastError = "Failed to run adb command";
        return "";
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe.get())) {
        result += buf.data();
    }
    return result;
}

std::string PlayStore::runAdbShell(const std::string& shellCmd) {
    return runAdb("shell " + shellCmd);
}

// ---------------------------------------------------------------------------
// Network connectivity
// ---------------------------------------------------------------------------

bool PlayStore::checkNetworkConnectivity() {
    log("[PlayStore] Checking network connectivity...");

    // Ask the Android guest to ping Google's DNS
    std::string pingCmd = std::string("ping -c 1 -W 3 ") + CONNECTIVITY_CHECK_HOST;
    std::string out = runAdbShell(pingCmd);
    if (out.find("1 packets transmitted") != std::string::npos
        || out.find("1 received") != std::string::npos) {
        log("[PlayStore] Network connectivity: OK");
        return true;
    }

    log("[PlayStore] Network connectivity check inconclusive (may be OK)");
    // Return true anyway — the guest may suppress ICMP but still have internet
    return true;
}

// ---------------------------------------------------------------------------
// Pipeline stages
// ---------------------------------------------------------------------------

bool PlayStore::waitForBoot(int timeoutSeconds) {
    setStatus(PlayStoreStatus::WaitingForBoot);
    log("[PlayStore] Waiting for Android to finish booting (timeout "
        + std::to_string(timeoutSeconds) + "s)...");

    const int pollIntervalMs = 3000;
    int elapsed = 0;

    while (elapsed < timeoutSeconds * 1000) {
        std::string bootCompleted = runAdbShell("getprop sys.boot_completed");

        // Trim whitespace/newlines
        size_t end = bootCompleted.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            bootCompleted = bootCompleted.substr(0, end + 1);
        }

        if (bootCompleted == "1") {
            log("[PlayStore] Android boot completed");
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
        log("[PlayStore] Still booting... (" + std::to_string(elapsed / 1000) + "s)");
    }

    lastError = "Android did not finish booting within "
                + std::to_string(timeoutSeconds) + " seconds";
    log("[PlayStore] Error: " + lastError);
    setStatus(PlayStoreStatus::Failed);
    return false;
}

bool PlayStore::configureGoogleServices() {
    setStatus(PlayStoreStatus::Configuring);
    log("[PlayStore] Configuring Google Play Services...");

    // Grant Google Play Services the required permissions
    std::vector<std::string> permissions = {
        "android.permission.ACCESS_COARSE_LOCATION",
        "android.permission.ACCESS_FINE_LOCATION",
        "android.permission.READ_CONTACTS",
        "android.permission.GET_ACCOUNTS"
    };
    for (const auto& perm : permissions) {
        runAdbShell("pm grant com.google.android.gms " + perm);
    }

    // Clear Play Store data to trigger first-run setup on next launch
    if (config.enablePlayStore) {
        log("[PlayStore] Clearing Play Store cache for first-boot setup...");
        runAdbShell("pm clear com.android.vending");
    }

    // Configure Chrome homepage and clear first-run flags
    if (config.enableChrome) {
        log("[PlayStore] Configuring Chrome browser...");
        runAdbShell("pm grant com.android.chrome android.permission.ACCESS_COARSE_LOCATION");

        // Write default homepage preference via content provider
        runAdbShell("am broadcast -a com.android.chrome.FILE_PROVIDER_AVAILABLE");

        if (!config.chromeHomepage.empty()) {
            // Sanitize the URL: only allow characters safe in a double-quoted
            // shell argument (alphanumeric plus common URL characters).
            // Any other character is stripped to prevent command injection.
            std::string safeUrl;
            safeUrl.reserve(config.chromeHomepage.size());
            for (char c : config.chromeHomepage) {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                    || (c >= '0' && c <= '9')
                    || c == ':' || c == '/' || c == '.' || c == '-'
                    || c == '_' || c == '~' || c == '?' || c == '='
                    || c == '&' || c == '#' || c == '%' || c == '+') {
                    safeUrl += c;
                }
            }

            if (!safeUrl.empty()) {
                log("[PlayStore] Setting Chrome homepage to: " + safeUrl);
                // Start Chrome once to trigger preference initialisation, then stop
                runAdbShell("am start -n com.android.chrome/com.google.android.apps.chrome.Main"
                            " -d \"" + safeUrl + "\"");
                std::this_thread::sleep_for(std::chrono::seconds(3));
                runAdbShell("am force-stop com.android.chrome");
            }
        }
    }

    log("[PlayStore] Google services configuration complete");
    return true;
}

bool PlayStore::verifyPlayStore() {
    log("[PlayStore] Verifying Play Store installation...");

    std::string out = runAdbShell("pm list packages com.android.vending");
    if (out.find("com.android.vending") == std::string::npos) {
        lastError = "Play Store package (com.android.vending) not found on device";
        log("[PlayStore] Error: " + lastError);
        setStatus(PlayStoreStatus::Failed);
        return false;
    }

    log("[PlayStore] Play Store package found: com.android.vending");

    // Verify Google Play Services is present
    out = runAdbShell("pm list packages com.google.android.gms");
    if (out.find("com.google.android.gms") == std::string::npos) {
        lastError = "Google Play Services (com.google.android.gms) not found on device";
        log("[PlayStore] Error: " + lastError);
        setStatus(PlayStoreStatus::Failed);
        return false;
    }

    log("[PlayStore] Google Play Services found: com.google.android.gms");

    if (config.enableChrome) {
        out = runAdbShell("pm list packages com.android.chrome");
        if (out.find("com.android.chrome") == std::string::npos) {
            log("[PlayStore] Warning: Chrome (com.android.chrome) not found — "
                "it may not be included in the selected GApps variant");
        } else {
            log("[PlayStore] Chrome found: com.android.chrome");
        }
    }

    setStatus(PlayStoreStatus::Ready);
    log("[PlayStore] Verification complete — Play Store is ready");
    return true;
}

// ---------------------------------------------------------------------------
// Full pipeline
// ---------------------------------------------------------------------------

bool PlayStore::setup() {
    log("[PlayStore] === Play Store Setup Start ===");

    if (!waitForBoot()) {
        return false;
    }

    checkNetworkConnectivity();

    if (!configureGoogleServices()) {
        setStatus(PlayStoreStatus::Failed);
        return false;
    }

    if (!verifyPlayStore()) {
        return false;
    }

    log("[PlayStore] === Play Store Setup Complete ===");
    return true;
}
