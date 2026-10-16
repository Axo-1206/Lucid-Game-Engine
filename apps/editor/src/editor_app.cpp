#include "editor/editor_app.h"

#include "editor/platform_paths.h"
#include "engine/core/log.h"
#include "engine/core/version.h"

#include "imgui.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace editor
{

    EditorApp::EditorApp()
        : window_(1280, 720, "Lucid Editor")
    {
    }

    EditorApp::~EditorApp() = default;

    bool EditorApp::init()
    {
        GLFWwindow *handle = static_cast<GLFWwindow *>(window_.native_handle());
        if (!handle)
        {
            engine::log_error("EditorApp: window creation failed");
            return false;
        }

        if (!imgui_.init(handle))
        {
            engine::log_error("EditorApp: ImGui initialization failed");
            return false;
        }

        console_.attach_to_log();

        settings_ = SettingsStore::load_global();
        recent_ = SettingsStore::load_recent();
        roots_ = ProjectRoots::load(settings_);

        launch_.set_recent(&recent_);
        launch_.set_default_project_root(roots_.default_root());
        launch_.set_callbacks(
            [this](const std::filesystem::path &p)
            { on_open_project(p); },
            [this](const std::filesystem::path &p, const std::string &n)
            {
                on_create_project(p, n);
            },
            [this]
            { on_quit(); },
            [this]
            { refresh_launch_scan(); });

        main_shell_.set_callbacks(
            [this]
            { leave_project(); },
            [this]
            { on_quit(); },
            [this]
            { on_about(); },
            [this]
            { on_save(); },
            [this]
            { on_save_all(); });

        main_shell_.set_panel_visibility(&console_.visible(),
                                         &explorer_.visible(),
                                         &script_editor_.visible());

        refresh_launch_scan();

        initialized_ = true;
        engine::log_info("Editor initialized");
        return true;
    }

    int EditorApp::run()
    {
        if (!initialized_)
        {
            engine::log_error("EditorApp::run called before init");
            return 1;
        }

        engine::log_info("Editor loop starting");

        while (!window_.should_close() && !quit_requested_)
        {
            clock_.start_frame();
            window_.poll_events();

            imgui_.begin_frame();
            frame();
            imgui_.end_frame();

            window_.swap_buffers();
        }

        imgui_.shutdown();
        engine::log_info("Editor loop ended");
        return 0;
    }

    void EditorApp::process_shortcuts()
    {
        // Do not process shortcuts while a modal is open. This
        // prevents Ctrl+S from firing while the unsaved-changes
        // prompt is up.
        const bool any_popup = ImGui::IsPopupOpen(
            nullptr,
            ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        if (any_popup)
            return;

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_E,
                            ImGuiInputFlags_RouteGlobal))
        {
            explorer_.visible() = !explorer_.visible();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_C,
                            ImGuiInputFlags_RouteGlobal))
        {
            console_.visible() = !console_.visible();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S,
                            ImGuiInputFlags_RouteGlobal))
        {
            on_save();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
                            ImGuiInputFlags_RouteGlobal))
        {
            on_save_all();
        }
    }

    void EditorApp::frame()
    {
        process_shortcuts();

        switch (mode_)
        {
        case Mode::Launch:
            launch_.draw();
            break;
        case Mode::Project:
            main_shell_.draw();
            explorer_.draw();
            script_editor_.draw();
            console_.draw();

            if (auto request = explorer_.take_open_request())
            {
                std::string error;
                const int idx = script_editor_.open(*request, error);
                if (idx < 0 && !error.empty())
                {
                    engine::log_error("%s", error.c_str());
                }
            }
            break;
        }
    }

    void EditorApp::refresh_launch_scan()
    {
        launch_.set_scan_results(roots_.scan());
    }

    void EditorApp::save_settings()
    {
        settings_.project_roots = roots_.user_roots_as_strings();
        std::string error;
        if (!SettingsStore::save_global(settings_, error))
        {
            engine::log_warn("Could not save settings: %s", error.c_str());
        }
    }

    void EditorApp::save_recent()
    {
        std::string error;
        if (!SettingsStore::save_recent(recent_, error))
        {
            engine::log_warn("Could not save recent projects: %s",
                             error.c_str());
        }
    }

    void EditorApp::enter_project(std::unique_ptr<Project> project)
    {
        roots_.add_root(project->root().parent_path());
        save_settings();

        SettingsStore::push_recent(recent_,
                                   project->root().string(),
                                   project->name(),
                                   SettingsStore::now_seconds());
        save_recent();

        project_ = std::move(project);
        main_shell_.set_project(project_.get());
        explorer_.set_root(project_->root());

        launch_.set_error("");
        mode_ = Mode::Project;

        engine::log_info("Opened project '%s'",
                         project_->name().c_str());
    }

    void EditorApp::leave_project()
    {
        // Route through the script editor's prompt. If there are
        // unsaved documents, the prompt shows first.
        script_editor_.request_close_all([this]
                                         { do_leave_project(); });
    }

    void EditorApp::do_leave_project()
    {
        if (project_)
        {
            engine::log_info("Closed project '%s'",
                             project_->name().c_str());
        }
        project_.reset();
        main_shell_.set_project(nullptr);
        explorer_.clear();
        launch_.set_error("");
        mode_ = Mode::Launch;

        refresh_launch_scan();
    }

    void EditorApp::on_open_project(const std::filesystem::path &folder)
    {
        std::string error;
        auto project = Project::open(folder, error);
        if (!project)
        {
            launch_.set_error(error);
            return;
        }
        enter_project(std::move(project));
    }

    void EditorApp::on_create_project(const std::filesystem::path &folder,
                                      const std::string &name)
    {
        std::string error;
        auto project = Project::create(folder, name, error);
        if (!project)
        {
            launch_.set_error(error);
            return;
        }
        enter_project(std::move(project));
    }

    void EditorApp::on_quit()
    {
        if (mode_ == Mode::Project)
        {
            script_editor_.request_close_all([this]
                                             { quit_requested_ = true; });
        }
        else
        {
            quit_requested_ = true;
        }
    }

    void EditorApp::on_about()
    {
        engine::log_info("About: Lucid Editor, engine %s",
                         engine::kEngineVersion);
    }

    void EditorApp::on_save()
    {
        std::string error;
        if (!script_editor_.save_active(error) && !error.empty())
        {
            engine::log_error("%s", error.c_str());
        }
    }

    void EditorApp::on_save_all()
    {
        std::string error;
        if (!script_editor_.save_all(error) && !error.empty())
        {
            engine::log_error("%s", error.c_str());
        }
    }

} // namespace editor