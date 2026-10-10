#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace engine {

// -----------------------------------------------------------------------------
// Levels
// -----------------------------------------------------------------------------

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5,
};

// Human-readable name for a level, e.g. "INFO". Never returns nullptr.
const char* log_level_name(LogLevel level);

// -----------------------------------------------------------------------------
// Global minimum level
// -----------------------------------------------------------------------------
//
// Messages below the minimum level are dropped. Default is LogLevel::Info.

void     set_log_level(LogLevel level);
LogLevel get_log_level();

// -----------------------------------------------------------------------------
// Log records and sinks
// -----------------------------------------------------------------------------
//
// A log record is the structured form of a message. Sinks receive records
// and format them however they like.
//
// The default sink writes to stdout, formatted as:
//     [HH:MM:SS] [LEVEL] message
//
// Additional sinks can be added (e.g. an editor console). All active sinks
// receive every message that passes the level filter.

struct LogRecord {
    LogLevel    level;
    std::string timestamp;  // "HH:MM:SS"
    std::string message;
};

using LogSink   = std::function<void(const LogRecord&)>;
using LogSinkId = std::uint32_t;

// Add a sink. Returns an ID for removal.
LogSinkId add_log_sink(LogSink sink);

// Remove a sink by ID. No-op if the ID is unknown.
void remove_log_sink(LogSinkId id);

// Remove all sinks, including the default stdout sink.
void clear_log_sinks();

// Install the default stdout sink. Called once at startup.
// Idempotent: calling twice does nothing.
void install_default_log_sink();

// -----------------------------------------------------------------------------
// Logging functions
// -----------------------------------------------------------------------------
//
// log_message is the base function. All others dispatch to it.
//
// Formatting is printf-style. Use %% to write a literal '%'.
// Messages longer than 1024 bytes are truncated with "...".
//
// log_fatal logs the message at Fatal level and calls std::abort().

void log_message(LogLevel level, const char* fmt, ...);
void log_info (const char* fmt, ...);
void log_warn (const char* fmt, ...);
void log_error(const char* fmt, ...);
[[noreturn]] void log_fatal(const char* fmt, ...);

} // namespace engine

// -----------------------------------------------------------------------------
// Trace/Debug macros
// -----------------------------------------------------------------------------
//
// In release builds (NDEBUG or ENGINE_RELEASE defined), these expand to
// ((void)0) and do NOT evaluate their arguments. Do not put side effects
// in Trace/Debug calls.

#ifdef ENGINE_LOG_DISABLE_TRACE_DEBUG

    #define ENGINE_LOG_TRACE(...) ((void)0)
    #define ENGINE_LOG_DEBUG(...) ((void)0)

#else

    #define ENGINE_LOG_TRACE(...) \
        ::engine::log_message(::engine::LogLevel::Trace, __VA_ARGS__)
    #define ENGINE_LOG_DEBUG(...) \
        ::engine::log_message(::engine::LogLevel::Debug, __VA_ARGS__)

#endif

// Release-build defaults: disable Trace and Debug.
#if defined(NDEBUG) && !defined(ENGINE_LOG_FORCE_TRACE_DEBUG)
    #undef  ENGINE_LOG_TRACE
    #undef  ENGINE_LOG_DEBUG
    #define ENGINE_LOG_TRACE(...) ((void)0)
    #define ENGINE_LOG_DEBUG(...) ((void)0)
#endif
