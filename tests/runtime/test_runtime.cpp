#include <catch2/catch_test_macros.hpp>
#include "lucid/runtime.h"

using namespace lucid;

TEST_CASE("Runtime: create and get table", "[runtime]")
{
    Runtime world;
    Table *table = world.create_table("Person");
    REQUIRE(table != nullptr);
    REQUIRE(world.has_table("Person"));
    REQUIRE(world.get_table("Person") == table);
    REQUIRE(world.get_table("Nonexistent") == nullptr);
}

TEST_CASE("Runtime: duplicate table throws", "[runtime]")
{
    Runtime world;
    world.create_table("Person");
    REQUIRE_THROWS(world.create_table("Person"));
}

TEST_CASE("Runtime: host functions", "[runtime]")
{
    Runtime world;
    world.register_host_fn("add", [](const std::vector<CellValue> &args)
                           { return CellValue::from_int(args[0].as_int() + args[1].as_int()); });
    REQUIRE(world.has_host_fn("add"));
    REQUIRE(world.call_host_fn("add", {CellValue::from_int(2), CellValue::from_int(3)}).as_int() == 5);
}

TEST_CASE("Runtime: each_table iterates in creation order", "[runtime]")
{
    Runtime world;
    world.create_table("A");
    world.create_table("B");
    world.create_table("C");
    std::vector<std::string> names;
    world.each_table([&](Table &table)
                     { names.push_back(table.name()); });
    REQUIRE(names == std::vector<std::string>{"A", "B", "C"});
}
