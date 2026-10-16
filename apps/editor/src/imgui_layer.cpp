#include "editor/imgui_layer.h"
#include "editor/platform_paths.h"

#include "engine/core/log.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

namespace editor
{

    bool ImGuiLayer::init(GLFWwindow *window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Point ImGui at the launch ini by default. The editor
        // switches to a per-project ini when a project is opened.
        set_ini_file(paths::global_config_dir() / "launch.ini");

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
            return false;
        if (!ImGui_ImplOpenGL3_Init("#version 330"))
            return false;

        initialized_ = true;
        return true;
    }

    void ImGuiLayer::shutdown()
    {
        if (!initialized_)
            return;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        // Flush the last ini before destroying the context.
        ImGuiIO &io = ImGui::GetIO();
        if (io.IniFilename != nullptr)
            ImGui::SaveIniSettingsToDisk(io.IniFilename);

        ImGui::DestroyContext();
        initialized_ = false;
    }

    void ImGuiLayer::begin_frame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::end_frame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiLayer::set_ini_file(const std::filesystem::path &path)
    {
        ImGuiIO &io = ImGui::GetIO();

        // Flush the current ini to disk, if we have one and if
        // the backends are up (which implies we are between
        // frames).
        if (initialized_ && io.IniFilename != nullptr)
            ImGui::SaveIniSettingsToDisk(io.IniFilename);

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        ini_path_ = path.string();
        io.IniFilename = ini_path_.c_str();

        ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    }

} // namespace editor