#include "engine/platform/window.h"
#include "engine/core/clock.h"
#include "engine/core/log.h"
#include "engine/render/renderer.h"
#include "engine/platform/input.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/core/system_context.h"
#include "engine/systems/systems.h"
#include "engine/graph/graph_runtime.h"
#include "engine/package/package_manager.h"
#include "engine/package/core_package.h"
#include "engine/package/logic_package.h"

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
    renderer->set_viewport(1280, 720);

    std::unique_ptr<engine::Input> input(
        engine::create_input_glfw(window.native_handle()));

    engine::ecs::World world;
    engine::setup_core_tables(world);

    // Build the package manager. A real game would also load
    // game-specific packages here.
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
    camera.viewport_w = 1280.0f;
    camera.viewport_h = 720.0f;
    renderer->set_camera_2d(camera);

    engine::RenderList render_list;
    engine::SystemContext ctx{
        world,
        *renderer,
        *input,
        clock,
        render_list,
    };

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

        runtime.tick(dt);

        engine::hierarchy_system(ctx, dt);
        engine::movement_system(ctx, dt);
        engine::hierarchy_system(ctx, dt);

        renderer->begin_frame(cam_data.clear_color);
        engine::render_system(ctx, dt);
        renderer->end_frame();
        renderer->present();
    }

    runtime.clear();
    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}