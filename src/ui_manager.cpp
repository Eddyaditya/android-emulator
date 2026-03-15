#include "ui_manager.h"

#include <iostream>

UIManager::UIManager() {}

UIManager::~UIManager() {}

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
}

void UIManager::showError(const std::string& message) {
    std::cerr << "[Error]  " << message << std::endl;
}

void UIManager::createWindow(int id, const std::string& title) {
    windows.push_back({id, title});
}

void UIManager::renderWindows() {
    for (const auto& window : windows) {
        std::cout << "[Window " << window.id << "] " << window.title << std::endl;
    }
}

void UIManager::processEvent(const std::string& event) {
    std::cout << "[Event] " << event << std::endl;
}