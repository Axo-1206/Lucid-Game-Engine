#include "engine/window.h"
#include "engine/log.h"

#include <GLFW/glfw3.h>

namespace engine {

namespace {
bool g_glfw_initialized = false;

void glfw_error_callback(int code, const char* description) {
    log_error("GLFW error %d: %s", code, description);
}

bool ensure_glfw_initialized() {
    if (g_glfw_initialized) return true;

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        log_error("Failed to initialize GLFW");
        return false;
    }

    g_glfw_initialized = true;
    return true;
}

void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}
} // namespace

struct Window::Impl {
    GLFWwindow* handle = nullptr;
};

Window::Window(int width, int height, const std::string& title)
    : width_(width), height_(height)
{
    if (!ensure_glfw_initialized()) return;

    // Phase 0: no graphics API context yet. Just a window.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    impl_ = new Impl();
    impl_->handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!impl_->handle) {
        log_error("Failed to create GLFW window");
        delete impl_;
        impl_ = nullptr;
        return;
    }

    glfwSetWindowUserPointer(impl_->handle, this);
    glfwSetKeyCallback(impl_->handle, key_callback);

    log_info("Window created: %dx%d \"%s\"", width, height, title.c_str());
}

Window::~Window() {
    if (impl_) {
        if (impl_->handle) {
            glfwDestroyWindow(impl_->handle);
        }
        delete impl_;
    }
}

bool Window::should_close() const {
    if (!impl_ || !impl_->handle) return true;
    return glfwWindowShouldClose(impl_->handle) != 0;
}

void Window::request_close() {
    if (impl_ && impl_->handle) {
        glfwSetWindowShouldClose(impl_->handle, GLFW_TRUE);
    }
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::swap_buffers() {
    if (impl_ && impl_->handle) {
        glfwSwapBuffers(impl_->handle);
    }
}

void* Window::native_handle() const {
    return impl_ ? impl_->handle : nullptr;
}

} // namespace engine
