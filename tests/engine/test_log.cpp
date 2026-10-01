#include <catch2/catch_test_macros.hpp>
#include "engine/log.h"

#include <string>
#include <vector>

using namespace engine;

namespace {

struct SinkCapture {
    std::vector<LogRecord> records;
    LogSinkId              id = 0;

    void attach() {
        id = add_log_sink([this](const LogRecord& r) { records.push_back(r); });
    }
    void detach() {
        if (id != 0) remove_log_sink(id);
        id = 0;
    }
};

} // namespace

TEST_CASE("log: default level is Info", "[log]") {
    set_log_level(LogLevel::Info);
    REQUIRE(get_log_level() == LogLevel::Info);
}

TEST_CASE("log: sink receives messages", "[log]") {
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Trace);

    log_info("hello %s", "world");

    REQUIRE(cap.records.size() == 1);
    REQUIRE(cap.records[0].level == LogLevel::Info);
    REQUIRE(cap.records[0].message == "hello world");
    REQUIRE_FALSE(cap.records[0].timestamp.empty());

    cap.detach();
}

TEST_CASE("log: level filter drops lower levels", "[log]") {
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Warn);

    log_info("info");
    log_warn("warn");
    log_error("error");

    REQUIRE(cap.records.size() == 2);
    REQUIRE(cap.records[0].level == LogLevel::Warn);
    REQUIRE(cap.records[1].level == LogLevel::Error);

    cap.detach();
}

TEST_CASE("log: multiple sinks receive the same message", "[log]") {
    clear_log_sinks();
    SinkCapture a;
    SinkCapture b;
    a.attach();
    b.attach();
    set_log_level(LogLevel::Trace);

    log_info("shared");

    REQUIRE(a.records.size() == 1);
    REQUIRE(b.records.size() == 1);
    REQUIRE(a.records[0].message == "shared");
    REQUIRE(b.records[0].message == "shared");

    a.detach();
    b.detach();
}

TEST_CASE("log: remove_log_sink stops delivery", "[log]") {
    clear_log_sinks();
    SinkCapture cap;
    cap.attach();
    set_log_level(LogLevel::Trace);

    log_info("first");
    cap.detach();
    log_info("second");

    REQUIRE(cap.records.size() == 1);
    REQUIRE(cap.records[0].message == "first");
}

TEST_CASE("log: level names", "[log]") {
    REQUIRE(std::string(log_level_name(LogLevel::Trace)) == "TRACE");
    REQUIRE(std::string(log_level_name(LogLevel::Info))  == "INFO ");
    REQUIRE(std::string(log_level_name(LogLevel::Fatal)) == "FATAL");
}
