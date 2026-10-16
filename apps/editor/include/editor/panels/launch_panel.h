#pragma once

#include "editor/project_roots.h"
#include "editor/settings.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace editor
{

    // The launch panel: shown when no project is loaded.
    //
    // Draws a single list of projects (scanned from the roots,
    // plus recent projects not under any current root) and two
    // buttons: "Open Project" and "New Project". Each opens a
    // modal. When the user confirms, the panel calls back into
    // EditorApp.
    //
    // The panel does not own the project, the recent list, or the
    // scan results. It reads them for display; it calls callbacks
    // for actions.
    class LaunchPanel
    {
    public:
        using OpenCallback =
            std::function<void(const std::filesystem::path &)>;
        using CreateCallback =
            std::function<void(const std::filesystem::path &,
                               const std::string &)>;
        using QuitCallback = std::function<void()>;
        using RefreshCallback = std::function<void()>;

        void set_callbacks(OpenCallback on_open,
                           CreateCallback on_create,
                           QuitCallback on_quit,
                           RefreshCallback on_refresh);

        // Display sources. The panel does not own any of these.
        void set_recent(const RecentProjects *recent);
        void set_scan_results(std::vector<ProjectEntry> entries);
        void set_default_project_root(std::filesystem::path root);

        // Set or clear the error message shown under the buttons.
        void set_error(std::string error);

        // Draw one frame.
        void draw();

    private:
        // Merged display entry: a project with a folder and name.
        struct ListEntry
        {
            std::filesystem::path folder;
            std::string name;
            bool valid = true;
            bool from_scan = false;
        };

        char open_path_[512] = {};
        char new_name_[128] = {};
        char new_parent_[512] = {};

        bool show_open_ = false;
        bool show_new_ = false;
        bool new_advanced_ = false;

        std::string error_;

        OpenCallback on_open_;
        CreateCallback on_create_;
        QuitCallback on_quit_;
        RefreshCallback on_refresh_;

        const RecentProjects *recent_ = nullptr;
        std::vector<ProjectEntry> scan_;
        std::filesystem::path default_root_;

        void draw_list();
        void draw_open_modal();
        void draw_new_modal();
        std::vector<ListEntry> merged_entries() const;
    };

} // namespace editor