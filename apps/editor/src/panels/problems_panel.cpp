#include "editor/panels/problems_panel.h"

#include "imgui.h"

namespace editor
{

    namespace
    {

        ImVec4 severity_color(engine::CompileDiagnostic::Severity s)
        {
            using S = engine::CompileDiagnostic::Severity;
            switch (s)
            {
            case S::Error:
            case S::Fatal:
                return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            case S::Warning:
                return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            case S::Note:
            case S::Hint:
            default:
                return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            }
        }

        const char *severity_short(engine::CompileDiagnostic::Severity s)
        {
            using S = engine::CompileDiagnostic::Severity;
            switch (s)
            {
            case S::Error:
                return "error";
            case S::Fatal:
                return "fatal";
            case S::Warning:
                return "warning";
            case S::Note:
                return "note";
            case S::Hint:
                return "hint";
            }
            return "?";
        }

    } // namespace

    void ProblemsPanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Problems", &visible_))
        {
            ImGui::End();
            return;
        }

        if (!store_ || store_->empty())
        {
            ImGui::TextDisabled("(no problems)");
            ImGui::End();
            return;
        }

        ImGui::Text("%d errors, %d warnings",
                    store_->error_count(),
                    store_->warning_count());
        ImGui::SameLine();
        ImGui::Checkbox("Show notes", &show_notes_);
        ImGui::Separator();

        ImGui::BeginChild("##problems_list",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        // Iterate a snapshot? The store is not modified during draw
        // (compile runs in the panel, not here), so no snapshot
        // needed. Just iterate the store's vector directly.
        for (const auto &e : store_->all())
        {
            // Filter notes if not shown.
            using S = engine::CompileDiagnostic::Severity;
            if (!show_notes_ &&
                (e.severity == S::Note || e.severity == S::Hint))
                continue;

            ImGui::PushID(&e);
            ImGui::PushStyleColor(ImGuiCol_Text, severity_color(e.severity));

            // A row: "file:line:col  message".
            const std::string file_name =
                e.file.filename().string();
            const std::string location =
                e.line > 0
                    ? file_name + ":" +
                          std::to_string(e.line) + ":" +
                          std::to_string(e.column)
                    : file_name;

            const std::string label = location + "  " + e.message;

            if (ImGui::Selectable(label.c_str()))
            {
                if (on_jump_)
                    on_jump_(e.file, e.line);
            }

            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s:%u:%u\n%s [%s]",
                                  e.file.string().c_str(),
                                  e.line, e.column,
                                  e.message.c_str(),
                                  severity_short(e.severity));
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace editor