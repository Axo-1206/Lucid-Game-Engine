#pragma once

#include "editor/imgui_layer.h"
#include "editor/panels/console_panel.h"
#include "editor/panels/explorer_panel.h"
#include "editor/panels/launch_panel.h"
#include "editor/panels/main_shell.h"
#include "editor/project.h"
#include "editor/project_roots.h"
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

        void frame();

        // Called once per frame, before any panel draws. Handles
        // the keyboard shortcuts that toggle panel visibility.
        void process_shortcuts();

        void enter_project(std::unique_ptr<Project> project);
        void leave_project();

        void on_open_project(const std::filesystem::path &folder);
        void on_create_project(const std::filesystem::path &folder,
                               const std::string &name);
        void on_quit();
        void on_about();

        void refresh_launch_scan();
        void save_settings();
        void save_recent();

        Mode mode_ = Mode::Launch;
        std::unique_ptr<Project> project_;

        Settings settings_;
        RecentProjects recent_;
        ProjectRoots roots_;

        LaunchPanel launch_;
        MainShell main_shell_;
        ConsolePanel console_;
        ExplorerPanel explorer_;

        engine::Window window_;
        engine::Clock clock_;
        ImGuiLayer imgui_;

        bool initialized_ = false;
        bool quit_requested_ = false;
    };

} // namespace editor