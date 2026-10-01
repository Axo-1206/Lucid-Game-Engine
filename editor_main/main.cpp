#include "editor/editor_app.h"
#include "engine/log.h"

int main() {
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info_f("Lucid Editor starting");

    editor::EditorApp app;
    if (!app.init(1280, 720, "Lucid Editor")) {
        engine::log_error("Failed to initialize editor");
        return 1;
    }

    return app.run();
}
