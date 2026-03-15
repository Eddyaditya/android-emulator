#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <cstdint>
#include <mutex>
#include <vector>

#include <SDL2/SDL.h>

// Pixel format of the data supplied to FrameBuffer::update()
enum class PixelFormat {
    ARGB8888,  // 32-bit: 0xAARRGGBB
    RGB888,    // 24-bit packed: R, G, B bytes (no alpha)
};

// FrameBuffer holds a CPU-side pixel array and an SDL_Texture that mirrors it.
// Pixel data may be updated from any thread; rendering must be done from the
// thread that owns the SDL_Renderer.
class FrameBuffer {
public:
    FrameBuffer();
    ~FrameBuffer();

    // Attach to a renderer; must be called from the render thread before any
    // calls to render().  Calling again recreates the internal texture.
    void attachRenderer(SDL_Renderer* renderer, int width, int height);

    // Thread-safe: copy pixel data into the internal buffer and mark dirty.
    // 'pitch' is the number of bytes per row (0 = width * bytes-per-pixel).
    void update(const void* data, int width, int height,
                PixelFormat format, int pitch = 0);

    // Fill the frame buffer with a colour-bar test pattern.
    void fillTestPattern();

    // Upload dirty pixel data to the SDL texture and draw it into 'dst'.
    // Must be called from the render thread.  Pass nullptr for 'dst' to
    // stretch the texture to fill the full renderer output.
    void render(SDL_Renderer* renderer, const SDL_Rect* dst = nullptr);

    int getWidth()  const { return width;  }
    int getHeight() const { return height; }

private:
    void releaseTexture();
    void uploadToTexture(SDL_Renderer* renderer);

    int width;
    int height;

    // CPU-side 32-bit ARGB pixel store
    std::vector<uint32_t> pixels;
    bool dirty;           // texture needs re-upload

    SDL_Texture* texture; // GPU texture owned by this class

    mutable std::mutex mutex;
};

#endif // FRAME_BUFFER_H
