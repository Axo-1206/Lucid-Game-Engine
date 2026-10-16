#include "editor/panels/console_panel.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace editor
{

    // ---- ConsoleBuffer ----

    ConsoleBuffer::ConsoleBuffer(std::size_t max_lines)
        : max_lines_(max_lines ? max_lines : 1)
    {
    }

    void ConsoleBuffer::push(const engine::LogRecord &record)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        ConsoleLine line;
        line.level = record.level;
        line.timestamp = record.timestamp;
        line.message = record.message;

        lines_.push_back(std::move(line));

        while (lines_.size() > max_lines_)
        {
            lines_.pop_front();
        }
    }

    std::deque<ConsoleLine> ConsoleBuffer::snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }

    std::size_t ConsoleBuffer::size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lines_.size();
    }

    void ConsoleBuffer::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_.clear();
    }

    // ---- ConsolePanel ----

    ConsolePanel::ConsolePanel() = default;

    ConsolePanel::~ConsolePanel()
    {
        detach_from_log();
    }

    void ConsolePanel::attach_to_log()
    {
        if (sink_id_ != 0)
            return;

        sink_id_ = engine::add_log_sink(
            [this](const engine::LogRecord &record)
            {
                buffer_.push(record);
            });
    }

    void ConsolePanel::detach_from_log()
    {
        if (sink_id_ == 0)
            return;

        engine::remove_log_sink(sink_id_);
        sink_id_ = 0;
    }

    bool ConsolePanel::passes_filter(const ConsoleLine &line) const
    {
        // Level filter: show lines at or above min_level_.
        if (static_cast<int>(line.level) < static_cast<int>(min_level_))
            return false;

        // Text filter: substring match on the message. If the
        // text filter is empty, all lines pass.
        if (text_filter_[0] != '\0')
        {
            if (line.message.find(text_filter_) == std::string::npos)
                return false;
        }

        return true;
    }

    void ConsolePanel::draw_filter_bar()
    {
        ImGui::SetNextItemWidth(120);
        const char *levels[] = {"Trace", "Debug", "Info", "Warn", "Error", "Fatal"};
        int current = static_cast<int>(min_level_);
        if (ImGui::Combo("##min_level", &current, levels, IM_COUNTOF(levels)))
        {
            min_level_ = static_cast<engine::LogLevel>(current);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##filter", "filter...", text_filter_,
                                 sizeof(text_filter_));

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            buffer_.clear();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll_);

        ImGui::Separator();
    }

    void ConsolePanel::draw_lines()
    {
        // Snapshot under the lock, then draw without holding it.
        // The snapshot copies all lines; for 10,000 lines this is
        // ~10,000 string copies. It is fine at the frame rates an
        // editor produces.
        const std::deque<ConsoleLine> lines = buffer_.snapshot();

        // Use a child so the console's header (filter bar) stays
        // fixed while the lines scroll.
        ImGui::BeginChild("##console_lines",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        for (const ConsoleLine &line : lines)
        {
            if (!passes_filter(line))
                continue;

            // Color by level.
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            switch (line.level)
            {
            case engine::LogLevel::Trace:
            case engine::LogLevel::Debug:
                color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                break;
            case engine::LogLevel::Warn:
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                break;
            case engine::LogLevel::Error:
            case engine::LogLevel::Fatal:
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                break;
            case engine::LogLevel::Info:
            default:
                break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(line.message.c_str());
            ImGui::PopStyleColor();
        }

        // Auto-scroll: if the user is at the bottom (or the flag
        // is set), scroll to the bottom after laying out.
        if (auto_scroll_)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        else
        {
            // If the user scrolled back to the bottom manually,
            // re-enable auto-scroll.
            const float scroll_max = ImGui::GetScrollMaxY();
            const float scroll_y = ImGui::GetScrollY();
            if (scroll_max > 0.0f && scroll_y >= scroll_max - 1.0f)
            {
                auto_scroll_ = true;
            }
        }

        ImGui::EndChild();
    }

    void ConsolePanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Console", &visible_))
        {
            ImGui::End();
            return;
        }

        draw_filter_bar();
        draw_lines();

        ImGui::End();
    }

} // namespace editor