#include "frame_buffer.h"

#include <cstring>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

FrameBuffer::FrameBuffer()
    : width(0)
    , height(0)
    , dirty(false)
    , texture(nullptr)
{}

FrameBuffer::~FrameBuffer() {
    releaseTexture();
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void FrameBuffer::attachRenderer(SDL_Renderer* renderer, int w, int h) {
    std::lock_guard<std::mutex> lock(mutex);

    releaseTexture();

    width  = w;
    height = h;
    pixels.assign(static_cast<size_t>(w * h), 0xFF000000u); // opaque black
    dirty = true;

    // SDL_PIXELFORMAT_ARGB8888 matches our uint32_t layout
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                w, h);
    if (!texture) {
        // Non-fatal: render() will just skip the blit
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                     "FrameBuffer: SDL_CreateTexture failed: %s", SDL_GetError());
    }
}

// ---------------------------------------------------------------------------
// Pixel update (thread-safe)
// ---------------------------------------------------------------------------

void FrameBuffer::update(const void* data, int w, int h,
                          PixelFormat format, int pitch) {
    if (!data || w <= 0 || h <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);

    if (w != width || h != height) {
        // Resize internal buffer; texture will be recreated on next render()
        width  = w;
        height = h;
        pixels.assign(static_cast<size_t>(w * h), 0xFF000000u);
        releaseTexture();
    }

    const int srcPitch = (pitch > 0) ? pitch : w * (format == PixelFormat::RGB888 ? 3 : 4);

    const uint8_t* src = static_cast<const uint8_t*>(data);
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = src + y * srcPitch;
        uint32_t* dst = pixels.data() + y * w;
        if (format == PixelFormat::ARGB8888) {
            std::memcpy(dst, row, static_cast<size_t>(w) * sizeof(uint32_t));
        } else {
            // RGB888 → ARGB8888
            for (int x = 0; x < w; ++x) {
                uint8_t r = row[x * 3 + 0];
                uint8_t g = row[x * 3 + 1];
                uint8_t b = row[x * 3 + 2];
                dst[x] = 0xFF000000u
                         | (static_cast<uint32_t>(r) << 16)
                         | (static_cast<uint32_t>(g) <<  8)
                         | static_cast<uint32_t>(b);
            }
        }
    }

    dirty = true;
}

// ---------------------------------------------------------------------------
// Test pattern
// ---------------------------------------------------------------------------

void FrameBuffer::fillTestPattern() {
    if (width <= 0 || height <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);

    // Classic 8-colour vertical bars
    static const uint32_t kBars[8] = {
        0xFFFFFFFF, // white
        0xFFFFFF00, // yellow
        0xFF00FFFF, // cyan
        0xFF00FF00, // green
        0xFFFF00FF, // magenta
        0xFFFF0000, // red
        0xFF0000FF, // blue
        0xFF000000, // black
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int bar = x * 8 / width;
            pixels[static_cast<size_t>(y * width + x)] = kBars[bar];
        }
    }
    dirty = true;
}

// ---------------------------------------------------------------------------
// Rendering (render thread only)
// ---------------------------------------------------------------------------

void FrameBuffer::uploadToTexture(SDL_Renderer* renderer) {
    if (!texture) {
        // Try to (re-)create the texture now that we have the renderer
        texture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    width, height);
        if (!texture) {
            return;
        }
    }

    void*  texPixels = nullptr;
    int    texPitch  = 0;
    if (SDL_LockTexture(texture, nullptr, &texPixels, &texPitch) == 0) {
        const int rowBytes = width * static_cast<int>(sizeof(uint32_t));
        if (texPitch == rowBytes) {
            std::memcpy(texPixels, pixels.data(),
                        static_cast<size_t>(rowBytes * height));
        } else {
            for (int y = 0; y < height; ++y) {
                std::memcpy(static_cast<uint8_t*>(texPixels) + y * texPitch,
                            pixels.data() + y * width,
                            static_cast<size_t>(rowBytes));
            }
        }
        SDL_UnlockTexture(texture);
    }
    dirty = false;
}

void FrameBuffer::render(SDL_Renderer* renderer, const SDL_Rect* dst) {
    std::lock_guard<std::mutex> lock(mutex);

    if (width <= 0 || height <= 0) {
        return;
    }

    if (dirty) {
        uploadToTexture(renderer);
    }

    if (texture) {
        SDL_RenderCopy(renderer, texture, nullptr, dst);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FrameBuffer::releaseTexture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}
