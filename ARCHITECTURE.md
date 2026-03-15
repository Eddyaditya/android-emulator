# EDU Emulator Architecture

The EDU Emulator is designed to provide a lightweight and efficient emulation of the Android operating system. Its architecture is composed of several key components that work together seamlessly to provide a comprehensive emulation experience.

---

## 1. QEMU Integration
The core of the EDU Emulator is built upon QEMU (Quick Emulator), an open-source machine emulator and virtualizer. QEMU performs hardware emulation and allows the emulator to run on various architectures. It provides the following functionalities:
- **CPU emulation:** QEMU can emulate different CPU architectures, allowing the emulator to run on both x86 and ARM hardware.
- **Device Models:** QEMU provides models for various devices (e.g., network adapters, storage devices) that interact with the Android guest system.
- **Performance Optimization:** By including hardware virtualization support, QEMU enhances performance, especially for ARM images running on x86 hosts.

Relevant source files:
- `src/qemu_config.h / .cpp` – Centralised QEMU configuration (image path, memory, CPU count, acceleration flags).
- `src/qemu_manager.h / .cpp` – QEMU process lifecycle: spawn, pause, resume, stop; hardware-acceleration auto-detection (KVM / HAXM / TCG); boot-status tracking; output-pipe logging.

---

## 2. Android System
The emulated Android system runs in a virtualized environment, with the following aspects:
- **Android Runtime (ART):** The emulator runs the Android Runtime, which executes Android applications.
- **System Services:** Various system services such as Activity Manager, Package Manager, and Window Manager are emulated to provide a full Android experience.
- **APK Support:** Users can install and run APK files, allowing for testing applications as they would on a physical device.

---

## 3. SDL2 UI Layer

The UI layer uses **SDL2** (Simple DirectMedia Layer 2) to provide a native display window and low-latency input processing.

### Architecture overview

```
┌─────────────────────────────────────────────┐
│                 Main Thread                 │
│  Emulator::initialize() → start() → run()  │
│           UIManager::processEvents()        │
└───────────────────┬─────────────────────────┘
                    │ owns
        ┌───────────▼───────────┐
        │      UIManager        │
        │  (src/ui_manager.h)   │
        └──┬────────────────────┘
           │ owns
    ┌──────▼──────────────────────────────────┐
    │             UIWindow                    │
    │         (src/ui_window.h)               │
    │                                         │
    │  SDL2 Window + Renderer                 │
    │  Render thread (std::thread)            │
    │  ┌───────────────┐  ┌────────────────┐  │
    │  │  FrameBuffer  │  │  InputHandler  │  │
    │  │(frame_buffer) │  │(input_handler) │  │
    │  └───────────────┘  └────────────────┘  │
    └─────────────────────────────────────────┘
```

### Component descriptions

#### `UIManager` (`src/ui_manager.h / .cpp`)
Top-level coordinator. Owns the `UIWindow` and exposes a high-level API to the rest of the emulator:
- `initSDL()` – Initialise SDL2 and open the display window.
- `startSDL()` – Launch the background render thread.
- `processEvents()` – Called from the main loop to check whether the window is still open.
- `showBootStatus()` / `showError()` – Update the window title bar and print to the console.
- `getFrameBuffer()` / `getInputHandler()` – Access the shared subsystems.

#### `UIWindow` (`src/ui_window.h / .cpp`)
Owns the `SDL_Window` and `SDL_Renderer`. Runs an event-poll + render loop in a **dedicated background thread** (via `std::thread`) so that the QEMU process and the main application thread are never blocked:
- Default resolution: **1080 × 1920** (Android portrait); automatically scales down when the desktop is smaller.
- `SDL_RenderSetLogicalSize` preserves aspect ratio on resize.
- **Fullscreen toggle:** press **F11** at runtime, or call `setFullscreen(true)` programmatically.
- Window title bar displays the current boot status and — in debug mode — a live FPS counter.
- Target frame rate: **30+ FPS** (capped with `SDL_Delay`; vsync enabled when the hardware supports it).

#### `FrameBuffer` (`src/frame_buffer.h / .cpp`)
CPU-side pixel buffer with an SDL2 streaming texture mirror:
- Thread-safe `update()` accepts 24-bit (RGB888) or 32-bit (ARGB8888) pixel data from any thread (e.g. a future QEMU VNC/SPICE reader).
- `render()` uploads dirty pixels to the GPU texture and blits it to the renderer; must be called from the render thread.
- `fillTestPattern()` populates the buffer with a colour-bar pattern (useful during boot before QEMU output is available).

#### `InputHandler` (`src/input_handler.h / .cpp`)
Translates raw SDL2 events into typed `InputEvent` records:
- **Keyboard events** (keydown / keyup) with full modifier-key tracking (Ctrl, Shift, Alt, Cmd/Super).
- **Mouse events** (button press/release, motion) forwarded directly.
- **Touch simulation:** left-button drag events are automatically translated into `TouchBegin` / `TouchMove` / `TouchEnd` events that map to Android touch-screen input.
- **Text input** events for typed characters (UTF-8).
- Thread-safe `pollEvent()` queue; optional synchronous callback via `setEventCallback()`.
- `qemuKeyName()` converts SDL keycodes to QEMU monitor `sendkey` names.

### Threading model

| Thread | Responsibility |
|--------|----------------|
| **Main thread** | `Emulator::run()` main loop; calls `UIManager::processEvents()` to check window liveness |
| **Render thread** (inside `UIWindow`) | SDL2 event polling (`SDL_PollEvent`), frame buffer upload, renderer draw calls, FPS tracking |
| **QEMU process** | Separate OS process; output piped back to the parent via an fd |

The `FrameBuffer` mutex ensures pixel data can be pushed from any future background thread (e.g. a VNC frame decoder) without race conditions.

---

## 4. Root Access Implementation
Root access is a key feature for developers and testers:
- **Root Environment:** The emulator supports a root access environment, enabling users to perform operations that require elevated privileges.
- **Superuser Access:** The emulator includes a superuser application that manages root permissions for apps, providing flexibility in testing various scenarios.

---

## Conclusion
The EDU Emulator's architecture is designed to provide a robust and flexible platform for Android application development and testing. With its integration of QEMU for hardware emulation, an SDL2 UI layer for real-time display and input, and a fully functional Android system, it offers a comprehensive solution for Android developers.

This document will continue to evolve as new features and enhancements are integrated into the EDU Emulator architecture.