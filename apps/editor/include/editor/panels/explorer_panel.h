#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace editor
{

    // What kind of entry the tree holds.
    enum class ExplorerEntryKind
    {
        Directory,
        File,
    };

    // One node in the tree. Directories have children; files do not.
    struct ExplorerEntry
    {
        ExplorerEntryKind kind = ExplorerEntryKind::File;
        std::filesystem::path path;          // absolute
        std::string name;                    // just the filename
        std::vector<ExplorerEntry> children; // empty for files
    };

    // Walk `root` and build a tree. Directories are walked
    // recursively. Hidden directories, common build/tool
    // directories, compiled output, temp files, and symlinks are
    // skipped.
    //
    // If `root` does not exist or is not a directory, returns an
    // empty tree with `kind == Directory`.
    ExplorerEntry build_explorer_tree(const std::filesystem::path &root);

    // The Explorer panel. Shows the project's file tree.
    //
    // The panel does not own the tree's source data; it reads from
    // the project root and rebuilds on demand (open, refresh).
    // Files that the user double-clicks are recorded as pending
    // open requests; the editor consumes them via
    // `take_open_request`.
    class ExplorerPanel
    {
    public:
        void set_root(const std::filesystem::path &project_root);
        void clear();

        // Rebuild the tree from the current root.
        void refresh();

        void draw();

        // If the user double-clicked a file since the last call,
        // returns its path and clears the request.
        std::optional<std::filesystem::path> take_open_request();

        // Visibility. Called by the Window menu and by the
        // keyboard shortcut.
        bool &visible() { return visible_; }

        // For tests.
        const ExplorerEntry &tree() const { return tree_; }
        bool has_tree() const { return has_tree_; }

    private:
        ExplorerEntry tree_;
        bool has_tree_ = false;
        bool visible_ = true;
        std::filesystem::path root_;
        std::optional<std::filesystem::path> pending_open_;

        void draw_entry(const ExplorerEntry &entry);
    };

} // namespace editor