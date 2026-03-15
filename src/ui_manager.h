#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <string>
#include <vector>

// Lightweight console UI manager for displaying emulator status and windows
class UIManager {
public:
    UIManager();
    ~UIManager();

    // General message display
    void display(const std::string& message);

    // Read a line of input from stdin
    std::string getInput();

    // Display boot status ("Initializing...", "Booting Android...", "Ready", etc.)
    void showBootStatus(const std::string& status);

    // Display a QEMU warning or error in the UI
    void showError(const std::string& message);

    // Window management
    void createWindow(int id, const std::string& title);
    void renderWindows();

    // Event handling
    void processEvent(const std::string& event);

private:
    struct Window {
        int id;
        std::string title;
    };
    std::vector<Window> windows;
};

#endif // UI_MANAGER_H