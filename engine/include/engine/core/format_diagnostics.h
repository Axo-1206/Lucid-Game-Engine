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

    // The raw DiagCode value, as a plain uint32_t. For callers that
    // must not include lucid/* headers (the editor). Returns 0 for
    // free-text diagnostics.
    std::uint32_t diagnostic_code_raw(const lucid::diag::Diagnostic &d);

    // The human-readable band name for a code, derived from the
    // code's numeric range (DiagCode.hpp's bands):
    //   "Lexical", "Syntax", "Name", "Type", "Attribute", "Import",
    //   "Trigger", "Serialization", "Registry", "Internal",
    //   "Warning", "FreeText", "Unknown".
    //
    // Returns "FreeText" for code 0 and "Unknown" for codes that do
    // not fall in any band. Never returns nullptr.
    //
    // The exact enum spelling (e.g. "Ser_FingerprintMismatch") is
    // deferred: a full spelling switch is 60 cases and must be kept
    // in sync with DiagCode.hpp's enum. The band is enough for the
    // Diagnostics panel's first version.
    const char *diagnostic_code_name(std::uint32_t code);

} // namespace engine