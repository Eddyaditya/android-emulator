#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "frame_buffer.h"
#include "input_handler.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <SDL2/SDL.h>

// UIWindow owns the SDL2 window and renderer.  It runs the render loop in a
// dedicated background thread so that the main application thread (and the
// QEMU process) are never blocked by display operations.
//
// Thread safety:
//   - create() / destroy() must be called from the same thread.
//   - setStatusMessage() / setErrorMessage() / setFullscreen() are
//     thread-safe and can be called from any thread.
//   - The render thread calls FrameBuffer::render() and InputHandler callbacks
//     should be registered before start() is called.
class UIWindow {
public:
    // Default Android portrait resolution
    static constexpr int kDefaultWidth  = 1080;
    static constexpr int kDefaultHeight = 1920;

    UIWindow();
    ~UIWindow();

    // Create the SDL2 window and renderer; returns false on failure.
    // Must be called before start().
    bool create(int width     = kDefaultWidth,
                int height    = kDefaultHeight,
                const std::string& title = "EDU Android Emulator");

    // Start the background render+event thread.  Requires a prior create().
    void start();

    // Signal the render thread to stop and wait for it to exit.
    void stop();

    // Destroy the SDL2 window/renderer (call after stop()).
    void destroy();

    // True while the render thread is running and the window hasn't been
    // closed by the user.
    bool isRunning() const;

    // ---- Frame buffer --------------------------------------------------
    // Return the shared FrameBuffer that callers can push pixels into.
    std::shared_ptr<FrameBuffer> getFrameBuffer() const;

    // ---- Input handling ------------------------------------------------
    // Return the InputHandler so callers can register callbacks or drain
    // the event queue.
    std::shared_ptr<InputHandler> getInputHandler() const;

    // ---- Status overlays -----------------------------------------------
    // These are rendered in the window title bar (no SDL_ttf required).

    // Message shown in the title bar (e.g. "Booting Android...")
    void setStatusMessage(const std::string& msg);

    // Error message appended to the title (e.g. "Error: image not found")
    void setErrorMessage(const std::string& msg);

    // ---- Window modes --------------------------------------------------
    void setFullscreen(bool enable);
    bool isFullscreen() const;

    // ---- Debug ---------------------------------------------------------
    // When enabled the title bar shows a live FPS counter.
    void setShowFPS(bool enable);

private:
    // Entry point for the render thread
    void renderThreadMain();

    // Called once from the render thread to set up the frame buffer.
    void initFrameBuffer();

    // Render one frame; returns false if the window should close.
    bool renderFrame();

    // Poll all pending SDL events and forward to the InputHandler.
    // Returns false if a SDL_QUIT event was received.
    bool pumpEvents();

    // Update the window title with current status / FPS.
    void refreshTitle();

    SDL_Window*   sdlWindow;
    SDL_Renderer* sdlRenderer;

    int  windowWidth;
    int  windowHeight;
    std::string windowTitle;

    std::shared_ptr<FrameBuffer>  frameBuffer;
    std::shared_ptr<InputHandler> inputHandler;

    std::thread       renderThread;
    std::atomic<bool> threadRunning;
    std::atomic<bool> windowClosed;
    std::atomic<bool> fullscreen;
    std::atomic<bool> showFPS;

    mutable std::mutex statusMutex;
    std::string statusMessage;
    std::string errorMessage;

    // FPS tracking
    Uint32 fpsLastTime;
    int    fpsFrameCount;
    int    fpsCurrent;
};

#endif // UI_WINDOW_H
