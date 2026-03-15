#include "emulator.h"
#include <iostream>

Emulator::Emulator()
    : initialized(false)
    , running(false)
    , version("1.0.0")
{
    std::cout << "[Emulator] Instance created (v" << version << ")" << std::endl;
}

Emulator::~Emulator() {
    if (running) {
        shutdown();
    }
    std::cout << "[Emulator] Instance destroyed" << std::endl;
}

bool Emulator::initialize() {
    if (initialized) {
        return true;
    }

    // Forward QEMU log output to the UI manager
    qemuManager.setLogCallback([this](const std::string& msg) {
        uiManager.display(msg);
    });

    // Auto-detect and configure hardware acceleration
    qemuManager.configureHardwareAcceleration();

    initialized = true;
    return true;
}

void Emulator::start() {
    if (!initialized) {
        std::cerr << "[Emulator] Cannot start: not initialized" << std::endl;
        return;
    }

    uiManager.showBootStatus("Initializing...");

    // Load the default Android x86 system image path (configurable at runtime)
    const std::string defaultImage = "android-x86.img";
    if (!qemuManager.loadImage(defaultImage)) {
        uiManager.showBootStatus("Error: image not found (" + defaultImage + ")");
        uiManager.showError(qemuManager.getLastError());
        return;
    }

    uiManager.showBootStatus("Booting Android...");

    if (!qemuManager.start()) {
        uiManager.showBootStatus("Error: QEMU failed to start");
        uiManager.showError(qemuManager.getLastError());
        return;
    }

    running = true;
    uiManager.showBootStatus(qemuManager.getBootStatusString());
}

void Emulator::run() {
    if (!running) {
        return;
    }
    // Main event loop placeholder — a real implementation would pump SDL2/Qt events
    std::cout << "[Emulator] Running (press Ctrl+C to stop)" << std::endl;
}

void Emulator::shutdown() {
    if (!running) {
        return;
    }
    std::cout << "[Emulator] Shutting down..." << std::endl;
    qemuManager.stop();
    running = false;
    uiManager.showBootStatus("Stopped");
}

bool Emulator::isRunning() const {
    return running;
}

std::string Emulator::getVersion() const {
    return version;
}