#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "ui_window.h"
#include "frame_buffer.h"
#include "input_handler.h"

#include <memory>
#include <string>
#include <vector>

// UIManager is the top-level UI coordinator.
//
// It owns the SDL2 UIWindow (and its associated FrameBuffer / InputHandler)
// and exposes a high-level interface used by the rest of the emulator.
// All console-compatible methods are preserved so existing code continues to
// compile unchanged.
class UIManager {
public:
    UIManager();
    ~UIManager();

    // ---- SDL2 lifecycle ------------------------------------------------

    // Initialise SDL2 and create the display window.
    // Returns true on success; on failure the UI falls back to console mode.
    bool initSDL(int width  = UIWindow::kDefaultWidth,
                 int height = UIWindow::kDefaultHeight,
                 const std::string& title = "EDU Android Emulator");

    // Start the render thread (call after initSDL()).
    void startSDL();

    // Pump SDL events; call regularly from the main loop.
    // Returns false if the user has closed the window.
    bool processEvents();

    // Stop the render thread and tear down SDL2.
    void shutdownSDL();

    // True when the SDL2 window is open and the render thread is running.
    bool isSDLRunning() const;

    // ---- Frame buffer access -------------------------------------------
    std::shared_ptr<FrameBuffer>  getFrameBuffer()  const;
    std::shared_ptr<InputHandler> getInputHandler() const;

    // ---- High-level status methods (work in both SDL and console modes) -

    // General message (logged to console; also forwarded to window title)
    void display(const std::string& message);

    // Read a line of input from stdin
    std::string getInput();

    // Display boot status ("Initializing...", "Booting Android...", "Ready")
    void showBootStatus(const std::string& status);

    // Display a QEMU warning or error
    void showError(const std::string& message);

    // ---- Legacy window management (console mode) -----------------------
    void createWindow(int id, const std::string& title);
    void renderWindows();
    void processEvent(const std::string& event);

    // ---- Window mode controls ------------------------------------------
    void setFullscreen(bool enable);
    void setShowFPS(bool enable);

private:
    std::unique_ptr<UIWindow> uiWindow;
    bool sdlInitialized;

    struct Window {
        int id;
        std::string title;
    };
    std::vector<Window> windows;
};

#endif // UI_MANAGER_H