#pragma once

#include <chrono>
#include <cstdint>

namespace engine {

// Frame timing.
//
// Usage:
//     Clock clock;
//     while (running) {
//         clock.start_frame();
//         float dt = clock.dt();
//         // ...
//     }
//
// The first frame's dt is 0. Subsequent frames report the time since the
// previous start_frame().
//
// dt() is clamped to max_dt() to prevent spiral-of-death after a
// breakpoint or a long stall. raw_dt() returns the unclamped value.
class Clock {
public:
    Clock();

    // Call at the start of each frame.
    void start_frame();

    // Seconds since the previous start_frame(), clamped to max_dt().
    float dt() const { return dt_; }

    // Seconds since the previous start_frame(), unclamped.
    float raw_dt() const { return raw_dt_; }

    // Seconds since construction.
    double total_time() const { return total_time_; }

    // Frames since construction.
    std::uint64_t frame_count() const { return frame_count_; }

    // Maximum dt. Default 0.1 (100 ms).
    float max_dt() const { return max_dt_; }
    void  set_max_dt(float seconds);

    // Reset all state. total_time and frame_count become 0.
    void reset();

private:
    using ClockImpl = std::chrono::steady_clock;

    ClockImpl::time_point start_time_;
    ClockImpl::time_point last_frame_time_;

    float         dt_          = 0.0f;
    float         raw_dt_      = 0.0f;
    double        total_time_  = 0.0;
    std::uint64_t frame_count_ = 0;
    float         max_dt_      = 0.1f;
};

} // namespace engine
