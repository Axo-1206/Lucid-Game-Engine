#include "editor/diagnostics.h"

#include <algorithm>

namespace editor
{

    namespace
    {

        bool is_error(engine::CompileDiagnostic::Severity s)
        {
            using S = engine::CompileDiagnostic::Severity;
            return s == S::Error || s == S::Fatal;
        }

        bool is_warning(engine::CompileDiagnostic::Severity s)
        {
            return s == engine::CompileDiagnostic::Severity::Warning;
        }

    } // namespace

    void DiagnosticStore::replace_for_file(
        const std::filesystem::path &file,
        std::vector<DiagnosticEntry> entries)
    {
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                           [&](const DiagnosticEntry &e)
                           {
                               return e.file == file;
                           }),
            entries_.end());

        entries_.insert(entries_.end(),
                        std::make_move_iterator(entries.begin()),
                        std::make_move_iterator(entries.end()));
    }

    void DiagnosticStore::clear_for_file(const std::filesystem::path &file)
    {
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                           [&](const DiagnosticEntry &e)
                           {
                               return e.file == file;
                           }),
            entries_.end());
    }

    void DiagnosticStore::clear()
    {
        entries_.clear();
    }

    void DiagnosticStore::add(DiagnosticEntry entry)
    {
        entries_.push_back(std::move(entry));
    }

    std::vector<DiagnosticEntry> DiagnosticStore::for_file(
        const std::filesystem::path &file) const
    {
        std::vector<DiagnosticEntry> out;
        for (const auto &e : entries_)
            if (e.file == file)
                out.push_back(e);
        return out;
    }

    int DiagnosticStore::error_count() const
    {
        int n = 0;
        for (const auto &e : entries_)
            if (is_error(e.severity))
                ++n;
        return n;
    }

    int DiagnosticStore::warning_count() const
    {
        int n = 0;
        for (const auto &e : entries_)
            if (is_warning(e.severity))
                ++n;
        return n;
    }

} // namespace editor