#pragma once

#include <string>

namespace engine {

enum class LogLevel {
    Info,
    Warn,
    Error,
};

// Set the minimum level that will be printed.
void set_log_level(LogLevel level);

// Get the current minimum level.
LogLevel get_log_level();

// Log a message at the given level.
void log(LogLevel level, const std::string& message);

// Convenience functions.
void log_info(const std::string& message);
void log_warn(const std::string& message);
void log_error(const std::string& message);

// printf-style variants.
void log_info_f(const char* fmt, ...);
void log_warn_f(const char* fmt, ...);
void log_error_f(const char* fmt, ...);

} // namespace engine
