#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace editor
{

    // One hit: where the match is and the line's text (trimmed
    // to a reasonable length for display).
    struct FindMatch
    {
        std::filesystem::path file;
        std::uint32_t line = 0;   // 1-based
        std::uint32_t column = 0; // 1-based
        std::string line_text;    // the full line, trimmed of trailing CR
    };

    struct FindOptions
    {
        bool case_sensitive = false;
    };

    // Search a single file for `query` and append hits to `out`.
    // Empty query matches nothing. Reads the file as text; binary
    // files (a NUL in the first few hundred bytes) are skipped.
    //
    // Lines are capped at a display length (`kMaxLineText`) so a
    // pathological long line (minified JSON) doesn't bloat memory.
    void search_file(const std::filesystem::path &file,
                     const std::string &query,
                     const FindOptions &options,
                     std::vector<FindMatch> &out);

    // The set of files the search walks: every file under
    // `project_root` that the Explorer would show. This is the
    // Explorer's tree flattened, sorted. `project_root` is used only
    // for display in callers; the actual walk is over the Explorer's
    // tree, passed in.
    void search_files(const std::vector<std::filesystem::path> &files,
                      const std::string &query,
                      const FindOptions &options,
                      std::vector<FindMatch> &out);

    // The maximum line length kept in `FindMatch::line_text`.
    inline constexpr std::size_t kMaxLineText = 512;

} // namespace editor