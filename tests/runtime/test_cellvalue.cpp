#include <catch2/catch_test_macros.hpp>
#include "lucid/cellvalue.h"

using namespace lucid;

TEST_CASE("CellValue: default is nil", "[cellvalue]") {
    CellValue v;
    REQUIRE(v.kind() == CellValue::Kind::Nil);
    REQUIRE(v.is_nil());
}

TEST_CASE("CellValue: primitive and reference values", "[cellvalue]") {
    REQUIRE(CellValue::from_bool(true).as_bool());
    REQUIRE(CellValue::from_int(42).as_int() == 42);
    REQUIRE(CellValue::from_float(3.14).as_float() == 3.14);
    REQUIRE(CellValue::from_string("hello").as_string() == "hello");
    REQUIRE(CellValue::from_row_ref(RowRef(5, 3)).as_row_ref() == RowRef(5, 3));
    REQUIRE(CellValue::from_function(FunctionHandle{7}).as_function() == FunctionHandle{7});
}

TEST_CASE("CellValue: wrong kind accessor throws", "[cellvalue]") {
    auto v = CellValue::from_int(1);
    REQUIRE_THROWS(v.as_bool());
    REQUIRE_THROWS(v.as_string());
}

TEST_CASE("CellValue: equality", "[cellvalue]") {
    REQUIRE(CellValue::from_int(1) == CellValue::from_int(1));
    REQUIRE(CellValue::from_int(1) != CellValue::from_int(2));
    REQUIRE(CellValue::from_string("a") == CellValue::from_string("a"));
    REQUIRE(CellValue::nil() == CellValue::nil());
}
