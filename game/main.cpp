#include "engine/window.h"
#include "engine/clock.h"
#include "engine/log.h"

int main() {
    engine::install_default_log_sink();
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info("Lucid Game starting");

    engine::Window window(1280, 720, "Lucid Game");
    engine::Clock  clock;

    while (!window.should_close()) {
        clock.start_frame();
        window.poll_events();
        // Step 3 will add the renderer; Step 6 will add the scene.
        window.swap_buffers();
    }

    engine::log_info("Lucid Game ended");
    return 0;
}
