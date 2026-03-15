#include "ui_window.h"

#include <iostream>
#include <sstream>

// Target frame duration in milliseconds (~30 FPS)
static constexpr Uint32 kFrameMs = 33;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

UIWindow::UIWindow()
    : sdlWindow(nullptr)
    , sdlRenderer(nullptr)
    , windowWidth(kDefaultWidth)
    , windowHeight(kDefaultHeight)
    , threadRunning(false)
    , windowClosed(false)
    , fullscreen(false)
    , showFPS(false)
    , fpsLastTime(0)
    , fpsFrameCount(0)
    , fpsCurrent(0)
{
    frameBuffer  = std::make_shared<FrameBuffer>();
    inputHandler = std::make_shared<InputHandler>();
}

UIWindow::~UIWindow() {
    stop();
    destroy();
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------

bool UIWindow::create(int width, int height, const std::string& title) {
    windowWidth  = width;
    windowHeight = height;
    windowTitle  = title;

    // SDL2 must be initialized before creating a window.
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            std::cerr << "[UIWindow] SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    // Scale down if the screen is smaller than the default 1080×1920
    SDL_DisplayMode dm{};
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        const int maxW = dm.w  - 40;
        const int maxH = dm.h  - 80;
        if (windowWidth > maxW || windowHeight > maxH) {
            float scaleW = static_cast<float>(maxW) / windowWidth;
            float scaleH = static_cast<float>(maxH) / windowHeight;
            float scale  = (scaleW < scaleH) ? scaleW : scaleH;
            windowWidth  = static_cast<int>(windowWidth  * scale);
            windowHeight = static_cast<int>(windowHeight * scale);
        }
    }

    sdlWindow = SDL_CreateWindow(
        windowTitle.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!sdlWindow) {
        std::cerr << "[UIWindow] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    sdlRenderer = SDL_CreateRenderer(
        sdlWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!sdlRenderer) {
        // Fall back to software renderer
        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_SOFTWARE);
        if (!sdlRenderer) {
            std::cerr << "[UIWindow] SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(sdlWindow);
            sdlWindow = nullptr;
            return false;
        }
    }

    // Keep aspect ratio when the window is resized
    SDL_RenderSetLogicalSize(sdlRenderer, kDefaultWidth, kDefaultHeight);

    return true;
}

void UIWindow::start() {
    if (threadRunning || !sdlWindow) {
        return;
    }
    windowClosed  = false;
    threadRunning = true;
    renderThread  = std::thread(&UIWindow::renderThreadMain, this);
}

void UIWindow::stop() {
    threadRunning = false;
    if (renderThread.joinable()) {
        renderThread.join();
    }
}

void UIWindow::destroy() {
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = nullptr;
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = nullptr;
    }
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool UIWindow::isRunning() const {
    return threadRunning && !windowClosed;
}

std::shared_ptr<FrameBuffer> UIWindow::getFrameBuffer() const {
    return frameBuffer;
}

std::shared_ptr<InputHandler> UIWindow::getInputHandler() const {
    return inputHandler;
}

// ---------------------------------------------------------------------------
// Status / error messages (thread-safe setters)
// ---------------------------------------------------------------------------

void UIWindow::setStatusMessage(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        statusMessage = msg;
    }
    refreshTitle();
}

void UIWindow::setErrorMessage(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        errorMessage = msg;
    }
    refreshTitle();
}

// ---------------------------------------------------------------------------
// Window modes
// ---------------------------------------------------------------------------

void UIWindow::setFullscreen(bool enable) {
    fullscreen = enable;
    if (sdlWindow) {
        SDL_SetWindowFullscreen(sdlWindow,
            enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
}

bool UIWindow::isFullscreen() const {
    return fullscreen;
}

void UIWindow::setShowFPS(bool enable) {
    showFPS = enable;
}

// ---------------------------------------------------------------------------
// Title bar refresh
// ---------------------------------------------------------------------------

void UIWindow::refreshTitle() {
    if (!sdlWindow) {
        return;
    }

    std::string title;
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        title = windowTitle;
        if (!statusMessage.empty()) {
            title += " — " + statusMessage;
        }
        if (!errorMessage.empty()) {
            title += " [" + errorMessage + "]";
        }
    }
    if (showFPS) {
        title += " [" + std::to_string(fpsCurrent) + " FPS]";
    }
    SDL_SetWindowTitle(sdlWindow, title.c_str());
}

// ---------------------------------------------------------------------------
// Render thread
// ---------------------------------------------------------------------------

void UIWindow::renderThreadMain() {
    initFrameBuffer();

    fpsLastTime   = SDL_GetTicks();
    fpsFrameCount = 0;
    fpsCurrent    = 0;

    while (threadRunning) {
        Uint32 frameStart = SDL_GetTicks();

        // Poll SDL events (must happen on the thread that calls SDL_PollEvent).
        if (!pumpEvents()) {
            windowClosed  = true;
            threadRunning = false;
            break;
        }

        // Render the frame.
        if (!renderFrame()) {
            windowClosed  = true;
            threadRunning = false;
            break;
        }

        // FPS tracking
        ++fpsFrameCount;
        Uint32 now = SDL_GetTicks();
        if (now - fpsLastTime >= 1000) {
            fpsCurrent    = fpsFrameCount;
            fpsFrameCount = 0;
            fpsLastTime   = now;
            if (showFPS) {
                refreshTitle();
            }
        }

        // Cap to ~30 FPS
        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < kFrameMs) {
            SDL_Delay(kFrameMs - elapsed);
        }
    }
}

void UIWindow::initFrameBuffer() {
    // Attach the frame buffer to our renderer at the logical resolution.
    frameBuffer->attachRenderer(sdlRenderer, kDefaultWidth, kDefaultHeight);

    // Show a colour-bar test pattern until real QEMU data arrives.
    frameBuffer->fillTestPattern();
}

bool UIWindow::pumpEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            inputHandler->processSDLEvent(ev);
            return false;
        }
        if (ev.type == SDL_WINDOWEVENT) {
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                InputEvent quit{};
                quit.type = InputEventType::Quit;
                // Enqueue directly via the handler's public poll path
                // by synthesising a SDL_QUIT event.
                SDL_Event quitEv;
                SDL_zero(quitEv);
                quitEv.type = SDL_QUIT;
                inputHandler->processSDLEvent(quitEv);
                return false;
            }
            // Fullscreen toggle: F11 is handled below; window resize is
            // handled automatically by SDL_RenderSetLogicalSize().
        }
        inputHandler->processSDLEvent(ev);

        // Fullscreen toggle via F11
        if (ev.type == SDL_KEYDOWN &&
            ev.key.keysym.sym == SDLK_F11) {
            setFullscreen(!isFullscreen());
        }
    }
    return true;
}

bool UIWindow::renderFrame() {
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);

    // Draw the Android frame buffer
    frameBuffer->render(sdlRenderer, nullptr);

    SDL_RenderPresent(sdlRenderer);
    return true;
}
