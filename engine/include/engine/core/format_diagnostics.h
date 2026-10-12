#pragma once

#include "core/diagnostics/Diagnostic.hpp"

#include <vector>

namespace engine
{

    // Route a format-library diagnostic to the engine's log.
    //
    // The format library hands the engine a structured Diagnostic —
    // severity, code, message, and an optional source location. This
    // function formats it as a one-line string and calls the engine's
    // log_message at the matching severity.
    //
    // Severity mapping:
    //   Fatal   → log_fatal   (aborts)
    //   Error   → log_error
    //   Warning → log_warn
    //   Note    → log_info
    //   Hint    → log_info
    //
    // A diagnostic with a known source location prints " at L:C".
    // A diagnostic without one (the deserialize path) omits it. This
    // is important: printing " at 0:0" for a file-level diagnostic is
    // worse than printing nothing.
    void log_diagnostic(const lucid::diag::Diagnostic &d);

    // Log every diagnostic in the vector, in order. Convenience for
    // the common case where a caller receives a result struct and wants
    // to log all of its diagnostics before checking `ok`.
    void log_diagnostics(const std::vector<lucid::diag::Diagnostic> &diags);

    // A short human-readable name for a diagnostic's severity, e.g.
    // "error". Matches the spelling the format library uses in its own
    // CLI output, so an engine log line and a `lucid check` line agree.
    const char *diagnostic_severity_name(lucid::diag::Severity s);

} // namespace engine