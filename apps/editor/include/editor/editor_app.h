#pragma once

#include "editor/imgui_layer.h"
#include "editor/panels/console_panel.h"
#include "editor/panels/explorer_panel.h"
#include "editor/panels/launch_panel.h"
#include "editor/panels/main_shell.h"
#include "editor/panels/package_panel.h"
#include "editor/panels/problems_panel.h"
#include "editor/panels/script_editor_panel.h"
#include "editor/panels/diagnostics_panel.h"
#include "editor/panels/file_finder.h"
#include "editor/panels/symbol_finder.h"
#include "editor/panels/find_in_files_panel.h"
#include "editor/diagnostics.h"
#include "editor/project.h"
#include "editor/project_roots.h"
#include "editor/settings.h"
#include "editor/run.h"
#include "editor/runtime_diagnostics.h"
#include "editor/symbol_index.h"
#include "editor/find_in_files.h"

#include "engine/core/clock.h"
#include "engine/platform/window.h"
#include "engine/registry/package_manager.h"

#include <filesystem>
#include <memory>
#include <optional>
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
        static void load_default_packages(engine::PackageManager &mgr);

        enum class Mode
        {
            Launch,
            Project,
        };

        void frame();
        void process_shortcuts();

        void enter_project(std::unique_ptr<Project> project);
        void leave_project();
        void do_leave_project();

        void open_file_finder();
        void open_symbol_finder();

        void on_open_project(const std::filesystem::path &folder);
        void on_create_project(const std::filesystem::path &folder,
                               const std::string &name);
        void on_quit();
        void on_about();
        void on_save();
        void on_save_all();
        void on_reset_layout();

        void on_jump_to_diagnostic(const std::filesystem::path &file,
                                   std::uint32_t line);

        void refresh_launch_scan();
        void save_settings();
        void save_recent();

        Mode mode_ = Mode::Launch;
        std::unique_ptr<Project> project_;

        Settings settings_;
        RecentProjects recent_;
        ProjectRoots roots_;

        std::unique_ptr<engine::PackageManager> package_manager_;
        DiagnosticStore diagnostics_;
        RuntimeDiagnosticStore runtime_diagnostics_;

        LaunchPanel launch_;
        MainShell main_shell_;
        ConsolePanel console_;
        ExplorerPanel explorer_;
        ScriptEditorPanel script_editor_;
        ProblemsPanel problems_;
        PackagePanel package_panel_;
        DiagnosticsPanel diagnostics_panel_;
        FileFinderPanel file_finder_;
        FindInFilesPanel find_in_files_;

        SymbolIndex symbol_index_;
        SymbolFinderPanel symbol_finder_;

        engine::Window window_;
        engine::Clock clock_;
        ImGuiLayer imgui_;

        std::optional<std::filesystem::path> pending_ini_;

        bool initialized_ = false;
        bool quit_requested_ = false;

        void on_build();
        std::string build_status_;

        RunProcess run_;
        std::string run_status_;
        void update_run_status();

        void on_run();
        void on_stop();
        void on_reload_packages();
        void open_find_in_files();
        void on_find_in_files(const std::string &query, bool case_sensitive);
    };

} // namespace editor