#include <catch2/catch_test_macros.hpp>

#include "engine/core/format_diagnostics.h"
#include "engine/core/log.h"

#include "core/diagnostics/Diagnostic.hpp"

#include <string>
#include <vector>

using namespace engine;

namespace
{

    struct SinkCapture
    {
        std::vector<LogRecord> records;
        LogSinkId id = 0;
        void attach()
        {
            id = add_log_sink([this](const LogRecord &r)
                              { records.push_back(r); });
        }
        void detach()
        {
            if (id != 0)
                remove_log_sink(id);
            id = 0;
        }
    };

} // namespace

TEST_CASE("diagnostic_log: severity names", "[diag]")
{
    using lucid::diag::Severity;
    REQUIRE(std::string(diagnostic_severity_name(Severity::Fatal)) == "fatal");
    REQUIRE(std::string(diagnostic_severity_name(Severity::Error)) == "error");
    REQUIRE(std::string(diagnostic_severity_name(Severity::Warning)) == "warning");
    REQUIRE(std::string(diagnostic_severity_name(Severity::Note)) == "note");
    REQUIRE(std::string(diagnostic_severity_name(Severity::Hint)) == "hint");
}

TEST_CASE("diagnostic_log: error routes to log_error", "[diag]")
{
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Trace);

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Error;
    d.message = "something broke";
    // d.location: default-constructed, isKnown() == false

    log_diagnostic(d);

    REQUIRE(cap.records.size() == 1);
    REQUIRE(cap.records[0].level == LogLevel::Error);
    REQUIRE(cap.records[0].message.find("something broke") != std::string::npos);
    REQUIRE(cap.records[0].message.find("[error]") != std::string::npos);
    // No "at 0:0" when the location is unknown.
    REQUIRE(cap.records[0].message.find(" at ") == std::string::npos);

    cap.detach();
}

TEST_CASE("diagnostic_log: warning routes to log_warn", "[diag]")
{
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Trace);

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Warning;
    d.message = "just so you know";

    log_diagnostic(d);

    REQUIRE(cap.records.size() == 1);
    REQUIRE(cap.records[0].level == LogLevel::Warn);

    cap.detach();
}

TEST_CASE("diagnostic_log: fatal does not abort", "[diag]")
{
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Trace);

    lucid::diag::Diagnostic d;
    d.severity = lucid::diag::Severity::Fatal;
    d.message = "file is unrecoverable";

    // The point of this test: calling log_diagnostic with Fatal
    // does NOT abort. If it did, the test process would die.
    log_diagnostic(d);

    REQUIRE(cap.records.size() == 1);
    REQUIRE(cap.records[0].level == LogLevel::Error); // not Fatal
    REQUIRE(cap.records[0].message.find("file is unrecoverable") != std::string::npos);

    cap.detach();
}

TEST_CASE("diagnostic_log: a vector logs all diagnostics in order", "[diag]")
{
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Trace);

    std::vector<lucid::diag::Diagnostic> diags(3);
    diags[0].severity = lucid::diag::Severity::Error;
    diags[0].message = "first";
    diags[1].severity = lucid::diag::Severity::Warning;
    diags[1].message = "second";
    diags[2].severity = lucid::diag::Severity::Error;
    diags[2].message = "third";

    log_diagnostics(diags);

    REQUIRE(cap.records.size() == 3);
    REQUIRE(cap.records[0].message.find("first") != std::string::npos);
    REQUIRE(cap.records[1].message.find("second") != std::string::npos);
    REQUIRE(cap.records[2].message.find("third") != std::string::npos);

    cap.detach();
}