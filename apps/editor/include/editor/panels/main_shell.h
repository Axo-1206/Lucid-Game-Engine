#pragma once

#include "editor/project.h"

#include <cstddef>
#include <functional>

namespace editor
{

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

        void draw();

    private:
        const Project *project_ = nullptr;
        std::size_t package_count_ = 0;
        bool show_about_ = false;

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