#include "engine/log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <vector>

namespace engine {

namespace {

LogLevel g_min_level = LogLevel::Info;

struct SinkEntry {
    LogSinkId id;
    LogSink   fn;
};

std::vector<SinkEntry> g_sinks;
LogSinkId              g_next_sink_id = 1;
bool                   g_default_sink_installed = false;

// Format the current wall-clock time as "HH:MM:SS".
std::string format_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return std::string(buf);
}

// Format a message with vsnprintf. Truncates to kMaxMessageBytes.
constexpr std::size_t kMaxMessageBytes = 1024;

std::string format_message(const char* fmt, va_list args) {
    char buf[kMaxMessageBytes];
    // vsnprintf returns the number of characters that WOULD have been
    // written. If it's >= the buffer size, the message was truncated.
    int written = std::vsnprintf(buf, sizeof(buf), fmt, args);

    if (written < 0) {
        // Encoding error. Return a marker.
        return "<format error>";
    }
    if (static_cast<std::size_t>(written) >= sizeof(buf)) {
        // Truncated. Overwrite the tail with "...".
        if (sizeof(buf) >= 4) {
            buf[sizeof(buf) - 4] = '.';
            buf[sizeof(buf) - 3] = '.';
            buf[sizeof(buf) - 2] = '.';
            buf[sizeof(buf) - 1] = '\0';
        }
    }
    return std::string(buf);
}

// Default stdout sink.
void stdout_sink(const LogRecord& rec) {
    std::fprintf(stdout, "[%s] [%s] %s\n",
                 rec.timestamp.c_str(),
                 log_level_name(rec.level),
                 rec.message.c_str());
    std::fflush(stdout);
}

// Dispatch a record to all sinks.
void dispatch(const LogRecord& rec) {
    // Copy the sink list to avoid issues if a sink adds/removes sinks.
    // (Single-threaded, but defensive.)
    for (const auto& entry : g_sinks) {
        entry.fn(rec);
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Level name
// -----------------------------------------------------------------------------

const char* log_level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

// -----------------------------------------------------------------------------
// Global level
// -----------------------------------------------------------------------------

void     set_log_level(LogLevel level) { g_min_level = level; }
LogLevel get_log_level()                { return g_min_level; }

// -----------------------------------------------------------------------------
// Sinks
// -----------------------------------------------------------------------------

LogSinkId add_log_sink(LogSink sink) {
    LogSinkId id = g_next_sink_id++;
    g_sinks.push_back({id, std::move(sink)});
    return id;
}

void remove_log_sink(LogSinkId id) {
    for (auto it = g_sinks.begin(); it != g_sinks.end(); ++it) {
        if (it->id == id) {
            g_sinks.erase(it);
            return;
        }
    }
}

void clear_log_sinks() {
    g_sinks.clear();
    g_default_sink_installed = false;
}

void install_default_log_sink() {
    if (g_default_sink_installed) return;
    add_log_sink(&stdout_sink);
    g_default_sink_installed = true;
}

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------

void log_message(LogLevel level, const char* fmt, ...) {
    if (level < g_min_level) return;

    va_list args;
    va_start(args, fmt);
    std::string message = format_message(fmt, args);
    va_end(args);

    LogRecord rec {
        level,
        format_timestamp(),
        std::move(message),
    };

    // Ensure at least one sink exists.
    if (g_sinks.empty() && !g_default_sink_installed) {
        install_default_log_sink();
    }

    dispatch(rec);
}

void log_info(const char* fmt, ...) {
    if (LogLevel::Info < g_min_level) return;
    va_list args;
    va_start(args, fmt);
    std::string message = format_message(fmt, args);
    va_end(args);
    LogRecord rec { LogLevel::Info, format_timestamp(), std::move(message) };
    if (g_sinks.empty() && !g_default_sink_installed) install_default_log_sink();
    dispatch(rec);
}

void log_warn(const char* fmt, ...) {
    if (LogLevel::Warn < g_min_level) return;
    va_list args;
    va_start(args, fmt);
    std::string message = format_message(fmt, args);
    va_end(args);
    LogRecord rec { LogLevel::Warn, format_timestamp(), std::move(message) };
    if (g_sinks.empty() && !g_default_sink_installed) install_default_log_sink();
    dispatch(rec);
}

void log_error(const char* fmt, ...) {
    if (LogLevel::Error < g_min_level) return;
    va_list args;
    va_start(args, fmt);
    std::string message = format_message(fmt, args);
    va_end(args);
    LogRecord rec { LogLevel::Error, format_timestamp(), std::move(message) };
    if (g_sinks.empty() && !g_default_sink_installed) install_default_log_sink();
    dispatch(rec);
}

[[noreturn]] void log_fatal(const char* fmt, ...) {
    // Always log fatal, regardless of the minimum level.
    va_list args;
    va_start(args, fmt);
    std::string message = format_message(fmt, args);
    va_end(args);
    LogRecord rec { LogLevel::Fatal, format_timestamp(), std::move(message) };
    if (g_sinks.empty() && !g_default_sink_installed) install_default_log_sink();
    dispatch(rec);
    std::abort();
}

} // namespace engine
