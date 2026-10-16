#pragma once

#include "editor/project.h"

#include <cstddef>
#include <functional>

namespace editor
{

    // The main shell: shown when a project is loaded.
    //
    // Draws a menu bar, a toolbar, a full-viewport dockspace, and
    // a status bar. Panels dock into the dockspace.
    //
    // The shell does not own the project or the panels. It holds
    // a const pointer to the project and bool pointers to each
    // panel's visibility flag, set by EditorApp.
    class MainShell
    {
    public:
        using CloseProjectCallback = std::function<void()>;
        using QuitCallback = std::function<void()>;
        using AboutCallback = std::function<void()>;

        void set_callbacks(CloseProjectCallback on_close_project,
                           QuitCallback on_quit,
                           AboutCallback on_about);

        void set_project(const Project *project);
        void set_package_count(std::size_t count);

        // Wire the panel visibility flags. Pass nullptr to disable
        // the corresponding menu item. Called by EditorApp at init.
        void set_panel_visibility(bool *console_visible,
                                  bool *explorer_visible);

        void draw();

    private:
        const Project *project_ = nullptr;
        std::size_t package_count_ = 0;
        bool show_about_ = false;

        bool *console_visible_ = nullptr;
        bool *explorer_visible_ = nullptr;

        CloseProjectCallback on_close_project_;
        QuitCallback on_quit_;
        AboutCallback on_about_;

        void draw_menu_bar();
        void draw_toolbar();
        void draw_dockspace();
        void draw_status_bar();
        void draw_about_modal();
    };

} // namespace editor