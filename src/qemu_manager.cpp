#include "qemu_manager.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

QEMUManager::QEMUManager()
    : bootStatus(BootStatus::Idle)
    , lastError("")
    , running(false)
    , processPid(-1)
    , outputPipeFd(-1)
{}

QEMUManager::~QEMUManager() {
    if (running) {
        stop();
    }
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void QEMUManager::setConfig(const QEMUConfig& cfg) {
    config = cfg;
}

QEMUConfig& QEMUManager::getConfig() {
    return config;
}

// ---------------------------------------------------------------------------
// Hardware Acceleration
// ---------------------------------------------------------------------------

bool QEMUManager::isKVMAvailable() const {
#ifdef __linux__
    struct stat st;
    return (stat("/dev/kvm", &st) == 0);
#else
    return false;
#endif
}

bool QEMUManager::isHAXMAvailable() const {
#if defined(_WIN32) || defined(__APPLE__)
    // Check for HAXM kernel module / device node
#  ifdef _WIN32
    HANDLE h = CreateFileA("\\\\.\\HAX", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return true;
    }
    return false;
#  else
    struct stat st;
    return (stat("/dev/HAX", &st) == 0);
#  endif
#else
    return false;
#endif
}

void QEMUManager::configureHardwareAcceleration() {
    if (!config.hardwareAccelEnabled) {
        logOutput("[QEMUManager] Hardware acceleration disabled by configuration");
        return;
    }

    if (isKVMAvailable()) {
        config.kvmEnabled = true;
        config.haxmEnabled = false;
        logOutput("[QEMUManager] KVM detected and enabled");
    } else if (isHAXMAvailable()) {
        config.kvmEnabled = false;
        config.haxmEnabled = true;
        logOutput("[QEMUManager] HAXM detected and enabled");
    } else {
        config.kvmEnabled = false;
        config.haxmEnabled = false;
        logOutput("[QEMUManager] No hardware acceleration available; using software (TCG) emulation");
    }
}

// ---------------------------------------------------------------------------
// System Image
// ---------------------------------------------------------------------------

bool QEMUManager::loadImage(const std::string& imagePath) {
    config.setImagePath(imagePath);
    if (!validateImage()) {
        return false;
    }
    logOutput("[QEMUManager] Image loaded: " + imagePath
              + " (format: " + config.imageFormat + ")");
    return true;
}

bool QEMUManager::validateImage() const {
    return config.validate();
}

// ---------------------------------------------------------------------------
// QEMU argument builder
// ---------------------------------------------------------------------------

std::vector<std::string> QEMUManager::buildQEMUArgs() const {
    std::vector<std::string> args;

    // Machine and CPU
    // Use "host" CPU only when KVM is active (requires virtualisation extensions);
    // fall back to a generic x86-64 CPU for HAXM and software (TCG) modes.
    args.push_back("-machine");
    args.push_back("pc");
    args.push_back("-cpu");
    args.push_back(config.kvmEnabled ? "host" : "qemu64");

    // Memory
    std::ostringstream mem;
    mem << config.memorySizeMB << "M";
    args.push_back("-m");
    args.push_back(mem.str());

    // SMP (virtual CPUs)
    std::ostringstream smp;
    smp << config.cpuCount;
    args.push_back("-smp");
    args.push_back(smp.str());

    // Hardware acceleration flag (KVM, HAXM, or TCG software)
    std::string accel = config.getAccelerationArg();
    if (accel == "-enable-kvm") {
        args.push_back("-enable-kvm");
    } else {
        // accel is "-accel hax" or "-accel tcg" — split into two separate tokens
        const std::string prefix = "-accel ";
        args.push_back("-accel");
        args.push_back(accel.substr(prefix.size()));
    }

    // Boot drive (Android x86 image)
    if (config.imageFormat == "iso") {
        args.push_back("-cdrom");
        args.push_back(config.imagePath);
        args.push_back("-boot");
        args.push_back("d");
    } else {
        args.push_back("-hda");
        args.push_back(config.imagePath);
        args.push_back("-boot");
        args.push_back("c");
    }

    // Display resolution
    std::ostringstream res;
    res << config.displayWidth << "x" << config.displayHeight;
    args.push_back("-vga");
    args.push_back("virtio");
    args.push_back("-display");
    args.push_back("sdl,gl=off");

    // Network
    args.push_back("-net");
    args.push_back("nic");
    args.push_back("-net");
    args.push_back("user");

    // Redirect QEMU monitor to stdio for logging
    args.push_back("-monitor");
    args.push_back("none");

    // Serial output for boot log capture
    args.push_back("-serial");
    args.push_back("stdio");

    return args;
}

// ---------------------------------------------------------------------------
// Process lifecycle
// ---------------------------------------------------------------------------

bool QEMUManager::start() {
    if (running) {
        logOutput("[QEMUManager] Already running");
        return true;
    }

    setBootStatus(BootStatus::Initializing);
    logOutput("[QEMUManager] Initializing...");

    // Auto-detect hardware acceleration if not already configured
    configureHardwareAcceleration();

    // Validate the image before launching
    if (!validateImage()) {
        lastError = "Image validation failed: " + config.imagePath;
        logOutput("[QEMUManager] Error: " + lastError);
        setBootStatus(BootStatus::Error);
        return false;
    }

    std::vector<std::string> args = buildQEMUArgs();

    // Log the full QEMU command line
    std::string cmdLine = "qemu-system-x86_64";
    for (const auto& a : args) {
        cmdLine += " " + a;
    }
    logOutput("[QEMUManager] Launching: " + cmdLine);

    setBootStatus(BootStatus::Booting);
    logOutput("[QEMUManager] Booting Android...");

#ifdef _WIN32
    // Windows: use CreateProcess
    // lpCommandLine must point to a writable buffer (CreateProcessA may modify it).
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string mutableCmdLine = cmdLine;
    if (!CreateProcessA(NULL,
                        &mutableCmdLine[0],
                        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        lastError = "CreateProcess failed (error " + std::to_string(GetLastError()) + ")";
        logOutput("[QEMUManager] Error: " + lastError);
        setBootStatus(BootStatus::Error);
        return false;
    }
    processPid = static_cast<int>(pi.dwProcessId);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

#else
    // POSIX: fork + execvp with a pipe to capture stdout/stderr
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        lastError = std::string("pipe() failed: ") + strerror(errno);
        logOutput("[QEMUManager] Error: " + lastError);
        setBootStatus(BootStatus::Error);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        lastError = std::string("fork() failed: ") + strerror(errno);
        logOutput("[QEMUManager] Error: " + lastError);
        setBootStatus(BootStatus::Error);
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        // Child process: redirect stdout/stderr into write end of pipe, then exec QEMU
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Build argv for execvp
        std::vector<char*> argv;
        std::string bin = "qemu-system-x86_64";
        argv.push_back(const_cast<char*>(bin.c_str()));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp("qemu-system-x86_64", argv.data());
        // execvp only returns on error
        std::cerr << "[QEMUManager] execvp failed: " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process: keep the read end open so QEMU output can be consumed.
    close(pipefd[1]);
    processPid = static_cast<int>(pid);

    // Set the read end non-blocking so log reading won't stall the caller.
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    outputPipeFd = pipefd[0]; // Caller / future reader thread drains this fd
#endif

    running = true;
    setBootStatus(BootStatus::Ready);
    logOutput("[QEMUManager] Ready (PID " + std::to_string(processPid) + ")");
    return true;
}

void QEMUManager::stop() {
    if (!running) {
        return;
    }

    logOutput("[QEMUManager] Stopping QEMU (PID " + std::to_string(processPid) + ")...");

#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(processPid));
    if (h) {
        TerminateProcess(h, 0);
        CloseHandle(h);
    }
#else
    kill(static_cast<pid_t>(processPid), SIGTERM);
    // Give it a moment to shut down gracefully, then force-kill
    int status;
    struct timespec ts = {2, 0};
    nanosleep(&ts, nullptr);
    if (waitpid(static_cast<pid_t>(processPid), &status, WNOHANG) == 0) {
        kill(static_cast<pid_t>(processPid), SIGKILL);
        waitpid(static_cast<pid_t>(processPid), &status, 0);
    }
#endif

    cleanupQEMU();
    logOutput("[QEMUManager] QEMU stopped");
}

void QEMUManager::pause() {
    if (!running) {
        return;
    }
#ifndef _WIN32
    kill(static_cast<pid_t>(processPid), SIGSTOP);
    logOutput("[QEMUManager] QEMU paused");
#else
    logOutput("[QEMUManager] Pause not supported on this platform");
#endif
}

void QEMUManager::resume() {
    if (!running) {
        return;
    }
#ifndef _WIN32
    kill(static_cast<pid_t>(processPid), SIGCONT);
    logOutput("[QEMUManager] QEMU resumed");
#else
    logOutput("[QEMUManager] Resume not supported on this platform");
#endif
}

bool QEMUManager::isRunning() const {
    return running;
}

// ---------------------------------------------------------------------------
// Status helpers
// ---------------------------------------------------------------------------

BootStatus QEMUManager::getBootStatus() const {
    return bootStatus;
}

std::string QEMUManager::getBootStatusString() const {
    switch (bootStatus) {
        case BootStatus::Idle:          return "Idle";
        case BootStatus::Initializing:  return "Initializing...";
        case BootStatus::Booting:       return "Booting Android...";
        case BootStatus::Ready:         return "Ready";
        case BootStatus::Error:         return "Error: " + lastError;
        case BootStatus::Stopped:       return "Stopped";
        default:                        return "Unknown";
    }
}

std::string QEMUManager::getLastError() const {
    return lastError;
}

void QEMUManager::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void QEMUManager::setBootStatus(BootStatus status) {
    bootStatus = status;
}

void QEMUManager::logOutput(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        std::cout << message << std::endl;
    }
}

void QEMUManager::initializeQEMU() {
    // Reserved for future platform-specific init (e.g. loading HAXM driver)
}

void QEMUManager::cleanupQEMU() {
    running = false;
    processPid = -1;
#ifndef _WIN32
    if (outputPipeFd != -1) {
        close(outputPipeFd);
        outputPipeFd = -1;
    }
#endif
    setBootStatus(BootStatus::Stopped);
}