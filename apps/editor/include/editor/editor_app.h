#pragma once

#include "editor/imgui_layer.h"
#include "editor/panels/launch_panel.h"
#include "editor/panels/main_shell.h"
#include "editor/project.h"
#include "editor/settings.h"

#include "engine/core/clock.h"
#include "engine/platform/window.h"

#include <memory>
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
        enum class Mode
        {
            Launch,
            Project,
        };

        // The frame. Dispatches on mode_.
        void frame();

        // Mode transitions.
        void enter_project(std::unique_ptr<Project> project);
        void leave_project();

        // Launch panel callbacks.
        void on_open_project(const std::filesystem::path &folder);
        void on_create_project(const std::filesystem::path &folder,
                               const std::string &name);
        void on_quit();

        // Main shell callbacks.
        void on_about();

        // The project state.
        Mode mode_ = Mode::Launch;
        std::unique_ptr<Project> project_;

        // Machine-local state.
        Settings settings_;
        RecentProjects recent_;

        // Panels.
        LaunchPanel launch_;
        MainShell main_shell_;

        // Engine-side.
        engine::Window window_;
        engine::Clock clock_;
        ImGuiLayer imgui_;

        bool initialized_ = false;
        bool quit_requested_ = false;
    };

} // namespace editor