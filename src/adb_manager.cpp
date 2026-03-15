#include "adb_manager.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>

#ifdef _WIN32
#  include <direct.h>
#endif

#ifndef _WIN32
#  include <unistd.h>
#  include <signal.h>
#  define MKDIR(p) mkdir((p), 0755)
#else
#  define MKDIR(p) _mkdir(p)
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AdbManager::AdbManager()
    : connectionState(AdbConnectionState::Disconnected)
    , lastError("")
    , logcatPid(-1)
{}

AdbManager::~AdbManager() {}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void AdbManager::setConfig(const AdbConfig& cfg) {
    config = cfg;
}

const AdbConfig& AdbManager::getConfig() const {
    return config;
}

// ---------------------------------------------------------------------------
// State helpers
// ---------------------------------------------------------------------------

AdbConnectionState AdbManager::getConnectionState() const {
    return connectionState;
}

std::string AdbManager::getConnectionStateString() const {
    switch (connectionState) {
        case AdbConnectionState::Disconnected:  return "Disconnected";
        case AdbConnectionState::Connecting:    return "Connecting...";
        case AdbConnectionState::Connected:     return "Connected";
        case AdbConnectionState::Unauthorized:  return "Unauthorized (check ADB keys)";
        case AdbConnectionState::Error:         return "Error: " + lastError;
        default:                                return "Unknown";
    }
}

std::string AdbManager::getLastError() const {
    return lastError;
}

void AdbManager::setConnectionState(AdbConnectionState state) {
    connectionState = state;
}

void AdbManager::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

void AdbManager::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        std::cout << message << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string AdbManager::buildAdbCommand(const std::string& args) const {
    std::ostringstream cmd;
    // Use the configured binary path, or fall back to "adb" on PATH
    const std::string binary = config.adbBinaryPath.empty() ? "adb" : config.adbBinaryPath;
    cmd << "\"" << binary << "\""
        << " -P " << config.serverPort
        << " " << args;
    return cmd.str();
}

bool AdbManager::runAdbCommand(const std::string& args) {
    const std::string cmd = buildAdbCommand(args);
    log("[AdbManager] Running: " + cmd);
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        lastError = "adb command failed (exit " + std::to_string(ret) + "): " + args;
        log("[AdbManager] Error: " + lastError);
        return false;
    }
    return true;
}

bool AdbManager::runAdbCommandWithOutput(const std::string& args, std::string& output) {
    const std::string cmd = buildAdbCommand(args);
    log("[AdbManager] Running (capturing output): " + cmd);

    output.clear();
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        lastError = "Failed to open pipe for command: " + cmd;
        log("[AdbManager] Error: " + lastError);
        return false;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }

#ifdef _WIN32
    int ret = _pclose(pipe);
#else
    int ret = pclose(pipe);
#endif
    if (ret != 0) {
        lastError = "adb command failed (exit " + std::to_string(ret) + "): " + args;
        log("[AdbManager] Error: " + lastError);
        return false;
    }
    return true;
}

bool AdbManager::ensureLogDir() {
    struct stat st;
    if (stat(config.logDir.c_str(), &st) == 0) {
        return true;
    }
    if (MKDIR(config.logDir.c_str()) != 0 && errno != EEXIST) {
        lastError = "Failed to create log directory: " + config.logDir
                    + " (" + strerror(errno) + ")";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Daemon lifecycle
// ---------------------------------------------------------------------------

bool AdbManager::startServer() {
    if (!config.validate()) {
        lastError = "Invalid ADB configuration";
        setConnectionState(AdbConnectionState::Error);
        return false;
    }

    log("[AdbManager] Starting ADB server on port " + std::to_string(config.serverPort) + "...");

    if (!runAdbCommand("start-server")) {
        setConnectionState(AdbConnectionState::Error);
        return false;
    }

    log("[AdbManager] ADB server started");
    return true;
}

bool AdbManager::stopServer() {
    log("[AdbManager] Stopping ADB server...");
    bool ok = runAdbCommand("kill-server");
    if (ok) {
        setConnectionState(AdbConnectionState::Disconnected);
        log("[AdbManager] ADB server stopped");
    }
    return ok;
}

bool AdbManager::isServerRunning() const {
    // A "devices" call with no side effects is the lightest way to probe the server
    const std::string cmd = buildAdbCommand("devices");
#ifdef _WIN32
    int ret = std::system((cmd + " >nul 2>&1").c_str());
#else
    int ret = std::system((cmd + " >/dev/null 2>&1").c_str());
#endif
    return ret == 0;
}

// ---------------------------------------------------------------------------
// Device connection
// ---------------------------------------------------------------------------

bool AdbManager::connect() {
    if (!config.enableTcpAdb) {
        log("[AdbManager] TCP ADB is disabled; skipping connect");
        return false;
    }

    setConnectionState(AdbConnectionState::Connecting);
    const std::string target = config.getConnectTarget();
    log("[AdbManager] Connecting to emulator at " + target + "...");

    std::string output;
    if (!runAdbCommandWithOutput("connect " + target, output)) {
        setConnectionState(AdbConnectionState::Error);
        return false;
    }

    // The "adb connect" output contains "connected" on success or "unauthorized"
    if (output.find("unauthorized") != std::string::npos) {
        log("[AdbManager] Device is unauthorized. Ensure ADB debugging is enabled.");
        setConnectionState(AdbConnectionState::Unauthorized);
        lastError = "Device unauthorized";
        return false;
    }

    if (output.find("connected") == std::string::npos
        && output.find("already connected") == std::string::npos) {
        log("[AdbManager] Unexpected connect output: " + output);
        setConnectionState(AdbConnectionState::Error);
        lastError = "Unexpected output from adb connect: " + output;
        return false;
    }

    setConnectionState(AdbConnectionState::Connected);
    log("[AdbManager] Connected to " + target);
    return true;
}

bool AdbManager::disconnect() {
    if (!config.enableTcpAdb) {
        return true;
    }
    const std::string target = config.getConnectTarget();
    log("[AdbManager] Disconnecting from " + target + "...");
    bool ok = runAdbCommand("disconnect " + target);
    if (ok) {
        setConnectionState(AdbConnectionState::Disconnected);
    }
    return ok;
}

bool AdbManager::waitForDevice(int timeoutSec) {
    log("[AdbManager] Waiting for device (timeout: " + std::to_string(timeoutSec) + "s)...");
    const std::string args = "-t " + std::to_string(timeoutSec) + " wait-for-device";
    if (!runAdbCommand(args)) {
        lastError = "Timed out waiting for device";
        setConnectionState(AdbConnectionState::Error);
        return false;
    }
    setConnectionState(AdbConnectionState::Connected);
    log("[AdbManager] Device is ready");
    return true;
}

// ---------------------------------------------------------------------------
// Shell access
// ---------------------------------------------------------------------------

bool AdbManager::runShellCommand(const std::string& command, std::string& output) {
    log("[AdbManager] Shell: " + command);
    // Escape single quotes in the command
    std::string escaped = command;
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "'\\''");
        pos += 4;
    }
    return runAdbCommandWithOutput("shell '" + escaped + "'", output);
}

bool AdbManager::openInteractiveShell() {
    log("[AdbManager] Opening interactive shell...");
    if (config.enableRootAccess) {
        log("[AdbManager] WARNING: Root access is enabled. Use with caution.");
    }
    // "adb shell" with no command drops into an interactive session;
    // std::system() inherits the parent's stdin/stdout/stderr.
    const std::string cmd = buildAdbCommand("shell");
    int ret = std::system(cmd.c_str());
    return ret == 0;
}

// ---------------------------------------------------------------------------
// File transfer
// ---------------------------------------------------------------------------

bool AdbManager::pushFile(const std::string& localPath, const std::string& remotePath) {
    log("[AdbManager] Push: " + localPath + " -> " + remotePath);
    struct stat st;
    if (stat(localPath.c_str(), &st) != 0) {
        lastError = "Local file not found: " + localPath;
        log("[AdbManager] Error: " + lastError);
        return false;
    }
    return runAdbCommand("push \"" + localPath + "\" \"" + remotePath + "\"");
}

bool AdbManager::pullFile(const std::string& remotePath, const std::string& localPath) {
    log("[AdbManager] Pull: " + remotePath + " -> " + localPath);
    return runAdbCommand("pull \"" + remotePath + "\" \"" + localPath + "\"");
}

// ---------------------------------------------------------------------------
// App installation
// ---------------------------------------------------------------------------

bool AdbManager::installApk(const std::string& apkPath) {
    log("[AdbManager] Installing APK: " + apkPath);

    struct stat st;
    if (stat(apkPath.c_str(), &st) != 0) {
        lastError = "APK file not found: " + apkPath;
        log("[AdbManager] Error: " + lastError);
        return false;
    }

    // -r reinstalls while keeping data; -d allows version downgrades
    if (!runAdbCommand("install -r \"" + apkPath + "\"")) {
        return false;
    }

    log("[AdbManager] APK installed successfully: " + apkPath);
    return true;
}

// ---------------------------------------------------------------------------
// Logcat
// ---------------------------------------------------------------------------

bool AdbManager::startLogcat(const std::string& outputFile) {
    if (!ensureLogDir()) {
        setConnectionState(AdbConnectionState::Error);
        return false;
    }

    std::string logPath = outputFile;
    if (logPath.empty()) {
        logPath = config.logDir + "/logcat.log";
    }

    log("[AdbManager] Starting logcat -> " + logPath);

    const std::string adbCmd = buildAdbCommand("logcat");

#ifdef _WIN32
    // On Windows, spawn a detached process and capture its PID via PowerShell
    std::string psCmd =
        "powershell -NoProfile -Command \""
        "$p = Start-Process -FilePath '" + adbCmd + "' "
        "-RedirectStandardOutput '" + logPath + "' "
        "-WindowStyle Hidden -PassThru; Write-Output $p.Id\"";
    FILE* pipe = _popen(psCmd.c_str(), "r");
    if (pipe) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), pipe)) {
            logcatPid = std::atoi(buf);
        }
        _pclose(pipe);
    }
    if (logcatPid <= 0) {
        lastError = "Failed to start logcat process";
        log("[AdbManager] Error: " + lastError);
        return false;
    }
#else
    // On POSIX, fork and exec so we have the child PID
    pid_t pid = fork();
    if (pid < 0) {
        lastError = "fork() failed for logcat";
        log("[AdbManager] Error: " + lastError);
        return false;
    }
    if (pid == 0) {
        // Child: redirect stdout to logPath, then exec adb logcat
        FILE* out = fopen(logPath.c_str(), "w");
        if (out) {
            dup2(fileno(out), STDOUT_FILENO);
            dup2(fileno(out), STDERR_FILENO);
            fclose(out);
        }
        // Parse binary and args from the command string for execvp
        execl("/bin/sh", "sh", "-c", adbCmd.c_str(), nullptr);
        _exit(1);
    }
    logcatPid = static_cast<int>(pid);
#endif

    log("[AdbManager] Logcat started (PID " + std::to_string(logcatPid) + "), output: " + logPath);
    return true;
}

bool AdbManager::stopLogcat() {
    if (logcatPid <= 0) {
        log("[AdbManager] Logcat is not running");
        return true;
    }

    log("[AdbManager] Stopping logcat (PID " + std::to_string(logcatPid) + ")...");

#ifdef _WIN32
    std::string killCmd = "taskkill /F /PID " + std::to_string(logcatPid) + " >nul 2>&1";
    std::system(killCmd.c_str());
#else
    kill(static_cast<pid_t>(logcatPid), SIGTERM);
#endif

    logcatPid = -1;
    log("[AdbManager] Logcat stopped");
    return true;
}

// ---------------------------------------------------------------------------
// Device properties
// ---------------------------------------------------------------------------

bool AdbManager::getProperty(const std::string& key, std::string& value) {
    std::string output;
    if (!runAdbCommandWithOutput("shell getprop " + key, output)) {
        return false;
    }
    // Strip trailing newline
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    if (!output.empty() && output.back() == '\r') {
        output.pop_back();
    }
    value = output;
    return true;
}
