#include "ui_manager.h"

#include <iostream>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

UIManager::UIManager()
    : sdlInitialized(false)
{}

UIManager::~UIManager() {
    if (sdlInitialized) {
        shutdownSDL();
    }
}

// ---------------------------------------------------------------------------
// SDL2 lifecycle
// ---------------------------------------------------------------------------

bool UIManager::initSDL(int width, int height, const std::string& title) {
    uiWindow = std::make_unique<UIWindow>();
    if (!uiWindow->create(width, height, title)) {
        std::cerr << "[UIManager] SDL2 window creation failed; "
                     "falling back to console mode." << std::endl;
        uiWindow.reset();
        return false;
    }
    sdlInitialized = true;
    return true;
}

void UIManager::startSDL() {
    if (uiWindow) {
        uiWindow->start();
    }
}

bool UIManager::processEvents() {
    // If SDL is not active, nothing to do
    if (!uiWindow) {
        return true;
    }
    return uiWindow->isRunning();
}

void UIManager::shutdownSDL() {
    if (uiWindow) {
        uiWindow->stop();
        uiWindow->destroy();
        uiWindow.reset();
    }
    SDL_Quit();
    sdlInitialized = false;
}

bool UIManager::isSDLRunning() const {
    return uiWindow && uiWindow->isRunning();
}

// ---------------------------------------------------------------------------
// Frame buffer / input handler accessors
// ---------------------------------------------------------------------------

std::shared_ptr<FrameBuffer> UIManager::getFrameBuffer() const {
    if (uiWindow) {
        return uiWindow->getFrameBuffer();
    }
    return nullptr;
}

std::shared_ptr<InputHandler> UIManager::getInputHandler() const {
    if (uiWindow) {
        return uiWindow->getInputHandler();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// High-level status methods
// ---------------------------------------------------------------------------

void UIManager::display(const std::string& message) {
    std::cout << message << std::endl;
}

std::string UIManager::getInput() {
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void UIManager::showBootStatus(const std::string& status) {
    std::cout << "[Status] " << status << std::endl;
    if (uiWindow) {
        uiWindow->setStatusMessage(status);
    }
}

void UIManager::showError(const std::string& message) {
    std::cerr << "[Error]  " << message << std::endl;
    if (uiWindow) {
        uiWindow->setErrorMessage(message);
    }
}

// ---------------------------------------------------------------------------
// Legacy console-mode window management
// ---------------------------------------------------------------------------

void UIManager::createWindow(int id, const std::string& title) {
    windows.push_back({id, title});
}

void UIManager::renderWindows() {
    for (const auto& w : windows) {
        std::cout << "[Window " << w.id << "] " << w.title << std::endl;
    }
}

void UIManager::processEvent(const std::string& event) {
    std::cout << "[Event] " << event << std::endl;
}

// ---------------------------------------------------------------------------
// Window mode controls
// ---------------------------------------------------------------------------

void UIManager::setFullscreen(bool enable) {
    if (uiWindow) {
        uiWindow->setFullscreen(enable);
    }
}

void UIManager::setShowFPS(bool enable) {
    if (uiWindow) {
        uiWindow->setShowFPS(enable);
    }
}