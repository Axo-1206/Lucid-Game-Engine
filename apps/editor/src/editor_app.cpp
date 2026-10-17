#include "editor/editor_app.h"

#include "editor/platform_paths.h"
#include "editor/platform_paths.h"
#include "editor/build.h"
#include "editor/run.h"

#include "engine/core/log.h"
#include "engine/core/version.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/registry/packages/math_package.h"
#include "engine/registry/packages/render_package.h"
#include "engine/registry/packages/ui_package.h"

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

        // Build the engine registry. This is what the compile loop
        // uses. The manager lives for the editor's lifetime.
        package_manager_ = std::make_unique<engine::PackageManager>();
        package_manager_->load_package(std::make_unique<engine::CorePackage>());
        package_manager_->load_package(std::make_unique<engine::LogicPackage>());
        package_manager_->load_package(std::make_unique<engine::MathPackage>());
        package_manager_->load_package(std::make_unique<engine::RenderPackage>());
        package_manager_->load_package(std::make_unique<engine::UIPackage>());

        if (!package_manager_->apply_registration_only())
        {
            engine::log_error("EditorApp: package registration failed");
            for (const auto &c : package_manager_->conflicts())
                engine::log_error("  %s", c.c_str());
            return false;
        }

        engine::log_info("Editor: registry ready (%zu node types)",
                         package_manager_->registry().nodeTypes.size());

        package_panel_.set_manager(package_manager_.get());
        main_shell_.set_package_count(package_manager_->package_infos().size());

        script_editor_.set_package_manager(package_manager_.get());
        script_editor_.set_diagnostic_store(&diagnostics_);

        problems_.set_store(&diagnostics_);
        problems_.set_jump_callback(
            [this](const std::filesystem::path &f, std::uint32_t l)
            {
                on_jump_to_diagnostic(f, l);
            });

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
            /*close_project=*/[this]
            { leave_project(); },
            /*quit=*/[this]
            { on_quit(); },
            /*about=*/[this]
            { on_about(); },
            /*save=*/[this]
            { on_save(); },
            /*save_all=*/[this]
            { on_save_all(); },
            /*reset_layout=*/[this]
            { on_reset_layout(); },
            /*build=*/[this]
            { on_build(); },
            /*run=*/[this]
            { on_run(); },
            /*stop=*/[this]
            { on_stop(); });

        main_shell_.set_panel_visibility(&console_.visible(),
                                         &explorer_.visible(),
                                         &script_editor_.visible(),
                                         &problems_.visible(),
                                         &package_panel_.visible());

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

    void EditorApp::update_run_status()
    {
        if (run_.is_running())
        {
            run_status_ = "Running";
        }
        else if (run_.has_exited())
        {
            run_status_ = "Exited (" + std::to_string(run_.exit_code()) + ")";
        }
        else
        {
            run_status_ = "";
        }
        main_shell_.set_run_status(run_status_);
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
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P,
                            ImGuiInputFlags_RouteGlobal))
        {
            problems_.visible() = !problems_.visible();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G,
                            ImGuiInputFlags_RouteGlobal))
        {
            package_panel_.visible() = !package_panel_.visible();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_B,
                            ImGuiInputFlags_RouteGlobal))
        {
            on_build();
        }
        if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))
        {
            if (run_.is_running())
                on_stop();
            else
                on_run();
        }
        if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))
        {
            on_stop();
        }
    }

    void EditorApp::frame()
    {
        if (pending_ini_)
        {
            imgui_.set_ini_file(*pending_ini_);
            pending_ini_.reset();
        }

        process_shortcuts();

        run_.poll(console_.buffer());
        update_run_status();

        switch (mode_)
        {
        case Mode::Launch:
            launch_.draw();
            break;
        case Mode::Project:
            main_shell_.draw();
            explorer_.draw();
            script_editor_.draw();
            problems_.draw();
            console_.draw();
            package_panel_.draw();

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

        build_status_.clear();
        main_shell_.set_build_status(build_status_);

        // Switch to the project's ini. Applied on the next frame.
        pending_ini_ = project_->lucid_dir() / "editor.ini";

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

        if (run_.is_running())
            run_.stop();
        run_status_.clear();
        main_shell_.set_run_status(run_status_);

        project_.reset();
        main_shell_.set_project(nullptr);
        explorer_.clear();
        diagnostics_.clear();
        build_status_.clear();
        main_shell_.set_build_status(build_status_);
        launch_.set_error("");
        mode_ = Mode::Launch;

        // Return to the launch ini.
        pending_ini_ = paths::global_config_dir() / "launch.ini";

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

    void EditorApp::on_jump_to_diagnostic(const std::filesystem::path &file,
                                          std::uint32_t line)
    {
        script_editor_.request_jump(file, line);
    }

    void EditorApp::on_reset_layout()
    {
        ImGuiIO &io = ImGui::GetIO();
        if (io.IniFilename != nullptr)
        {
            std::error_code ec;
            std::filesystem::remove(io.IniFilename, ec);
        }
        ImGui::LoadIniSettingsFromMemory("");

        engine::log_info("Layout reset; default will be rebuilt.");
    }

    void EditorApp::on_build()
    {
        if (mode_ != Mode::Project || !project_ || !package_manager_)
            return;

        const auto result = compile_project(project_->root(),
                                            *package_manager_,
                                            diagnostics_);

        // Log a one-line summary.
        const double kb = static_cast<double>(result.bytes_written) / 1024.0;
        if (result.files_total == 0)
        {
            engine::log_info("Build: no .lucid files to compile.");
            build_status_ = "Build: nothing to compile";
        }
        else if (result.files_failed == 0)
        {
            engine::log_info(
                "Build: %zu file%s, %zu succeeded, %.1f KB written.",
                result.files_total, result.files_total == 1 ? "" : "s",
                result.files_ok, kb);
            build_status_ = "Build: OK (" +
                            std::to_string(result.files_ok) + "/" +
                            std::to_string(result.files_total) + ")";
        }
        else
        {
            engine::log_warn(
                "Build: %zu file%s, %zu succeeded, %zu failed, %.1f KB written.",
                result.files_total, result.files_total == 1 ? "" : "s",
                result.files_ok, result.files_failed, kb);
            build_status_ = "Build: " +
                            std::to_string(result.files_failed) + " failed";
        }

        main_shell_.set_build_status(build_status_);
    }

    void EditorApp::on_run()
    {
        if (mode_ != Mode::Project || !project_)
            return;

        if (run_.is_running())
            return; // already running; the toolbar shows Stop

        const std::filesystem::path game_exe =
            paths::executable_dir() / "lucid_game"
#if defined(_WIN32)
                                      ".exe"
#endif
            ;

        std::string error;
        if (!run_.start(game_exe,
                        {project_->root().string()},
                        /*working_dir=*/project_->root(),
                        error))
        {
            engine::log_error("Run failed: %s", error.c_str());
            run_status_ = "Run failed";
            main_shell_.set_run_status(run_status_);
            return;
        }

        run_status_ = "Running";
        main_shell_.set_run_status(run_status_);
        engine::log_info("Started game: pid %llu",
                         static_cast<unsigned long long>(run_.pid()));
    }

    void EditorApp::on_stop()
    {
        if (!run_.is_running())
            return;
        run_.stop();
        update_run_status();
    }

} // namespace editor