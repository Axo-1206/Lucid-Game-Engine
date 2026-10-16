#pragma once

#include "editor/project.h"

#include <cstddef>
#include <functional>
#include <string>

namespace editor
{

    class MainShell
    {
    public:
        using CloseProjectCallback = std::function<void()>;
        using QuitCallback = std::function<void()>;
        using AboutCallback = std::function<void()>;
        using SaveCallback = std::function<void()>;
        using SaveAllCallback = std::function<void()>;
        using ResetLayoutCallback = std::function<void()>;

        void set_callbacks(CloseProjectCallback on_close_project,
                           QuitCallback on_quit,
                           AboutCallback on_about,
                           SaveCallback on_save,
                           SaveAllCallback on_save_all,
                           ResetLayoutCallback on_reset_layout);

        void set_project(const Project *project);
        void set_active_scene(std::string scene);
        void set_package_count(std::size_t count);

        void set_panel_visibility(bool *console_visible,
                                  bool *explorer_visible,
                                  bool *script_editor_visible,
                                  bool *problems_visible,
                                  bool *package_visible);

        void draw();

    private:
        const Project *project_ = nullptr;
        std::string active_scene_;
        std::size_t package_count_ = 0;
        bool show_about_ = false;

        bool *console_visible_ = nullptr;
        bool *explorer_visible_ = nullptr;
        bool *script_editor_visible_ = nullptr;
        bool *problems_visible_ = nullptr;
        bool *package_visible_ = nullptr;

        CloseProjectCallback on_close_project_;
        QuitCallback on_quit_;
        AboutCallback on_about_;
        SaveCallback on_save_;
        SaveAllCallback on_save_all_;
        ResetLayoutCallback on_reset_layout_;

        void draw_menu_bar();
        void draw_toolbar();
        void draw_dockspace();
        void draw_status_bar();
        void draw_about_modal();

        void build_default_layout(unsigned int dockspace_id,
                                  float width, float height);
    };

} // namespace editor