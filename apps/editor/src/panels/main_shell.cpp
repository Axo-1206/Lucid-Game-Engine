#include "editor/panels/main_shell.h"

#include "engine/core/version.h"

#include "imgui.h"
#include "imgui_internal.h" // for ImGuiDockNodeFlags_PassthruCentralNode

namespace editor
{

    void MainShell::set_callbacks(CloseProjectCallback on_close_project,
                                  QuitCallback on_quit,
                                  AboutCallback on_about)
    {
        on_close_project_ = std::move(on_close_project);
        on_quit_ = std::move(on_quit);
        on_about_ = std::move(on_about);
    }

    void MainShell::set_project(const Project *project)
    {
        project_ = project;
    }

    void MainShell::set_package_count(std::size_t count)
    {
        package_count_ = count;
    }

    void MainShell::set_panel_visibility(bool *console_visible,
                                         bool *explorer_visible)
    {
        console_visible_ = console_visible;
        explorer_visible_ = explorer_visible;
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
            // Placeholder. Wired in phase 2.
            ImGui::MenuItem("Reload Packages", nullptr, false, false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (explorer_visible_)
            {
                ImGui::MenuItem("Explorer", "Ctrl+Shift+E", explorer_visible_);
            }
            if (console_visible_)
            {
                ImGui::MenuItem("Console", "Ctrl+Shift+C", console_visible_);
            }
            // Placeholders for panels that do not exist yet.
            ImGui::MenuItem("Script Editor", nullptr, false, false);
            ImGui::MenuItem("Problems", nullptr, false, false);
            ImGui::MenuItem("Package", nullptr, false, false);
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

        // Placeholders. Wired in phase 2.
        ImGui::BeginDisabled();
        ImGui::Button("Build");
        ImGui::SameLine();
        ImGui::Button("Run");
        ImGui::EndDisabled();
    }

    void MainShell::draw_dockspace()
    {
        // A full-area dockspace under the toolbar. Panels dock
        // into it. The default flags give a passthru central node.
        const ImGuiID id = ImGui::GetID("LucidDockspace");
        ImGui::DockSpace(id,
                         ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()),
                         ImGuiDockNodeFlags_PassthruCentralNode);
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
        ImGui::Text("Packages: %zu", package_count_);

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