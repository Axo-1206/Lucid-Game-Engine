#include "editor/panels/find_in_files_panel.h"

#include "imgui.h"

#include <cstring>
#include <map>
#include <string>

namespace editor
{

    void FindInFilesPanel::set_results(std::string query,
                                       std::vector<FindMatch> matches)
    {
        results_query_ = std::move(query);
        matches_ = std::move(matches);
    }

    void FindInFilesPanel::draw_filter_bar()
    {
        if (request_focus_)
        {
            ImGui::SetKeyboardFocusHere();
            request_focus_ = false;
        }

        ImGui::SetNextItemWidth(-140.0f);
        const bool enter = ImGui::InputTextWithHint(
            "##query", "Find in files...", query_, sizeof(query_),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        ImGui::Checkbox("Aa", &case_sensitive_);

        ImGui::SameLine();
        const bool clicked = ImGui::Button("Search", ImVec2(80, 0));

        if ((enter || clicked) && query_[0] != '\0' && on_search_)
        {
            on_search_(query_, case_sensitive_);
        }

        ImGui::Separator();
    }

    void FindInFilesPanel::draw_results()
    {
        if (matches_.empty())
        {
            if (results_query_.empty())
                ImGui::TextDisabled("(no search yet)");
            else
                ImGui::TextDisabled("No matches for \"%s\"",
                                    results_query_.c_str());
            return;
        }

        ImGui::Text("Results for \"%s\": %zu match%s",
                    results_query_.c_str(),
                    matches_.size(),
                    matches_.size() == 1 ? "" : "es");
        ImGui::Separator();

        ImGui::BeginChild("##results_list",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        // Group by file, preserving order. A std::map keyed on the
        // file path, with an ordered value, gives deterministic
        // grouping.
        std::map<std::filesystem::path, std::vector<const FindMatch *>> by_file;
        for (const auto &m : matches_)
            by_file[m.file].push_back(&m);

        for (const auto &[file, entries] : by_file)
        {
            const std::string header =
                file.filename().string() + "  (" +
                std::to_string(entries.size()) + ")";

            if (ImGui::TreeNodeEx(
                    header.c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen |
                        ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", file.string().c_str());

                for (const FindMatch *m : entries)
                {
                    ImGui::PushID(m);

                    // Trim leading whitespace for display.
                    std::size_t start = 0;
                    while (start < m->line_text.size() &&
                           std::isspace(static_cast<unsigned char>(
                               m->line_text[start])))
                        ++start;
                    const std::string trimmed =
                        m->line_text.substr(start);

                    const std::string label =
                        std::to_string(m->line) + ":  " + trimmed;

                    if (ImGui::Selectable(label.c_str()))
                    {
                        if (on_jump_)
                            on_jump_(m->file, m->line);
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s:%u\n%s",
                                          file.filename().string().c_str(),
                                          m->line,
                                          m->line_text.c_str());
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }

        ImGui::EndChild();
    }

    void FindInFilesPanel::draw()
    {
        if (!visible_)
            return;

        if (!ImGui::Begin("Find Results", &visible_))
        {
            ImGui::End();
            return;
        }

        draw_filter_bar();
        draw_results();

        ImGui::End();
    }

} // namespace editor