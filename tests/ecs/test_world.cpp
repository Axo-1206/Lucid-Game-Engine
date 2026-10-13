#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/world.h"

using namespace engine::ecs;

TEST_CASE("world: create and look up a table", "[ecs][world]")
{
    World w;
    REQUIRE_FALSE(w.has_table("Engine_Entity"));

    Table *t = w.create_table("Engine_Entity");
    REQUIRE(t != nullptr);
    REQUIRE(w.has_table("Engine_Entity"));
    REQUIRE(w.get_table("Engine_Entity") == t);
    REQUIRE(w.table_count() == 1);
}

TEST_CASE("world: creating a duplicate table throws", "[ecs][world]")
{
    World w;
    w.create_table("Engine_Entity");
    REQUIRE_THROWS(w.create_table("Engine_Entity"));
}

TEST_CASE("world: missing table returns nullptr", "[ecs][world]")
{
    World w;
    REQUIRE(w.get_table("Engine_Nope") == nullptr);
    const World &cw = w;
    REQUIRE(cw.get_table("Engine_Nope") == nullptr);
}

TEST_CASE("world: each_table visits in creation order", "[ecs][world]")
{
    World w;
    w.create_table("A");
    w.create_table("B");
    w.create_table("C");

    std::vector<std::string> names;
    w.each_table([&](Table &t)
                 { names.push_back(t.name()); });

    REQUIRE(names == std::vector<std::string>{"A", "B", "C"});
}

TEST_CASE("world: clear removes all tables", "[ecs][world]")
{
    World w;
    w.create_table("A");
    w.create_table("B");
    w.clear();

    REQUIRE(w.table_count() == 0);
    REQUIRE_FALSE(w.has_table("A"));
    REQUIRE_FALSE(w.has_table("B"));
}

TEST_CASE("world: clear_rows keeps tables but empties them", "[ecs][world]")
{
    World w;
    Table *t = w.create_table("T");
    t->add_column("x", ColumnType::Float32, /*primary=*/false);
    t->add_partial("x", CellValue::from_float(1.0));

    REQUIRE(t->count() == 1);
    w.clear_rows();
    REQUIRE(t->count() == 0);
    REQUIRE(w.has_table("T"));
    REQUIRE(t->column_count() == 1);
}

TEST_CASE("world: clear preserves marked tables", "[ecs][world]")
{
    World w;
    w.mark_preserved("Keep");
    w.create_table("Keep");
    w.create_table("Drop");

    REQUIRE(w.has_table("Keep"));
    REQUIRE(w.has_table("Drop"));
    REQUIRE(w.is_preserved("Keep"));
    REQUIRE_FALSE(w.is_preserved("Drop"));

    w.clear();

    REQUIRE(w.has_table("Keep"));
    REQUIRE_FALSE(w.has_table("Drop"));
    REQUIRE(w.table_count() == 1);
}

TEST_CASE("world: clear_rows clears marked tables too", "[ecs][world]")
{
    World w;
    w.mark_preserved("Keep");
    Table *t = w.create_table("Keep");
    t->add_column("x", ColumnType::Float32, /*primary=*/false);
    t->add_partial("x", CellValue::from_float(1.0));

    REQUIRE(t->count() == 1);

    w.clear_rows();

    REQUIRE(t->count() == 0);
    REQUIRE(w.has_table("Keep"));
    REQUIRE(t->column_count() == 1);
}

TEST_CASE("world: mark_preserved is idempotent and order-independent",
          "[ecs][world]")
{
    // Mark before create.
    {
        World w;
        w.mark_preserved("A");
        w.mark_preserved("A"); // idempotent
        w.create_table("A");
        w.clear();
        REQUIRE(w.has_table("A"));
    }

    // Mark after create.
    {
        World w;
        w.create_table("A");
        w.mark_preserved("A");
        w.clear();
        REQUIRE(w.has_table("A"));
    }

    // Mark a name that is never created: no effect, no crash.
    {
        World w;
        w.mark_preserved("Never");
        w.create_table("A");
        w.clear();
        REQUIRE_FALSE(w.has_table("A"));
        REQUIRE_FALSE(w.has_table("Never"));
    }
}

TEST_CASE("world: each_table after clear visits only survivors",
          "[ecs][world]")
{
    World w;
    w.mark_preserved("Keep");
    w.create_table("A");
    w.create_table("Keep");
    w.create_table("B");

    w.clear();

    std::vector<std::string> names;
    w.each_table([&](Table &t)
                 { names.push_back(t.name()); });

    REQUIRE(names == std::vector<std::string>{"Keep"});
}

TEST_CASE("world: clear preserves the order of surviving tables",
          "[ecs][world]")
{
    World w;
    w.mark_preserved("P1");
    w.mark_preserved("P2");
    w.create_table("A");
    w.create_table("P1");
    w.create_table("B");
    w.create_table("P2");
    w.create_table("C");

    w.clear();

    std::vector<std::string> names;
    w.each_table([&](Table &t)
                 { names.push_back(t.name()); });

    REQUIRE(names == std::vector<std::string>{"P1", "P2"});
}