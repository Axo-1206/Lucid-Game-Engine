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

        // Wire the runtime diagnostics path. The store is created
        // with the EditorApp; the panel registers the sink below.
        diagnostics_panel_.set_store(&runtime_diagnostics_);
        diagnostics_panel_.set_jump_callback(
            [this](const std::filesystem::path &f, std::uint32_t l)
            { on_jump_to_diagnostic(f, l); });
        diagnostics_panel_.attach_to_engine();

        find_in_files_.set_search_callback(
            [this](const std::string &query, bool case_sensitive)
            { on_find_in_files(query, case_sensitive); });
        find_in_files_.set_jump_callback(
            [this](const std::filesystem::path &f, std::uint32_t l)
            { on_jump_to_diagnostic(f, l); });

        package_manager_ = std::make_unique<engine::PackageManager>();
        load_default_packages(*package_manager_);

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
            { on_stop(); },
            /*reload_packages=*/[this]
            { on_reload_packages(); },
            /*open_file_finder=*/[this]
            { open_file_finder(); },
            /*open_symbol_finder=*/[this]
            { open_symbol_finder(); },
            /*open_find_in_files=*/[this]
            { open_find_in_files(); });

        main_shell_.set_panel_visibility(&console_.visible(),
                                         &explorer_.visible(),
                                         &script_editor_.visible(),
                                         &problems_.visible(),
                                         &package_panel_.visible(),
                                         &diagnostics_panel_.visible(),
                                         &find_in_files_.visible());

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

    void EditorApp::load_default_packages(engine::PackageManager &mgr)
    {
        mgr.load_package(std::make_unique<engine::CorePackage>());
        mgr.load_package(std::make_unique<engine::LogicPackage>());
        mgr.load_package(std::make_unique<engine::MathPackage>());
        mgr.load_package(std::make_unique<engine::RenderPackage>());
        mgr.load_package(std::make_unique<engine::UIPackage>());
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
        if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_F5,
                            ImGuiInputFlags_RouteGlobal))
        {
            on_stop();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_D,
                            ImGuiInputFlags_RouteGlobal))
        {
            diagnostics_panel_.visible() = !diagnostics_panel_.visible();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P,
                            ImGuiInputFlags_RouteGlobal))
        {
            if (!file_finder_.is_open())
                open_file_finder();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_O,
                            ImGuiInputFlags_RouteGlobal))
        {
            if (!symbol_finder_.is_open())
                open_symbol_finder();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_F,
                            ImGuiInputFlags_RouteGlobal))
        {
            open_find_in_files();
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
            diagnostics_panel_.draw();
            console_.draw();
            package_panel_.draw();
            find_in_files_.draw();

            file_finder_.draw();
            if (auto chosen = file_finder_.take_open_request())
            {
                std::string error;
                const int idx = script_editor_.open(*chosen, error);
                if (idx < 0 && !error.empty())
                {
                    engine::log_error("%s", error.c_str());
                }
            }

            file_finder_.draw();
            if (auto chosen = file_finder_.take_open_request())
            {
                std::string error;
                const int idx = script_editor_.open(*chosen, error);
                if (idx < 0 && !error.empty())
                    engine::log_error("%s", error.c_str());
            }

            symbol_finder_.draw();
            if (auto chosen = symbol_finder_.take_open_request())
            {
                script_editor_.request_jump(chosen->first, chosen->second);
            }

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

        symbol_index_.rebuild(project_->root());
        engine::log_info("Symbol index: %zu entries",
                         symbol_index_.size());

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
        runtime_diagnostics_.clear();
        build_status_.clear();
        main_shell_.set_build_status(build_status_);
        symbol_index_.clear();
        launch_.set_error("");
        mode_ = Mode::Launch;

        // Return to the launch ini.
        pending_ini_ = paths::global_config_dir() / "launch.ini";

        refresh_launch_scan();
    }

    void EditorApp::open_file_finder()
    {
        if (mode_ != Mode::Project)
            return;

        // Flatten the Explorer's current tree. This is the same set
        // of files the Explorer shows; the finder does not re-walk
        // the project.
        std::vector<std::filesystem::path> files;
        files.reserve(256);

        const ExplorerEntry &tree = explorer_.tree();
        std::function<void(const ExplorerEntry &)> walk =
            [&](const ExplorerEntry &e)
        {
            if (e.kind == ExplorerEntryKind::File)
            {
                files.push_back(e.path);
            }
            else
            {
                for (const auto &child : e.children)
                    walk(child);
            }
        };
        walk(tree);

        std::sort(files.begin(), files.end());

        file_finder_.open(std::move(files));
    }

    void EditorApp::open_symbol_finder()
    {
        if (mode_ != Mode::Project)
            return;

        // The index is already built (enter_project built it, save
        // refreshes per-file). Find with an empty query returns
        // the full list.
        auto matches = symbol_index_.find("");
        symbol_finder_.open(std::move(matches));
    }

    void EditorApp::open_find_in_files()
    {
        if (mode_ != Mode::Project)
            return;

        find_in_files_.visible() = true;
        find_in_files_.focus_query();
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
            return;
        }
        if (mode_ == Mode::Project && project_)
            symbol_index_.rebuild(project_->root());
    }

    void EditorApp::on_save_all()
    {
        std::string error;
        if (!script_editor_.save_all(error) && !error.empty())
        {
            engine::log_error("%s", error.c_str());
            return;
        }
        if (mode_ == Mode::Project && project_)
            symbol_index_.rebuild(project_->root());
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

    void EditorApp::on_reload_packages()
    {
        if (mode_ != Mode::Project)
            return;

        engine::log_info("Reloading packages...");

        if (run_.is_running())
            engine::log_warn("Reload Packages does not affect the running game; "
                             "stop and run again to apply changes.");

        // Build the replacement first. If apply fails, keep the
        // current manager: a failed reload should not leave the
        // editor in a worse state than before.
        auto new_manager = std::make_unique<engine::PackageManager>();
        load_default_packages(*new_manager);

        if (!new_manager->apply_registration_only())
        {
            engine::log_error("Reload failed: package registration failed");
            for (const auto &c : new_manager->conflicts())
                engine::log_error("  %s", c.c_str());
            return;
        }

        // Re-point every consumer of the old manager BEFORE
        // replacing package_manager_. The old manager is destroyed
        // on the assignment below; any consumer still holding a
        // pointer to it would dangle.
        package_panel_.set_manager(new_manager.get());
        script_editor_.set_package_manager(new_manager.get());
        main_shell_.set_package_count(new_manager->package_infos().size());

        // Replace. The old manager is destroyed here.
        package_manager_ = std::move(new_manager);

        // Recompile every open document against the new registry.
        // The file text is unchanged; only the diagnostics are
        // recomputed. This updates the Problems panel via the store.
        script_editor_.recompile_all();

        engine::log_info("Packages reloaded: %zu package%s, %zu node types",
                         package_manager_->package_infos().size(),
                         package_manager_->package_infos().size() == 1 ? "" : "s",
                         package_manager_->registry().nodeTypes.size());
    }

    void EditorApp::on_find_in_files(const std::string &query,
                                     bool case_sensitive)
    {
        if (mode_ != Mode::Project || !project_)
            return;

        if (query.empty())
        {
            find_in_files_.set_results("", {});
            return;
        }

        // Walk the Explorer's tree, same skip rules as the file
        // finder. Flatten to a sorted list, then search each file.
        std::vector<std::filesystem::path> files;
        files.reserve(256);
        std::function<void(const ExplorerEntry &)> walk =
            [&](const ExplorerEntry &e)
        {
            if (e.kind == ExplorerEntryKind::File)
                files.push_back(e.path);
            else
                for (const auto &child : e.children)
                    walk(child);
        };
        walk(explorer_.tree());
        std::sort(files.begin(), files.end());

        FindOptions opts;
        opts.case_sensitive = case_sensitive;

        std::vector<FindMatch> matches;
        search_files(files, query, opts, matches);

        engine::log_info(
            "Find in files: \"%s\" -> %zu match%s in %zu file%s",
            query.c_str(),
            matches.size(), matches.size() == 1 ? "" : "es",
            files.size(), files.size() == 1 ? "" : "s");

        find_in_files_.set_results(query, std::move(matches));
    }

} // namespace editor