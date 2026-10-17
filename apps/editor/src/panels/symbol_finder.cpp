#include "editor/panels/symbol_finder.h"

#include "editor/panels/file_finder.h" // file_finder_icontains

#include "imgui.h"

#include <algorithm>
#include <string>

namespace editor
{

    bool SymbolFinderPanel::matches(const SymbolEntry &e, const char *query)
    {
        return file_finder_icontains(e.name, query ? query : "");
    }

    void SymbolFinderPanel::open(std::vector<SymbolEntry> entries)
    {
        entries_ = std::move(entries);
        open_ = true;
        request_focus_ = true;
        selected_ = 0;
        query_[0] = '\0';
        pending_open_.reset();
        refilter();
    }

    void SymbolFinderPanel::close()
    {
        open_ = false;
        request_focus_ = false;
        // pending_open_ is intentionally preserved, matching the
        // FileFinderPanel contract.
    }

    void SymbolFinderPanel::refilter()
    {
        filtered_indices_.clear();
        filtered_indices_.reserve(entries_.size());
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        {
            if (matches(entries_[i], query_))
                filtered_indices_.push_back(i);
        }

        if (filtered_indices_.empty())
            selected_ = -1;
        else if (selected_ < 0 ||
                 selected_ >= static_cast<int>(filtered_indices_.size()))
            selected_ = 0;
    }

    std::optional<std::pair<std::filesystem::path, std::uint32_t>>
    SymbolFinderPanel::take_open_request()
    {
        auto out = pending_open_;
        pending_open_.reset();
        return out;
    }

    void SymbolFinderPanel::draw()
    {
        if (!open_)
            return;

        const ImGuiViewport *vp = ImGui::GetMainViewport();
        const ImVec2 top_center(
            vp->WorkPos.x + vp->WorkSize.x * 0.5f,
            vp->WorkPos.y + vp->WorkSize.y * 0.10f);

        ImGui::SetNextWindowPos(top_center, ImGuiCond_Always,
                                ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(640, 0), ImGuiCond_Always);

        bool open = open_;
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

        if (!ImGui::Begin("##symbol_finder", &open, flags))
        {
            ImGui::End();
            close();
            return;
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            close();
            ImGui::End();
            return;
        }

        if (request_focus_)
        {
            ImGui::SetKeyboardFocusHere();
            request_focus_ = false;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##query", "Go to symbol...",
                                     query_, sizeof(query_)))
        {
            refilter();
        }

        if (!filtered_indices_.empty())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                selected_++;
                if (selected_ >= static_cast<int>(filtered_indices_.size()))
                    selected_ = static_cast<int>(filtered_indices_.size()) - 1;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                if (selected_ > 0)
                    selected_--;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                const auto &e = entries_[filtered_indices_[selected_]];
                pending_open_ = {e.file, e.line};
                close();
                ImGui::End();
                return;
            }
        }

        ImGui::Separator();

        if (filtered_indices_.empty())
        {
            ImGui::TextDisabled("(no matches)");
            ImGui::Separator();
            ImGui::TextDisabled(
                "Enter to open, Esc to cancel, Up/Down to navigate");
            ImGui::End();
            return;
        }

        const int max_rows = 20;
        const int rows = std::min(
            static_cast<int>(filtered_indices_.size()), max_rows);

        ImGui::BeginChild(
            "##symbol_list",
            ImVec2(0, rows * ImGui::GetTextLineHeightWithSpacing()),
            ImGuiChildFlags_None);

        for (int row = 0; row < static_cast<int>(filtered_indices_.size()); ++row)
        {
            const int idx = filtered_indices_[row];
            const SymbolEntry &e = entries_[idx];

            const std::string display =
                e.name + "  (" + symbol_kind_name(e.kind) + ")  " +
                e.file.filename().string() + ":" + std::to_string(e.line);

            ImGui::PushID(row);
            const bool is_selected = (row == selected_);
            if (ImGui::Selectable(display.c_str(), is_selected))
            {
                pending_open_ = {e.file, e.line};
                close();
                ImGui::PopID();
                ImGui::EndChild();
                ImGui::End();
                return;
            }
            if (is_selected)
                ImGui::SetScrollHereY(0.5f);
            ImGui::PopID();
        }

        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled(
            "Enter to open, Esc to cancel, Up/Down to navigate");

        ImGui::End();

        if (!open)
            close();
    }

} // namespace editor