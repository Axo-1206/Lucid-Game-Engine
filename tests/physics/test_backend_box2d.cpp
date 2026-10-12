#include <catch2/catch_test_macros.hpp>

#include "engine/physics/backend_box2d/backend_box2d.h"

#include <cmath>

using namespace engine;

namespace
{

    // Step the backend N times at 1/60 s.
    void step_n(physics::Backend &b, int n)
    {
        for (int i = 0; i < n; ++i)
            b.step(1.0f / 60.0f);
    }

} // namespace

TEST_CASE("box2d_backend: init and shutdown", "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.shutdown();
    // Idempotent shutdown: calling again is a no-op.
    b.shutdown();
}

TEST_CASE("box2d_backend: gravity is settable", "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -9.81f);
    // No assertion on the internal state; the API has no getter
    // for gravity. This test verifies the call does not crash.
    b.shutdown();
}

TEST_CASE("box2d_backend: a dynamic body falls under gravity",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -10.0f);

    physics::BodyDesc desc;
    desc.type = physics::BodyType::Dynamic;
    desc.shape = physics::ShapeType::Box;
    desc.size_x = 0.5f;
    desc.size_y = 0.5f;
    desc.mass = 1.0f;
    desc.pos_x = 0.0f;
    desc.pos_y = 10.0f;

    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());

    const auto initial = b.get_body_state(id);
    REQUIRE(initial.pos_y == 10.0f);

    // One second of simulation.
    step_n(b, 60);

    const auto final_state = b.get_body_state(id);
    REQUIRE(final_state.pos_y < initial.pos_y);
    REQUIRE(final_state.vel_y < 0.0f);

    b.shutdown();
}

TEST_CASE("box2d_backend: a static body does not move",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -10.0f);

    physics::BodyDesc desc;
    desc.type = physics::BodyType::Static;
    desc.shape = physics::ShapeType::Box;
    desc.size_x = 5.0f;
    desc.size_y = 0.5f;
    desc.pos_x = 0.0f;
    desc.pos_y = 0.0f;

    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());

    step_n(b, 60);

    const auto state = b.get_body_state(id);
    REQUIRE(state.pos_x == 0.0f);
    REQUIRE(state.pos_y == 0.0f);
    REQUIRE(state.vel_x == 0.0f);
    REQUIRE(state.vel_y == 0.0f);

    b.shutdown();
}

TEST_CASE("box2d_backend: a falling body collides with a static floor",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -10.0f);

    // Static floor.
    physics::BodyDesc floor;
    floor.type = physics::BodyType::Static;
    floor.shape = physics::ShapeType::Box;
    floor.size_x = 5.0f;
    floor.size_y = 0.5f;
    floor.pos_x = 0.0f;
    floor.pos_y = 0.0f;
    auto floor_id = b.create_body(floor);
    REQUIRE(floor_id.is_valid());

    // Falling box.
    physics::BodyDesc box;
    box.type = physics::BodyType::Dynamic;
    box.shape = physics::ShapeType::Box;
    box.size_x = 0.5f;
    box.size_y = 0.5f;
    box.mass = 1.0f;
    box.pos_x = 0.0f;
    box.pos_y = 5.0f;
    auto box_id = b.create_body(box);
    REQUIRE(box_id.is_valid());

    // Simulate for two seconds. The box should fall and land on
    // the floor. The floor top is at y = 0.5. The box bottom
    // starts at y - size_y = 4.5. When it comes to rest, its
    // center is around y = 1.0 (0.5 floor top + 0.5 box half-extent).
    step_n(b, 120);

    const auto state = b.get_body_state(box_id);
    // The box should be resting, not falling through the floor.
    REQUIRE(state.pos_y > 0.5f);
    REQUIRE(state.pos_y < 2.0f);
    // Velocity should be near zero (resting).
    REQUIRE(std::abs(state.vel_y) < 1.0f);

    b.shutdown();
}

TEST_CASE("box2d_backend: set_body_transform teleports",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());

    physics::BodyDesc desc;
    desc.type = physics::BodyType::Dynamic;
    desc.pos_x = 0.0f;
    desc.pos_y = 0.0f;

    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());

    b.set_body_transform(id, 5.0f, 7.0f, 1.5f);

    const auto state = b.get_body_state(id);
    REQUIRE(state.pos_x == 5.0f);
    REQUIRE(state.pos_y == 7.0f);
    REQUIRE(std::abs(state.rot_z - 1.5f) < 0.01f);
    REQUIRE(state.vel_x == 0.0f);
    REQUIRE(state.vel_y == 0.0f);

    b.shutdown();
}

TEST_CASE("box2d_backend: apply_impulse changes velocity",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, 0.0f); // disable gravity for this test

    physics::BodyDesc desc;
    desc.type = physics::BodyType::Dynamic;
    desc.mass = 1.0f;
    desc.size_x = 0.5f;
    desc.size_y = 0.5f;
    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());

    // 1 kg body, impulse of 2 N*s -> 2 m/s.
    b.apply_impulse(id, 2.0f, 0.0f);

    // Give Box2D a step to integrate the impulse.
    b.step(1.0f / 60.0f);

    const auto state = b.get_body_state(id);
    REQUIRE(state.vel_x > 1.5f); // near 2.0; allow for solver damping

    b.shutdown();
}

TEST_CASE("box2d_backend: destroy_body removes the body",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());

    physics::BodyDesc desc;
    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());

    b.destroy_body(id);

    // Reading the destroyed body returns a default-constructed
    // state (all zeros).
    const auto state = b.get_body_state(id);
    REQUIRE(state.pos_x == 0.0f);
    REQUIRE(state.pos_y == 0.0f);

    b.shutdown();
}

TEST_CASE("box2d_backend: contacts report a collision",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -10.0f);

    physics::BodyDesc floor;
    floor.type = physics::BodyType::Static;
    floor.shape = physics::ShapeType::Box;
    floor.size_x = 5.0f;
    floor.size_y = 0.5f;
    floor.pos_x = 0.0f;
    floor.pos_y = 0.0f;
    auto floor_id = b.create_body(floor);

    physics::BodyDesc box;
    box.type = physics::BodyType::Dynamic;
    box.shape = physics::ShapeType::Box;
    box.size_x = 0.5f;
    box.size_y = 0.5f;
    box.mass = 1.0f;
    box.pos_x = 0.0f;
    box.pos_y = 3.0f;
    auto box_id = b.create_body(box);

    // Step until contact. The box falls 2.5 units to touch the
    // floor; with g=-10, that takes about 0.7 s.
    bool contact_seen = false;
    for (int i = 0; i < 120 && !contact_seen; ++i)
    {
        b.step(1.0f / 60.0f);
        const auto &cs = b.contacts();
        for (const auto &c : cs)
        {
            const bool involves_floor = (c.a == floor_id) || (c.b == floor_id);
            const bool involves_box = (c.a == box_id) || (c.b == box_id);
            if (involves_floor && involves_box)
            {
                contact_seen = true;
                break;
            }
        }
    }
    REQUIRE(contact_seen);

    b.shutdown();
}

TEST_CASE("box2d_backend: a circle body falls",
          "[physics][box2d]")
{
    physics::Box2DBackend b;
    REQUIRE(b.init());
    b.set_gravity(0.0f, -10.0f);

    physics::BodyDesc desc;
    desc.type = physics::BodyType::Dynamic;
    desc.shape = physics::ShapeType::Circle;
    desc.size_x = 0.5f; // radius
    desc.mass = 1.0f;
    desc.pos_x = 0.0f;
    desc.pos_y = 10.0f;

    auto id = b.create_body(desc);
    REQUIRE(id.is_valid());

    step_n(b, 60);

    const auto state = b.get_body_state(id);
    REQUIRE(state.pos_y < 10.0f);

    b.shutdown();
}