#include "editor/editor_app.h"
#include "engine/core/log.h"

int main()
{
    engine::install_default_log_sink();
    engine::set_log_level(engine::LogLevel::Info);

    engine::log_info("Lucid Editor starting");

    editor::EditorApp app;
    if (!app.init())
    {
        engine::log_error("Failed to initialize editor");
        return 1;
    }

    return app.run();
}