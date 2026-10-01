#include <catch2/catch_test_macros.hpp>
#include "lucid/row_ref.h"

using namespace lucid;

TEST_CASE("RowRef: default is nil", "[row_ref]") {
    RowRef r;
    REQUIRE(r.is_nil());
    REQUIRE(r.index == 0);
}

TEST_CASE("RowRef: any index-0 ref is nil", "[row_ref]") {
    REQUIRE(RowRef(0, 42).is_nil());
}

TEST_CASE("RowRef: equality", "[row_ref]") {
    REQUIRE(RowRef(1, 1) == RowRef(1, 1));
    REQUIRE(RowRef(1, 1) != RowRef(1, 2));
    REQUIRE(RowRef(1, 1) != RowRef(2, 1));
}
