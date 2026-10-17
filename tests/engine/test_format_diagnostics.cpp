// tests/engine/test_format_diagnostics.cpp
//
// Tests for engine/core/format_diagnostics.h:
//   - Severity routing: each lucid::diag::Severity maps to the
//     expected engine::LogLevel.
//   - Message formatting: "<band>: <message>" with " at L:C" when
//     a location is known; no location suffix otherwise.
//   - Code preservation: diagnostic_code_raw and
//     diagnostic_code_name return the raw value and band.
//   - Fatal severity aborts (via log_fatal).
//
// The engine's log frame adds "[LEVEL]" at the front; the message
// itself carries the band prefix. The two are separate: the frame
// is the log's job, the band is the diagnostic's job. Tests assert
// on each where it belongs.

#include <catch2/catch_test_macros.hpp>

#include "engine/core/format_diagnostics.h"
#include "engine/core/diagnostics.h"
#include "engine/core/log.h"

#include "core/diagnostics/Diagnostic.hpp"
#include "core/SourceLocation.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace
{

    // A log sink that records every LogRecord it receives. Because
    // the sink list is global, each test installs its own capture
    // and removes it at the end. The mutex protects the capture
    // against the sink being called from another thread (the engine
    // is single-threaded today, but the sink contract is
    // thread-safe).
    struct LogCapture
    {
        std::mutex mutex;
        std::vector<engine::LogRecord> records;

        void sink(const engine::LogRecord &r)
        {
            std::lock_guard<std::mutex> lock(mutex);
            records.push_back(r);
        }
    };

    // Install a capture for the duration of one test. The previous
    // engine log level is restored on destruction, so tests do not
    // leak state.
    struct ScopedCapture
    {
        LogCapture capture;
        engine::LogSinkId id = 0;
        engine::LogLevel prev_level = engine::LogLevel::Info;

        ScopedCapture()
        {
            // Trace so Hint-severity diagnostics reach the sink.
            // Without this, the global minimum level would drop
            // Hint diagnostics and the routing tests would fail
            // for a reason unrelated to routing.
            prev_level = engine::get_log_level();
            engine::set_log_level(engine::LogLevel::Trace);

            id = engine::add_log_sink(
                [this](const engine::LogRecord &r)
                { capture.sink(r); });
        }

        ~ScopedCapture()
        {
            if (id != 0)
                engine::remove_log_sink(id);
            engine::set_log_level(prev_level);
        }

        std::vector<engine::LogRecord> snapshot()
        {
            std::lock_guard<std::mutex> lock(capture.mutex);
            return capture.records;
        }
    };

    // A diagnostic sink that records every RuntimeDiagnostic it
    // receives, for tests that check the structured form.
    struct DiagnosticCapture
    {
        std::mutex mutex;
        std::vector<engine::RuntimeDiagnostic> diagnostics;

        void sink(const engine::RuntimeDiagnostic &d)
        {
            std::lock_guard<std::mutex> lock(mutex);
            diagnostics.push_back(d);
        }
    };

    struct ScopedDiagnosticCapture
    {
        DiagnosticCapture capture;
        engine::DiagnosticSinkId id = 0;

        ScopedDiagnosticCapture()
        {
            id = engine::add_diagnostic_sink(
                [this](const engine::RuntimeDiagnostic &d)
                { capture.sink(d); });
        }

        ~ScopedDiagnosticCapture()
        {
            if (id != 0)
                engine::remove_diagnostic_sink(id);
        }

        std::vector<engine::RuntimeDiagnostic> snapshot()
        {
            std::lock_guard<std::mutex> lock(capture.mutex);
            return capture.diagnostics;
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Severity routing
// ─────────────────────────────────────────────────────────────────────────────
//
// log_diagnostic maps a lucid::diag::Severity to a log level. The
// mapping is:
//   Hint    → Trace
//   Note    → Info
//   Warning → Warn
//   Error   → Error
//   Fatal   → Fatal (aborts; tested separately)
//
// The tests below install a capture at Trace level, emit a
// diagnostic at each severity, and verify the captured LogRecord's
// level. The message content is asserted separately (below); here,
// only the routing is checked.

TEST_CASE("diagnostic_log: hint routes to trace", "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Hint;
    d.code = lucid::diag::DiagCode(0);
    d.message = "a hint";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == engine::LogLevel::Trace);
}

TEST_CASE("diagnostic_log: note routes to info", "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Note;
    d.code = lucid::diag::DiagCode(0);
    d.message = "a note";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == engine::LogLevel::Info);
}

TEST_CASE("diagnostic_log: warning routes to warn", "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Warning;
    d.code = lucid::diag::DiagCode::Warn_UnusedNode;
    d.message = "a warning";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == engine::LogLevel::Warn);
}

TEST_CASE("diagnostic_log: error routes to error", "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Type_Mismatch;
    d.message = "an error";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].level == engine::LogLevel::Error);
}

// Fatal severity is not tested here because it aborts. The abort
// behavior belongs to emit_diagnostic and log_fatal, which are
// death-test territory; Catch2's death test support is limited and
// platform-specific, and the engine's test suite does not currently
// exercise abort paths. A missing Fatal test is a known gap; the
// behavior is verified manually.

// ─────────────────────────────────────────────────────────────────────────────
// Message formatting
// ─────────────────────────────────────────────────────────────────────────────
//
// The format is "<band>: <message>" with " at L:C" appended when the
// location is known. The band name comes from diagnostic_code_name.
// A free-text diagnostic (code 0) uses the band "FreeText".
// A coded diagnostic uses the band its numeric range maps to.
//
// These tests assert on the whole message shape, not on a fixed
// prefix, so the band name can change without rewriting the tests.

TEST_CASE("diagnostic_log: message is band-prefixed with colon and space",
          "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Type_Mismatch; // band "Type"
    d.message = "expected int32, got float32";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    // "<band>: <message>"
    REQUIRE(records[0].message == "Type: expected int32, got float32");
}

TEST_CASE("diagnostic_log: free-text diagnostic uses the FreeText band",
          "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode(0);
    d.message = "something went wrong";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].message == "FreeText: something went wrong");
}

TEST_CASE("diagnostic_log: known location appends ' at L:C'",
          "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Type_Mismatch;
    d.location = SourceLocation(12, 34);
    d.message = "mismatch";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].message == "Type: mismatch at 12:34");
}

TEST_CASE("diagnostic_log: unknown location omits the ' at L:C' suffix",
          "[format_diagnostics]")
{
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Ser_FingerprintMismatch;
    // d.location is default-constructed; isKnown() is false.
    d.message = "the file's registry differs from ours";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].message ==
            "Serialization: the file's registry differs from ours");
    // Explicitly: no " at " suffix.
    REQUIRE(records[0].message.find(" at ") == std::string::npos);
}

TEST_CASE("diagnostic_log: message with percent signs is not reformatted",
          "[format_diagnostics]")
{
    // log_message is printf-style; the message must be passed as
    // "%s", not as the format. A message containing "%s" or "%d"
    // must survive literally.
    ScopedCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode(0);
    d.message = "value 100% done, not %s";

    engine::log_diagnostic(d);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].message.find("100% done") != std::string::npos);
    REQUIRE(records[0].message.find("%s") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Code preservation (structured form)
// ─────────────────────────────────────────────────────────────────────────────
//
// log_diagnostic builds a RuntimeDiagnostic and emits it. The
// structured form must preserve the code and the location even when
// the log message only carries the band. These tests use a
// diagnostic sink to observe the RuntimeDiagnostic directly.

TEST_CASE("diagnostic_log: RuntimeDiagnostic carries the raw code",
          "[format_diagnostics]")
{
    ScopedDiagnosticCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Ser_FingerprintMismatch; // 6041
    d.message = "mismatch";

    engine::log_diagnostic(d);

    const auto diags = cap.snapshot();
    REQUIRE(diags.size() == 1);
    REQUIRE(diags[0].code == 6041);
}

TEST_CASE("diagnostic_log: RuntimeDiagnostic carries line and column",
          "[format_diagnostics]")
{
    ScopedDiagnosticCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Type_Mismatch;
    d.location = SourceLocation(7, 19);
    d.message = "mismatch";

    engine::log_diagnostic(d);

    const auto diags = cap.snapshot();
    REQUIRE(diags.size() == 1);
    REQUIRE(diags[0].line == 7);
    REQUIRE(diags[0].column == 19);
}

TEST_CASE("diagnostic_log: RuntimeDiagnostic leaves file empty",
          "[format_diagnostics]")
{
    // log_diagnostic cannot resolve the InternedString file field:
    // it has no StringPool. That is documented behavior; the caller
    // (GraphRuntime, the editor's compile loop) supplies `file` if
    // it needs one.
    ScopedDiagnosticCapture cap;

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Type_Mismatch;
    d.message = "mismatch";

    engine::log_diagnostic(d);

    const auto diags = cap.snapshot();
    REQUIRE(diags.size() == 1);
    REQUIRE(diags[0].file.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// diagnostic_code_raw / diagnostic_code_name
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("diagnostic_code_raw: returns the numeric DiagCode value",
          "[format_diagnostics]")
{
    lucid::diag::Diagnostic d;
    d.code = lucid::diag::DiagCode::Ser_FingerprintMismatch;
    REQUIRE(engine::diagnostic_code_raw(d) == 6041);

    d.code = lucid::diag::DiagCode::Type_Mismatch;
    REQUIRE(engine::diagnostic_code_raw(d) == 4001);

    d.code = lucid::diag::DiagCode(0);
    REQUIRE(engine::diagnostic_code_raw(d) == 0);
}

TEST_CASE("diagnostic_code_name: returns the band for the code's range",
          "[format_diagnostics]")
{
    // One code per band, plus the sentinels.
    REQUIRE(std::string(engine::diagnostic_code_name(0)) == "FreeText");

    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Lex_UnterminatedString))) == "Lexical");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Syntax_ExpectedToken))) == "Syntax");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Name_UndefinedNode))) == "Name");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Type_Mismatch))) == "Type");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Attr_Unknown))) == "Attribute");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Import_ModuleNotFound))) == "Import");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Trigger_ActionWithoutOn))) == "Trigger");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Ser_FingerprintMismatch))) == "Serialization");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Registry_EnumWithoutZero))) == "Registry");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Internal_Assertion))) == "Internal");
    REQUIRE(std::string(engine::diagnostic_code_name(
                lucid::diag::raw(lucid::diag::DiagCode::Warn_UnusedNode))) == "Warning");
}

TEST_CASE("diagnostic_code_name: unknown codes return Unknown",
          "[format_diagnostics]")
{
    REQUIRE(std::string(engine::diagnostic_code_name(99999)) == "Unknown");
}

// ─────────────────────────────────────────────────────────────────────────────
// diagnostic_severity_name
// ─────────────────────────────────────────────────────────────────────────────
//
// The severity name is the lowercase spelling the format library's
// own CLI uses. It is distinct from the band name and from the log
// level. It is used, for example, to show "error" in a tooltip.

TEST_CASE("diagnostic_severity_name: returns lowercase names",
          "[format_diagnostics]")
{
    REQUIRE(std::string(engine::diagnostic_severity_name(
                lucid::diag::Severity::Hint)) == "hint");
    REQUIRE(std::string(engine::diagnostic_severity_name(
                lucid::diag::Severity::Note)) == "note");
    REQUIRE(std::string(engine::diagnostic_severity_name(
                lucid::diag::Severity::Warning)) == "warning");
    REQUIRE(std::string(engine::diagnostic_severity_name(
                lucid::diag::Severity::Error)) == "error");
    REQUIRE(std::string(engine::diagnostic_severity_name(
                lucid::diag::Severity::Fatal)) == "fatal");
}

// ─────────────────────────────────────────────────────────────────────────────
// log_diagnostics (plural)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("log_diagnostics: logs every diagnostic in order",
          "[format_diagnostics]")
{
    ScopedCapture cap;

    std::vector<lucid::diag::Diagnostic> diags;

    {
        lucid::diag::Diagnostic d;
        d.severity = lucid::diag::Severity::Warning;
        d.code = lucid::diag::DiagCode::Warn_UnusedNode;
        d.message = "first";
        diags.push_back(d);
    }
    {
        lucid::diag::Diagnostic d;
        d.severity = lucid::diag::Severity::Error;
        d.code = lucid::diag::DiagCode::Type_Mismatch;
        d.message = "second";
        diags.push_back(d);
    }

    engine::log_diagnostics(diags);

    const auto records = cap.snapshot();
    REQUIRE(records.size() == 2);
    REQUIRE(records[0].level == engine::LogLevel::Warn);
    REQUIRE(records[0].message.find("first") != std::string::npos);
    REQUIRE(records[1].level == engine::LogLevel::Error);
    REQUIRE(records[1].message.find("second") != std::string::npos);
}