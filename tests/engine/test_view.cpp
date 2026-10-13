#include <catch2/catch_test_macros.hpp>
#include "engine/render/view.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/ecs/core/entity.h"

#include <array>

using namespace engine;

namespace
{

    struct StubRenderer : Renderer
    {
        // Recorded calls, in order.
        struct SetCamera2D
        {
            Camera2D cam;
        };
        struct SetViewport
        {
            int x, y, w, h;
        };
        struct BeginPass
        {
            const char *name;
        };
        struct EndPass
        {
            const char *name;
        };

        std::vector<Camera2D> cameras;
        std::vector<std::array<int, 4>> viewports;
        std::vector<std::string> passes;

        // Reported by set_full_viewport. Defaults to 1280x720.
        int window_w = 1280;
        int window_h = 720;

        bool init(void *) override { return true; }
        void shutdown() override {}
        void begin_frame(Color) override { passes.push_back("frame_begin"); }
        void end_frame() override { passes.push_back("frame_end"); }
        void present() override {}

        void begin_world_2d() override { passes.push_back("world_begin"); }
        void end_world_2d() override { passes.push_back("world_end"); }
        void begin_ui() override { passes.push_back("ui_begin"); }
        void end_ui() override { passes.push_back("ui_end"); }

        void set_viewport(int x, int y, int w, int h) override
        {
            viewports.push_back({x, y, w, h});
        }
        void set_full_viewport() override
        {
            viewports.push_back({0, 0, window_w, window_h});
        }
        int viewport_x() const override { return 0; }
        int viewport_y() const override { return 0; }
        int viewport_width() const override { return 0; }
        int viewport_height() const override { return 0; }

        void set_camera_2d(const Camera2D &camera) override
        {
            cameras.push_back(camera);
        }
        Camera2D get_camera_2d() const override { return {}; }

        TextureHandle load_texture(const std::string &) override { return {}; }
        void free_texture(TextureHandle) override {}
        TextureHandle white_texture() const override { return {}; }

        void draw_rect(float, float, float, float, Color, int) override {}
        void draw_rect_outline(float, float, float, float, float, Color, int) override {}
        void draw_circle(float, float, float, Color, int, int) override {}
        void draw_line(float, float, float, float, float, Color, int) override {}
        void draw_sprite(TextureHandle, float, float, float, float, float, Color, float, float, int) override {}

        RenderStats stats() const override { return {}; }
        void reset_stats() override {}
    };

    struct StubInput : Input
    {
        void poll() override {}
    };

} // namespace

TEST_CASE("view: make_world_2d_view reads the camera by name", "[view]")
{
    engine::ecs::World world;
    setup_core_tables(world);

    // Write a camera row.
    CameraDesc desc;
    desc.name = "main";
    desc.pos_x = 100.0f;
    desc.pos_y = 200.0f;
    desc.ortho_size = 2.0f;
    write_camera_row(world, desc);

    View v = make_world_2d_view(world, "main", 0, 0, 800, 600);
    REQUIRE(v.world == &world);
    REQUIRE(v.camera_name == "main");
    REQUIRE(v.camera.name == "main");
    REQUIRE(v.camera.pos_x == 100.0f);
    REQUIRE(v.pass == View::Pass::World2D);
}

TEST_CASE("view: make_world_2d_view with an unknown name leaves defaults", "[view]")
{
    engine::ecs::World world;
    setup_core_tables(world);

    View v = make_world_2d_view(world, "nope", 0, 0, 800, 600);
    REQUIRE(v.camera_name == "nope");
    REQUIRE(v.camera.name.empty()); // default CameraDesc has an empty name
}

TEST_CASE("view: render_view sets the viewport and the camera", "[view]")
{
    engine::ecs::World world;
    setup_core_tables(world);

    CameraDesc desc;
    desc.name = "main";
    desc.pos_x = 10.0f;
    desc.pos_y = 20.0f;
    desc.ortho_size = 2.0f;
    write_camera_row(world, desc);

    View v = make_world_2d_view(world, "main", 100, 50, 800, 600);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    render_view(v, ctx, 0.0f, r);

    REQUIRE(r.viewports.size() == 2);
    REQUIRE(r.viewports[0] == std::array<int, 4>{100, 50, 800, 600});
    REQUIRE(r.viewports[1] == std::array<int, 4>{0, 0, r.window_w, r.window_h});
    REQUIRE(r.cameras.size() == 1);
    REQUIRE(r.cameras[0].x == 10.0f);
    REQUIRE(r.cameras[0].y == 20.0f);
    REQUIRE(r.cameras[0].zoom == 0.5f);
    REQUIRE(r.cameras[0].viewport_w == 800.0f);
    REQUIRE(r.cameras[0].viewport_h == 600.0f);

    // The pass names appear in order.
    // world_begin ... world_end
    auto find = [&](const char *name)
    {
        return std::find(r.passes.begin(), r.passes.end(), name) != r.passes.end();
    };
    REQUIRE(find("world_begin"));
    REQUIRE(find("world_end"));
}

TEST_CASE("view: two World2D views set two viewports and two cameras", "[view]")
{
    engine::ecs::World world;
    setup_core_tables(world);

    CameraDesc left;
    left.name = "left";
    left.pos_x = 0.0f;
    write_camera_row(world, left);

    CameraDesc right;
    right.name = "right";
    right.pos_x = 100.0f;
    write_camera_row(world, right);

    View v1 = make_world_2d_view(world, "left", 0, 0, 640, 720);
    View v2 = make_world_2d_view(world, "right", 640, 0, 640, 720);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    render_view(v1, ctx, 0.0f, r);
    render_view(v2, ctx, 0.0f, r);

    // Two camera sets.
    REQUIRE(r.cameras.size() == 2);
    REQUIRE(r.cameras[0].x == 0.0f);
    REQUIRE(r.cameras[1].x == 100.0f);

    // Each view: one set_viewport, then one set_full_viewport
    // (the restore). Four calls in order.
    REQUIRE(r.viewports.size() == 4);
    REQUIRE(r.viewports[0] == std::array<int, 4>{0, 0, 640, 720});
    REQUIRE(r.viewports[1] == std::array<int, 4>{0, 0, 1280, 720}); // restore
    REQUIRE(r.viewports[2] == std::array<int, 4>{640, 0, 640, 720});
    REQUIRE(r.viewports[3] == std::array<int, 4>{0, 0, 1280, 720}); // restore
}

TEST_CASE("view: a UI view enters the UI pass", "[view]")
{
    engine::ecs::World world;
    setup_core_tables(world);

    View v = make_ui_view(&world, 0, 0, 1280, 720);

    StubRenderer r;
    StubInput i;
    Clock c;
    RenderList rl;
    Services services;
    SystemContext ctx{world, r, i, c, rl, services};

    render_view(v, ctx, 0.0f, r);

    auto find = [&](const char *name)
    {
        return std::find(r.passes.begin(), r.passes.end(), name) != r.passes.end();
    };
    REQUIRE(find("ui_begin"));
    REQUIRE(find("ui_end"));
    REQUIRE_FALSE(find("world_begin"));
}