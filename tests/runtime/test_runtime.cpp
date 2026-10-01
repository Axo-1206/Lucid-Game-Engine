#include <catch2/catch_test_macros.hpp>
#include "lucid/runtime.h"

using namespace lucid;

TEST_CASE("Runtime: create and get table", "[runtime]") {
    Runtime rt;
    Table* table = rt.create_table("Person");
    REQUIRE(table != nullptr);
    REQUIRE(rt.has_table("Person"));
    REQUIRE(rt.get_table("Person") == table);
    REQUIRE(rt.get_table("Nonexistent") == nullptr);
}

TEST_CASE("Runtime: duplicate table throws", "[runtime]") {
    Runtime rt;
    rt.create_table("Person");
    REQUIRE_THROWS(rt.create_table("Person"));
}

TEST_CASE("Runtime: host functions", "[runtime]") {
    Runtime rt;
    rt.register_host_fn("add", [](const std::vector<CellValue>& args) {
        return CellValue::from_int(args[0].as_int() + args[1].as_int());
    });
    REQUIRE(rt.has_host_fn("add"));
    REQUIRE(rt.call_host_fn("add", {CellValue::from_int(2), CellValue::from_int(3)}).as_int() == 5);
}

TEST_CASE("Runtime: each_table iterates in creation order", "[runtime]") {
    Runtime rt;
    rt.create_table("A");
    rt.create_table("B");
    rt.create_table("C");
    std::vector<std::string> names;
    rt.each_table([&](Table& table) { names.push_back(table.name()); });
    REQUIRE(names == std::vector<std::string>{"A", "B", "C"});
}
