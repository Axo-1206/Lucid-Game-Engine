#include "engine/window.h"
#include "engine/clock.h"
#include "engine/log.h"

int main() {
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info_f("Lucid Game starting");

    engine::Window window(1280, 720, "Lucid Game");
    engine::Clock  clock;

    // Phase 0: nothing to update or render. Just tick and poll.
    // Step 3 will add the renderer; Step 6 will add the scene.

    while (!window.should_close()) {
        clock.start_frame();

        window.poll_events();

        // Update systems here (Step 8).

        // Render here (Step 3).

        window.swap_buffers();
    }

    engine::log_info("Lucid Game ended");
    return 0;
}
