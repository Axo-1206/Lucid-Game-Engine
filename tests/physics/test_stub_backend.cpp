#include <catch2/catch_test_macros.hpp>

#include "stub_backend.h"
#include "engine/services.h"

using namespace engine;

TEST_CASE("stub_backend: init and shutdown", "[physics][stub]")
{
    test::StubBackend b;
    REQUIRE(b.init());
    b.shutdown();
}

TEST_CASE("stub_backend: create and step a body", "[physics][stub]")
{
    test::StubBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -10.0f);

    physics::BodyDesc desc;
    desc.type = physics::BodyType::Dynamic;
    desc.pos_x = 0.0f;
    desc.pos_y = 100.0f;

    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());
    REQUIRE(b.body_count() == 1);

    // Step for 1 second in 60 steps.
    for (int i = 0; i < 60; ++i)
        b.step(1.0f / 60.0f);

    auto state = b.get_body_state(id);
    REQUIRE(state.pos_y < 100.0f); // fell
    REQUIRE(state.vel_y < 0.0f);   // moving down
}

TEST_CASE("stub_backend: destroy a body", "[physics][stub]")
{
    test::StubBackend b;
    REQUIRE(b.init());

    auto id = b.create_body(physics::BodyDesc{});
    REQUIRE(b.body_count() == 1);

    b.destroy_body(id);
    REQUIRE(b.body_count() == 0);

    auto state = b.get_body_state(id);
    REQUIRE(state.pos_x == 0.0f);
    REQUIRE(state.pos_y == 0.0f);
}

TEST_CASE("stub_backend: set_transform zeroes velocity", "[physics][stub]")
{
    test::StubBackend b;
    REQUIRE(b.init());

    auto id = b.create_body(physics::BodyDesc{});
    b.set_body_velocity(id, 10.0f, 20.0f);
    b.set_body_transform(id, 5.0f, 7.0f, 1.5f);

    auto state = b.get_body_state(id);
    REQUIRE(state.pos_x == 5.0f);
    REQUIRE(state.pos_y == 7.0f);
    REQUIRE(state.rot_z == 1.5f);
    REQUIRE(state.vel_x == 0.0f);
    REQUIRE(state.vel_y == 0.0f);
}

TEST_CASE("services: physics backend is reachable", "[services]")
{
    test::StubBackend backend;
    REQUIRE(backend.init());

    Services services;
    services.physics = &backend;

    REQUIRE(services.physics != nullptr);
    REQUIRE(services.physics->init() == true); // stub returns true
}

TEST_CASE("services: null physics backend is allowed", "[services]")
{
    Services services;
    REQUIRE(services.physics == nullptr);
}