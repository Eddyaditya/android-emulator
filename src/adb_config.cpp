#include "adb_config.h"

#include <iostream>
#include <sys/stat.h>

AdbConfig::AdbConfig()
    : serverPort(ADB_DEFAULT_PORT)
    , devicePort(ADB_DEFAULT_DEVICE_PORT)
    , enableTcpAdb(true)
    , enableRootAccess(false)
    , enableLogcat(false)
    , adbBinaryPath("")
#ifdef _WIN32
    , logDir(std::string(getenv("LOCALAPPDATA") ? getenv("LOCALAPPDATA")
                         : (getenv("TEMP") ? getenv("TEMP") : ".")) + "\\adb_logs")
    , trustedHostsFile(std::string(getenv("USERPROFILE") ? getenv("USERPROFILE")
                                                         : ".")
                       + "\\.android\\adb_trusted_hosts")
#else
    , logDir("/tmp/adb_logs")
    , trustedHostsFile(std::string(getenv("HOME") ? getenv("HOME") : "/root")
                       + "/.android/adb_trusted_hosts")
#endif
    , connectionTimeoutSec(30)
    , maxRetries(3)
{}

bool AdbConfig::validate() const {
    if (serverPort <= 0 || serverPort > 65535) {
        std::cerr << "[AdbConfig] Error: serverPort out of range: " << serverPort << std::endl;
        return false;
    }
    if (devicePort <= 0 || devicePort > 65535) {
        std::cerr << "[AdbConfig] Error: devicePort out of range: " << devicePort << std::endl;
        return false;
    }
    if (connectionTimeoutSec <= 0) {
        std::cerr << "[AdbConfig] Error: connectionTimeoutSec must be positive" << std::endl;
        return false;
    }
    if (maxRetries < 0) {
        std::cerr << "[AdbConfig] Error: maxRetries must be non-negative" << std::endl;
        return false;
    }
    if (logDir.empty()) {
        std::cerr << "[AdbConfig] Error: logDir is not set" << std::endl;
        return false;
    }
    return true;
}

std::string AdbConfig::getConnectTarget() const {
    return "localhost:" + std::to_string(devicePort);
}
