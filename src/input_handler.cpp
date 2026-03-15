#include "input_handler.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

InputHandler::InputHandler()
    : mouseButtonDown(false)
{}

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------

void InputHandler::setEventCallback(std::function<void(const InputEvent&)> cb) {
    std::lock_guard<std::mutex> lock(queueMutex);
    callback = std::move(cb);
}

// ---------------------------------------------------------------------------
// SDL event translation
// ---------------------------------------------------------------------------

bool InputHandler::processSDLEvent(const SDL_Event& sdlEvent) {
    InputEvent ev{};

    switch (sdlEvent.type) {

    // ---- Keyboard events -----------------------------------------------
    case SDL_KEYDOWN: {
        ev.type      = InputEventType::KeyDown;
        ev.keycode   = sdlEvent.key.keysym.sym;
        ev.scancode  = sdlEvent.key.keysym.scancode;
        ev.modifiers = sdlEvent.key.keysym.mod;
        enqueue(ev);
        return true;
    }
    case SDL_KEYUP: {
        ev.type      = InputEventType::KeyUp;
        ev.keycode   = sdlEvent.key.keysym.sym;
        ev.scancode  = sdlEvent.key.keysym.scancode;
        ev.modifiers = sdlEvent.key.keysym.mod;
        enqueue(ev);
        return true;
    }

    // ---- Text input event ----------------------------------------------
    case SDL_TEXTINPUT: {
        ev.type = InputEventType::TextInput;
        std::strncpy(ev.text, sdlEvent.text.text, sizeof(ev.text) - 1);
        ev.text[sizeof(ev.text) - 1] = '\0';
        enqueue(ev);
        return true;
    }

    // ---- Mouse button events ------------------------------------------
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        bool isDown = (sdlEvent.type == SDL_MOUSEBUTTONDOWN);
        ev.type       = InputEventType::MouseButton;
        ev.x          = sdlEvent.button.x;
        ev.y          = sdlEvent.button.y;
        ev.button     = sdlEvent.button.button;
        ev.buttonDown = isDown;
        enqueue(ev);

        // Simulate touch events from left-button actions
        if (sdlEvent.button.button == SDL_BUTTON_LEFT) {
            InputEvent touch{};
            touch.x = sdlEvent.button.x;
            touch.y = sdlEvent.button.y;
            if (isDown) {
                touch.type       = InputEventType::TouchBegin;
                mouseButtonDown  = true;
            } else {
                touch.type       = InputEventType::TouchEnd;
                mouseButtonDown  = false;
            }
            enqueue(touch);
        }
        return true;
    }

    // ---- Mouse motion --------------------------------------------------
    case SDL_MOUSEMOTION: {
        ev.type = InputEventType::MouseMotion;
        ev.x    = sdlEvent.motion.x;
        ev.y    = sdlEvent.motion.y;
        ev.dx   = sdlEvent.motion.xrel;
        ev.dy   = sdlEvent.motion.yrel;
        enqueue(ev);

        // Simulate touch-move when left button is held
        if (mouseButtonDown) {
            InputEvent touch{};
            touch.type = InputEventType::TouchMove;
            touch.x    = sdlEvent.motion.x;
            touch.y    = sdlEvent.motion.y;
            touch.dx   = sdlEvent.motion.xrel;
            touch.dy   = sdlEvent.motion.yrel;
            enqueue(touch);
        }
        return true;
    }

    // ---- Quit event ---------------------------------------------------
    case SDL_QUIT: {
        ev.type = InputEventType::Quit;
        enqueue(ev);
        return true;
    }

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Queue management
// ---------------------------------------------------------------------------

void InputHandler::enqueue(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push(event);
    if (callback) {
        callback(event);
    }
}

bool InputHandler::pollEvent(InputEvent& out) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (eventQueue.empty()) {
        return false;
    }
    out = eventQueue.front();
    eventQueue.pop();
    return true;
}

void InputHandler::clearEvents() {
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!eventQueue.empty()) {
        eventQueue.pop();
    }
}

size_t InputHandler::pendingCount() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return eventQueue.size();
}

// ---------------------------------------------------------------------------
// QEMU key name mapping
// ---------------------------------------------------------------------------

std::string InputHandler::qemuKeyName(SDL_Keycode keycode) {
    switch (keycode) {
    // Alphabet
    case SDLK_a: return "a"; case SDLK_b: return "b";
    case SDLK_c: return "c"; case SDLK_d: return "d";
    case SDLK_e: return "e"; case SDLK_f: return "f";
    case SDLK_g: return "g"; case SDLK_h: return "h";
    case SDLK_i: return "i"; case SDLK_j: return "j";
    case SDLK_k: return "k"; case SDLK_l: return "l";
    case SDLK_m: return "m"; case SDLK_n: return "n";
    case SDLK_o: return "o"; case SDLK_p: return "p";
    case SDLK_q: return "q"; case SDLK_r: return "r";
    case SDLK_s: return "s"; case SDLK_t: return "t";
    case SDLK_u: return "u"; case SDLK_v: return "v";
    case SDLK_w: return "w"; case SDLK_x: return "x";
    case SDLK_y: return "y"; case SDLK_z: return "z";
    // Digits
    case SDLK_0: return "0"; case SDLK_1: return "1";
    case SDLK_2: return "2"; case SDLK_3: return "3";
    case SDLK_4: return "4"; case SDLK_5: return "5";
    case SDLK_6: return "6"; case SDLK_7: return "7";
    case SDLK_8: return "8"; case SDLK_9: return "9";
    // Special keys
    case SDLK_RETURN:    return "ret";
    case SDLK_ESCAPE:    return "esc";
    case SDLK_BACKSPACE: return "backspace";
    case SDLK_TAB:       return "tab";
    case SDLK_SPACE:     return "spc";
    case SDLK_DELETE:    return "delete";
    case SDLK_INSERT:    return "insert";
    case SDLK_HOME:      return "home";
    case SDLK_END:       return "end";
    case SDLK_PAGEUP:    return "pgup";
    case SDLK_PAGEDOWN:  return "pgdn";
    // Arrow keys
    case SDLK_UP:    return "up";
    case SDLK_DOWN:  return "down";
    case SDLK_LEFT:  return "left";
    case SDLK_RIGHT: return "right";
    // Modifier keys
    case SDLK_LCTRL:  return "ctrl";
    case SDLK_RCTRL:  return "ctrl";
    case SDLK_LSHIFT: return "shift";
    case SDLK_RSHIFT: return "shift";
    case SDLK_LALT:   return "alt";
    case SDLK_RALT:   return "alt";
    case SDLK_LGUI:   return "meta_l";
    case SDLK_RGUI:   return "meta_r";
    // Function keys
    case SDLK_F1:  return "f1";  case SDLK_F2:  return "f2";
    case SDLK_F3:  return "f3";  case SDLK_F4:  return "f4";
    case SDLK_F5:  return "f5";  case SDLK_F6:  return "f6";
    case SDLK_F7:  return "f7";  case SDLK_F8:  return "f8";
    case SDLK_F9:  return "f9";  case SDLK_F10: return "f10";
    case SDLK_F11: return "f11"; case SDLK_F12: return "f12";
    default:        return "";
    }
}
