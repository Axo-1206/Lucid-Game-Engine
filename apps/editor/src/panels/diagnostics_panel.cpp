#include "editor/panels/diagnostics_panel.h"

#include "engine/core/format_diagnostics.h" // diagnostic_code_name

#include "imgui.h"

#include <cstring>
#include <string>

namespace editor
{

    namespace
    {

        // Format a code as "E6041" or "W8001", derived from the band
        // and the numeric value. The band prefix letter (E/W) reflects
        // severity; the number is the raw code. Codes without a number
        // (free text, code 0) render as "E-" / "W-".
        std::string format_code_badge(std::uint32_t code,
                                      engine::LogLevel severity)
        {
            const bool is_warning =
                (severity == engine::LogLevel::Warn);

            char prefix = is_warning ? 'W' : 'E';

            if (code == 0)
                return std::string(1, prefix) + "-";

            return std::string(1, prefix) + std::to_string(code);
        }

        ImVec4 severity_color(engine::LogLevel sev)
        {
            switch (sev)
            {
            case engine::LogLevel::Fatal:
            case engine::LogLevel::Error:
                return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            case engine::LogLevel::Warn:
                return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            case engine::LogLevel::Info:
                return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            case engine::LogLevel::Debug:
            case engine::LogLevel::Trace:
            default:
                return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            }
        }

    } // namespace

    DiagnosticsPanel::DiagnosticsPanel() = default;

    DiagnosticsPanel::~DiagnosticsPanel()
    {
        detach_from_engine();
    }

    void DiagnosticsPanel::attach_to_engine()
    {
        if (sink_id_ != 0)
            return;
        if (!store_)
            return;

        RuntimeDiagnosticStore *store = store_;
        sink_id_ = engine::add_diagnostic_sink(
            [store](const engine::RuntimeDiagnostic &d)
            { store->push(d); });
    }

    void DiagnosticsPanel::detach_from_engine()
    {
        if (sink_id_ == 0)
            return;
        engine::remove_diagnostic_sink(sink_id_);
        sink_id_ = 0;
    }

    bool DiagnosticsPanel::passes_filter(
        const RuntimeDiagnosticEntry &e) const
    {
        switch (e.severity)
        {
        case engine::LogLevel::Error:
        case engine::LogLevel::Fatal:
            if (!show_errors_)
                return false;
            break;
        case engine::LogLevel::Warn:
            if (!show_warnings_)
                return false;
            break;
        default:
            if (!show_notes_)
                return false;
            break;
        }

        if (text_filter_[0] != '\0')
        {
            if (e.message.find(text_filter_) == std::string::npos &&
                e.file.string().find(text_filter_) == std::string::npos)
                return false;
        }
        return true;
    }

    void DiagnosticsPanel::draw_filter_bar()
    {
        ImGui::Checkbox("Errors", &show_errors_);
        ImGui::SameLine();
        ImGui::Checkbox("Warnings", &show_warnings_);
        ImGui::SameLine();
        ImGui::Checkbox("Notes", &show_notes_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##filter", "filter...", text_filter_,
                                 sizeof(text_filter_));
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            if (store_)
                store_->clear();
        }
        ImGui::Separator();
    }

    void DiagnosticsPanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Diagnostics", &visible_))
        {
            ImGui::End();
            return;
        }

        if (!store_)
        {
            ImGui::TextDisabled("(no diagnostic store)");
            ImGui::End();
            return;
        }

        const int errors = store_->error_count();
        const int warnings = store_->warning_count();
        ImGui::Text("%d error%s, %d warning%s",
                    errors, errors == 1 ? "" : "s",
                    warnings, warnings == 1 ? "" : "s");
        ImGui::SameLine();
        ImGui::TextDisabled("(runtime; see Problems for compile errors)");
        ImGui::Separator();

        draw_filter_bar();

        const auto entries = store_->snapshot();
        if (entries.empty())
        {
            ImGui::TextDisabled("(no runtime diagnostics)");
            ImGui::End();
            return;
        }

        ImGui::BeginChild("##diag_list",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto &e : entries)
        {
            if (!passes_filter(e))
                continue;

            ImGui::PushID(&e);
            ImGui::PushStyleColor(ImGuiCol_Text, severity_color(e.severity));

            const std::string badge = format_code_badge(e.code, e.severity);
            const std::string file_name = e.file.empty()
                                              ? std::string("(no file)")
                                              : e.file.filename().string();
            const std::string location = e.line > 0
                                             ? file_name + ":" + std::to_string(e.line)
                                             : file_name;

            const std::string label =
                badge + "  " + location + "  " + e.message;

            if (ImGui::Selectable(label.c_str()))
            {
                if (!e.file.empty() && on_jump_)
                    on_jump_(e.file, e.line);
            }

            if (ImGui::IsItemHovered())
            {
                const char *band = engine::diagnostic_code_name(e.code);
                const std::string line_str = e.line > 0 ? std::to_string(e.line) : std::string();
                ImGui::SetTooltip("%s%s%s\nband: %s\n%s",
                                  e.file.empty() ? "(no file)" : e.file.string().c_str(),
                                  e.line > 0 ? ":" : "",
                                  line_str.c_str(),
                                  band,
                                  e.message.c_str());
            }

            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace editor