#include "input/input_glfw.h"
#include "engine/log.h"

#include <GLFW/glfw3.h>

namespace engine {

namespace {

// Map a GLFW key code to our Key enum.
// Returns false if the key is unmapped.
bool map_key(int glfw_key, Key& out_key) {
    switch (glfw_key) {
        // Letters
        case GLFW_KEY_A: out_key = Key::A; return true;
        case GLFW_KEY_B: out_key = Key::B; return true;
        case GLFW_KEY_C: out_key = Key::C; return true;
        case GLFW_KEY_D: out_key = Key::D; return true;
        case GLFW_KEY_E: out_key = Key::E; return true;
        case GLFW_KEY_F: out_key = Key::F; return true;
        case GLFW_KEY_G: out_key = Key::G; return true;
        case GLFW_KEY_H: out_key = Key::H; return true;
        case GLFW_KEY_I: out_key = Key::I; return true;
        case GLFW_KEY_J: out_key = Key::J; return true;
        case GLFW_KEY_K: out_key = Key::K; return true;
        case GLFW_KEY_L: out_key = Key::L; return true;
        case GLFW_KEY_M: out_key = Key::M; return true;
        case GLFW_KEY_N: out_key = Key::N; return true;
        case GLFW_KEY_O: out_key = Key::O; return true;
        case GLFW_KEY_P: out_key = Key::P; return true;
        case GLFW_KEY_Q: out_key = Key::Q; return true;
        case GLFW_KEY_R: out_key = Key::R; return true;
        case GLFW_KEY_S: out_key = Key::S; return true;
        case GLFW_KEY_T: out_key = Key::T; return true;
        case GLFW_KEY_U: out_key = Key::U; return true;
        case GLFW_KEY_V: out_key = Key::V; return true;
        case GLFW_KEY_W: out_key = Key::W; return true;
        case GLFW_KEY_X: out_key = Key::X; return true;
        case GLFW_KEY_Y: out_key = Key::Y; return true;
        case GLFW_KEY_Z: out_key = Key::Z; return true;

        // Digits
        case GLFW_KEY_0: out_key = Key::Num0; return true;
        case GLFW_KEY_1: out_key = Key::Num1; return true;
        case GLFW_KEY_2: out_key = Key::Num2; return true;
        case GLFW_KEY_3: out_key = Key::Num3; return true;
        case GLFW_KEY_4: out_key = Key::Num4; return true;
        case GLFW_KEY_5: out_key = Key::Num5; return true;
        case GLFW_KEY_6: out_key = Key::Num6; return true;
        case GLFW_KEY_7: out_key = Key::Num7; return true;
        case GLFW_KEY_8: out_key = Key::Num8; return true;
        case GLFW_KEY_9: out_key = Key::Num9; return true;

        // Function keys
        case GLFW_KEY_F1:  out_key = Key::F1;  return true;
        case GLFW_KEY_F2:  out_key = Key::F2;  return true;
        case GLFW_KEY_F3:  out_key = Key::F3;  return true;
        case GLFW_KEY_F4:  out_key = Key::F4;  return true;
        case GLFW_KEY_F5:  out_key = Key::F5;  return true;
        case GLFW_KEY_F6:  out_key = Key::F6;  return true;
        case GLFW_KEY_F7:  out_key = Key::F7;  return true;
        case GLFW_KEY_F8:  out_key = Key::F8;  return true;
        case GLFW_KEY_F9:  out_key = Key::F9;  return true;
        case GLFW_KEY_F10: out_key = Key::F10; return true;
        case GLFW_KEY_F11: out_key = Key::F11; return true;
        case GLFW_KEY_F12: out_key = Key::F12; return true;

        // Arrows
        case GLFW_KEY_LEFT:  out_key = Key::Left;  return true;
        case GLFW_KEY_RIGHT: out_key = Key::Right; return true;
        case GLFW_KEY_UP:    out_key = Key::Up;    return true;
        case GLFW_KEY_DOWN:  out_key = Key::Down;  return true;

        // Special
        case GLFW_KEY_SPACE:      out_key = Key::Space;      return true;
        case GLFW_KEY_ENTER:      out_key = Key::Enter;      return true;
        case GLFW_KEY_ESCAPE:     out_key = Key::Escape;     return true;
        case GLFW_KEY_TAB:        out_key = Key::Tab;        return true;
        case GLFW_KEY_BACKSPACE:  out_key = Key::Backspace;  return true;
        case GLFW_KEY_DELETE:     out_key = Key::Delete;     return true;
        case GLFW_KEY_INSERT:     out_key = Key::Insert;     return true;
        case GLFW_KEY_HOME:       out_key = Key::Home;       return true;
        case GLFW_KEY_END:        out_key = Key::End;        return true;
        case GLFW_KEY_PAGE_UP:    out_key = Key::PageUp;     return true;
        case GLFW_KEY_PAGE_DOWN:  out_key = Key::PageDown;   return true;

        // Modifiers
        case GLFW_KEY_LEFT_SHIFT:    out_key = Key::LShift; return true;
        case GLFW_KEY_RIGHT_SHIFT:   out_key = Key::RShift; return true;
        case GLFW_KEY_LEFT_CONTROL:  out_key = Key::LCtrl;  return true;
        case GLFW_KEY_RIGHT_CONTROL: out_key = Key::RCtrl;  return true;
        case GLFW_KEY_LEFT_ALT:      out_key = Key::LAlt;   return true;
        case GLFW_KEY_RIGHT_ALT:     out_key = Key::RAlt;   return true;

        // Punctuation
        case GLFW_KEY_MINUS:      out_key = Key::Minus;      return true;
        case GLFW_KEY_EQUAL:      out_key = Key::Equals;     return true;
        case GLFW_KEY_LEFT_BRACKET:  out_key = Key::LBracket; return true;
        case GLFW_KEY_RIGHT_BRACKET: out_key = Key::RBracket; return true;
        case GLFW_KEY_SEMICOLON:  out_key = Key::Semicolon;  return true;
        case GLFW_KEY_APOSTROPHE: out_key = Key::Apostrophe; return true;
        case GLFW_KEY_COMMA:      out_key = Key::Comma;      return true;
        case GLFW_KEY_PERIOD:     out_key = Key::Period;     return true;
        case GLFW_KEY_SLASH:      out_key = Key::Slash;      return true;
        case GLFW_KEY_BACKSLASH:  out_key = Key::Backslash;  return true;
        case GLFW_KEY_GRAVE_ACCENT: out_key = Key::Grave;    return true;

        default: return false;
    }
}

bool map_mouse_button(int glfw_button, MouseButton& out) {
    switch (glfw_button) {
        case GLFW_MOUSE_BUTTON_LEFT:   out = MouseButton::Left;   return true;
        case GLFW_MOUSE_BUTTON_RIGHT:  out = MouseButton::Right;  return true;
        case GLFW_MOUSE_BUTTON_MIDDLE: out = MouseButton::Middle; return true;
        default: return false;
    }
}

} // namespace

InputGLFW::InputGLFW(GLFWwindow* window)
    : window_(window)
{
    if (window_ == nullptr) {
        log_error("InputGLFW: null window");
        return;
    }

    // Register callbacks. The user pointer holds `this` so callbacks can
    // reach the InputState.
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, &InputGLFW::key_callback);
    glfwSetMouseButtonCallback(window_, &InputGLFW::mouse_button_callback);
    glfwSetCursorPosCallback(window_, &InputGLFW::cursor_pos_callback);
    glfwSetScrollCallback(window_, &InputGLFW::scroll_callback);

    // Initialize mouse position to the current cursor position.
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window_, &mx, &my);
    state_.set_mouse_pos(static_cast<float>(mx), static_cast<float>(my));
}

InputGLFW::~InputGLFW() {
    if (window_ != nullptr) {
        glfwSetKeyCallback(window_, nullptr);
        glfwSetMouseButtonCallback(window_, nullptr);
        glfwSetCursorPosCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
    }
}

void InputGLFW::poll() {
    // The caller calls state().begin_frame() before poll().
    glfwPollEvents();
}

// -----------------------------------------------------------------------------
// GLFW callbacks
// -----------------------------------------------------------------------------

void InputGLFW::key_callback(GLFWwindow* w, int key, int /*sc*/, int action, int /*mods*/) {
    auto* self = static_cast<InputGLFW*>(glfwGetWindowUserPointer(w));
    if (self == nullptr) return;

    Key k;
    if (!map_key(key, k)) return;

    if (action == GLFW_PRESS)   self->state_.set_key_down(k, true);
    if (action == GLFW_RELEASE) self->state_.set_key_down(k, false);
    // GLFW_REPEAT: leave the down state unchanged.
}

void InputGLFW::mouse_button_callback(GLFWwindow* w, int button, int action, int /*mods*/) {
    auto* self = static_cast<InputGLFW*>(glfwGetWindowUserPointer(w));
    if (self == nullptr) return;

    MouseButton b;
    if (!map_mouse_button(button, b)) return;

    if (action == GLFW_PRESS)   self->state_.set_mouse_button(b, true);
    if (action == GLFW_RELEASE) self->state_.set_mouse_button(b, false);
}

void InputGLFW::cursor_pos_callback(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<InputGLFW*>(glfwGetWindowUserPointer(w));
    if (self == nullptr) return;
    self->state_.set_mouse_pos(static_cast<float>(x), static_cast<float>(y));
}

void InputGLFW::scroll_callback(GLFWwindow* w, double dx, double dy) {
    auto* self = static_cast<InputGLFW*>(glfwGetWindowUserPointer(w));
    if (self == nullptr) return;
    self->state_.add_scroll(static_cast<float>(dx), static_cast<float>(dy));
}

// -----------------------------------------------------------------------------
// Factory
// -----------------------------------------------------------------------------

Input* create_input_glfw(void* native_window) {
    return new InputGLFW(static_cast<GLFWwindow*>(native_window));
}

} // namespace engine
