#include "editor/editor_app.h"
#include "engine/core/log.h"

#include "imgui.h"

#include <glad/glad.h> // GLAD before GLFW (NOTE: DO NOT REMOVE THIS COMMENT)
#include <GLFW/glfw3.h>

namespace editor
{

    EditorApp::EditorApp()
        : window_(1280, 720, "Lucid Editor")
    {
    }

    EditorApp::~EditorApp() = default;

    bool EditorApp::init()
    {
        GLFWwindow *handle = static_cast<GLFWwindow *>(window_.native_handle());
        if (!handle)
        {
            engine::log_error("EditorApp: window creation failed");
            return false;
        }

        if (!imgui_.init(handle))
        {
            engine::log_error("EditorApp: ImGui initialization failed");
            return false;
        }

        initialized_ = true;
        return true;
    }

    int EditorApp::run()
    {
        if (!initialized_)
        {
            engine::log_error("EditorApp::run called before init");
            return 1;
        }

        engine::log_info("Editor loop starting");

        while (!window_.should_close())
        {
            clock_.start_frame();
            window_.poll_events();

            imgui_.begin_frame();

            ImGui::Begin("Lucid Editor");
            ImGui::Text("ImGui is working.");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();

            imgui_.end_frame();

            window_.swap_buffers();
        }

        imgui_.shutdown();
        engine::log_info("Editor loop ended");
        return 0;
    }

} // namespace editor