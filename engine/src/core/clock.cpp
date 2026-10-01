#include "engine/clock.h"

#include <algorithm>

namespace engine {

Clock::Clock()
    : start_time_(ClockImpl::now())
    , last_frame_time_(start_time_)
{}

void Clock::start_frame() {
    const auto now = ClockImpl::now();

    const std::chrono::duration<double> total = now - start_time_;
    total_time_ = total.count();

    if (frame_count_ == 0) {
        raw_dt_ = 0.0f;
        dt_ = 0.0f;
        last_frame_time_ = now;
        ++frame_count_;
        return;
    }

    const std::chrono::duration<float> delta = now - last_frame_time_;
    raw_dt_ = delta.count();
    dt_ = std::min(raw_dt_, max_dt_);

    last_frame_time_ = now;
    ++frame_count_;
}

void Clock::set_max_dt(float seconds) {
    max_dt_ = seconds > 0.0f ? seconds : 0.0f;
}

void Clock::reset() {
    const auto now = ClockImpl::now();
    start_time_ = now;
    last_frame_time_ = now;
    dt_ = 0.0f;
    raw_dt_ = 0.0f;
    total_time_ = 0.0;
    frame_count_ = 0;
}

} // namespace engine
