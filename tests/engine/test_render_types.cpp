#include <catch2/catch_test_macros.hpp>

#include "engine/render/color.h"
#include "engine/render/render_types.h"
#include "engine/math/math.h"

TEST_CASE("color: components default to opaque white", "[render][color]")
{
    engine::Color c;
    REQUIRE(c.r == 1.0f);
    REQUIRE(c.g == 1.0f);
    REQUIRE(c.b == 1.0f);
    REQUIRE(c.a == 1.0f);
    REQUIRE(engine::Color::clear().a == 0.0f);
}

TEST_CASE("vector2: components default to zero", "[math][vector2]")
{
    engine::math::Vector2 v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
}