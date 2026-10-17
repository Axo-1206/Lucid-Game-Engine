#include "editor/panels/file_finder.h"

#include "engine/core/log.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace editor
{

    // ─────────────────────────────────────────────────────────────────
    // file_finder_icontains
    //
    // Case-insensitive substring match. Empty needle matches
    // everything. Non-ASCII bytes pass through unchanged; ASCII is
    // lowercased on both sides. The pre-lowered needle makes the
    // inner loop one tolower per haystack byte.
    //
    // Exposed as a free function so find-in-files (step 7) can
    // reuse it.
    // ─────────────────────────────────────────────────────────────────

    bool file_finder_icontains(const std::string &haystack,
                               const std::string &needle)
    {
        if (needle.empty())
            return true;
        if (needle.size() > haystack.size())
            return false;

        auto lower = [](unsigned char c)
        { return static_cast<char>(std::tolower(c)); };

        std::string needle_lower;
        needle_lower.reserve(needle.size());
        for (unsigned char c : needle)
            needle_lower.push_back(lower(c));

        const std::size_t last = haystack.size() - needle.size();
        for (std::size_t i = 0; i <= last; ++i)
        {
            bool ok = true;
            for (std::size_t j = 0; j < needle.size(); ++j)
            {
                const unsigned char h =
                    static_cast<unsigned char>(haystack[i + j]);
                if (lower(h) != needle_lower[j])
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return true;
        }
        return false;
    }

    // ─────────────────────────────────────────────────────────────────
    // FileFinderPanel
    // ─────────────────────────────────────────────────────────────────

    bool FileFinderPanel::matches(const std::filesystem::path &path,
                                  const char *query)
    {
        return file_finder_icontains(path.generic_string(),
                                     query ? query : "");
    }

    void FileFinderPanel::open(std::vector<std::filesystem::path> files)
    {
        files_ = std::move(files);
        open_ = true;
        request_focus_ = true;
        selected_ = 0;
        query_[0] = '\0';

        // A fresh session starts with no pending request. If a
        // previous session's request was never consumed (a caller
        // bug), drop it rather than carry it into the new session.
        if (pending_open_.has_value())
        {
            engine::log_warn(
                "FileFinderPanel::open: dropping an unconsumed open "
                "request from a previous session");
        }
        pending_open_.reset();

        refilter();
    }

    void FileFinderPanel::close()
    {
        open_ = false;
        request_focus_ = false;

        // pending_open_ is deliberately NOT cleared here. A
        // confirmation (click or Enter) sets it and then calls
        // close(); the request must survive until the caller
        // consumes it via take_open_request(). A cancel (Escape)
        // never set it, so there is nothing to clear.
    }

    void FileFinderPanel::refilter()
    {
        filtered_indices_.clear();
        filtered_indices_.reserve(files_.size());

        for (int i = 0; i < static_cast<int>(files_.size()); ++i)
        {
            if (matches(files_[i], query_))
                filtered_indices_.push_back(i);
        }

        // Clamp the selection to the new filtered range.
        if (filtered_indices_.empty())
        {
            selected_ = -1;
        }
        else if (selected_ < 0 ||
                 selected_ >= static_cast<int>(filtered_indices_.size()))
        {
            selected_ = 0;
        }
    }

    std::optional<std::filesystem::path>
    FileFinderPanel::take_open_request()
    {
        auto out = pending_open_;
        pending_open_.reset();
        return out;
    }

    void FileFinderPanel::draw()
    {
        if (!open_)
            return;

        // Position: top-center of the viewport, top edge at 10% down.
        // Pivot (0.5, 0.0) puts the window's top-center on the
        // given point — near the top of the screen, like most
        // editors' quick-open dialogs.
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
        // No AlwaysAutoResize: it conflicts with SetNextWindowSize
        // and makes the 640-width hint ineffective.

        if (!ImGui::Begin("##file_finder", &open, flags))
        {
            ImGui::End();
            close();
            return;
        }

        // Escape cancels. The root/child check keeps the key from
        // firing when the input text field has eaten it (it
        // doesn't handle Escape, but this is future-proof).
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
        if (ImGui::InputTextWithHint("##query", "Go to file...",
                                     query_, sizeof(query_)))
        {
            refilter();
        }

        // Arrow keys move the selection; Enter confirms.
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
                pending_open_ = files_[filtered_indices_[selected_]];
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

        // Cap the list to 20 rows. A project with thousands of
        // files is a wall of text; the filter narrows it down.
        const int max_rows = 20;
        const int rows = std::min(
            static_cast<int>(filtered_indices_.size()), max_rows);

        ImGui::BeginChild(
            "##file_list",
            ImVec2(0, rows * ImGui::GetTextLineHeightWithSpacing()),
            ImGuiChildFlags_None);

        for (int row = 0; row < static_cast<int>(filtered_indices_.size()); ++row)
        {
            const int file_idx = filtered_indices_[row];
            const std::string display = files_[file_idx].generic_string();

            ImGui::PushID(row);
            const bool is_selected = (row == selected_);

            if (ImGui::Selectable(display.c_str(), is_selected))
            {
                pending_open_ = files_[file_idx];
                close();
                ImGui::PopID();
                ImGui::EndChild();
                ImGui::End();
                return;
            }

            if (is_selected)
            {
                // Keep the selected row visible when arrow keys
                // move the selection past the bottom of the list.
                ImGui::SetScrollHereY(0.5f);
            }

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