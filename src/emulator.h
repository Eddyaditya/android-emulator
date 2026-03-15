#ifndef EMULATOR_H
#define EMULATOR_H

#include "qemu_manager.h"
#include "ui_manager.h"
#include <string>

class Emulator {
private:
    QEMUManager qemuManager;
    UIManager uiManager;
    bool initialized;
    bool running;
    std::string version;

public:
    Emulator();
    ~Emulator();

    bool initialize();
    void start();
    void run();
    void shutdown();
    bool isRunning() const;
    std::string getVersion() const;
};

#endif // EMULATOR_H