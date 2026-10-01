#include <catch2/catch_test_macros.hpp>
#include "lucid/column.h"

using namespace lucid;

TEST_CASE("Column: typed access", "[column]") {
    Column col("age", ColumnType::Int32);
    col.ensure_size(3);
    col.set_int32(0, 10);
    col.set_int32(1, 20);
    col.set_int32(2, 30);
    REQUIRE(col.get_int32(0) == 10);
    REQUIRE(col.get_int32(1) == 20);
    REQUIRE(col.get_int32(2) == 30);
}

TEST_CASE("Column: primary index", "[column]") {
    Column col("id", ColumnType::Uint64);
    col.make_primary();
    col.ensure_size(3);
    col.set_uint64(0, 100);
    col.set_uint64(1, 200);
    col.set_uint64(2, 300);
    REQUIRE(col.find_primary_uint64(100) == 0);
    REQUIRE(col.find_primary_uint64(200) == 1);
    REQUIRE(col.find_primary_uint64(300) == 2);
    REQUIRE(col.find_primary_uint64(999) == std::nullopt);
}

TEST_CASE("Column: duplicate primary throws", "[column]") {
    Column col("id", ColumnType::Uint64);
    col.make_primary();
    col.ensure_size(2);
    col.set_uint64(0, 100);
    REQUIRE_THROWS(col.set_uint64(1, 100));
}

TEST_CASE("Column: primary no-op and row cleanup", "[column]") {
    Column col("id", ColumnType::Uint64);
    col.make_primary();
    col.ensure_size(3);
    col.set_uint64(0, 100);
    col.set_uint64(1, 200);
    col.set_uint64(2, 300);
    REQUIRE_NOTHROW(col.set_uint64(0, 100));
    col.remove_index_for_row(1);
    REQUIRE(col.find_primary_uint64(100) == 0);
    REQUIRE(col.find_primary_uint64(200) == std::nullopt);
    REQUIRE(col.find_primary_uint64(300) == 2);
}

TEST_CASE("Column: function and wrong type access", "[column]") {
    Column col("on_update", ColumnType::Function);
    col.ensure_size(2);
    col.set_function(0, FunctionHandle{10});
    col.set_function(1, FunctionHandle{20});
    REQUIRE(col.get_function(0) == FunctionHandle{10});
    REQUIRE(col.get_function(1) == FunctionHandle{20});

    Column int_col("x", ColumnType::Int32);
    int_col.ensure_size(1);
    REQUIRE_THROWS(int_col.get_float32(0));
    REQUIRE_THROWS(int_col.get_string(0));
}
