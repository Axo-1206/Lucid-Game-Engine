// tests/engine/test_diagnostics.cpp
//
// Tests for engine/core/diagnostics.h:
//   - Sink registration, removal, and emission.
//   - log_diagnostic (format_diagnostics.h) emits a RuntimeDiagnostic
//     with the code and location preserved.
//   - Fatal severity aborts (checked via death-test if Catch2 supports
//     it; otherwise skipped — abort() is hard to test portably).

#include <catch2/catch_test_macros.hpp>

#include "engine/core/diagnostics.h"
#include "engine/core/format_diagnostics.h"
#include "engine/core/log.h"

#include "core/diagnostics/Diagnostic.hpp"
#include "core/SourceLocation.hpp"

#include <string>
#include <vector>

namespace
{

    // A sink that records every diagnostic it receives.
    struct CollectingSink
    {
        std::vector<engine::RuntimeDiagnostic> received;

        void operator()(const engine::RuntimeDiagnostic &d)
        {
            received.push_back(d);
        }
    };

} // namespace

TEST_CASE("diagnostics: emit_diagnostic fires a registered sink",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    CollectingSink collector;
    const auto id = engine::add_diagnostic_sink(
        [&collector](const engine::RuntimeDiagnostic &d)
        { collector(d); });
    REQUIRE(id != 0);

    engine::RuntimeDiagnostic d;
    d.code = 6041;
    d.severity = engine::LogLevel::Error;
    d.file = "test.lucgraph";
    d.line = 3;
    d.column = 7;
    d.message = "registry mismatch";
    engine::emit_diagnostic(d);

    engine::remove_diagnostic_sink(id);

    REQUIRE(collector.received.size() == 1);
    REQUIRE(collector.received[0].code == 6041);
    REQUIRE(collector.received[0].file == "test.lucgraph");
    REQUIRE(collector.received[0].line == 3);
    REQUIRE(collector.received[0].column == 7);
    REQUIRE(collector.received[0].message == "registry mismatch");
}

TEST_CASE("diagnostics: removing a sink stops delivery",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    CollectingSink collector;
    const auto id = engine::add_diagnostic_sink(
        [&collector](const engine::RuntimeDiagnostic &d)
        { collector(d); });
    REQUIRE(id != 0);

    engine::RuntimeDiagnostic d;
    d.severity = engine::LogLevel::Info;
    d.message = "first";

    engine::emit_diagnostic(d);
    REQUIRE(collector.received.size() == 1);

    engine::remove_diagnostic_sink(id);

    engine::emit_diagnostic(d);
    REQUIRE(collector.received.size() == 1); // unchanged
}

TEST_CASE("diagnostics: multiple sinks all receive a diagnostic",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    CollectingSink a;
    CollectingSink b;

    const auto id_a = engine::add_diagnostic_sink(
        [&a](const engine::RuntimeDiagnostic &d)
        { a(d); });
    const auto id_b = engine::add_diagnostic_sink(
        [&b](const engine::RuntimeDiagnostic &d)
        { b(d); });
    REQUIRE(id_a != id_b);
    REQUIRE(id_a != 0);
    REQUIRE(id_b != 0);

    engine::RuntimeDiagnostic d;
    d.code = 4001;
    d.severity = engine::LogLevel::Error;
    d.message = "mismatch";
    engine::emit_diagnostic(d);

    engine::remove_diagnostic_sink(id_a);
    engine::remove_diagnostic_sink(id_b);

    REQUIRE(a.received.size() == 1);
    REQUIRE(b.received.size() == 1);
    REQUIRE(a.received[0].code == 4001);
    REQUIRE(b.received[0].code == 4001);
}

TEST_CASE("diagnostics: emit with no sinks registered does not crash",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    engine::RuntimeDiagnostic d;
    d.severity = engine::LogLevel::Info;
    d.message = "no sinks";
    REQUIRE_NOTHROW(engine::emit_diagnostic(d));
}

TEST_CASE("diagnostics: add_diagnostic_sink rejects a null sink",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    const auto id = engine::add_diagnostic_sink(engine::DiagnosticSink{});
    REQUIRE(id == 0);
}

TEST_CASE("diagnostics: a message with printf specifiers is not formatted",
          "[diagnostics]")
{
    // log_message is printf-style; a message containing %s or %d must
    // not be interpreted as a format string. emit_diagnostic passes
    // it as "%s", so the specifiers survive literally.
    engine::clear_diagnostic_sinks();

    CollectingSink collector;
    const auto id = engine::add_diagnostic_sink(
        [&collector](const engine::RuntimeDiagnostic &d)
        { collector(d); });

    engine::RuntimeDiagnostic d;
    d.severity = engine::LogLevel::Info;
    d.message = "value is 100% done, %s";
    REQUIRE_NOTHROW(engine::emit_diagnostic(d));

    engine::remove_diagnostic_sink(id);

    REQUIRE(collector.received.size() == 1);
    REQUIRE(collector.received[0].message == "value is 100% done, %s");
}

TEST_CASE("diagnostics: log_diagnostic preserves code and location",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    CollectingSink collector;
    const auto id = engine::add_diagnostic_sink(
        [&collector](const engine::RuntimeDiagnostic &d)
        { collector(d); });

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Ser_FingerprintMismatch;
    d.location = SourceLocation(3, 7);
    d.message = "the file's registry differs from ours";

    engine::log_diagnostic(d);

    engine::remove_diagnostic_sink(id);

    REQUIRE(collector.received.size() == 1);
    REQUIRE(collector.received[0].code == 6041);
    REQUIRE(collector.received[0].line == 3);
    REQUIRE(collector.received[0].column == 7);
    REQUIRE(collector.received[0].severity == engine::LogLevel::Error);
    REQUIRE(collector.received[0].message ==
            "the file's registry differs from ours");
    // `file` is empty: log_diagnostic has no StringPool to resolve
    // the InternedString. That is documented behavior.
    REQUIRE(collector.received[0].file.empty());
}

TEST_CASE("diagnostics: log_diagnostic without a location leaves line/col at 0",
          "[diagnostics]")
{
    engine::clear_diagnostic_sinks();

    CollectingSink collector;
    const auto id = engine::add_diagnostic_sink(
        [&collector](const engine::RuntimeDiagnostic &d)
        { collector(d); });

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.code = lucid::diag::DiagCode::Ser_TruncatedSection;
    // No location: a file-level error.
    d.message = "section extends past the file's end";

    engine::log_diagnostic(d);

    engine::remove_diagnostic_sink(id);

    REQUIRE(collector.received.size() == 1);
    REQUIRE(collector.received[0].code == 6006);
    REQUIRE(collector.received[0].line == 0);
    REQUIRE(collector.received[0].column == 0);
}

TEST_CASE("diagnostics: diagnostic_code_name returns the category band",
          "[diagnostics]")
{
    REQUIRE(std::string(engine::diagnostic_code_name(0)) == "FreeText");
    REQUIRE(std::string(engine::diagnostic_code_name(1003)) == "Lexical");
    REQUIRE(std::string(engine::diagnostic_code_name(4001)) == "Type");
    REQUIRE(std::string(engine::diagnostic_code_name(5002)) == "Attribute");
    REQUIRE(std::string(engine::diagnostic_code_name(5101)) == "Import");
    REQUIRE(std::string(engine::diagnostic_code_name(5301)) == "Trigger");
    REQUIRE(std::string(engine::diagnostic_code_name(6041)) == "Serialization");
    REQUIRE(std::string(engine::diagnostic_code_name(6201)) == "Registry");
    REQUIRE(std::string(engine::diagnostic_code_name(7001)) == "Internal");
    REQUIRE(std::string(engine::diagnostic_code_name(8005)) == "Warning");
    REQUIRE(std::string(engine::diagnostic_code_name(99999)) == "Unknown");
}