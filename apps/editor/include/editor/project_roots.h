#pragma once

#include "editor/settings.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor
{

    // A directory the editor scans for projects. Two kinds:
    // the platform default root, and any user-added root.
    struct ProjectRoot
    {
        std::filesystem::path path;
        bool is_default = false;
    };

    // One project found by scanning a root.
    struct ProjectEntry
    {
        std::filesystem::path folder;
        std::string name;   // from the project file, or the folder name
        bool valid = false; // has a parseable lucid.project.json
    };

    // The set of roots the editor scans for projects.
    //
    // The platform default root is always present and cannot be
    // removed. User roots come from settings; a user root whose
    // directory does not exist is dropped on load. A user root
    // that exists but contains no projects is kept.
    //
    // Scanning is shallow: one level deep. A subdirectory of a
    // root that contains lucid.project.json is a project.
    class ProjectRoots
    {
    public:
        // Load roots from settings. `default_root`, if non-empty,
        // overrides the platform default. Callers pass an empty
        // path in production and a temp path in tests.
        //
        // The default root directory is created if it does not
        // exist.
        static ProjectRoots load(const Settings &settings,
                                 std::filesystem::path default_root = {});

        // The current list of roots, default first.
        const std::vector<ProjectRoot> &roots() const { return roots_; }

        // The root new projects go into by default.
        const std::filesystem::path &default_root() const { return default_root_; }

        // Add a user root. No-op if it equals an existing root, or
        // equals the default root.
        void add_root(const std::filesystem::path &path);

        // Remove a user root. No-op if the path is the default root
        // or is not present.
        void remove_root(const std::filesystem::path &path);

        // Scan every root, one level deep. For each subdirectory
        // that contains lucid.project.json, add an entry.
        //
        // The entry's `valid` is true if the project file parsed
        // and had a non-empty `name`. Otherwise the entry is
        // marked invalid and its display name is the folder name.
        //
        // Entries are sorted by name (case-insensitive).
        std::vector<ProjectEntry> scan() const;

        // The user roots only, as strings, in order. Used for
        // persistence.
        std::vector<std::string> user_roots_as_strings() const;

    private:
        std::filesystem::path default_root_;
        std::vector<ProjectRoot> roots_;
    };

} // namespace editor