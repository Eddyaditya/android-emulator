#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <functional>
#include <mutex>
#include <queue>
#include <string>

#include <SDL2/SDL.h>

// High-level input event types forwarded to the emulator/QEMU layer
enum class InputEventType {
    KeyDown,       // Key pressed
    KeyUp,         // Key released
    MouseButton,   // Mouse button press/release
    MouseMotion,   // Mouse cursor movement
    TouchBegin,    // Touch-start (simulated from mouse)
    TouchEnd,      // Touch-end   (simulated from mouse)
    TouchMove,     // Touch-move  (simulated from mouse drag)
    TextInput,     // UTF-8 text from SDL_TEXTINPUT events
    Quit,          // Window close / application exit
};

// Unified input event descriptor
struct InputEvent {
    InputEventType type;

    // Key events
    SDL_Keycode keycode;     // SDL virtual key (e.g. SDLK_a, SDLK_RETURN)
    SDL_Scancode scancode;   // Physical key position
    uint16_t     modifiers;  // SDL_Keymod bitmask (Ctrl, Shift, Alt, …)

    // Mouse / touch coordinates (pixels, relative to the SDL window)
    int x;
    int y;
    int dx;   // delta-x (MouseMotion / TouchMove)
    int dy;   // delta-y

    // Mouse button index (1=left, 2=middle, 3=right)
    uint8_t button;
    bool    buttonDown; // true = pressed, false = released

    // Text input (UTF-8, up to 32 bytes)
    char text[32];
};

// InputHandler translates SDL2 events into InputEvent records, queues them,
// and optionally forwards them via a registered callback.
//
// SDL2 *must* be polled from a single thread.  Call processSDLEvent() from
// the thread that owns the SDL event loop.  Events can be consumed from any
// thread via pollEvent() or the registered callback.
class InputHandler {
public:
    InputHandler();
    ~InputHandler() = default;

    // Register a callback invoked synchronously inside processSDLEvent()
    // for every translated input event.  Pass an empty function to clear.
    void setEventCallback(std::function<void(const InputEvent&)> cb);

    // Translate one raw SDL_Event; must be called from the event-loop thread.
    // Returns true if the event was consumed (i.e. it was input-related).
    bool processSDLEvent(const SDL_Event& sdlEvent);

    // Thread-safe: dequeue one event; returns false if the queue is empty.
    bool pollEvent(InputEvent& out);

    // Clear all queued events.
    void clearEvents();

    // Returns the number of events currently queued.
    size_t pendingCount() const;

    // Convenience: build a QEMU-compatible keyname string for a key event.
    // Returns an empty string when no mapping is available.
    static std::string qemuKeyName(SDL_Keycode keycode);

private:
    void enqueue(const InputEvent& event);

    std::queue<InputEvent>               eventQueue;
    mutable std::mutex                   queueMutex;
    std::function<void(const InputEvent&)> callback;
    bool                                 mouseButtonDown; // tracks left-button state
};

#endif // INPUT_HANDLER_H
