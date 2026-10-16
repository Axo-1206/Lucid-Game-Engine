#pragma once

#include "editor/settings.h"

#include <filesystem>
#include <functional>
#include <string>

namespace editor
{

    // The launch panel: shown when no project is loaded.
    //
    // Draws a recent-projects list and two buttons ("Open Project",
    // "New Project"). Each opens a modal with a text field. When
    // the user confirms, the panel calls back into EditorApp.
    //
    // The panel does not own the project or the recent list. It
    // reads the recent list for display; it calls callbacks for
    // actions.
    class LaunchPanel
    {
    public:
        using OpenCallback =
            std::function<void(const std::filesystem::path &)>;
        using CreateCallback =
            std::function<void(const std::filesystem::path &,
                               const std::string &)>;
        using QuitCallback = std::function<void()>;

        void set_callbacks(OpenCallback on_open,
                           CreateCallback on_create,
                           QuitCallback on_quit);

        // Set the recent-projects list for display. The panel does
        // not own it; the caller (EditorApp) must keep it alive
        // for as long as the panel is drawn.
        void set_recent(const RecentProjects *recent);

        // Set or clear the error message shown under the buttons.
        void set_error(std::string error);

        // Draw one frame.
        void draw();

    private:
        char open_path_[512] = {};
        char new_parent_[512] = {};
        char new_name_[128] = {};

        bool show_open_ = false;
        bool show_new_ = false;

        std::string error_;

        OpenCallback on_open_;
        CreateCallback on_create_;
        QuitCallback on_quit_;

        const RecentProjects *recent_ = nullptr;

        void draw_recent();
        void draw_open_modal();
        void draw_new_modal();
    };

} // namespace editors