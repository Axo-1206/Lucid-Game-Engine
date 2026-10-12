#include "engine/core/format_diagnostics.h"
#include "engine/core/log.h"

#include <string>

namespace engine
{

    const char *diagnostic_severity_name(lucid::diag::Severity s)
    {
        switch (s)
        {
        case lucid::diag::Severity::Fatal:
            return "fatal";
        case lucid::diag::Severity::Error:
            return "error";
        case lucid::diag::Severity::Warning:
            return "warning";
        case lucid::diag::Severity::Note:
            return "note";
        case lucid::diag::Severity::Hint:
            return "hint";
        }
        return "unknown";
    }

    void log_diagnostic(const lucid::diag::Diagnostic &d)
    {
        std::string line;
        line.reserve(d.message.size() + 32);

        line += "[";
        line += diagnostic_severity_name(d.severity);
        line += "] ";
        line += d.message;

        if (d.location.isKnown())
        {
            line += " at ";
            line += std::to_string(d.location.line());
            line += ":";
            line += std::to_string(d.location.column());
        }

        // Fatal and Error both route to log_error. Fatal is the format
        // library's severity; the engine decides whether an unrecoverable
        // file ends the program or is skipped. The logger does not abort.
        switch (d.severity)
        {
        case lucid::diag::Severity::Fatal:
        case lucid::diag::Severity::Error:
            log_error("%s", line.c_str());
            break;
        case lucid::diag::Severity::Warning:
            log_warn("%s", line.c_str());
            break;
        case lucid::diag::Severity::Note:
        case lucid::diag::Severity::Hint:
            log_info("%s", line.c_str());
            break;
        }
    }

    void log_diagnostics(const std::vector<lucid::diag::Diagnostic> &diags)
    {
        for (const auto &d : diags)
        {
            log_diagnostic(d);
        }
    }

} // namespace engine