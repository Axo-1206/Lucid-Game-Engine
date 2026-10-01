#include "engine/log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace engine {

namespace {

LogLevel g_min_level = LogLevel::Info;

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void vlog(LogLevel level, const char* fmt, va_list args) {
    if (level < g_min_level) return;

    // Timestamp
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif

    char time_buf[16];
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_buf);

    std::fprintf(stdout, "[%s] [%s] ", time_buf, level_name(level));
    std::vfprintf(stdout, fmt, args);
    std::fprintf(stdout, "\n");
    std::fflush(stdout);
}

} // namespace

void set_log_level(LogLevel level) { g_min_level = level; }
LogLevel get_log_level()           { return g_min_level; }

void log(LogLevel level, const std::string& message) {
    log_info_f("%s", message.c_str());
    (void)level;
}

void log_info(const std::string& message)  { log_info_f("%s", message.c_str()); }
void log_warn(const std::string& message)  { log_warn_f("%s", message.c_str()); }
void log_error(const std::string& message) { log_error_f("%s", message.c_str()); }

void log_info_f(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(LogLevel::Info, fmt, args);
    va_end(args);
}

void log_warn_f(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(LogLevel::Warn, fmt, args);
    va_end(args);
}

void log_error_f(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(LogLevel::Error, fmt, args);
    va_end(args);
}

} // namespace engine
