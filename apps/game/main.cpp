#include "engine/platform/window.h"
#include "engine/core/clock.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/platform/input.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/systems/system_context.h"
#include "engine/systems/systems.h"
#include "engine/graph/graph_runtime.h"
#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"

#include <filesystem>
#include <memory>

int main()
{
    engine::install_default_log_sink();
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info("Lucid Game starting");

    engine::Window window(1280, 720, "Lucid Game");
    engine::Clock clock;

    std::unique_ptr<engine::Renderer> renderer(engine::create_renderer_gl());
    if (!renderer->init(window.native_handle()))
    {
        engine::log_error("Failed to initialize renderer");
        return 1;
    }

    std::unique_ptr<engine::Input> input(
        engine::create_input_glfw(window.native_handle()));

    engine::ecs::World world;
    engine::setup_core_tables(world);

    engine::PackageManager packages;
    packages.load_package(std::make_unique<engine::CorePackage>());
    packages.load_package(std::make_unique<engine::LogicPackage>());
    if (!packages.apply(world))
    {
        engine::log_error("Failed to apply packages");
        return 1;
    }

    engine::Scene scene(world);
    if (!scene.load_from_file("assets/scenes/demo.lscene"))
    {
        engine::log_error("Failed to load scene");
        return 1;
    }

    engine::graph::GraphRuntime runtime(world, packages);
    runtime.set_asset_root(std::filesystem::current_path());
    if (!runtime.load())
    {
        engine::log_warn("Some graphs failed to load; continuing without them.");
    }

    engine::CameraData cam_data = scene.get_active_camera_data();
    engine::Camera2D camera;
    camera.x = cam_data.pos_x;
    camera.y = cam_data.pos_y;
    camera.zoom = 1.0f / cam_data.ortho_size;
    camera.viewport_w = static_cast<float>(window.width());
    camera.viewport_h = static_cast<float>(window.height());
    renderer->set_camera_2d(camera);

    engine::RenderList render_list;
    engine::SystemContext ctx{
        world,
        *renderer,
        *input,
        clock,
        render_list,
    };

    // Track the window size so we can reset the viewport on resize.
    int last_w = window.width();
    int last_h = window.height();

    while (!window.should_close())
    {
        clock.start_frame();
        const float dt = clock.dt();

        input->state().begin_frame();
        input->poll();

        if (input->state().key_pressed(engine::Key::Escape))
        {
            window.request_close();
        }

        // Reset the viewport if the window resized.
        if (window.width() != last_w || window.height() != last_h)
        {
            last_w = window.width();
            last_h = window.height();
            renderer->set_full_viewport();
        }

        runtime.tick(dt);

        renderer->begin_frame(cam_data.clear_color);

        renderer->begin_world_2d();
        engine::hierarchy_system(ctx, dt);
        engine::movement_system(ctx, dt);
        engine::hierarchy_system(ctx, dt);
        engine::render_system(ctx, dt); // fills and submits inside
        renderer->end_world_2d();

        renderer->begin_ui();
        // No UI yet. This is where a HUD would draw.
        renderer->end_ui();

        renderer->end_frame();
        renderer->present();
    }

    runtime.clear();
    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}