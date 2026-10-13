/// @file apps/game/main.cpp

#include "engine/platform/window.h"
#include "engine/core/clock.h"
#include "engine/core/log.h"
#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include "engine/platform/input.h"
#include "engine/ecs/core/entity.h"
#include "engine/scene/scene.h"
#include "engine/systems/system_context.h"
#include "engine/systems/systems.h"
#include "engine/graph/graph_runtime.h"
#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/services.h"

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

    engine::Scene scene = engine::SceneLoader_load("assets/scenes/demo.lscene");
    if (scene.json_text().empty())
    {
        engine::log_error("Failed to load scene");
        return 1;
    }
    scene.apply_to(world, packages.component_registry(), engine::ApplyMode::Clear);

    // Services. No physics backend yet.
    engine::Services services;

    engine::graph::GraphRuntime runtime(world, packages, services);
    runtime.set_asset_root(std::filesystem::current_path());

    // Load the graphs referenced by the scene's Engine_Script table.
    // This runs after scene.apply_to so the Script rows exist. On a
    // scene reload, call scene.apply_to and then runtime.load() again;
    // load() is idempotent.
    if (!runtime.load())
    {
        engine::log_warn("Some graphs failed to load; continuing without them.");
    }

    // Camera bridge: read the active camera row, convert to Camera2D,
    // hand it to the renderer. The World never talks to the renderer;
    // the renderer never reads the World. The loop is the bridge.
    // See docs/Decision.md, Decision 3.
    engine::CameraDesc cam_desc;
    engine::Color clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    {
        const auto cam_ref =
            engine::find_camera(world, scene.active_camera_name());
        if (!cam_ref.is_nil())
        {
            cam_desc = engine::read_camera_row(world, cam_ref);
            renderer->set_camera_2d(
                engine::to_camera2d(cam_desc, window.width(), window.height()));
            clear_color = engine::Color{
                cam_desc.clear_r, cam_desc.clear_g, cam_desc.clear_b, cam_desc.clear_a};
        }
    }

    engine::RenderList render_list;
    engine::SystemContext ctx{
        world,
        *renderer,
        *input,
        clock,
        render_list,
        services,
    };

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

        if (window.width() != last_w || window.height() != last_h)
        {
            last_w = window.width();
            last_h = window.height();
            renderer->set_full_viewport();
        }

        runtime.tick(dt);

        renderer->begin_frame(clear_color);

        renderer->begin_world_2d();
        engine::hierarchy_system(ctx, dt);
        engine::movement_system(ctx, dt);
        engine::hierarchy_system(ctx, dt);
        engine::render_system(ctx, dt);
        renderer->end_world_2d();

        renderer->begin_ui();
        renderer->end_ui();

        renderer->end_frame();
        renderer->present();
    }

    runtime.clear();
    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}