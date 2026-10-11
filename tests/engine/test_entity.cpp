#include <catch2/catch_test_macros.hpp>
#include "engine/scene/entity.h"
#include "engine/ecs/world.h"

using namespace engine;

namespace
{

    struct Fixture
    {
        engine::ecs::World world;

        Fixture()
        {
            setup_core_tables(world);
        }

        EntityRef create(const std::string &name = "")
        {
            return create_entity(world, allocate_entity_id(world), name);
        }
    };

} // namespace

TEST_CASE("entity: create and destroy", "[entity]")
{
    Fixture f;
    EntityRef e = f.create("Test");
    REQUIRE(is_alive(f.world, e));
    REQUIRE(get_entity_name(f.world, e) == "Test");

    destroy_entity(f.world, e);
    REQUIRE_FALSE(is_alive(f.world, e));
}

TEST_CASE("entity: id is allocated monotonically", "[entity]")
{
    Fixture f;
    EntityRef a = f.create();
    EntityRef b = f.create();
    EntityRef c = f.create();

    REQUIRE(get_entity_id(f.world, a) == 1);
    REQUIRE(get_entity_id(f.world, b) == 2);
    REQUIRE(get_entity_id(f.world, c) == 3);
}

TEST_CASE("entity: add and get transform", "[entity]")
{
    Fixture f;
    EntityRef e = f.create();

    auto t = add_component<TransformTag>(f.world, e);
    f.world.get_table("")->set_float32(t, "x", 100.0f);
    f.world.get_table("Engine_Transform")->set_float32(t, "y", 200.0f);

    REQUIRE(has_component<TransformTag>(f.world, e));
    REQUIRE(has_component<SpriteTag>(f.world, e) == false);

    auto t2 = get_component<TransformTag>(f.world, e);
    REQUIRE(t2 == t);
    REQUIRE(f.world.get_table("Engine_Transform")->get_float32(t2, "x") == 100.0f);
    REQUIRE(f.world.get_table("Engine_Transform")->get_float32(t2, "y") == 200.0f);
}

TEST_CASE("entity: duplicate component throws", "[entity]")
{
    Fixture f;
    EntityRef e = f.create();
    add_component<TransformTag>(f.world, e);
    REQUIRE_THROWS(add_component<TransformTag>(f.world, e));
}

TEST_CASE("entity: remove component", "[entity]")
{
    Fixture f;
    EntityRef e = f.create();
    add_component<TransformTag>(f.world, e);
    REQUIRE(has_component<TransformTag>(f.world, e));

    remove_component<TransformTag>(f.world, e);
    REQUIRE_FALSE(has_component<TransformTag>(f.world, e));
}

TEST_CASE("entity: destroying an entity removes its components", "[entity]")
{
    Fixture f;
    EntityRef e = f.create();
    add_component<TransformTag>(f.world, e);
    add_component<SpriteTag>(f.world, e);

    destroy_entity(f.world, e);

    // The component rows should be gone.
    REQUIRE_FALSE(has_component<TransformTag>(f.world, e));
    REQUIRE_FALSE(has_component<SpriteTag>(f.world, e));
}

TEST_CASE("entity: parent and child", "[entity]")
{
    Fixture f;
    EntityRef parent = f.create("Parent");
    EntityRef child = f.create("Child");

    set_parent(f.world, child, parent);

    REQUIRE(get_parent(f.world, child) == parent);
    REQUIRE(get_parent(f.world, parent).is_nil());
}

TEST_CASE("entity: two entities with the same component type", "[entity]")
{
    Fixture f;
    EntityRef a = f.create();
    EntityRef b = f.create();

    add_component<TransformTag>(f.world, a);
    add_component<TransformTag>(f.world, b);

    f.world.get_table("Engine_Transform")->set_float32(get_component<TransformTag>(f.world, a), "x", 1.0f);
    f.world.get_table("Engine_Transform")->set_float32(get_component<TransformTag>(f.world, b), "x", 2.0f);

    REQUIRE(f.world.get_table("Engine_Transform")->get_float32(get_component<TransformTag>(f.world, a), "x") == 1.0f);
    REQUIRE(f.world.get_table("Engine_Transform")->get_float32(get_component<TransformTag>(f.world, b), "x") == 2.0f);
}

TEST_CASE("entity: iterate all entities", "[entity]")
{
    Fixture f;
    for (int i = 0; i < 5; ++i)
        f.create();

    std::size_t count = 0;
    f.world.get_table("Entity")->each([&](engine::ecs::RowRef)
                                      { ++count; });
    REQUIRE(count == 5);
}
