#include <catch2/catch_test_macros.hpp>
#include "lucid/table.h"

using namespace lucid;

TEST_CASE("Table: add, get, and primary lookup", "[table]") {
    Table t("Person");
    t.add_column("id", ColumnType::Uint64, true);
    t.add_column("name", ColumnType::String);
    t.add_column("age", ColumnType::Int32);

    RowRef alice = t.add({CellValue::from_uint64(1), CellValue::from_string("Alice"), CellValue::from_int32(30)});
    RowRef bob = t.add({CellValue::from_uint64(2), CellValue::from_string("Bob"), CellValue::from_int32(25)});

    REQUIRE(t.count() == 2);
    REQUIRE(t.is_valid(alice));
    REQUIRE(t.get_string(alice, "name") == "Alice");
    REQUIRE(t.get_int32(bob, "age") == 25);
    REQUIRE(t.find_by_primary("id", CellValue::from_uint64(2)) == bob);
    REQUIRE(t.find_by_primary("id", CellValue::from_uint64(999)).is_nil());
}

TEST_CASE("Table: remove invalidates and cleans up references", "[table]") {
    Table t("Person");
    t.add_column("id", ColumnType::Uint64, true);
    RowRef a = t.add({CellValue::from_uint64(1)});
    RowRef b = t.add({CellValue::from_uint64(2)});
    t.remove(a);
    REQUIRE_FALSE(t.is_valid(a));
    REQUIRE(t.find_by_primary("id", CellValue::from_uint64(1)).is_nil());
    REQUIRE(t.find_by_primary("id", CellValue::from_uint64(2)) == b);
}

TEST_CASE("Table: slot reuse bumps generation", "[table]") {
    Table t("Person");
    t.add_column("id", ColumnType::Uint64, true);
    RowRef a = t.add({CellValue::from_uint64(1)});
    t.remove(a);
    RowRef b = t.add({CellValue::from_uint64(2)});
    REQUIRE(b.index == a.index);
    REQUIRE(b.generation != a.generation);
    REQUIRE_FALSE(t.is_valid(a));
    REQUIRE(t.is_valid(b));
}

TEST_CASE("Table: iteration, user index, and clear", "[table]") {
    Table t("Person");
    t.add_column("id", ColumnType::Uint64, true);
    RowRef a = t.add({CellValue::from_uint64(10)});
    RowRef b = t.add({CellValue::from_uint64(20)});
    REQUIRE(t.at(0) == a);
    REQUIRE(t.at(1) == b);
    REQUIRE(t.at(2).is_nil());

    std::size_t count = 0;
    t.each([&](RowRef) { ++count; });
    REQUIRE(count == 2);
    t.clear();
    REQUIRE_FALSE(t.is_valid(a));
    REQUIRE_FALSE(t.is_valid(b));
    REQUIRE(t.count() == 0);
}

TEST_CASE("Table: function column", "[table]") {
    Table t("Behavior");
    t.add_column("name", ColumnType::String, true);
    t.add_column("on_update", ColumnType::Function);
    RowRef row = t.add({CellValue::from_string("Idle"), CellValue::from_function(FunctionHandle{1})});
    REQUIRE(t.get_function(row, "on_update") == FunctionHandle{1});
    t.set_function(row, "on_update", FunctionHandle{2});
    REQUIRE(t.get_function(row, "on_update") == FunctionHandle{2});
}
