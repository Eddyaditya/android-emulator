#ifndef ADB_MANAGER_H
#define ADB_MANAGER_H

#include "adb_config.h"
#include <string>
#include <functional>

// Connection state of the ADB bridge
enum class AdbConnectionState {
    Disconnected,   // No active connection
    Connecting,     // Attempting to connect to adbd
    Connected,      // ADB connection established
    Unauthorized,   // Device exists but host key not accepted
    Error           // An unrecoverable error occurred
};

// Manages the Android Debug Bridge (ADB) daemon lifecycle and developer features:
//   start daemon → connect → authenticate → shell / push / pull / install
class AdbManager {
public:
    AdbManager();
    ~AdbManager();

    // Configuration
    void setConfig(const AdbConfig& config);
    const AdbConfig& getConfig() const;

    // Daemon lifecycle
    bool startServer();
    bool stopServer();
    bool isServerRunning() const;

    // Device connection
    bool connect();
    bool disconnect();
    bool waitForDevice(int timeoutSec = 30);

    // Connection state
    AdbConnectionState getConnectionState() const;
    std::string getConnectionStateString() const;

    // Shell access — runs a command in the emulator shell and returns output
    bool runShellCommand(const std::string& command, std::string& output);

    // Interactive shell (blocks until the shell session ends)
    bool openInteractiveShell();

    // File transfer
    bool pushFile(const std::string& localPath, const std::string& remotePath);
    bool pullFile(const std::string& remotePath, const std::string& localPath);

    // App installation
    bool installApk(const std::string& apkPath);

    // Logcat
    bool startLogcat(const std::string& outputFile = "");
    bool stopLogcat();

    // Device properties
    bool getProperty(const std::string& key, std::string& value);

    // Error information
    std::string getLastError() const;

    // Optional log callback
    void setLogCallback(std::function<void(const std::string&)> callback);

private:
    AdbConfig config;
    AdbConnectionState connectionState;
    std::string lastError;
    std::function<void(const std::string&)> logCallback;
    int logcatPid;   // PID of the background logcat process (-1 when not running)

    void log(const std::string& message);
    void setConnectionState(AdbConnectionState state);

    // Build a full adb command string with the correct binary and server port
    std::string buildAdbCommand(const std::string& args) const;

    // Run an adb command; returns true on exit code 0
    bool runAdbCommand(const std::string& args);

    // Run an adb command and capture stdout; returns true on exit code 0
    bool runAdbCommandWithOutput(const std::string& args, std::string& output);

    // Ensure the log directory exists
    bool ensureLogDir();
};

#endif // ADB_MANAGER_H
