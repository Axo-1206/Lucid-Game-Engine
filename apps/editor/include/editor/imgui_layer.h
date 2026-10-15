#pragma once

struct GLFWwindow;

namespace editor
{

    class ImGuiLayer
    {
    public:
        bool init(GLFWwindow *window);
        void shutdown();

        void begin_frame();
        void end_frame();

    private:
        bool initialized_ = false;
    };

} // namespace editor