#pragma once

#include "engine/platform/window.h"
#include "engine/core/clock.h"
#include "editor/imgui_layer.h"

#include <string>

namespace editor
{

    class EditorApp
    {
    public:
        EditorApp();
        ~EditorApp();

        bool init();
        int run();

    private:
        engine::Window window_;
        engine::Clock clock_;
        ImGuiLayer imgui_;

        bool initialized_ = false;
    };
}