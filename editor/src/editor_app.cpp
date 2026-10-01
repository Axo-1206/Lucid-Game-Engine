#include "editor/editor_app.h"
#include "engine/log.h"

namespace editor {

EditorApp::EditorApp()
    : window_(1280, 720, "Lucid Editor")
{}

EditorApp::~EditorApp() = default;

bool EditorApp::init(int width, int height, const std::string& title) {
    // Phase 0: the Window is constructed in the initializer list.
    // For now, we just check that it succeeded.
    if (!window_.native_handle()) {
        engine::log_error("EditorApp: window creation failed");
        return false;
    }

    initialized_ = true;
    engine::log_info("Editor initialized");
    return true;
}

int EditorApp::run() {
    if (!initialized_) {
        engine::log_error("EditorApp::run called before init");
        return 1;
    }

    engine::log_info("Editor loop starting");

    while (!window_.should_close()) {
        clock_.start_frame();

        window_.poll_events();

        // Step 3 will draw the editor UI here.
        // For now, nothing is drawn.

        window_.swap_buffers();
    }

    engine::log_info("Editor loop ended");
    return 0;
}

} // namespace editor
