#include "engine/window.h"
#include "engine/clock.h"
#include "engine/log.h"
#include "engine/renderer.h"

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

    // Set up a camera at screen center, no zoom.
    engine::Camera2D camera;
    camera.x = 640.0f;
    camera.y = 360.0f;
    camera.zoom = 1.0f;
    camera.viewport_w = 1280.0f;
    camera.viewport_h = 720.0f;
    renderer->set_camera_2d(camera);

    // Try to load a texture (optional; skip if it doesn't exist).
    engine::TextureHandle tex = renderer->load_texture("assets/test.png");

    while (!window.should_close()) {
        clock.start_frame();
        window.poll_events();

        renderer->begin_frame(engine::Color::black());

        // Rectangles.
        renderer->draw_rect(100, 100, 200, 100, engine::Color::red(), 0);
        renderer->draw_rect(400, 200, 150, 150, engine::Color::green(), 0);
        renderer->draw_rect(600, 300, 100, 100, engine::Color::blue(), 0);

        // Outline.
        renderer->draw_rect_outline(50, 50, 400, 300, 2.0f, engine::Color::white(), 1);

        // Circle.
        renderer->draw_circle(900, 400, 80, engine::Color::yellow(), 0);

        // Line.
        renderer->draw_line(100, 600, 900, 500, 3.0f, engine::Color::magenta(), 0);

        // Sprite (if loaded).
        if (tex.is_valid()) {
            renderer->draw_sprite(tex, 1000.0f, 150.0f, 128.0f, 128.0f,
                                  0.0f, engine::Color::white(),
                                  0.5f, 0.5f, 0);
        }

        renderer->end_frame();
        renderer->present();
    }

    renderer->shutdown();
    engine::log_info("Lucid Game ended");
    return 0;
}
