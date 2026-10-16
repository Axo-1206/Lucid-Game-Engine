#pragma once

#include "engine/core/log.h"

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace editor
{

    // One line in the console. A copy of the log record's
    // fields we care about, plus the level.
    struct ConsoleLine
    {
        engine::LogLevel level;
        std::string timestamp;
        std::string message;
    };

    // The console's ring buffer. Plain data, no ImGui. The panel
    // reads from it; a log sink writes to it. Thread-safe.
    //
    // Capped at max_lines. New lines push to the back; when the
    // cap is reached, the oldest line is dropped.
    class ConsoleBuffer
    {
    public:
        explicit ConsoleBuffer(std::size_t max_lines = 10000);

        // Called from the log sink. Thread-safe.
        void push(const engine::LogRecord &record);

        // Thread-safe. Copies out a snapshot of the buffer.
        std::deque<ConsoleLine> snapshot() const;

        std::size_t size() const;
        void clear();

        std::size_t max_lines() const { return max_lines_; }

    private:
        std::size_t max_lines_;
        std::deque<ConsoleLine> lines_;
        mutable std::mutex mutex_;
    };

    // The console panel. Draws a scrolling log view with a
    // minimum-level filter and a text filter.
    class ConsolePanel
    {
    public:
        ConsolePanel();
        ~ConsolePanel();

        // Register/unregister the log sink. Called by EditorApp.
        // The two are idempotent: calling attach_to_log twice is
        // a no-op for the second call; detaching when not
        // attached is a no-op.
        void attach_to_log();
        void detach_from_log();

        // Draw the window. Called once per frame by EditorApp
        // when the shell is in Project mode.
        void draw();

        // Visibility. Called by the Window menu.
        bool &visible() { return visible_; }

        // The buffer, for tests. Do not read it from panel code
        // other than the panel itself.
        ConsoleBuffer &buffer() { return buffer_; }

    private:
        ConsoleBuffer buffer_;

        engine::LogSinkId sink_id_ = 0;
        bool visible_ = true;

        engine::LogLevel min_level_ = engine::LogLevel::Info;
        char text_filter_[128] = {};
        bool auto_scroll_ = true;

        void draw_filter_bar();
        void draw_lines();
        bool passes_filter(const ConsoleLine &line) const;
    };

} // namespace editor