#include "engine/input.h"
#include <catch2/catch_test_macros.hpp>

using namespace engine;

TEST_CASE("input: default state is all up", "[input]") {
  InputState s;
  REQUIRE_FALSE(s.key_down(Key::A));
  REQUIRE_FALSE(s.key_pressed(Key::A));
  REQUIRE_FALSE(s.key_released(Key::A));
  REQUIRE_FALSE(s.mouse_down(MouseButton::Left));
}

TEST_CASE("input: key_down reflects set_key_down", "[input]") {
  InputState s;
  s.set_key_down(Key::A, true);
  REQUIRE(s.key_down(Key::A));
  s.set_key_down(Key::A, false);
  REQUIRE_FALSE(s.key_down(Key::A));
}

TEST_CASE("input: key_pressed fires on transition up -> down", "[input]") {
  InputState s;

  // Frame 0: nothing pressed.
  s.begin_frame();
  REQUIRE_FALSE(s.key_pressed(Key::A));

  // Between frames: key goes down.
  s.set_key_down(Key::A, true);

  // Frame 1: A is now pressed, and A is down.
  s.begin_frame();
  REQUIRE(s.key_pressed(Key::A));
  REQUIRE(s.key_down(Key::A));
  REQUIRE_FALSE(s.key_released(Key::A));

  // Frame 2: A is still held. Not pressed this frame.
  s.begin_frame();
  REQUIRE_FALSE(s.key_pressed(Key::A));
  REQUIRE(s.key_down(Key::A));

  // Key released.
  s.set_key_down(Key::A, false);

  // Frame 3: A is released.
  s.begin_frame();
  REQUIRE(s.key_released(Key::A));
  REQUIRE_FALSE(s.key_down(Key::A));
  REQUIRE_FALSE(s.key_pressed(Key::A));
}

TEST_CASE("input: mouse_dx/dy are deltas between frames", "[input]") {
  InputState s;

  s.set_mouse_pos(100.0f, 200.0f);
  s.begin_frame();

  // First frame: dx/dy are 0 (no previous).
  // (Actually the prev is set to the current in begin_frame, so dx is 0.)
  REQUIRE(s.mouse_dx() == 0.0f);
  REQUIRE(s.mouse_dy() == 0.0f);

  s.set_mouse_pos(150.0f, 250.0f);
  // s.begin_frame();

  REQUIRE(s.mouse_dx() == 50.0f);
  REQUIRE(s.mouse_dy() == 50.0f);
}

TEST_CASE("input: scroll accumulates within a frame then resets", "[input]") {
  InputState s;

  s.begin_frame();
  s.add_scroll(1.0f, 2.0f);
  s.add_scroll(0.5f, 1.5f);

  REQUIRE(s.scroll_dx() == 1.5f);
  REQUIRE(s.scroll_dy() == 3.5f);

  s.begin_frame();
  REQUIRE(s.scroll_dx() == 0.0f);
  REQUIRE(s.scroll_dy() == 0.0f);
}
