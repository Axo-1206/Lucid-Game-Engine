#pragma once

#include "engine/script/compiler.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor
{

    enum class DiagnosticSource
    {
        Compiler,
        Formatter,
        Loader,
        Runtime,
    };

    struct DiagnosticEntry
    {
        std::filesystem::path file;
        std::uint32_t line = 0;   // 1-based; 0 = unknown
        std::uint32_t column = 0; // 1-based; 0 = unknown
        engine::CompileDiagnostic::Severity severity =
            engine::CompileDiagnostic::Severity::Note;
        std::string message;
        DiagnosticSource source = DiagnosticSource::Compiler;
    };

    // A flat collection of diagnostics, grouped by file on replace.
    // Not thread-safe; the editor's compile loop runs on the main
    // thread.
    class DiagnosticStore
    {
    public:
        // Replace all diagnostics for a file.
        void replace_for_file(const std::filesystem::path &file,
                              std::vector<DiagnosticEntry> entries);

        // Remove all diagnostics for a file.
        void clear_for_file(const std::filesystem::path &file);

        // Remove everything.
        void clear();

        // Append one entry. For sources that append rather than
        // replace.
        void add(DiagnosticEntry entry);

        const std::vector<DiagnosticEntry> &all() const { return entries_; }

        std::vector<DiagnosticEntry> for_file(
            const std::filesystem::path &file) const;

        int error_count() const;
        int warning_count() const;

        std::size_t size() const { return entries_.size(); }
        bool empty() const { return entries_.empty(); }

    private:
        std::vector<DiagnosticEntry> entries_;
    };

} // namespace editor