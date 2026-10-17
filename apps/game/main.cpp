/// @file apps/game/main.cpp
///
/// The game entry point.
///
/// Usage:
///   lucid_game [<project-root> | <scene.lscene>]
///
/// With no argument, runs the engine's demo scene
/// (assets/scenes/demo.lscene relative to the current directory).
/// This is what `lucid_game.exe` does when run from the SDK's bin/
/// for a sanity check.
///
/// With a project root, reads <root>/lucid.project.json to find
/// the active scene and active camera, then loads that scene.
/// This is what the editor's Run action spawns.
///
/// With a .lscene path, loads that scene directly. The asset root
/// is inferred as the parent of the scene's parent directory.

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
#include "engine/registry/packages/render_package.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{

    namespace fs = std::filesystem;

    // What the launch resolved to.
    struct LaunchSpec
    {
        fs::path project_root;     // directory that contains lucid.project.json
        fs::path scene_path;       // absolute path to the .lscene
        std::string active_camera; // empty = use the scene's own
        bool is_demo = false;
    };

    // Read <root>/lucid.project.json. Returns false and logs on
    // failure. The two fields this file needs are `active_scene`
    // and `active_camera`; everything else in the project file is
    // the editor's business.
    bool read_project_file(const fs::path &root,
                           std::string &active_scene,
                           std::string &active_camera)
    {
        const fs::path project_file = root / "lucid.project.json";

        std::ifstream in(project_file, std::ios::binary);
        if (!in)
        {
            engine::log_error("Cannot open project file: %s",
                              project_file.string().c_str());
            return false;
        }

        std::ostringstream ss;
        ss << in.rdbuf();

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(ss.str());
        }
        catch (const nlohmann::json::exception &e)
        {
            engine::log_error("Invalid %s: %s",
                              project_file.string().c_str(), e.what());
            return false;
        }

        active_scene = j.value("active_scene", std::string{});
        active_camera = j.value("active_camera", std::string{});
        return true;
    }

    // Resolve argv into a LaunchSpec. On failure, logs an error and
    // returns false.
    bool resolve_launch(int argc, char **argv, LaunchSpec &spec)
    {
        // Case 1: no argument. Demo mode.
        if (argc < 2)
        {
            spec.is_demo = true;
            spec.project_root = fs::current_path();
            spec.scene_path = fs::current_path() / "assets" / "scenes" / "demo.lscene";
            return true;
        }

        const fs::path arg = argv[1];

        // Case 2: a .lscene path. Load it directly; infer the asset
        // root from the path's grandparent (scenes/<name>.lscene ->
        // project root is the grandparent).
        if (arg.extension() == ".lscene")
        {
            std::error_code ec;
            const fs::path abs = fs::absolute(arg, ec);
            if (ec)
            {
                engine::log_error("Cannot resolve path %s: %s",
                                  arg.string().c_str(), ec.message().c_str());
                return false;
            }
            spec.project_root = abs.parent_path().parent_path();
            spec.scene_path = abs;
            return true;
        }

        // Case 3: a project root. Read lucid.project.json for the
        // active scene and camera.
        std::error_code ec;
        const fs::path abs_root = fs::absolute(arg, ec);
        if (ec)
        {
            engine::log_error("Cannot resolve path %s: %s",
                              arg.string().c_str(), ec.message().c_str());
            return false;
        }

        if (!fs::is_directory(abs_root, ec) || ec)
        {
            engine::log_error("Not a directory: %s",
                              abs_root.string().c_str());
            return false;
        }

        std::string active_scene;
        std::string active_camera;
        if (!read_project_file(abs_root, active_scene, active_camera))
            return false;

        if (active_scene.empty())
        {
            engine::log_error("Project file at %s has no active_scene.",
                              abs_root.string().c_str());
            return false;
        }

        spec.project_root = abs_root;
        spec.scene_path = abs_root / active_scene;
        spec.active_camera = std::move(active_camera);
        return true;
    }

} // namespace

int main(int argc, char **argv)
{
    engine::install_default_log_sink();
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info("Lucid Game starting");

    LaunchSpec spec;
    if (!resolve_launch(argc, argv, spec))
    {
        engine::log_error("Launch resolution failed.");
        return 1;
    }

    bool smoke_test = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--smoke-test") == 0)
            smoke_test = true;
    }

    if (smoke_test)
    {
        engine::ecs::World world;
        engine::setup_core_tables(world);

        engine::PackageManager packages;
        packages.load_package(std::make_unique<engine::CorePackage>());
        packages.load_package(std::make_unique<engine::LogicPackage>());
        packages.load_package(std::make_unique<engine::MathPackage>());
        packages.load_package(std::make_unique<engine::RenderPackage>());
        packages.load_package(std::make_unique<engine::UIPackage>());
        if (!packages.apply(world))
        {
            engine::log_error("Smoke test: package apply failed.");
            return 2;
        }

        engine::Scene scene = engine::SceneLoader_load(spec.scene_path.string());
        if (scene.json_text().empty())
        {
            engine::log_error("Smoke test: scene load failed for %s",
                              spec.scene_path.string().c_str());
            return 3;
        }
        scene.apply_to(world, packages.component_registry(),
                       engine::ApplyMode::Clear);

        engine::Services services;
        engine::graph::GraphRuntime runtime(world, packages, services);
        runtime.set_asset_root(spec.project_root);
        if (!runtime.load())
        {
            engine::log_error("Smoke test: runtime.load() failed.");
            return 4;
        }

        engine::log_info("Smoke test OK: scene=%s graphs=%zu",
                         spec.scene_path.string().c_str(),
                         runtime.graph_count());
        runtime.clear();
        return 0;
    }

    if (spec.is_demo)
    {
        engine::log_info("Running demo scene: %s",
                         spec.scene_path.string().c_str());
    }
    else
    {
        engine::log_info("Running project: %s (scene: %s)",
                         spec.project_root.string().c_str(),
                         spec.scene_path.string().c_str());
    }

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
    packages.load_package(std::make_unique<engine::RenderPackage>());
    packages.load_package(std::make_unique<engine::UIPackage>());
    if (!packages.apply(world))
    {
        engine::log_error("Failed to apply packages");
        for (const auto &c : packages.conflicts())
            engine::log_error("  %s", c.c_str());
        return 1;
    }

    engine::Scene scene = engine::SceneLoader_load(spec.scene_path.string());
    if (scene.json_text().empty())
    {
        engine::log_error("Failed to load scene: %s",
                          spec.scene_path.string().c_str());
        return 1;
    }
    scene.apply_to(world, packages.component_registry(), engine::ApplyMode::Clear);

    // The active camera: the project file's choice if it names one,
    // otherwise the scene's own.
    const std::string camera_name =
        spec.active_camera.empty() ? scene.active_camera_name()
                                   : spec.active_camera;

    if (camera_name.empty())
    {
        engine::log_warn("No active camera (project and scene both empty).");
    }

    // A demo UI element: only in demo mode, so a real project's UI
    // is whatever its scene says it is.
    if (spec.is_demo)
    {
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
    }

    // Services. No physics backend yet.
    engine::Services services;

    engine::graph::GraphRuntime runtime(world, packages, services);
    runtime.set_asset_root(spec.project_root);

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
        camera_name,
        0, 0, window.width(), window.height()));

    // The UI view covers the window. It draws after the world
    // views, on top.
    views.push_back(engine::make_ui_view(&world,
                                         0, 0, window.width(), window.height()));

    // The clear color comes from the active camera.
    engine::Color clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    if (!camera_name.empty())
    {
        const auto cam_ref = engine::find_camera(world, camera_name);
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