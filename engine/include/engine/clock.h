#pragma once

#include <chrono>
#include <cstdint>

namespace engine {

// Frame timing. Call start_frame() at the top of every frame;
// read dt() / total_time() / frame_count() after.
class Clock {
public:
    Clock();

    // Call at the start of each frame.
    void start_frame();

    // Seconds since the last start_frame().
    // Clamped to a maximum to prevent spiral-of-death after a breakpoint.
    float dt() const { return dt_; }

    // Seconds since the Clock was constructed.
    double total_time() const { return total_time_; }

    // Number of frames elapsed since construction.
    std::uint64_t frame_count() const { return frame_count_; }

private:
    using ClockImpl = std::chrono::steady_clock;

    ClockImpl::time_point start_time_;
    ClockImpl::time_point last_frame_time_;

    float dt_ = 0.0f;
    double total_time_ = 0.0;
    std::uint64_t frame_count_ = 0;

    static constexpr float kMaxDt = 0.1f;  // 100 ms
};

} // namespace engine
