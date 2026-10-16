#include "editor/panels/launch_panel.h"

#include "imgui.h"

#include <cstring>
#include <unordered_set>

namespace editor
{

    void LaunchPanel::set_callbacks(OpenCallback on_open,
                                    CreateCallback on_create,
                                    QuitCallback on_quit,
                                    RefreshCallback on_refresh)
    {
        on_open_ = std::move(on_open);
        on_create_ = std::move(on_create);
        on_quit_ = std::move(on_quit);
        on_refresh_ = std::move(on_refresh);
    }

    void LaunchPanel::set_recent(const RecentProjects *recent)
    {
        recent_ = recent;
    }

    void LaunchPanel::set_scan_results(std::vector<ProjectEntry> entries)
    {
        scan_ = std::move(entries);
    }

    void LaunchPanel::set_default_project_root(std::filesystem::path root)
    {
        default_root_ = std::move(root);
        if (new_parent_[0] == '\0')
        {
            std::snprintf(new_parent_, sizeof(new_parent_),
                          "%s", default_root_.string().c_str());
        }
    }

    void LaunchPanel::set_error(std::string error)
    {
        error_ = std::move(error);
    }

    std::vector<LaunchPanel::ListEntry> LaunchPanel::merged_entries() const
    {
        std::vector<ListEntry> out;
        std::unordered_set<std::string> seen;

        // Scanned projects first.
        for (const auto &e : scan_)
        {
            ListEntry le;
            le.folder = e.folder;
            le.name = e.name;
            le.valid = e.valid;
            le.from_scan = true;
            seen.insert(e.folder.string());
            out.push_back(std::move(le));
        }

        // Then recent entries not already in the scan.
        if (recent_)
        {
            for (const auto &r : recent_->entries)
            {
                if (seen.count(r.path))
                    continue;
                seen.insert(r.path);

                ListEntry le;
                le.folder = r.path;
                le.name = r.name.empty()
                              ? std::filesystem::path(r.path).filename().string()
                              : r.name;
                le.valid = true; // recents are presumed valid
                le.from_scan = false;
                out.push_back(std::move(le));
            }
        }

        return out;
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

        draw_list();

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
        if (ImGui::Button("Refresh", ImVec2(100, 0)))
        {
            if (on_refresh_)
                on_refresh_();
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

    void LaunchPanel::draw_list()
    {
        ImGui::Text("Projects");
        ImGui::Separator();

        const std::vector<ListEntry> entries = merged_entries();

        if (entries.empty())
        {
            ImGui::TextDisabled("(no projects found)");
            if (!default_root_.empty())
            {
                ImGui::TextDisabled("Default location: %s",
                                    default_root_.string().c_str());
            }
            return;
        }

        ImGui::BeginChild("##project_list",
                          ImVec2(0, 300),
                          ImGuiChildFlags_Borders);

        for (const auto &e : entries)
        {
            ImGui::PushID(e.folder.string().c_str());

            const std::string label = e.name;
            if (ImGui::Selectable(label.c_str(),
                                  false,
                                  ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0) && on_open_)
                    on_open_(e.folder);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", e.folder.string().c_str());
            }

            ImGui::SameLine();
            if (e.valid)
                ImGui::TextDisabled("  %s", e.folder.string().c_str());
            else
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                                   "  %s  (invalid project file)",
                                   e.folder.string().c_str());

            ImGui::PopID();
        }

        ImGui::EndChild();
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

        ImGui::Text("Project name:");
        ImGui::InputText("##new_name", new_name_, sizeof(new_name_));

        ImGui::Spacing();
        ImGui::Checkbox("Advanced", &new_advanced_);

        const std::filesystem::path parent =
            new_advanced_ && new_parent_[0] != '\0'
                ? std::filesystem::path(new_parent_)
                : default_root_;

        if (new_advanced_)
        {
            ImGui::Text("Parent folder:");
            ImGui::InputText("##new_parent", new_parent_, sizeof(new_parent_));
        }

        const bool can_create = new_name_[0] != '\0' && !parent.empty();

        if (can_create)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Will create: %s",
                                (parent / new_name_).string().c_str());
        }

        ImGui::Spacing();
        if (!can_create)
            ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(100, 0)))
        {
            const std::filesystem::path folder = parent / new_name_;
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