#include <catch2/catch_test_macros.hpp>

#include "editor/panels/console_panel.h"

#include "engine/core/log.h"

using namespace editor;

namespace
{
    engine::LogRecord make_record(engine::LogLevel level,
                                  const std::string &message)
    {
        engine::LogRecord r;
        r.level = level;
        r.timestamp = "12:00:00";
        r.message = message;
        return r;
    }
}

TEST_CASE("console_buffer: push adds lines", "[console]")
{
    ConsoleBuffer buf(100);
    REQUIRE(buf.size() == 0);

    buf.push(make_record(engine::LogLevel::Info, "hello"));
    REQUIRE(buf.size() == 1);

    buf.push(make_record(engine::LogLevel::Warn, "warning"));
    REQUIRE(buf.size() == 2);

    auto lines = buf.snapshot();
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0].message == "hello");
    REQUIRE(lines[0].level == engine::LogLevel::Info);
    REQUIRE(lines[1].message == "warning");
    REQUIRE(lines[1].level == engine::LogLevel::Warn);
}

TEST_CASE("console_buffer: caps at max_lines", "[console]")
{
    ConsoleBuffer buf(5);

    for (int i = 0; i < 10; ++i)
    {
        buf.push(make_record(engine::LogLevel::Info,
                             "line " + std::to_string(i)));
    }

    REQUIRE(buf.size() == 5);

    auto lines = buf.snapshot();
    REQUIRE(lines.size() == 5);
    REQUIRE(lines.front().message == "line 5");
    REQUIRE(lines.back().message == "line 9");
}

TEST_CASE("console_buffer: clear empties the buffer", "[console]")
{
    ConsoleBuffer buf(100);
    buf.push(make_record(engine::LogLevel::Info, "a"));
    buf.push(make_record(engine::LogLevel::Info, "b"));
    REQUIRE(buf.size() == 2);

    buf.clear();
    REQUIRE(buf.size() == 0);
    REQUIRE(buf.snapshot().empty());
}

TEST_CASE("console_buffer: preserves timestamp and message", "[console]")
{
    ConsoleBuffer buf(100);
    engine::LogRecord r;
    r.level = engine::LogLevel::Error;
    r.timestamp = "14:30:05";
    r.message = "something failed";
    buf.push(r);

    auto lines = buf.snapshot();
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].timestamp == "14:30:05");
    REQUIRE(lines[0].message == "something failed");
    REQUIRE(lines[0].level == engine::LogLevel::Error);
}