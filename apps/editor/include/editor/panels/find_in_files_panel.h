#pragma once

#include "editor/find_in_files.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace editor
{

    // The Find in Files panel: a dockable results list. The user
    // types a query, presses Enter (or the Search button), and
    // results appear grouped by file. Clicking a result jumps to
    // that file and line.
    //
    // The panel does not walk the project; it calls back into
    // EditorApp for the file list, which comes from the Explorer's
    // tree. That keeps the skip rules in one place.
    class FindInFilesPanel
    {
    public:
        using SearchCallback =
            std::function<void(const std::string &query, bool case_sensitive)>;
        using JumpCallback =
            std::function<void(const std::filesystem::path &, std::uint32_t)>;

        void set_search_callback(SearchCallback cb) { on_search_ = std::move(cb); }
        void set_jump_callback(JumpCallback cb) { on_jump_ = std::move(cb); }

        // Set the results and the query that produced them. The
        // panel displays the query as a summary; the query field
        // itself is not overwritten (so the user can edit it and
        // search again).
        void set_results(std::string query, std::vector<FindMatch> matches);

        // Focus the query field. Called when Ctrl+Shift+F opens
        // the panel.
        void focus_query() { request_focus_ = true; }

        void draw();
        bool &visible() { return visible_; }

    private:
        SearchCallback on_search_;
        JumpCallback on_jump_;

        bool visible_ = true;
        bool request_focus_ = false;
        char query_[256] = {};
        bool case_sensitive_ = false;

        std::string results_query_;
        std::vector<FindMatch> matches_;

        void draw_filter_bar();
        void draw_results();
    };

} // namespace editor