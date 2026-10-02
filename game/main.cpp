#include "engine/window.h"
#include "engine/clock.h"
#include "engine/log.h"
#include "engine/renderer.h"
#include "engine/input.h"
#include "engine/entity.h"
#include "engine/scene.h"
#include "engine/system_context.h"
#include "engine/systems.h"

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

    lucid::Runtime rt;
    engine::setup_core_tables(rt);

    engine::Scene scene(rt);
    if (!scene.load_from_file("assets/scenes/demo.lscene"))
    {
        engine::log_error("Failed to load scene");
        return 1;
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
        rt,
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

        const auto &in = input->state();
        if (in.key_pressed(engine::Key::Escape))
        {
            window.request_close();
        }

        engine::hierarchy_system(ctx, dt);
        engine::movement_system(ctx, dt);
        engine::hierarchy_system(ctx, dt);

        engine::physics_sync_system(ctx, dt);
        engine::collision_system(ctx, dt);
        engine::script_system(ctx, dt);

        renderer->begin_frame(cam_data.clear_color);
        engine::render_system(ctx, dt);
        renderer->end_frame();
        renderer->present();
    }

    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}
