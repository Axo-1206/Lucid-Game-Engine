#include "engine/input.h"

namespace engine {

namespace {

inline std::size_t key_index(Key k) {
    return static_cast<std::size_t>(k);
}

inline std::size_t button_index(MouseButton b) {
    return static_cast<std::size_t>(b);
}

} // namespace

InputState::InputState() = default;

// -----------------------------------------------------------------------------
// Keyboard
// -----------------------------------------------------------------------------

bool InputState::key_down(Key k) const {
    return keys_down_.test(key_index(k));
}

bool InputState::key_pressed(Key k) const {
    return keys_pressed_.test(key_index(k));
}

bool InputState::key_released(Key k) const {
    return keys_released_.test(key_index(k));
}

// -----------------------------------------------------------------------------
// Mouse
// -----------------------------------------------------------------------------

bool InputState::mouse_down(MouseButton b) const {
    return mouse_down_.test(button_index(b));
}

bool InputState::mouse_pressed(MouseButton b) const {
    return mouse_pressed_.test(button_index(b));
}

bool InputState::mouse_released(MouseButton b) const {
    return mouse_released_.test(button_index(b));
}

// -----------------------------------------------------------------------------
// Backend updates
// -----------------------------------------------------------------------------

void InputState::set_key_down(Key k, bool down) {
    keys_down_.set(key_index(k), down);
}

void InputState::set_mouse_button(MouseButton b, bool down) {
    mouse_down_.set(button_index(b), down);
}

void InputState::set_mouse_pos(float x, float y) {
    mouse_x_ = x;
    mouse_y_ = y;
}

void InputState::add_scroll(float dx, float dy) {
    scroll_x_ += dx;
    scroll_y_ += dy;
}

// -----------------------------------------------------------------------------
// Frame boundary
// -----------------------------------------------------------------------------

void InputState::begin_frame() {
    // Compute pressed/released from the previous frame's down state.
    for (std::size_t i = 0; i < kKeyCount; ++i) {
        const bool now  = keys_down_.test(i);
        const bool prev = keys_prev_down_.test(i);
        keys_pressed_.set(i, now && !prev);
        keys_released_.set(i, !now && prev);
    }

    for (std::size_t i = 0; i < kMouseButtonCount; ++i) {
        const bool now  = mouse_down_.test(i);
        const bool prev = mouse_prev_down_.test(i);
        mouse_pressed_.set(i, now && !prev);
        mouse_released_.set(i, !now && prev);
    }

    // Capture this frame's state for the next frame's comparison.
    keys_prev_down_  = keys_down_;
    mouse_prev_down_ = mouse_down_;

    // Capture mouse position for delta computation.
    mouse_prev_x_ = mouse_x_;
    mouse_prev_y_ = mouse_y_;

    // Scroll is a per-frame accumulator; reset it.
    scroll_x_ = 0.0f;
    scroll_y_ = 0.0f;
}

} // namespace engine
