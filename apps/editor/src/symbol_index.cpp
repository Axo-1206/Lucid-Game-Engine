#include "editor/symbol_index.h"

#include "editor/panels/file_finder.h" // file_finder_icontains

#include "engine/core/log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>
#include <string>

namespace editor
{

    const char *symbol_kind_name(SymbolKind k)
    {
        switch (k)
        {
        case SymbolKind::Import:
            return "import";
        case SymbolKind::Enum:
            return "enum";
        case SymbolKind::Resource:
            return "resource";
        case SymbolKind::Node:
            return "node";
        }
        return "symbol";
    }

    // ─────────────────────────────────────────────────────────────────
    // strip_comments_and_strings
    // ─────────────────────────────────────────────────────────────────

    std::string strip_comments_and_strings(const std::string &source)
    {
        std::string out;
        out.reserve(source.size());

        enum class State
        {
            Normal,
            LineComment,
            BlockComment,
            String,
        };
        State state = State::Normal;

        std::size_t i = 0;
        while (i < source.size())
        {
            const char c = source[i];
            const char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (state)
            {
            case State::Normal:
                if (c == '/' && next == '/')
                {
                    state = State::LineComment;
                    out.push_back(' ');
                    out.push_back(' ');
                    i += 2;
                    continue;
                }
                if (c == '/' && next == '*')
                {
                    state = State::BlockComment;
                    out.push_back(' ');
                    out.push_back(' ');
                    i += 2;
                    continue;
                }
                if (c == '"')
                {
                    state = State::String;
                    out.push_back(' ');
                    ++i;
                    continue;
                }
                out.push_back(c);
                ++i;
                break;

            case State::LineComment:
                if (c == '\n')
                {
                    state = State::Normal;
                    out.push_back('\n');
                }
                else
                {
                    out.push_back(' ');
                }
                ++i;
                break;

            case State::BlockComment:
                // Inside a block comment, a newline is preserved
                // so line numbers stay accurate; other characters
                // become spaces.
                if (c == '*' && next == '/')
                {
                    state = State::Normal;
                    out.push_back(' ');
                    out.push_back(' ');
                    i += 2;
                    continue;
                }
                out.push_back(c == '\n' ? '\n' : ' ');
                ++i;
                break;

            case State::String:
                // Escape handling: a backslash inside a string
                // skips the next character so "\"" does not
                // terminate the string.
                if (c == '\\' && i + 1 < source.size())
                {
                    out.push_back(' ');
                    out.push_back(' ');
                    i += 2;
                    continue;
                }
                if (c == '"')
                {
                    state = State::Normal;
                    out.push_back(' ');
                    ++i;
                    continue;
                }
                out.push_back(c == '\n' ? '\n' : ' ');
                ++i;
                break;
            }
        }

        return out;
    }

    // ─────────────────────────────────────────────────────────────────
    // scan_declarations
    // ─────────────────────────────────────────────────────────────────

    void scan_declarations(const std::filesystem::path &file,
                           const std::string &cleaned_source,
                           std::vector<SymbolEntry> &out)
    {
        // One regex per declaration form. All four use an optional
        // attribute prefix.
        static const std::regex import_re(
            R"(^\s*(?:@\w+\s+)*import\s+([A-Za-z_]\w*))");
        static const std::regex enum_re(
            R"(^\s*(?:@\w+\s+)*enum\s+([A-Za-z_]\w*)\s*\{)");
        static const std::regex resource_re(
            R"(^\s*(?:@\w+\s+)*resource\s+([A-Za-z_]\w*)\s*\{)");
        static const std::regex node_re(
            R"(^\s*(?:@\w+\s+)*node\s+([A-Za-z_]\w*)\s*=)");

        std::uint32_t line = 1;
        std::size_t line_start = 0;

        while (line_start <= cleaned_source.size())
        {
            std::size_t line_end = cleaned_source.find('\n', line_start);
            if (line_end == std::string::npos)
                line_end = cleaned_source.size();

            const std::string_view line_text(
                cleaned_source.data() + line_start,
                line_end - line_start);

            // Match each form. A line is at most one declaration.
            std::match_results<std::string_view::const_iterator> m;

            auto try_match = [&](const std::regex &re,
                                 SymbolKind kind) -> bool
            {
                if (std::regex_search(line_text.begin(), line_text.end(),
                                      m, re))
                {
                    SymbolEntry e;
                    e.name = m[1].str();
                    e.kind = kind;
                    e.file = file;
                    e.line = line;
                    out.push_back(std::move(e));
                    return true;
                }
                return false;
            };

            if (try_match(import_re, SymbolKind::Import))
            {
            }
            else if (try_match(enum_re, SymbolKind::Enum))
            {
            }
            else if (try_match(resource_re, SymbolKind::Resource))
            {
            }
            else
            {
                try_match(node_re, SymbolKind::Node);
            }

            if (line_end == cleaned_source.size())
                break;
            line_start = line_end + 1;
            ++line;
        }
    }

    // ─────────────────────────────────────────────────────────────────
    // SymbolIndex
    // ─────────────────────────────────────────────────────────────────

    namespace
    {
        // Collect every .lucid under `root`, recursively. Sorted
        // for determinism.
        std::vector<std::filesystem::path> collect_lucid_files(
            const std::filesystem::path &root)
        {
            std::vector<std::filesystem::path> files;

            std::error_code ec;
            if (!std::filesystem::is_directory(root, ec) || ec)
                return files;

            std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            if (ec)
                return files;

            for (const auto &entry : it)
            {
                std::error_code entry_ec;
                if (!entry.is_regular_file(entry_ec) || entry_ec)
                    continue;
                if (entry.path().extension() == ".lucid")
                    files.push_back(entry.path());
            }

            std::sort(files.begin(), files.end());
            return files;
        }

        bool read_file(const std::filesystem::path &path, std::string &out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return false;
            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }
    } // namespace

    void SymbolIndex::rebuild(const std::filesystem::path &project_root)
    {
        entries_.clear();

        const auto files = collect_lucid_files(project_root / "scripts");
        for (const auto &file : files)
        {
            std::string source;
            if (!read_file(file, source))
            {
                engine::log_warn("SymbolIndex: cannot read %s",
                                 file.string().c_str());
                continue;
            }
            const std::string cleaned = strip_comments_and_strings(source);
            scan_declarations(file, cleaned, entries_);
        }
    }

    void SymbolIndex::refresh_file(const std::filesystem::path &file)
    {
        // Remove this file's entries, then re-scan.
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                           [&](const SymbolEntry &e)
                           { return e.file == file; }),
            entries_.end());

        if (file.extension() != ".lucid")
            return;

        std::string source;
        if (!read_file(file, source))
            return;

        const std::string cleaned = strip_comments_and_strings(source);
        scan_declarations(file, cleaned, entries_);
    }

    void SymbolIndex::clear()
    {
        entries_.clear();
    }

    std::vector<SymbolEntry> SymbolIndex::find(const std::string &query) const
    {
        std::vector<SymbolEntry> out;
        out.reserve(entries_.size());
        for (const auto &e : entries_)
        {
            if (file_finder_icontains(e.name, query))
                out.push_back(e);
        }
        return out;
    }

} // namespace editor