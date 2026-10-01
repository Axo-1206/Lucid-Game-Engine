#pragma once

#include <string>

namespace engine {

// A minimal window abstraction.
// Phase 0: GLFW-backed. Later: a swappable IWindowProvider.
class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Returns true if the window should close (X button or Escape).
    bool should_close() const;

    // Request the window to close.
    void request_close();

    // Process OS events. Call once per frame, before reading input.
    void poll_events();

    // Swap the front and back buffers. Call once per frame, after drawing.
    void swap_buffers();

    int width() const  { return width_; }
    int height() const { return height_; }

    // The underlying GLFWwindow*, for backends that need it.
    void* native_handle() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace engine
