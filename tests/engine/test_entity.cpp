#include <catch2/catch_test_macros.hpp>
#include "engine/entity.h"
#include "lucid/runtime.h"

using namespace engine;

namespace {

struct Fixture {
    lucid::Runtime rt;

    Fixture() {
        setup_core_tables(rt);
    }

    EntityRef create(const std::string& name = "") {
        return create_entity(rt, allocate_entity_id(rt), name);
    }
};

} // namespace

TEST_CASE("entity: create and destroy", "[entity]") {
    Fixture f;
    EntityRef e = f.create("Test");
    REQUIRE(is_alive(f.rt, e));
    REQUIRE(get_entity_name(f.rt, e) == "Test");

    destroy_entity(f.rt, e);
    REQUIRE_FALSE(is_alive(f.rt, e));
}

TEST_CASE("entity: id is allocated monotonically", "[entity]") {
    Fixture f;
    EntityRef a = f.create();
    EntityRef b = f.create();
    EntityRef c = f.create();

    REQUIRE(get_entity_id(f.rt, a) == 1);
    REQUIRE(get_entity_id(f.rt, b) == 2);
    REQUIRE(get_entity_id(f.rt, c) == 3);
}

TEST_CASE("entity: add and get transform", "[entity]") {
    Fixture f;
    EntityRef e = f.create();

    auto t = add_component<TransformTag>(f.rt, e);
    f.rt.get_table("Transform")->set_float32(t, "x", 100.0f);
    f.rt.get_table("Transform")->set_float32(t, "y", 200.0f);

    REQUIRE(has_component<TransformTag>(f.rt, e));
    REQUIRE(has_component<SpriteTag>(f.rt, e) == false);

    auto t2 = get_component<TransformTag>(f.rt, e);
    REQUIRE(t2 == t);
    REQUIRE(f.rt.get_table("Transform")->get_float32(t2, "x") == 100.0f);
    REQUIRE(f.rt.get_table("Transform")->get_float32(t2, "y") == 200.0f);
}

TEST_CASE("entity: duplicate component throws", "[entity]") {
    Fixture f;
    EntityRef e = f.create();
    add_component<TransformTag>(f.rt, e);
    REQUIRE_THROWS(add_component<TransformTag>(f.rt, e));
}

TEST_CASE("entity: remove component", "[entity]") {
    Fixture f;
    EntityRef e = f.create();
    add_component<TransformTag>(f.rt, e);
    REQUIRE(has_component<TransformTag>(f.rt, e));

    remove_component<TransformTag>(f.rt, e);
    REQUIRE_FALSE(has_component<TransformTag>(f.rt, e));
}

TEST_CASE("entity: destroying an entity removes its components", "[entity]") {
    Fixture f;
    EntityRef e = f.create();
    add_component<TransformTag>(f.rt, e);
    add_component<SpriteTag>(f.rt, e);
    add_component<ScriptTag>(f.rt, e);

    destroy_entity(f.rt, e);

    // The component rows should be gone.
    REQUIRE_FALSE(has_component<TransformTag>(f.rt, e));
    REQUIRE_FALSE(has_component<SpriteTag>(f.rt, e));
    REQUIRE_FALSE(has_component<ScriptTag>(f.rt, e));
}

TEST_CASE("entity: parent and child", "[entity]") {
    Fixture f;
    EntityRef parent = f.create("Parent");
    EntityRef child  = f.create("Child");

    set_parent(f.rt, child, parent);

    REQUIRE(get_parent(f.rt, child) == parent);
    REQUIRE(get_parent(f.rt, parent).is_nil());
}

TEST_CASE("entity: two entities with the same component type", "[entity]") {
    Fixture f;
    EntityRef a = f.create();
    EntityRef b = f.create();

    add_component<TransformTag>(f.rt, a);
    add_component<TransformTag>(f.rt, b);

    f.rt.get_table("Transform")->set_float32(get_component<TransformTag>(f.rt, a), "x", 1.0f);
    f.rt.get_table("Transform")->set_float32(get_component<TransformTag>(f.rt, b), "x", 2.0f);

    REQUIRE(f.rt.get_table("Transform")->get_float32(get_component<TransformTag>(f.rt, a), "x") == 1.0f);
    REQUIRE(f.rt.get_table("Transform")->get_float32(get_component<TransformTag>(f.rt, b), "x") == 2.0f);
}

TEST_CASE("entity: iterate all entities", "[entity]") {
    Fixture f;
    for (int i = 0; i < 5; ++i) f.create();

    std::size_t count = 0;
    f.rt.get_table("Entity")->each([&](lucid::RowRef) { ++count; });
    REQUIRE(count == 5);
}
