#include "editor/runtime_diagnostics.h"

namespace editor
{

    RuntimeDiagnosticStore::RuntimeDiagnosticStore(std::size_t max_entries)
        : max_entries_(max_entries ? max_entries : 1)
    {
    }

    void RuntimeDiagnosticStore::push(const engine::RuntimeDiagnostic &d)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        RuntimeDiagnosticEntry e;
        e.code = d.code;
        e.severity = d.severity;
        e.file = d.file;
        e.line = d.line;
        e.column = d.column;
        e.message = d.message;

        entries_.push_back(std::move(e));
        while (entries_.size() > max_entries_)
            entries_.pop_front();
    }

    std::deque<RuntimeDiagnosticEntry>
    RuntimeDiagnosticStore::snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    std::size_t RuntimeDiagnosticStore::size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    void RuntimeDiagnosticStore::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    int RuntimeDiagnosticStore::error_count() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int n = 0;
        for (const auto &e : entries_)
            if (e.severity == engine::LogLevel::Error ||
                e.severity == engine::LogLevel::Fatal)
                ++n;
        return n;
    }

    int RuntimeDiagnosticStore::warning_count() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int n = 0;
        for (const auto &e : entries_)
            if (e.severity == engine::LogLevel::Warn)
                ++n;
        return n;
    }

} // namespace editor