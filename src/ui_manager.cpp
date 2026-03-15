#include <iostream>
#include <vector>

class Window {
public:
    int id;
    std::string title;

    Window(int id, const std::string& title) : id(id), title(title) {}
    void render() {
        std::cout << "Rendering window: " << title << std::endl;
    }
};

class UIManager {
private:
    std::vector<Window> windows;

public:
    void createWindow(int id, const std::string& title) {
        windows.emplace_back(id, title);
    }

    void renderWindows() {
        for (auto& window : windows) {
            window.render();
        }
    }

    void processEvent(const std::string& event) {
        std::cout << "Processing event: " << event << std::endl;
        // Add event processing logic here
    }
};

int main() {
    UIManager uiManager;
    uiManager.createWindow(1, "Main Window");
    uiManager.createWindow(2, "Settings Window");
    uiManager.renderWindows();
    uiManager.processEvent("Window Resized");
    return 0;
}