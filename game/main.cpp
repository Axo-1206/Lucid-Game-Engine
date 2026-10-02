#include "engine/window.h"
#include "engine/clock.h"
#include "engine/log.h"
#include "engine/renderer.h"
#include "engine/input.h"
#include "engine/entity.h"

#include <memory>
#include <vector>

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

    engine::Camera2D camera;
    camera.x = 640.0f;
    camera.y = 360.0f;
    camera.zoom = 1.0f;
    camera.viewport_w = 1280.0f;
    camera.viewport_h = 720.0f;
    renderer->set_camera_2d(camera);

    // Runtime + core tables.
    lucid::Runtime rt;
    engine::setup_core_tables(rt);

    // Spawn a few entities with Transform + Sprite.
    struct SpawnSpec { float x, y, w, h; engine::Color c; };
    const SpawnSpec specs[] = {
        {100.0f, 100.0f, 120.0f,  80.0f, engine::Color::red()},
        {400.0f, 200.0f, 150.0f, 150.0f, engine::Color::green()},
        {600.0f, 300.0f, 100.0f, 100.0f, engine::Color::blue()},
        {800.0f, 400.0f, 200.0f,  50.0f, engine::Color::yellow()},
    };

    std::vector<engine::EntityRef> entities;
    for (const auto& spec : specs) {
        engine::EntityRef e = engine::create_entity(rt, engine::allocate_entity_id(rt), "");
        auto t = engine::add_component<engine::TransformTag>(rt, e);
        rt.get_table("Transform")->set_float32(t, "x", spec.x);
        rt.get_table("Transform")->set_float32(t, "y", spec.y);

        auto s = engine::add_component<engine::SpriteTag>(rt, e);
        rt.get_table("Sprite")->set_string(s, "texture_path", "");
        rt.get_table("Sprite")->set_float32(s, "w", spec.w);
        rt.get_table("Sprite")->set_float32(s, "h", spec.h);
        rt.get_table("Sprite")->set_float32(s, "tint_r", spec.c.r);
        rt.get_table("Sprite")->set_float32(s, "tint_g", spec.c.g);
        rt.get_table("Sprite")->set_float32(s, "tint_b", spec.c.b);
        rt.get_table("Sprite")->set_float32(s, "tint_a", spec.c.a);
        rt.get_table("Sprite")->set_int32(s, "layer", 0);

        entities.push_back(e);
    }

    engine::log_info("Spawned %zu entities", entities.size());

    while (!window.should_close()) {
        clock.start_frame();
        const float dt = clock.dt();

        input->state().begin_frame();
        input->poll();

        const auto& in = input->state();

        if (in.key_pressed(engine::Key::Escape)) {
            window.request_close();
        }

        // Move entity 0 with arrow keys.
        if (!entities.empty()) {
            auto t = engine::get_component<engine::TransformTag>(rt, entities[0]);
            if (!t.is_nil()) {
                auto* tbl = rt.get_table("Transform");
                float x = tbl->get_float32(t, "x");
                float y = tbl->get_float32(t, "y");
                const float speed = 400.0f * dt;
                if (in.key_down(engine::Key::Left))  x -= speed;
                if (in.key_down(engine::Key::Right)) x += speed;
                if (in.key_down(engine::Key::Up))    y -= speed;
                if (in.key_down(engine::Key::Down))  y += speed;
                tbl->set_float32(t, "x", x);
                tbl->set_float32(t, "y", y);
            }
        }

        // Render.
        renderer->begin_frame(engine::Color::black());

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
