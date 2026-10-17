// editor/src/panels/preferences_panel.cpp

#include "editor/panels/preferences_panel.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace editor
{

    void PreferencesPanel::open(const Settings &current)
    {
        working_ = current;
        roots_ = current.project_roots;
        new_root_buf_[0] = '\0';
        open_ = true;
        pending_open_ = true; // OpenPopup runs from draw(), same ID stack
    }

    void PreferencesPanel::close()
    {
        open_ = false;
    }

    void PreferencesPanel::apply()
    {
        working_.project_roots = roots_;
        if (on_save_)
            on_save_(working_);
        open_ = false;
    }

    void PreferencesPanel::draw_theme_section()
    {
        // Theme and font size are visible but disabled. The
        // fields exist so a user knows they're coming; the
        // tooltips name the decision so a curious user can look
        // it up. Enabling them later is a two-line change (remove
        // BeginDisabled/EndDisabled, add the callback).

        ImGui::TextUnformatted("Theme");
        ImGui::SameLine(220.0f);

        int theme_idx = 0;
        const char *themes[] = {"dark", "light"};
        for (int i = 0; i < 2; ++i)
        {
            if (working_.theme == themes[i])
            {
                theme_idx = i;
                break;
            }
        }

        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::Combo("##theme", &theme_idx, themes, 2);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(
                "Themes are not yet configurable "
                "(EditorPlan Decision 42).");
        }

        ImGui::TextUnformatted("Font size");
        ImGui::SameLine(220.0f);

        float fs = working_.font_size;
        ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("##font_size", &fs, 10.0f, 24.0f, "%.1f");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(
                "Fonts are not yet configurable "
                "(EditorPlan Decision 42).");
        }
    }

    void PreferencesPanel::draw_default_parent_section()
    {
        ImGui::TextUnformatted("Default new-project parent");
        ImGui::SameLine(220.0f);

        // ImGui's InputText writes to a char* buffer. A local
        // buffer synced from the std::string and copied back is
        // the standard pattern; the copy-back is cheap and only
        // fires when the user actually edits.
        char buf[512] = {};
        std::strncpy(buf, working_.default_new_project_parent.c_str(),
                     sizeof(buf) - 1);

        ImGui::SetNextItemWidth(400.0f);
        if (ImGui::InputText("##parent", buf, sizeof(buf)))
        {
            working_.default_new_project_parent = buf;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Where New Project places new folders. Empty means "
                "the platform default root.");
        }
    }

    void PreferencesPanel::draw_console_section()
    {
        ImGui::TextUnformatted("Show Console on start");
        ImGui::SameLine(220.0f);

        ImGui::Checkbox("##show_console",
                        &working_.show_console_on_start);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Honored at the next launch. Does not hide or show "
                "the Console now.");
        }
    }

    void PreferencesPanel::draw_project_roots_section()
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Project roots");
        ImGui::TextDisabled(
            "Extra folders the launch panel scans for projects. "
            "The platform default root is always scanned and is "
            "not listed here.");

        if (roots_.empty())
        {
            ImGui::TextDisabled("(no extra roots)");
        }
        else
        {
            ImGui::BeginChild("##roots", ImVec2(0, 120.0f),
                              ImGuiChildFlags_Borders);
            for (int i = 0; i < static_cast<int>(roots_.size()); ++i)
            {
                ImGui::PushID(i);
                ImGui::TextUnformatted(roots_[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    roots_.erase(roots_.begin() + i);
                    ImGui::PopID();
                    // Re-iterate next frame so the list is
                    // consistent even if it shrank under us.
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputTextWithHint("##new_root", "Path to add...",
                                 new_root_buf_, sizeof(new_root_buf_));
        ImGui::SameLine();

        const bool can_add = new_root_buf_[0] != '\0';
        if (!can_add)
            ImGui::BeginDisabled();
        if (ImGui::Button("Add"))
        {
            const std::string p = new_root_buf_;
            if (std::find(roots_.begin(), roots_.end(), p) == roots_.end())
                roots_.push_back(p);
            new_root_buf_[0] = '\0';
        }
        if (!can_add)
            ImGui::EndDisabled();
    }

    void PreferencesPanel::draw()
    {
        if (!open_)
            return;

        if (pending_open_)
        {
            ImGui::OpenPopup("Preferences");
            pending_open_ = false;
        }

        // Position and size must be set *before* BeginPopupModal.
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        const ImVec2 center(
            vp->WorkPos.x + vp->WorkSize.x * 0.5f,
            vp->WorkPos.y + vp->WorkSize.y * 0.30f);

        ImGui::SetNextWindowPos(center, ImGuiCond_Always,
                                ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(720.0f, 0), ImGuiCond_Always);

        bool popup_open = true;
        if (!ImGui::BeginPopupModal("Preferences", &popup_open,
                                    ImGuiWindowFlags_NoSavedSettings))
        {
            if (!popup_open)
                close();
            return;
        }

        draw_theme_section();
        draw_default_parent_section();
        draw_console_section();
        draw_project_roots_section();

        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(100, 0)))
        {
            apply();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            close();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

        if (!popup_open)
            close();
    }

} // namespace editor