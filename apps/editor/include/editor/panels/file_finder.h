#pragma once

#include "editor/panels/explorer_panel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace editor
{

    // Case-insensitive substring match. Empty needle matches
    // everything. Non-ASCII bytes are compared as-is (lowercased
    // only for ASCII); this is the same behavior the file finder,
    // the symbol finder, and find-in-files all want.
    //
    // Exposed as a free function for reuse and for tests. The file
    // finder uses it on the file's generic_string(); the symbol
    // finder will use it on a symbol's name; find-in-files will use
    // it on line text.
    bool file_finder_icontains(const std::string &haystack,
                               const std::string &needle);

    // The file finder: a modal opened with Ctrl+P. It flattens the
    // Explorer's tree into a list of file paths, filters by
    // substring as the user types, and on Enter or click records a
    // pending open request that the editor consumes at the end of
    // the frame.
    //
    // The panel does not open files itself; that is EditorApp's job.
    // The panel does not own the tree; EditorApp feeds it on open.
    //
    // Filtering: case-insensitive substring on the file's *path*
    // relative to the project root. A query of "ui" matches
    // "scripts/ui/hud.lucid" and "assets/ui/button.png". Matching on
    // the full relative path (not just the filename) is what makes
    // "scripts/math" narrow to that folder quickly.
    class FileFinderPanel
    {
    public:
        // Open the modal with the given list of candidate files.
        // Paths are expected to be absolute or project-root-relative;
        // the display is whatever `path` converts to, with the query
        // matched against the same.
        void open(std::vector<std::filesystem::path> files);

        // Draw the modal. Called once per frame by EditorApp while
        // the panel is open. If the user confirms, records a pending
        // open request; the editor reads it with take_open_request().
        void draw();

        // True if the modal is currently shown.
        bool is_open() const { return open_; }

        // If the user confirmed since the last call, returns the
        // selected path and clears the request. Otherwise nullopt.
        std::optional<std::filesystem::path> take_open_request();

        // Close without opening anything. Called by EditorApp when
        // a modal supersedes this one, or by the panel itself on
        // Escape.
        void close();

    private:
        bool open_ = false;
        bool request_focus_ = false;

        std::vector<std::filesystem::path> files_;
        int selected_ = 0;

        char query_[256] = {};

        std::optional<std::filesystem::path> pending_open_;

        // The current filtered view. Recomputed when the query
        // changes or the panel opens.
        std::vector<int> filtered_indices_;

        void refilter();

        // True if `path` contains `query` (case-insensitive). Empty
        // query matches everything.
        static bool matches(const std::filesystem::path &path,
                            const char *query);
    };

} // namespace editor