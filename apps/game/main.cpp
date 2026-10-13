/// @file apps/game/main.cpp

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
#include "engine/services.h"

#include <filesystem>
#include <memory>
#include <string>

namespace
{

    // Read the Engine_Camera row named `name` into a CameraDesc.
    //
    // This is a temporary local helper. S3 introduces
    // engine::read_camera_row(World&, RowRef) and
    // engine::to_camera2d(CameraDesc, int, int); when that lands, this
    // function is deleted and its call sites collapse to two lines.
    engine::CameraDesc read_camera_by_name(const engine::ecs::World &world,
                                           const std::string &name)
    {
        engine::CameraDesc desc;

        const auto *cam_tbl = world.get_table("Engine_Camera");
        if (cam_tbl == nullptr || name.empty())
        {
            return desc;
        }

        const auto ref = cam_tbl->find_by_primary(
            "name", engine::ecs::CellValue::from_string(name));
        if (ref.is_nil())
        {
            return desc;
        }

        desc.name = cam_tbl->get_string(ref, "name");
        desc.pos_x = cam_tbl->get_float32(ref, "pos_x");
        desc.pos_y = cam_tbl->get_float32(ref, "pos_y");
        desc.pos_z = cam_tbl->get_float32(ref, "pos_z");
        desc.target_x = cam_tbl->get_float32(ref, "target_x");
        desc.target_y = cam_tbl->get_float32(ref, "target_y");
        desc.target_z = cam_tbl->get_float32(ref, "target_z");
        desc.fov = cam_tbl->get_float32(ref, "fov");
        desc.near_plane = cam_tbl->get_float32(ref, "near");
        desc.far_plane = cam_tbl->get_float32(ref, "far");
        desc.is_orthographic = cam_tbl->get_bool(ref, "is_orthographic");
        desc.ortho_size = cam_tbl->get_float32(ref, "ortho_size");
        desc.clear_r = cam_tbl->get_float32(ref, "clear_r");
        desc.clear_g = cam_tbl->get_float32(ref, "clear_g");
        desc.clear_b = cam_tbl->get_float32(ref, "clear_b");
        desc.clear_a = cam_tbl->get_float32(ref, "clear_a");

        return desc;
    }

    // Build a Camera2D from a CameraDesc and a viewport size. A
    // temporary local helper; S3 hoists this into the engine.
    engine::Camera2D to_camera2d(const engine::CameraDesc &desc,
                                 int viewport_w,
                                 int viewport_h)
    {
        engine::Camera2D cam;
        cam.x = desc.pos_x;
        cam.y = desc.pos_y;
        cam.zoom = desc.ortho_size > 0.0f ? 1.0f / desc.ortho_size : 1.0f;
        cam.viewport_w = static_cast<float>(viewport_w);
        cam.viewport_h = static_cast<float>(viewport_h);
        return cam;
    }

} // namespace

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

    // Load the scene. A failed load leaves the world untouched and
    // returns false; the scene variable is kept so the active camera
    // name is available to the loop.
    engine::Scene scene = engine::SceneLoader_load("assets/scenes/demo.lscene");
    if (scene.json_text().empty())
    {
        engine::log_error("Failed to load scene");
        return 1;
    }
    scene.apply_to(world, engine::ApplyMode::Clear);

    // Services. No physics backend yet.
    engine::Services services;

    engine::graph::GraphRuntime runtime(world, packages, services);
    runtime.set_asset_root(std::filesystem::current_path());
    if (!runtime.load())
    {
        engine::log_warn("Some graphs failed to load; continuing without them.");
    }

    // Camera. Read the row named by the scene's active camera name.
    const engine::CameraDesc cam_desc =
        read_camera_by_name(world, scene.active_camera_name());
    renderer->set_camera_2d(
        to_camera2d(cam_desc, window.width(), window.height()));

    const engine::Color clear_color{
        cam_desc.clear_r, cam_desc.clear_g, cam_desc.clear_b, cam_desc.clear_a};

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