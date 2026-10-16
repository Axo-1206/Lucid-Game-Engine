#include "editor/panels/main_shell.h"

#include "engine/core/version.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace editor
{

    void MainShell::set_callbacks(CloseProjectCallback on_close_project,
                                  QuitCallback on_quit,
                                  AboutCallback on_about,
                                  SaveCallback on_save,
                                  SaveAllCallback on_save_all,
                                  ResetLayoutCallback on_reset_layout)
    {
        on_close_project_ = std::move(on_close_project);
        on_quit_ = std::move(on_quit);
        on_about_ = std::move(on_about);
        on_save_ = std::move(on_save);
        on_save_all_ = std::move(on_save_all);
        on_reset_layout_ = std::move(on_reset_layout);
    }

    void MainShell::set_project(const Project *project)
    {
        project_ = project;
    }

    void MainShell::set_active_scene(std::string scene)
    {
        active_scene_ = std::move(scene);
    }

    void MainShell::set_package_count(std::size_t count)
    {
        package_count_ = count;
    }

    void MainShell::set_panel_visibility(bool *console_visible,
                                         bool *explorer_visible,
                                         bool *script_editor_visible,
                                         bool *problems_visible,
                                         bool *package_visible)
    {
        console_visible_ = console_visible;
        explorer_visible_ = explorer_visible;
        script_editor_visible_ = script_editor_visible;
        problems_visible_ = problems_visible;
        package_visible_ = package_visible;
    }

    void MainShell::draw()
    {
        const ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGui::Begin("##main_shell",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_MenuBar |
                         ImGuiWindowFlags_NoNavFocus);

        draw_menu_bar();
        draw_toolbar();
        draw_dockspace();
        draw_status_bar();
        draw_about_modal();

        ImGui::End();
    }

    void MainShell::draw_menu_bar()
    {
        if (!ImGui::BeginMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save", "Ctrl+S"))
            {
                if (on_save_)
                    on_save_();
            }
            if (ImGui::MenuItem("Save All", "Ctrl+Shift+S"))
            {
                if (on_save_all_)
                    on_save_all_();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Project"))
            {
                if (on_close_project_)
                    on_close_project_();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                if (on_quit_)
                    on_quit_();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Project"))
        {
            ImGui::MenuItem("Reload Packages", nullptr, false, false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (explorer_visible_)
                ImGui::MenuItem("Explorer", "Ctrl+Shift+E", explorer_visible_);
            if (script_editor_visible_)
                ImGui::MenuItem("Script Editor", nullptr, script_editor_visible_);
            if (console_visible_)
                ImGui::MenuItem("Console", "Ctrl+Shift+C", console_visible_);
            if (problems_visible_)
                ImGui::MenuItem("Problems", "Ctrl+Shift+P", problems_visible_);
            if (package_visible_)
                ImGui::MenuItem("Package", "Ctrl+Shift+G", package_visible_);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                if (on_reset_layout_)
                    on_reset_layout_();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                if (on_about_)
                    on_about_();
                show_about_ = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    void MainShell::draw_toolbar()
    {
        const std::string name =
            project_ ? project_->name() : std::string("(no project)");
        ImGui::Text("Project: %s", name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        ImGui::BeginDisabled();
        ImGui::Button("Build");
        ImGui::SameLine();
        ImGui::Button("Run");
        ImGui::EndDisabled();
    }

    void MainShell::build_default_layout(unsigned int dockspace_id,
                                         float width, float height)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(
            dockspace_id,
            ImGuiDockNodeFlags_DockSpace |
                ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImVec2(width, height));

        ImGuiID central = dockspace_id;

        ImGuiID left = ImGui::DockBuilderSplitNode(
            central, ImGuiDir_Left, 0.20f, nullptr, &central);

        ImGuiID bottom = ImGui::DockBuilderSplitNode(
            central, ImGuiDir_Down, 0.25f, nullptr, &central);

        ImGuiID bottom_left = bottom;
        ImGuiID bottom_right = ImGui::DockBuilderSplitNode(
            bottom_left, ImGuiDir_Right, 0.5f, nullptr, &bottom_left);

        ImGui::DockBuilderDockWindow("Explorer", left);
        ImGui::DockBuilderDockWindow("Script Editor", central);
        ImGui::DockBuilderDockWindow("Package", central);
        ImGui::DockBuilderDockWindow("Console", bottom_left);
        ImGui::DockBuilderDockWindow("Problems", bottom_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    void MainShell::draw_dockspace()
    {
        const ImGuiID id = ImGui::GetID("LucidDockspace");
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 size(0.0f, -ImGui::GetFrameHeightWithSpacing());

        ImGui::DockSpace(id, size, ImGuiDockNodeFlags_PassthruCentralNode);

        ImGuiDockNode *node = ImGui::DockBuilderGetNode(id);
        if (node == nullptr || node->ChildNodes[0] == nullptr)
        {
            build_default_layout(id, avail.x, avail.y);
        }
    }

    void MainShell::draw_status_bar()
    {
        ImGui::Separator();

        const std::string name =
            project_ ? project_->name() : std::string("(no project)");
        ImGui::Text("%s", name.c_str());

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (active_scene_.empty())
            ImGui::TextDisabled("Scene: (none)");
        else
            ImGui::Text("Scene: %s", active_scene_.c_str());

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("Packages: %zu", package_count_);

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
#if defined(NDEBUG)
        ImGui::Text("Release");
#else
        ImGui::Text("Debug");
#endif

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    }

    void MainShell::draw_about_modal()
    {
        if (show_about_)
        {
            ImGui::OpenPopup("About Lucid Editor");
            show_about_ = false;
        }

        if (ImGui::BeginPopupModal("About Lucid Editor", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Lucid Editor");
            ImGui::Separator();
            ImGui::Text("Engine: %s", engine::kEngineVersion);
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

} // namespace editor