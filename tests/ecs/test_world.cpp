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