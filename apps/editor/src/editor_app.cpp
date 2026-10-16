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

        // Attach the console to the engine log before any other
        // init logs happen, so the console catches them.
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
            { on_about(); });

        // Wire the Window menu's visibility toggles to the panels'
        // own bools. The X buttons and the menu items write the
        // same bool.
        main_shell_.set_panel_visibility(&console_.visible(),
                                         &explorer_.visible());

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
    }

    void EditorApp::frame()
    {
        // Shortcuts are processed first so they work regardless of
        // which window has focus.
        process_shortcuts();

        switch (mode_)
        {
        case Mode::Launch:
            launch_.draw();
            break;
        case Mode::Project:
            main_shell_.draw();
            explorer_.draw();
            console_.draw();

            // Consume any open request the Explorer recorded.
            if (auto request = explorer_.take_open_request())
            {
                // The script editor (step 8) will handle this.
                // For now, log it.
                engine::log_info("Would open: %s",
                                 request->string().c_str());
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
        quit_requested_ = true;
    }

    void EditorApp::on_about()
    {
        engine::log_info("About: Lucid Editor, engine %s",
                         engine::kEngineVersion);
    }

} // namespace editor