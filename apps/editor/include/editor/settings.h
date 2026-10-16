#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace editor
{

    // The editor's global preferences. Machine-local; not stored
    // in the project. Persisted as settings.json in
    // paths::global_config_dir().
    //
    // Defaults are chosen so that a user who never opens the
    // Preferences panel gets a working editor.
    struct Settings
    {
        std::string theme = "dark";
        float font_size = 14.0f;
        bool show_console_on_start = true;

        // The parent folder the "New Project" dialog opens in.
        // Empty means "use the OS default" (home directory, or
        // the OS's Documents folder).
        std::string default_new_project_parent;

        // Extra roots the editor scans for projects. The platform
        // default root is not stored here; it is computed from
        // PlatformPaths on every launch.
        std::vector<std::string> project_roots;
    };

    // One entry in the recent-projects list. `path` is the
    // absolute path to the project folder. `name` is the display
    // name cached at the time the project was last opened; it may
    // be stale if the project was renamed on disk. `last_opened`
    // is seconds since the Unix epoch, used only for sorting.
    struct RecentEntry
    {
        std::string path;
        std::string name;
        std::int64_t last_opened = 0;
    };

    // The recent-projects list. Most recent first. Capped at
    // kMaxEntries; the oldest entry is dropped when the list is
    // full.
    struct RecentProjects
    {
        std::vector<RecentEntry> entries;
        static constexpr std::size_t kMaxEntries = 20;
    };

    // Reads and writes the editor's machine-local state.
    //
    // The load functions never fail: a missing file, a malformed
    // file, or an I/O error returns a default-constructed value.
    // An editor that refuses to start because a settings file is
    // bad is worse than an editor that starts with defaults.
    //
    // The save functions return false and fill `error` on failure.
    // The caller (a future Preferences panel) reports the failure.
    class SettingsStore
    {
    public:
        static Settings load_global();
        static bool save_global(const Settings &settings, std::string &error);

        static RecentProjects load_recent();
        static bool save_recent(const RecentProjects &recent, std::string &error);

        // Push an entry to the front of the list. Removes any
        // existing entry with the same path. Truncates to
        // kMaxEntries. Does not write; the caller calls save_recent.
        static void push_recent(RecentProjects &recent,
                                const std::string &path,
                                const std::string &name,
                                std::int64_t timestamp);

        // The current time as seconds since the Unix epoch. A
        // convenience for callers that do not have their own clock.
        static std::int64_t now_seconds();
    };

} // namespace editor