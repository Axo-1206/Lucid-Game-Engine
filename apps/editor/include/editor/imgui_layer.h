#pragma once

#include <filesystem>
#include <string>

struct GLFWwindow;

namespace editor
{

    // Owns the ImGui context and the two backends. One instance
    // per editor.
    //
    // The layer also owns ImGui's ini persistence: the path is
    // stored as a std::string so the c_str() lives as long as
    // the ImGui context.
    class ImGuiLayer
    {
    public:
        bool init(GLFWwindow *window);
        void shutdown();

        void begin_frame();
        void end_frame();

        // Switch the ini file. Flushes the current ini to disk,
        // then loads the new one. The parent directory is created
        // if needed. Safe to call between frames; if called from
        // inside a frame (e.g. from a panel's menu), it is still
        // safe, but the visual effect appears on the next frame.
        void set_ini_file(const std::filesystem::path &path);

    private:
        bool initialized_ = false;
        std::string ini_path_;
    };

} // namespace editor