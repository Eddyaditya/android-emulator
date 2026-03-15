#include <iostream>
#include "emulator.h"

int main(int argc, char* argv[]) {
    std::cout << "=====================================" << std::endl;
    std::cout << "  EDU Android Emulator" << std::endl;
    std::cout << "  Lightweight · Fast · Educational" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;

    // Create emulator instance
    Emulator emulator;
    
    // Initialize
    std::cout << "[Main] Initializing..." << std::endl;
    if (!emulator.initialize()) {
        std::cerr << "[Main] Failed to initialize emulator" << std::endl;
        return 1;
    }
    
    // Start
    std::cout << "[Main] Starting emulator..." << std::endl;
    emulator.start();
    
    // Run main loop
    std::cout << "[Main] Entering main loop..." << std::endl;
    emulator.run();
    
    // Shutdown
    std::cout << "[Main] Shutting down..." << std::endl;
    emulator.shutdown();
    
    std::cout << "[Main] Thank you for using EDU Emulator!" << std::endl;
    return 0;
}