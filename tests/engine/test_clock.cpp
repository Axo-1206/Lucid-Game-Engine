#include <catch2/catch_test_macros.hpp>
#include "engine/clock.h"

#include <thread>
#include <chrono>

using namespace engine;

TEST_CASE("clock: first frame has dt == 0", "[clock]") {
    Clock c;
    c.start_frame();
    REQUIRE(c.dt() == 0.0f);
    REQUIRE(c.frame_count() == 1);
}

TEST_CASE("clock: second frame has positive dt", "[clock]") {
    Clock c;
    c.start_frame();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    c.start_frame();
    REQUIRE(c.dt() > 0.0f);
    REQUIRE(c.frame_count() == 2);
}

TEST_CASE("clock: dt is clamped to max_dt", "[clock]") {
    Clock c;
    c.set_max_dt(0.001f);  // 1 ms

    c.start_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    c.start_frame();

    REQUIRE(c.dt() <= 0.001f);
    REQUIRE(c.raw_dt() >= 0.015f);  // at least 15 ms
}

TEST_CASE("clock: reset clears state", "[clock]") {
    Clock c;
    c.start_frame();
    c.start_frame();

    REQUIRE(c.frame_count() == 2);
    REQUIRE(c.total_time() >= 0.0);

    c.reset();

    REQUIRE(c.frame_count() == 0);
    REQUIRE(c.total_time() == 0.0);
    REQUIRE(c.dt() == 0.0f);
}

TEST_CASE("clock: total_time increases", "[clock]") {
    Clock c;
    c.start_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    c.start_frame();

    REQUIRE(c.total_time() > 0.0);
}
