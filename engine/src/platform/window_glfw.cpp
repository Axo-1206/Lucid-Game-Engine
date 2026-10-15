#include "engine/platform/window.h"
#include "engine/core/log.h"

#include <glad/glad.h> // for gladLoadGLLoader
#include <GLFW/glfw3.h>

namespace engine
{

    namespace
    {
        bool g_glfw_initialized = false;

        void glfw_error_callback(int code, const char *description)
        {
            log_error("GLFW error %d: %s", code, description);
        }

        bool ensure_glfw_initialized()
        {
            if (g_glfw_initialized)
                return true;

            glfwSetErrorCallback(glfw_error_callback);

            if (!glfwInit())
            {
                log_error("Failed to initialize GLFW");
                return false;
            }

            g_glfw_initialized = true;
            return true;
        }
    } // namespace

    struct Window::Impl
    {
        GLFWwindow *handle = nullptr;
    };

    Window::Window(int width, int height, const std::string &title)
        : width_(width), height_(height)
    {
        if (!ensure_glfw_initialized())
            return;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

        impl_ = new Impl();
        impl_->handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        if (!impl_->handle)
        {
            log_error("Failed to create GLFW window");
            delete impl_;
            impl_ = nullptr;
            return;
        }

        // NEW: bind the GL context to this thread.
        glfwMakeContextCurrent(impl_->handle);

        // NEW: load GL function pointers. After this, any OpenGL call
        // from any consumer of this Window is safe.
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            log_error("Window: failed to load OpenGL function pointers");
            glfwDestroyWindow(impl_->handle);
            delete impl_;
            impl_ = nullptr;
            return;
        }

        log_info("Window created: %dx%d \"%s\"", width, height, title.c_str());
        log_info("OpenGL %s", glGetString(GL_VERSION)); // NEW: useful for logs
    }

    Window::~Window()
    {
        if (impl_)
        {
            if (impl_->handle)
                glfwDestroyWindow(impl_->handle);
            delete impl_;
        }
    }

    bool Window::should_close() const
    {
        if (!impl_ || !impl_->handle)
            return true;
        return glfwWindowShouldClose(impl_->handle) != 0;
    }

    void Window::request_close()
    {
        if (impl_ && impl_->handle)
            glfwSetWindowShouldClose(impl_->handle, GLFW_TRUE);
    }

    void Window::poll_events()
    {
        // REMOVED? See Part 5. For now, keep the body.
        glfwPollEvents();
    }

    void Window::swap_buffers()
    {
        if (impl_ && impl_->handle)
            glfwSwapBuffers(impl_->handle);
    }

    void *Window::native_handle() const
    {
        return impl_ ? impl_->handle : nullptr;
    }

} // namespace engine