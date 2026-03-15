#include <iostream>
#include "emulator.h"

int main() {
    std::cout << "=====================================\n";
    std::cout << "  Welcome to EDU Android Emulator\n";
    std::cout << "=====================================\n";
    std::cout << std::endl;

    // Initialize the emulator
    Emulator emulator;
    
    std::cout << "Initializing EDU Emulator..." << std::endl;
    if (emulator.initialize()) {
        std::cout << "✓ EDU Emulator initialized successfully!" << std::endl;
        std::cout << std::endl;
        
        // Start the emulator
        std::cout << "Starting EDU Emulator..." << std::endl;
        emulator.start();
        
        // Run the emulator
        std::cout << "EDU Emulator is running..." << std::endl;
        emulator.run();
        
        // Shutdown
        std::cout << "Shutting down EDU Emulator..." << std::endl;
        emulator.shutdown();
    } else {
        std::cout << "✗ Failed to initialize EDU Emulator" << std::endl;
        return 1;
    }

    return 0;
}