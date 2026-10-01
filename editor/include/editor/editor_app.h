#pragma once

#include "engine/window.h"
#include "engine/clock.h"

namespace editor {

// The editor application. Phase 0: opens a window, runs a loop, closes on Escape.
// Later: owns the runtime, renderer, scene, and panels.
class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    // Initialize the editor. Returns false on failure.
    bool init(int width, int height, const std::string& title);

    // Run the editor loop. Blocks until the window closes.
    int run();

private:
    engine::Window  window_;
    engine::Clock   clock_;

    bool initialized_ = false;
};

} // namespace editor
