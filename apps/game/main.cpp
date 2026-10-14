/// @file apps/game/main.cpp

#include "engine/core/clock.h"
#include "engine/core/log.h"

#include "engine/render/camera.h"
#include "engine/render/renderer.h"
#include "engine/render/view.h"

#include "engine/platform/window.h"
#include "engine/platform/input.h"

#include "engine/ecs/core/entity.h"

#include "engine/systems/system_context.h"
#include "engine/systems/systems.h"

#include "engine/graph/graph_runtime.h"
#include "engine/scene/scene.h"
#include "engine/services.h"

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/registry/packages/ui_package.h"
#include "engine/registry/packages/math_package.h"

#include <vector>
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
    packages.load_package(std::make_unique<engine::MathPackage>());
    packages.load_package(std::make_unique<engine::UIPackage>());
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

    // A demo UI element: a red rectangle in the top-left corner.
    if (auto *ui_tbl = world.get_table("Engine_UIElement"))
    {
        auto r = ui_tbl->add_partial("x", engine::ecs::CellValue::from_float(10.0f));
        ui_tbl->set_float32(r, "y", 10.0f);
        ui_tbl->set_float32(r, "w", 200.0f);
        ui_tbl->set_float32(r, "h", 40.0f);
        ui_tbl->set_int32(r, "anchor", 0); // top-left
        ui_tbl->set_int32(r, "layer", 0);
        ui_tbl->set_string(r, "texture_path", "");
        ui_tbl->set_float32(r, "tint_r", 0.8f);
        ui_tbl->set_float32(r, "tint_g", 0.1f);
        ui_tbl->set_float32(r, "tint_b", 0.1f);
        ui_tbl->set_float32(r, "tint_a", 1.0f);
        ui_tbl->set_bool(r, "visible", true);
    }

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

    // Build the view list. A one-view game: one World2D view of the
    // scene's active camera, filling the window. An editor would
    // build more.
    std::vector<engine::View> views;
    views.push_back(engine::make_world_2d_view(
        world,
        scene.active_camera_name(),
        0, 0, window.width(), window.height()));

    // The UI view covers the window. It draws after the world
    // views, on top.
    views.push_back(engine::make_ui_view(&world,
                                         0, 0, window.width(), window.height()));

    // The clear color comes from the scene's active camera.
    engine::Color clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    {
        const auto cam_ref =
            engine::find_camera(world, scene.active_camera_name());
        if (!cam_ref.is_nil())
        {
            const engine::CameraDesc desc = engine::read_camera_row(world, cam_ref);
            clear_color = engine::Color{
                desc.clear_r, desc.clear_g, desc.clear_b, desc.clear_a};
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

            // Resize the views that cover the full window.
            // A view with a fixed viewport (a minimap, a preview
            // panel) keeps its rectangle; a full-window view is
            // resized to the new window.
            for (auto &view : views)
            {
                if (view.resize_with_window)
                {
                    view.w = window.width();
                    view.h = window.height();
                }
            }
        }

        runtime.tick(dt);

        // Systems that produce world state run once per frame.
        engine::hierarchy_system(ctx, dt);
        engine::movement_system(ctx, dt);
        engine::hierarchy_system(ctx, dt);

        renderer->begin_frame(clear_color);

        // Views that draw the world run once per view. A one-view
        // game draws once; an editor draws once per panel.
        for (const auto &view : views)
        {
            engine::render_view(view, ctx, dt, *renderer);
        }

        renderer->end_frame();
        renderer->present();
    }

    runtime.clear();
    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}