#include "engine/core/format_diagnostics.h"

#include "engine/core/diagnostics.h" // emit_diagnostic, RuntimeDiagnostic

#include <cstdint>
#include <string>

namespace engine
{

    namespace
    {

        LogLevel map_severity(lucid::diag::Severity s)
        {
            using S = lucid::diag::Severity;
            switch (s)
            {
            case S::Hint:
                return LogLevel::Trace;
            case S::Note:
                return LogLevel::Info;
            case S::Warning:
                return LogLevel::Warn;
            case S::Error:
                return LogLevel::Error;
            case S::Fatal:
                return LogLevel::Fatal;
            }
            return LogLevel::Info;
        }

    } // namespace

    const char *diagnostic_severity_name(lucid::diag::Severity s)
    {
        using S = lucid::diag::Severity;
        switch (s)
        {
        case S::Hint:
            return "hint";
        case S::Note:
            return "note";
        case S::Warning:
            return "warning";
        case S::Error:
            return "error";
        case S::Fatal:
            return "fatal";
        }
        return "?";
    }

    std::uint32_t diagnostic_code_raw(const lucid::diag::Diagnostic &d)
    {
        return lucid::diag::raw(d.code);
    }

    const char *diagnostic_code_name(std::uint32_t code)
    {
        // The band names come from DiagCode.hpp's comments:
        //   1000-1099  Lexical
        //   2000-2199  Syntax
        //   3000-3099  Name
        //   4000-4099  Type
        //   5000-5099  Attribute
        //   5100-5199  Import
        //   5300-5399  Trigger
        //   6000-6099  Serialization
        //   6200-6299  Registry
        //   7000-7099  Internal
        //   8000-8299  Warning
        if (code == 0)
            return "FreeText";
        if (code < 2000)
            return "Lexical";
        if (code < 3000)
            return "Syntax";
        if (code < 4000)
            return "Name";
        if (code < 5000)
            return "Type";
        if (code < 5100)
            return "Attribute";
        if (code < 5200)
            return "Import";
        if (code < 5400)
            return "Trigger";
        if (code < 6100)
            return "Serialization";
        if (code < 6300)
            return "Registry";
        if (code < 7100)
            return "Internal";
        if (code < 9000)
            return "Warning";
        return "Unknown";
    }

    void log_diagnostic(const lucid::diag::Diagnostic &d)
    {
        RuntimeDiagnostic rd;
        rd.code = diagnostic_code_raw(d);
        rd.severity = map_severity(d.severity);
        if (d.location.isKnown())
        {
            rd.line = d.location.line();
            rd.column = d.location.column();
        }
        rd.message = d.message;
        // d.file is an InternedString; resolving it requires the
        // StringPool, which this function does not have. Leave `file`
        // empty. The caller (GraphRuntime::load, the editor's build
        // loop) knows which file it asked about.

        emit_diagnostic(rd);
    }

    void log_diagnostics(const std::vector<lucid::diag::Diagnostic> &diags)
    {
        for (const auto &d : diags)
            log_diagnostic(d);
    }

} // namespace engine