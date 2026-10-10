#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace engine {

// -----------------------------------------------------------------------------
// Key and mouse button enums
// -----------------------------------------------------------------------------

enum class Key : std::uint8_t {
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Digits (top row)
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    // Arrow keys
    Left, Right, Up, Down,
    // Common special keys
    Space, Enter, Escape, Tab, Backspace, Delete,
    Insert, Home, End, PageUp, PageDown,
    // Modifiers
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,
    // Punctuation
    Minus, Equals, LBracket, RBracket, Semicolon, Apostrophe,
    Comma, Period, Slash, Backslash, Grave,
    // Sentinel — must be last
    COUNT,
};

enum class MouseButton : std::uint8_t {
    Left, Right, Middle,
    // Sentinel — must be last
    COUNT,
};

constexpr std::size_t kKeyCount        = static_cast<std::size_t>(Key::COUNT);
constexpr std::size_t kMouseButtonCount = static_cast<std::size_t>(MouseButton::COUNT);

// -----------------------------------------------------------------------------
// InputState
// -----------------------------------------------------------------------------
//
// Snapshot of the input devices for the current frame.
//
// "down"     — the key/button is currently held.
// "pressed"  — the key/button transitioned from up to down this frame.
// "released" — the key/button transitioned from down to up this frame.
//
// mouse_dx / mouse_dy are the mouse movement since the previous frame.
// scroll_dx / scroll_dy are the scroll delta this frame.
//
// The backend updates the state via the set_* methods. begin_frame() must be
// called at the start of each frame, before polling events.
class InputState {
public:
    InputState();

    // --- Keyboard ---
    bool key_down(Key k)     const;
    bool key_pressed(Key k)  const;
    bool key_released(Key k) const;

    // --- Mouse ---
    bool  mouse_down(MouseButton b)     const;
    bool  mouse_pressed(MouseButton b)  const;
    bool  mouse_released(MouseButton b) const;
    float mouse_x()  const { return mouse_x_; }
    float mouse_y()  const { return mouse_y_; }
    float mouse_dx() const { return mouse_x_ - mouse_prev_x_; }
    float mouse_dy() const { return mouse_y_ - mouse_prev_y_; }
    float scroll_dx() const { return scroll_x_; }
    float scroll_dy() const { return scroll_y_; }

    // --- Backend updates (called by the input backend) ---
    void set_key_down(Key k, bool down);
    void set_mouse_button(MouseButton b, bool down);
    void set_mouse_pos(float x, float y);
    void add_scroll(float dx, float dy);

    // Called at the start of each frame. Captures the previous frame's state
    // and clears per-frame deltas.
    void begin_frame();

private:
    std::bitset<kKeyCount> keys_down_;
    std::bitset<kKeyCount> keys_prev_down_;
    std::bitset<kKeyCount> keys_pressed_;
    std::bitset<kKeyCount> keys_released_;

    std::bitset<kMouseButtonCount> mouse_down_;
    std::bitset<kMouseButtonCount> mouse_prev_down_;
    std::bitset<kMouseButtonCount> mouse_pressed_;
    std::bitset<kMouseButtonCount> mouse_released_;

    float mouse_x_      = 0.0f;
    float mouse_y_      = 0.0f;
    float mouse_prev_x_ = 0.0f;
    float mouse_prev_y_ = 0.0f;
    float scroll_x_     = 0.0f;
    float scroll_y_     = 0.0f;
};

// -----------------------------------------------------------------------------
// Input backend interface
// -----------------------------------------------------------------------------
//
// The backend owns the platform-specific event handling. poll() processes
// pending OS events and updates the associated InputState.
//
// begin_frame() on the state should be called before poll() so that events
// fired during poll() set the "pressed" / "released" flags correctly.
class Input {
public:
    virtual ~Input() = default;

    // Process pending OS events. Updates state().
    virtual void poll() = 0;

    InputState&       state()       { return state_; }
    const InputState& state() const { return state_; }

protected:
    InputState state_;
};

// Factory: creates a GLFW-backed input. Takes a GLFWwindow* (as void*).
// The Input does not own the window.
Input* create_input_glfw(void* native_window);

} // namespace engine
