#include <catch2/catch_test_macros.hpp>

#include "stub_renderer.h"

#include "engine/systems/systems.h"
#include "engine/systems/system_context.h"
#include "engine/render/render_list.h"
#include "engine/render/view.h"
#include "engine/ecs/world.h"
#include "engine/ecs/core/entity.h"

using namespace engine;

namespace
{

    // Build the Engine_UIElement table the way UIPackage does.
    void create_ui_table(engine::ecs::World &world)
    {
        auto *t = world.create_table("Engine_UIElement");
        t->add_column("x", engine::ecs::ColumnType::Float32);
        t->add_column("y", engine::ecs::ColumnType::Float32);
        t->add_column("w", engine::ecs::ColumnType::Float32);
        t->add_column("h", engine::ecs::ColumnType::Float32);
        t->add_column("anchor", engine::ecs::ColumnType::Int32);
        t->add_column("layer", engine::ecs::ColumnType::Int32);
        t->add_column("texture_path", engine::ecs::ColumnType::String);
        t->add_column("tint_r", engine::ecs::ColumnType::Float32);
        t->add_column("tint_g", engine::ecs::ColumnType::Float32);
        t->add_column("tint_b", engine::ecs::ColumnType::Float32);
        t->add_column("tint_a", engine::ecs::ColumnType::Float32);
        t->add_column("visible", engine::ecs::ColumnType::Bool);
    }

    engine::ecs::RowRef add_ui_rect(engine::ecs::World &world,
                                    float x, float y, float w, float h,
                                    int anchor, int layer, bool visible)
    {
        auto *t = world.get_table("Engine_UIElement");
        auto r = t->add_partial("x", engine::ecs::CellValue::from_float(x));
        t->set_float32(r, "y", y);
        t->set_float32(r, "w", w);
        t->set_float32(r, "h", h);
        t->set_int32(r, "anchor", anchor);
        t->set_int32(r, "layer", layer);
        t->set_string(r, "texture_path", "");
        t->set_float32(r, "tint_r", 1.0f);
        t->set_float32(r, "tint_g", 1.0f);
        t->set_float32(r, "tint_b", 1.0f);
        t->set_float32(r, "tint_a", 1.0f);
        t->set_bool(r, "visible", visible);
        return r;
    }

} // namespace

TEST_CASE("ui_system: two rects are submitted in layer order", "[ui]")
{
    engine::ecs::World world;
    setup_core_tables(world);
    create_ui_table(world);

    add_ui_rect(world, 0.0f, 0.0f, 100.0f, 50.0f, /*anchor=*/0, /*layer=*/10, true);
    add_ui_rect(world, 0.0f, 0.0f, 100.0f, 50.0f, /*anchor=*/0, /*layer=*/5, true);

    engine::tests::StubRenderer r;
    engine::tests::StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    View ui = make_ui_view(&world, 0, 0, 800, 600);
    ctx.current_view = &ui;

    ui_system(ctx, 0.0f);

    REQUIRE(rl.size() == 2);
    const auto &items = rl.items();
    REQUIRE(items[0].layer == 5);
    REQUIRE(items[1].layer == 10);
}

TEST_CASE("ui_system: an invisible row is not submitted", "[ui]")
{
    engine::ecs::World world;
    setup_core_tables(world);
    create_ui_table(world);

    add_ui_rect(world, 0.0f, 0.0f, 100.0f, 50.0f, 0, 0, /*visible=*/false);

    engine::tests::StubRenderer r;
    engine::tests::StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    View ui = make_ui_view(&world, 0, 0, 800, 600);
    ctx.current_view = &ui;

    ui_system(ctx, 0.0f);

    REQUIRE(rl.size() == 0);
}

TEST_CASE("ui_system: top-left anchor places the rect at (x, y)", "[ui]")
{
    engine::ecs::World world;
    setup_core_tables(world);
    create_ui_table(world);

    add_ui_rect(world, 10.0f, 20.0f, 100.0f, 50.0f, /*anchor=*/0, 0, true);

    engine::tests::StubRenderer r;
    engine::tests::StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    View ui = make_ui_view(&world, 0, 0, 800, 600);
    ctx.current_view = &ui;

    ui_system(ctx, 0.0f);

    REQUIRE(rl.size() == 1);
    const auto &item = rl.items()[0];
    REQUIRE(item.x == 10.0f);
    REQUIRE(item.y == 20.0f);
    REQUIRE(item.w == 100.0f);
    REQUIRE(item.h == 50.0f);
}

TEST_CASE("ui_system: bottom-right anchor places the rect relative to the corner", "[ui]")
{
    engine::ecs::World world;
    setup_core_tables(world);
    create_ui_table(world);

    // View is 800x600. Bottom-right anchor, offset (-10, -20).
    add_ui_rect(world, -10.0f, -20.0f, 100.0f, 50.0f, /*anchor=*/8, 0, true);

    engine::tests::StubRenderer r;
    engine::tests::StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    View ui = make_ui_view(&world, 0, 0, 800, 600);
    ctx.current_view = &ui;

    ui_system(ctx, 0.0f);

    REQUIRE(rl.size() == 1);
    const auto &item = rl.items()[0];
    REQUIRE(item.x == 790.0f); // 800 + (-10)
    REQUIRE(item.y == 580.0f); // 600 + (-20)
}

TEST_CASE("ui_system: centered anchor places the rect at the view's center", "[ui]")
{
    engine::ecs::World world;
    setup_core_tables(world);
    create_ui_table(world);

    add_ui_rect(world, 0.0f, 0.0f, 100.0f, 50.0f, /*anchor=*/4, 0, true);

    engine::tests::StubRenderer r;
    engine::tests::StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    View ui = make_ui_view(&world, 0, 0, 800, 600);
    ctx.current_view = &ui;

    ui_system(ctx, 0.0f);

    REQUIRE(rl.size() == 1);
    const auto &item = rl.items()[0];
    REQUIRE(item.x == 400.0f);
    REQUIRE(item.y == 300.0f);
}