#include "engine/core/diagnostics.h"

#include "engine/core/format_diagnostics.h" // diagnostic_code_name

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace engine
{

    namespace
    {

        struct SinkEntry
        {
            DiagnosticSinkId id;
            DiagnosticSink sink;
        };

        // The sink list. A parallel type to log.cpp's; kept separate
        // because the two streams are separate.
        struct SinkState
        {
            std::mutex mutex;
            std::vector<SinkEntry> sinks;
            DiagnosticSinkId next_id = 1;
        };

        SinkState &state()
        {
            static SinkState s;
            return s;
        }

        // Format one line for the log. Matches the shape the existing
        // log_diagnostic used: "<code-or-band>: <message>" with
        // " at L:C" appended when the location is known.
        std::string format_one_line(const RuntimeDiagnostic &d)
        {
            const char *band = diagnostic_code_name(d.code);

            std::string out;
            std::size_t cap = d.message.size() + 32;
            if (band != nullptr)
                cap += std::char_traits<char>::length(band);
            out.reserve(cap);

            if (band != nullptr && *band != '\0')
            {
                out += band;
                out += ": ";
            }
            out += d.message;

            if (d.line != 0)
            {
                out += " at ";
                out += std::to_string(d.line);
                out += ':';
                out += std::to_string(d.column);
            }

            return out;
        }

    } // namespace

    DiagnosticSinkId add_diagnostic_sink(DiagnosticSink sink)
    {
        if (!sink)
            return 0;

        SinkState &s = state();
        std::lock_guard<std::mutex> lock(s.mutex);

        const DiagnosticSinkId id = s.next_id++;
        s.sinks.push_back({id, std::move(sink)});
        return id;
    }

    void remove_diagnostic_sink(DiagnosticSinkId id)
    {
        if (id == 0)
            return;

        SinkState &s = state();
        std::lock_guard<std::mutex> lock(s.mutex);

        for (auto it = s.sinks.begin(); it != s.sinks.end(); ++it)
        {
            if (it->id == id)
            {
                s.sinks.erase(it);
                return;
            }
        }
    }

    void clear_diagnostic_sinks()
    {
        SinkState &s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.sinks.clear();
    }

    void emit_diagnostic(const RuntimeDiagnostic &d)
    {
        // Snapshot the sinks under the lock, then call them outside
        // it. A sink may register another sink; calling outside the
        // lock avoids a re-entrant deadlock.
        std::vector<DiagnosticSink> sinks_copy;
        {
            SinkState &s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            sinks_copy.reserve(s.sinks.size());
            for (const auto &e : s.sinks)
                sinks_copy.push_back(e.sink);
        }

        for (const auto &sink : sinks_copy)
            sink(d);

        // Log the human-readable line at the given severity. Fatal
        // aborts, via log_fatal.
        const std::string line = format_one_line(d);
        if (d.severity == LogLevel::Fatal)
            log_fatal("%s", line.c_str());
        else
            log_message(d.severity, "%s", line.c_str());
    }

} // namespace engine