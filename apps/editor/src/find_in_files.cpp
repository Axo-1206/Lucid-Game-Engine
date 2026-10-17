#include "editor/find_in_files.h"

#include "editor/panels/file_finder.h" // file_finder_icontains

#include "engine/core/log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace editor
{

    namespace
    {

        // Case-sensitive substring. The counterpart to
        // file_finder_icontains; same shape, no lowercasing.
        bool contains(const std::string &haystack, const std::string &needle)
        {
            if (needle.empty())
                return true;
            if (needle.size() > haystack.size())
                return false;

            const std::size_t last = haystack.size() - needle.size();
            for (std::size_t i = 0; i <= last; ++i)
            {
                if (std::memcmp(haystack.data() + i, needle.data(),
                                needle.size()) == 0)
                    return true;
            }
            return false;
        }

        // Find the position of `needle` in `line`, honoring the
        // case sensitivity. Returns the byte offset, or
        // std::string::npos.
        std::size_t find_in_line(const std::string &line,
                                 const std::string &needle,
                                 bool case_sensitive)
        {
            if (needle.empty())
                return std::string::npos;

            if (case_sensitive)
            {
                return line.find(needle);
            }

            // Case-insensitive: manual scan.
            auto lower = [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); };

            std::string needle_lower;
            needle_lower.reserve(needle.size());
            for (unsigned char c : needle)
                needle_lower.push_back(lower(c));

            if (needle.size() > line.size())
                return std::string::npos;

            const std::size_t last = line.size() - needle.size();
            for (std::size_t i = 0; i <= last; ++i)
            {
                bool ok = true;
                for (std::size_t j = 0; j < needle.size(); ++j)
                {
                    if (lower(static_cast<unsigned char>(line[i + j])) !=
                        needle_lower[j])
                    {
                        ok = false;
                        break;
                    }
                }
                if (ok)
                    return i;
            }
            return std::string::npos;
        }

        // Read a file into a string. Returns false on failure or
        // if the file looks binary (a NUL byte in the first 8 KiB).
        bool read_text_file(const std::filesystem::path &path,
                            std::string &out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return false;
            std::ostringstream ss;
            ss << in.rdbuf();
            const std::string text = ss.str();

            const std::size_t probe = std::min<std::size_t>(text.size(), 8192);
            for (std::size_t i = 0; i < probe; ++i)
            {
                if (text[i] == '\0')
                    return false; // binary
            }
            out = text;
            return true;
        }

    } // namespace

    void search_file(const std::filesystem::path &file,
                     const std::string &query,
                     const FindOptions &options,
                     std::vector<FindMatch> &out)
    {
        if (query.empty())
            return;

        std::string text;
        if (!read_text_file(file, text))
            return;

        std::uint32_t line = 1;
        std::size_t line_start = 0;
        while (line_start <= text.size())
        {
            std::size_t line_end = text.find('\n', line_start);
            const bool last_line = (line_end == std::string::npos);
            if (last_line)
                line_end = text.size();

            // Strip a trailing CR (Windows line endings).
            std::size_t content_end = line_end;
            if (content_end > line_start &&
                text[content_end - 1] == '\r')
                --content_end;

            const std::string line_text =
                text.substr(line_start, content_end - line_start);

            if (find_in_line(line_text, query, options.case_sensitive) !=
                std::string::npos)
            {
                FindMatch m;
                m.file = file;
                m.line = line;
                m.column = 0; // not filled in; a column is nice but not required
                m.line_text = line_text.size() > kMaxLineText
                                  ? line_text.substr(0, kMaxLineText)
                                  : line_text;
                out.push_back(std::move(m));
            }

            if (last_line)
                break;
            line_start = line_end + 1;
            ++line;
        }
    }

    void search_files(const std::vector<std::filesystem::path> &files,
                      const std::string &query,
                      const FindOptions &options,
                      std::vector<FindMatch> &out)
    {
        if (query.empty())
            return;

        for (const auto &file : files)
        {
            search_file(file, query, options, out);
        }
    }

} // namespace editor