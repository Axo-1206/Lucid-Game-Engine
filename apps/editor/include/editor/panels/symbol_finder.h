#pragma once

#include "editor/symbol_index.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace editor
{

    // The symbol finder modal. Same structure as FileFinderPanel:
    // open with a list of entries, filter by substring, confirm
    // with Enter or click, and dispatch a jump request that the
    // editor consumes at the end of the frame.
    //
    // A symbol "open" is a (file, line) pair; the editor routes it
    // through ScriptEditorPanel::request_jump, so the file opens
    // (if not open) and the cursor moves to the line.
    class SymbolFinderPanel
    {
    public:
        void open(std::vector<SymbolEntry> entries);
        void draw();
        bool is_open() const { return open_; }

        // If the user confirmed, returns the chosen (path, line).
        // Otherwise nullopt.
        std::optional<std::pair<std::filesystem::path, std::uint32_t>>
        take_open_request();

        void close();

    private:
        bool open_ = false;
        bool request_focus_ = false;

        std::vector<SymbolEntry> entries_;
        int selected_ = 0;
        char query_[256] = {};

        std::optional<std::pair<std::filesystem::path, std::uint32_t>>
            pending_open_;

        std::vector<int> filtered_indices_;

        void refilter();

        static bool matches(const SymbolEntry &e, const char *query);
    };

} // namespace editor