#ifndef ADB_CONFIG_H
#define ADB_CONFIG_H

#include <string>

// Default ADB port used by the ADB server and daemon
static const int ADB_DEFAULT_PORT = 5037;

// Default ADB device port exposed over TCP from the emulator
static const int ADB_DEFAULT_DEVICE_PORT = 5555;

// ADB connection and daemon configuration
class AdbConfig {
public:
    int    serverPort;          // Port the local ADB server listens on (default: 5037)
    int    devicePort;          // TCP port exposed by adbd inside the emulator (default: 5555)
    bool   enableTcpAdb;        // Connect to the emulator over TCP (adb connect)
    bool   enableRootAccess;    // Request root shell on connection
    bool   enableLogcat;        // Start logcat on successful connection
    std::string adbBinaryPath;  // Path to the adb executable (empty = search PATH)
    std::string logDir;         // Directory where ADB and logcat logs are written
    std::string trustedHostsFile; // Path to file listing pre-approved host fingerprints
    int    connectionTimeoutSec; // Seconds to wait when connecting to adbd
    int    maxRetries;          // Number of connection retries before giving up

    AdbConfig();

    // Validate the configuration; logs errors to stderr and returns false on failure
    bool validate() const;

    // Build the "adb connect" target string (e.g. "localhost:5555")
    std::string getConnectTarget() const;
};

#endif // ADB_CONFIG_H
