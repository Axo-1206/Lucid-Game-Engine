#pragma once

#include "engine/core/diagnostics.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

namespace editor
{

    // One entry in the diagnostics panel. A copy of the fields the
    // panel draws, plus the engine's severity mapped to the editor's
    // CompileDiagnostic::Severity so the two panels can share
    // rendering helpers if they ever want to.
    struct RuntimeDiagnosticEntry
    {
        std::uint32_t code = 0;
        engine::LogLevel severity = engine::LogLevel::Info;
        std::filesystem::path file; // empty = no file
        std::uint32_t line = 0;
        std::uint32_t column = 0;
        std::string message;
    };

    // The ring buffer that the Diagnostics panel reads. Fed by a
    // diagnostic sink the panel registers at startup. Thread-safe:
    // the sink may be called from any thread that emits.
    //
    // Capped at max_entries. New entries push to the back; when the
    // cap is reached, the oldest entry is dropped. This matches the
    // Console's ring buffer behavior.
    class RuntimeDiagnosticStore
    {
    public:
        explicit RuntimeDiagnosticStore(std::size_t max_entries = 10000);

        // Called from the diagnostic sink. Thread-safe.
        void push(const engine::RuntimeDiagnostic &d);

        // Snapshot. Thread-safe.
        std::deque<RuntimeDiagnosticEntry> snapshot() const;

        std::size_t size() const;
        void clear();

        // Counts by severity. Thread-safe.
        int error_count() const;
        int warning_count() const;

    private:
        std::size_t max_entries_;
        std::deque<RuntimeDiagnosticEntry> entries_;
        mutable std::mutex mutex_;
    };

} // namespace editor