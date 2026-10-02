#pragma once

#include "engine/input.h"

struct GLFWwindow;

namespace engine {

// GLFW-backed input implementation.
//
// Registers GLFW callbacks that forward to the underlying InputState.
// poll() calls glfwPollEvents(), which fires those callbacks.
class InputGLFW : public Input {
public:
    explicit InputGLFW(GLFWwindow* window);
    ~InputGLFW() override;

    void poll() override;

private:
    GLFWwindow* window_ = nullptr;

    // GLFW callbacks
    static void key_callback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* w, double x, double y);
    static void scroll_callback(GLFWwindow* w, double dx, double dy);
};

} // namespace engine
