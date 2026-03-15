#include "emulator.h"
#include <iostream>

Emulator::Emulator() {
    std::cout << "Emulator instance created" << std::endl;
}

Emulator::~Emulator() {
    std::cout << "Emulator instance destroyed" << std::endl;
}

void Emulator::start() {
    std::cout << "Starting Android virtual machine..." << std::endl;
    std::cout << "Loading Android system image..." << std::endl;
    std::cout << "Installing Google Play Services..." << std::endl;
    std::cout << "Installing Google Play Store..." << std::endl;
    std::cout << "Installing Chrome browser..." << std::endl;
    std::cout << "Emulator started successfully" << std::endl;
}

void Emulator::stop() {
    std::cout << "Stopping emulator..." << std::endl;
}