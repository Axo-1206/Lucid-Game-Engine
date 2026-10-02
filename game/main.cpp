#include "engine/window.h"
#include "engine/clock.h"
#include "engine/log.h"
#include "engine/renderer.h"
#include "engine/input.h"
#include "engine/entity.h"
#include "engine/scene.h"

#include <memory>

int main() {
    engine::install_default_log_sink();
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info("Lucid Game starting");

    engine::Window window(1280, 720, "Lucid Game");
    engine::Clock  clock;

    std::unique_ptr<engine::Renderer> renderer(engine::create_renderer_gl());
    if (!renderer->init(window.native_handle())) {
        engine::log_error("Failed to initialize renderer");
        return 1;
    }
    renderer->set_viewport(1280, 720);

    std::unique_ptr<engine::Input> input(
        engine::create_input_glfw(window.native_handle()));

    lucid::Runtime rt;
    engine::setup_core_tables(rt);

    engine::Scene scene(rt);
    if (!scene.load_from_file("assets/scenes/demo.lscene")) {
        engine::log_error("Failed to load scene");
        return 1;
    }

    // Set up the 2D camera from the active camera.
    engine::CameraData cam_data = scene.get_active_camera_data();
    engine::Camera2D camera;
    camera.x = cam_data.pos_x;
    camera.y = cam_data.pos_y;
    camera.zoom = 1.0f / cam_data.ortho_size;
    camera.viewport_w = 1280.0f;
    camera.viewport_h = 720.0f;
    renderer->set_camera_2d(camera);

    while (!window.should_close()) {
        clock.start_frame();
        const float dt = clock.dt();

        input->state().begin_frame();
        input->poll();

        const auto& in = input->state();
        if (in.key_pressed(engine::Key::Escape)) {
            window.request_close();
        }

        renderer->begin_frame(cam_data.clear_color);

        // Iterate entities with Transform + Sprite, draw them.
        rt.get_table("Transform")->each([&](lucid::RowRef t_ref) {
            auto* ttbl = rt.get_table("Transform");
            lucid::RowRef e = ttbl->get_row_ref(t_ref, "entity");
            auto s = engine::get_component<engine::SpriteTag>(rt, e);
            if (s.is_nil()) return;

            auto* stbl = rt.get_table("Sprite");
            float x = ttbl->get_float32(t_ref, "x");
            float y = ttbl->get_float32(t_ref, "y");
            float w = stbl->get_float32(s, "w");
            float h = stbl->get_float32(s, "h");
            engine::Color c {
                stbl->get_float32(s, "tint_r"),
                stbl->get_float32(s, "tint_g"),
                stbl->get_float32(s, "tint_b"),
                stbl->get_float32(s, "tint_a"),
            };
            int layer = stbl->get_int32(s, "layer");
            renderer->draw_rect(x, y, w, h, c, layer);
        });

        renderer->end_frame();
        renderer->present();
    }

    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}
