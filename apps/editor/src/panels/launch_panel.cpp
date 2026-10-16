#include "editor/panels/launch_panel.h"

#include "imgui.h"

#include <cstring>

namespace editor
{

    void LaunchPanel::set_callbacks(OpenCallback on_open,
                                    CreateCallback on_create,
                                    QuitCallback on_quit)
    {
        on_open_ = std::move(on_open);
        on_create_ = std::move(on_create);
        on_quit_ = std::move(on_quit);
    }

    void LaunchPanel::set_recent(const RecentProjects *recent)
    {
        recent_ = recent;
    }

    void LaunchPanel::set_error(std::string error)
    {
        error_ = std::move(error);
    }

    void LaunchPanel::draw()
    {
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGui::Begin("Lucid Editor",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoNavFocus);

        ImGui::Text("Lucid Editor");
        ImGui::Separator();
        ImGui::Spacing();

        draw_recent();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Open Project...", ImVec2(160, 0)))
        {
            show_open_ = true;
            ImGui::OpenPopup("Open Project");
        }
        ImGui::SameLine();
        if (ImGui::Button("New Project...", ImVec2(160, 0)))
        {
            show_new_ = true;
            ImGui::OpenPopup("New Project");
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(80, 0)))
        {
            if (on_quit_)
                on_quit_();
        }

        if (!error_.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "%s", error_.c_str());
        }

        draw_open_modal();
        draw_new_modal();

        ImGui::End();
    }

    void LaunchPanel::draw_recent()
    {
        ImGui::Text("Recent Projects");
        ImGui::Separator();

        if (!recent_ || recent_->entries.empty())
        {
            ImGui::TextDisabled("(no recent projects)");
            return;
        }

        for (const auto &e : recent_->entries)
        {
            // Each entry: clickable name on the left, path in a
            // disabled color on the right.
            ImGui::PushID(e.path.c_str());

            if (ImGui::Selectable(e.name.c_str(),
                                  false,
                                  ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0) && on_open_)
                {
                    on_open_(e.path);
                }
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", e.path.c_str());
            }

            ImGui::SameLine();
            ImGui::TextDisabled("  %s", e.path.c_str());
            ImGui::PopID();
        }
    }

    void LaunchPanel::draw_open_modal()
    {
        if (!ImGui::BeginPopupModal("Open Project", &show_open_))
            return;

        ImGui::Text("Project folder:");
        ImGui::InputText("##open_path", open_path_, sizeof(open_path_));
        ImGui::Spacing();

        const bool can_open = open_path_[0] != '\0';
        if (!can_open)
            ImGui::BeginDisabled();
        if (ImGui::Button("Open", ImVec2(100, 0)))
        {
            if (on_open_)
                on_open_(open_path_);
            show_open_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (!can_open)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            show_open_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void LaunchPanel::draw_new_modal()
    {
        if (!ImGui::BeginPopupModal("New Project", &show_new_))
            return;

        ImGui::Text("Parent folder:");
        ImGui::InputText("##new_parent", new_parent_, sizeof(new_parent_));
        ImGui::Text("Project name:");
        ImGui::InputText("##new_name", new_name_, sizeof(new_name_));

        const bool can_create = new_parent_[0] != '\0' && new_name_[0] != '\0';

        // The resulting folder is parent/name. Show it so the user
        // sees what will be created.
        if (can_create)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Will create: %s/%s", new_parent_, new_name_);
        }

        ImGui::Spacing();
        if (!can_create)
            ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(100, 0)))
        {
            const std::filesystem::path folder =
                std::filesystem::path(new_parent_) / new_name_;
            if (on_create_)
                on_create_(folder, new_name_);
            show_new_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (!can_create)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            show_new_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

} // namespace editor