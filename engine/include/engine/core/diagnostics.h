#pragma once

#include "engine/core/log.h"

#include <cstdint>
#include <functional>
#include <string>

namespace engine
{

    // ─── What this is ─────────────────────────────────────────────────────
    //
    // A structured runtime diagnostic. Unlike LogRecord, this carries
    // the diagnostic code and the source location, not just prose.
    //
    // The two streams are distinct on purpose:
    //
    //   - log_* / LogRecord / LogSink   — human-readable messages,
    //     ordered, filtered by level. The Console panel reads this.
    //
    //   - emit_diagnostic / RuntimeDiagnostic / DiagnosticSink — the
    //     machine-readable form: a code, a severity, an optional
    //     location, a message. The Diagnostics panel (phase 2,
    //     step 4) reads this.
    //
    // The two do not share data. A Ser_FingerprintMismatch is a
    // diagnostic; the Console sees its human-readable line because
    // emit_diagnostic also logs, but the Console's sink receives a
    // LogRecord, not a RuntimeDiagnostic. The Diagnostics panel
    // receives a RuntimeDiagnostic, not a LogRecord. Decision 6.
    //
    // ─── Lifecycle ────────────────────────────────────────────────────────
    //
    // A sink is registered with add_diagnostic_sink and removed with
    // remove_diagnostic_sink, exactly like log sinks. Both are
    // thread-safe. A sink may be called from any thread that emits;
    // the sink itself is responsible for its own locking if it does
    // anything beyond copying the value.

    struct RuntimeDiagnostic
    {
        // The raw lucid::diag::DiagCode value (e.g. 6041 for
        // Ser_FingerprintMismatch). 0 means "free text" — the
        // diagnostic has no code identity, matching
        // lucid::diag::DiagCode(0). engine::diagnostic_code_name
        // maps a value to a display band.
        std::uint32_t code = 0;

        // The severity, in the log's vocabulary. Severity is derived
        // from the code's range for compile-time diagnostics (per
        // DiagCode.hpp); runtime call sites set it explicitly.
        LogLevel severity = LogLevel::Info;

        // Source location. May be empty/zero when unknown (e.g. a
        // file-level serialization error has a `file` but no
        // `line`).
        std::string file;
        std::uint32_t line = 0;
        std::uint32_t column = 0;

        // The message text. No substitutions are performed here; the
        // producer has already formatted it.
        std::string message;
    };

    using DiagnosticSink = std::function<void(const RuntimeDiagnostic &)>;
    using DiagnosticSinkId = std::uint32_t;

    // Register a sink. Returns an ID for removal. Thread-safe.
    // Registering a null sink is a no-op and returns 0.
    DiagnosticSinkId add_diagnostic_sink(DiagnosticSink sink);

    // Remove a sink by ID. No-op if the ID is unknown. Thread-safe.
    void remove_diagnostic_sink(DiagnosticSinkId id);

    // Remove every sink. Used by tests; a production process does not
    // call this.
    void clear_diagnostic_sinks();

    // Emit a diagnostic to every registered sink, then log a
    // human-readable line via log_message at the given severity.
    //
    // Fatal severity aborts: emit_diagnostic with LogLevel::Fatal
    // fires the sinks, then calls log_fatal (which aborts). This is
    // the single place the abort behavior lives. Do not call
    // log_fatal directly from a diagnostic producer.
    //
    // A message containing printf format specifiers is safe: the
    // implementation passes it as "%s", not as the format.
    void emit_diagnostic(const RuntimeDiagnostic &d);

} // namespace engine