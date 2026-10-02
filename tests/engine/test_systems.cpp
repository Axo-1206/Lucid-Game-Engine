#include <catch2/catch_test_macros.hpp>
#include "engine/entity.h"
#include "engine/systems.h"
#include "engine/system_context.h"
#include "engine/render_list.h"
#include "lucid/runtime.h"

using namespace engine;

namespace
{

    struct StubRenderer : Renderer
    {
        int rect_count = 0;
        int sprite_count = 0;

        bool init(void *) override { return true; }
        void shutdown() override {}
        void begin_frame(Color) override {}
        void end_frame() override {}
        void present() override {}
        void set_camera_2d(const Camera2D &) override {}
        Camera2D get_camera_2d() const override { return {}; }
        void set_viewport(int, int) override {}
        int viewport_width() const override { return 0; }
        int viewport_height() const override { return 0; }
        TextureHandle load_texture(const std::string &) override { return {}; }
        void free_texture(TextureHandle) override {}
        TextureHandle white_texture() const override { return {}; }
        void draw_rect(float, float, float, float, Color, int) override { ++rect_count; }
        void draw_rect_outline(float, float, float, float, float, Color, int) override {}
        void draw_circle(float, float, float, Color, int, int) override {}
        void draw_line(float, float, float, float, float, Color, int) override {}
        void draw_sprite(TextureHandle, float, float, float, float, float, Color, float, float, int) override { ++sprite_count; }
        RenderStats stats() const override { return {}; }
        void reset_stats() override {}
    };

    struct StubInput : Input
    {
        void poll() override {}
    };

} // namespace

TEST_CASE("systems: hierarchy computes world transform for root", "[systems]")
{
    lucid::Runtime rt;
    setup_core_tables(rt);

    EntityRef e = create_entity(rt, 1, "E");
    auto t = add_component<TransformTag>(rt, e);
    rt.get_table("Transform")->set_float32(t, "x", 100.0f);
    rt.get_table("Transform")->set_float32(t, "y", 200.0f);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    SystemContext ctx{rt, r, i, c, rl};

    hierarchy_system(ctx, 0.0f);

    auto w = get_component<WorldTransformTag>(rt, e);
    REQUIRE_FALSE(w.is_nil());
    REQUIRE(rt.get_table("WorldTransform")->get_float32(w, "x") == 100.0f);
    REQUIRE(rt.get_table("WorldTransform")->get_float32(w, "y") == 200.0f);
}

TEST_CASE("systems: hierarchy composes parent transforms", "[systems]")
{
    lucid::Runtime rt;
    setup_core_tables(rt);

    EntityRef parent = create_entity(rt, 1, "P");
    auto tp = add_component<TransformTag>(rt, parent);
    rt.get_table("Transform")->set_float32(tp, "x", 100.0f);
    rt.get_table("Transform")->set_float32(tp, "y", 200.0f);

    EntityRef child = create_entity(rt, 2, "C");
    auto tc = add_component<TransformTag>(rt, child);
    rt.get_table("Transform")->set_float32(tc, "x", 10.0f);
    rt.get_table("Transform")->set_float32(tc, "y", 20.0f);
    set_parent(rt, child, parent);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    SystemContext ctx{rt, r, i, c, rl};

    hierarchy_system(ctx, 0.0f);

    auto wp = get_component<WorldTransformTag>(rt, parent);
    auto wc = get_component<WorldTransformTag>(rt, child);

    REQUIRE(rt.get_table("WorldTransform")->get_float32(wp, "x") == 100.0f);
    REQUIRE(rt.get_table("WorldTransform")->get_float32(wc, "x") == 110.0f);
    REQUIRE(rt.get_table("WorldTransform")->get_float32(wc, "y") == 220.0f);
}

TEST_CASE("systems: movement applies velocity to transform", "[systems]")
{
    lucid::Runtime rt;
    setup_core_tables(rt);

    EntityRef e = create_entity(rt, 1, "E");
    auto t = add_component<TransformTag>(rt, e);
    rt.get_table("Transform")->set_float32(t, "x", 0.0f);
    rt.get_table("Transform")->set_float32(t, "y", 0.0f);

    auto v = add_component<VelocityTag>(rt, e);
    rt.get_table("Velocity")->set_float32(v, "dx", 100.0f);
    rt.get_table("Velocity")->set_float32(v, "dy", 50.0f);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    SystemContext ctx{rt, r, i, c, rl};

    movement_system(ctx, 1.0f);

    REQUIRE(rt.get_table("Transform")->get_float32(t, "x") == 100.0f);
    REQUIRE(rt.get_table("Transform")->get_float32(t, "y") == 50.0f);
}

TEST_CASE("systems: render submits draw calls", "[systems]")
{
    lucid::Runtime rt;
    setup_core_tables(rt);

    EntityRef e = create_entity(rt, 1, "E");
    auto t = add_component<TransformTag>(rt, e);
    rt.get_table("Transform")->set_float32(t, "x", 10.0f);
    auto s = add_component<SpriteTag>(rt, e);
    rt.get_table("Sprite")->set_float32(s, "w", 32.0f);
    rt.get_table("Sprite")->set_float32(s, "h", 16.0f);
    rt.get_table("Sprite")->set_float32(s, "tint_r", 1.0f);
    rt.get_table("Sprite")->set_float32(s, "tint_g", 1.0f);
    rt.get_table("Sprite")->set_float32(s, "tint_b", 1.0f);
    rt.get_table("Sprite")->set_float32(s, "tint_a", 1.0f);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    SystemContext ctx{rt, r, i, c, rl};

    hierarchy_system(ctx, 0.0f);
    render_system(ctx, 0.0f);

    REQUIRE(r.rect_count == 1);
    REQUIRE(rl.size() == 1);
}
