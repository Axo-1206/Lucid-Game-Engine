#include "engine/clock.h"

#include <algorithm>

namespace engine {

Clock::Clock()
    : start_time_(ClockImpl::now())
    , last_frame_time_(start_time_)
{}

void Clock::start_frame() {
    const auto now = ClockImpl::now();

    const std::chrono::duration<float> delta = now - last_frame_time_;
    dt_ = std::min(delta.count(), kMaxDt);

    const std::chrono::duration<double> total = now - start_time_;
    total_time_ = total.count();

    last_frame_time_ = now;
    ++frame_count_;
}

} // namespace engine
