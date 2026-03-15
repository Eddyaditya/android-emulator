#pragma once

#include <iostream>

class UIManager {
public:
    UIManager();
    ~UIManager();
    void display(const std::string& message);
    std::string getInput();
};

UIManager::UIManager() {
    // Initialization code
}

UIManager::~UIManager() {
    // Cleanup code
}

void UIManager::display(const std::string& message) {
    std::cout << message << std::endl;
}

std::string UIManager::getInput() {
    std::string input;
    std::getline(std::cin, input);
    return input;
}