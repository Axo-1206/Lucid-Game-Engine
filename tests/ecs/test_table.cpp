#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/world.h"

using namespace engine::ecs;

TEST_CASE("table: add_partial into a fresh slot starts at default",
          "[ecs][table]")
{
    World w;
    Table *t = w.create_table("T");
    t->add_column("entity", ColumnType::RowRef, /*primary=*/true);
    t->add_column("x", ColumnType::Float32);
    t->add_column("name", ColumnType::String);
    t->add_column("alive", ColumnType::Bool);

    auto r = t->add_partial("entity", CellValue::from_row_ref(RowRef{1, 1}));

    REQUIRE(t->get_float32(r, "x") == 0.0f);
    REQUIRE(t->get_string(r, "name") == "");
    REQUIRE(t->get_bool(r, "alive") == false);
}

TEST_CASE("table: add_partial into a reused slot starts at default",
          "[ecs][table]")
{
    World w;
    Table *t = w.create_table("T");
    t->add_column("entity", ColumnType::RowRef, /*primary=*/true);
    t->add_column("x", ColumnType::Float32);
    t->add_column("name", ColumnType::String);

    auto r1 = t->add_partial("entity", CellValue::from_row_ref(RowRef{1, 1}));
    t->set_float32(r1, "x", 99.0f);
    t->set_string(r1, "name", "old");

    t->remove(r1);

    // The slot is reused. The new row must not see the old values.
    auto r2 = t->add_partial("entity", CellValue::from_row_ref(RowRef{2, 1}));

    REQUIRE(t->get_float32(r2, "x") == 0.0f);
    REQUIRE(t->get_string(r2, "name") == "");
    REQUIRE(t->get_row_ref(r2, "entity") == RowRef(2, 1));
}

TEST_CASE("table: add with all values is unaffected by the reset",
          "[ecs][table]")
{
    // add() overwrites every column, so the reset in add_partial is
    // not needed there. Verify that a reused slot via add() gets the
    // new values, not the old.
    World w;
    Table *t = w.create_table("T");
    t->add_column("entity", ColumnType::RowRef, /*primary=*/true);
    t->add_column("x", ColumnType::Float32);

    auto r1 = t->add({CellValue::from_row_ref(RowRef{1, 1}),
                      CellValue::from_float(99.0f)});
    t->remove(r1);

    auto r2 = t->add({CellValue::from_row_ref(RowRef{2, 1}),
                      CellValue::from_float(1.0f)});

    REQUIRE(t->get_float32(r2, "x") == 1.0f);
    REQUIRE(t->get_row_ref(r2, "entity") == RowRef(2, 1));
}