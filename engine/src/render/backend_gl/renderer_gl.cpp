#include "engine/render/backend_gl/renderer_gl.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cmath>
#include <cstring>

namespace engine
{

    namespace
    {

        const char *kSpriteVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;

uniform mat4 u_projection;

out vec2 v_uv;
out vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
}
)";

        const char *kSpriteFragmentShader = R"(
#version 330 core
in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    frag_color = texture(u_texture, v_uv) * v_color;
}
)";

    } // namespace

    RendererGL::RendererGL()
        : batch_(8192)
    {
    }

    RendererGL::~RendererGL()
    {
        shutdown();
    }

    bool RendererGL::init(void *native_window)
    {
        window_ = static_cast<GLFWwindow *>(native_window);
        if (window_ == nullptr)
        {
            log_error("RendererGL: null window");
            return false;
        }

        glfwMakeContextCurrent(window_);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            log_error("RendererGL: failed to load OpenGL functions");
            return false;
        }

        log_info("OpenGL version: %s", glGetString(GL_VERSION));
        log_info("GPU renderer:   %s", glGetString(GL_RENDERER));

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (!shader_.compile(kSpriteVertexShader, kSpriteFragmentShader))
        {
            log_error("RendererGL: failed to compile sprite shader");
            return false;
        }

        batch_.init();
        batch_.set_shader(&shader_);

        // Create the white texture and cache its GL id.
        {
            std::uint32_t gl_tex = 0;
            glGenTextures(1, &gl_tex);
            glBindTexture(GL_TEXTURE_2D, gl_tex);
            const std::uint8_t white_pixel[4] = {255, 255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            white_texture_.id = next_texture_id_++;
            white_gl_id_ = gl_tex;
            textures_[white_texture_.id] = gl_tex;
        }

        // Sync the window size.
        int ww = 0, wh = 0;
        glfwGetFramebufferSize(window_, &ww, &wh);
        window_w_ = ww;
        window_h_ = wh;
        viewport_x_ = 0;
        viewport_y_ = 0;
        viewport_w_ = ww;
        viewport_h_ = wh;

        // Set the initial world camera's viewport in sync.
        camera_.viewport_w = static_cast<float>(ww);
        camera_.viewport_h = static_cast<float>(wh);

        initialized_ = true;
        return true;
    }

    void RendererGL::shutdown()
    {
        if (!initialized_)
            return;

        for (auto &[handle, gl_tex] : textures_)
        {
            GLuint t = gl_tex;
            glDeleteTextures(1, &t);
        }
        textures_.clear();
        texture_cache_.clear();
        white_gl_id_ = 0;

        batch_.destroy();
        shader_.destroy();

        initialized_ = false;
        window_ = nullptr;
    }

    // =============================================================================
    // Frame
    // =============================================================================

    void RendererGL::begin_frame(Color clear_color)
    {
        // Clear the whole window, regardless of the current viewport.
        // The default framebuffer's full extent is what we clear.
        glViewport(0, 0, window_w_, window_h_);
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        reset_stats();
    }

    void RendererGL::end_frame()
    {
        // The batch's per-pass end handles flushing. Nothing to do here.
    }

    void RendererGL::present()
    {
        if (window_)
            glfwSwapBuffers(window_);
    }

    // =============================================================================
    // Passes
    // =============================================================================

    void RendererGL::begin_world_2d()
    {
        // Bind the viewport.
        glViewport(viewport_x_, viewport_y_, viewport_w_, viewport_h_);

        // The world camera's viewport in the projection is the current
        // viewport's size, not the camera's stored viewport. This lets a
        // sub-viewport render the world correctly.
        Camera2D cam = camera_;
        cam.viewport_w = static_cast<float>(viewport_w_);
        cam.viewport_h = static_cast<float>(viewport_h_);

        float proj[16];
        {
            const float half_w = cam.viewport_w * 0.5f / cam.zoom;
            const float half_h = cam.viewport_h * 0.5f / cam.zoom;
            const float left = cam.x - half_w;
            const float right = cam.x + half_w;
            const float bottom = cam.y + half_h;
            const float top = cam.y - half_h;
            glm::mat4 m = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
            std::memcpy(proj, &m[0][0], sizeof(float) * 16);
        }

        shader_.bind();
        shader_.set_mat4("u_projection", proj);
        shader_.set_int("u_texture", 0);

        batch_.begin();
    }

    void RendererGL::end_world_2d()
    {
        batch_.end();
        frame_stats_.draw_calls += batch_.draw_calls();
        frame_stats_.quads_drawn += batch_.quads_drawn();
        frame_stats_.triangles_drawn += batch_.triangles_drawn();
    }

    void RendererGL::begin_ui()
    {
        glViewport(viewport_x_, viewport_y_, viewport_w_, viewport_h_);

        // Screen-space orthographic: (0, 0) top-left, (w, h) bottom-right,
        // +y down. The viewport's dimensions define the coordinate space.
        float proj[16];
        {
            const float w = static_cast<float>(viewport_w_);
            const float h = static_cast<float>(viewport_h_);
            glm::mat4 m = glm::ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
            std::memcpy(proj, &m[0][0], sizeof(float) * 16);
        }

        shader_.bind();
        shader_.set_mat4("u_projection", proj);
        shader_.set_int("u_texture", 0);

        batch_.begin();
    }

    void RendererGL::end_ui()
    {
        batch_.end();
        frame_stats_.draw_calls += batch_.draw_calls();
        frame_stats_.quads_drawn += batch_.quads_drawn();
        frame_stats_.triangles_drawn += batch_.triangles_drawn();
    }

    // =============================================================================
    // Viewport
    // =============================================================================

    void RendererGL::set_viewport(int x, int y, int w, int h)
    {
        viewport_x_ = x;
        viewport_y_ = y;
        viewport_w_ = w;
        viewport_h_ = h;
    }

    void RendererGL::set_full_viewport()
    {
        viewport_x_ = 0;
        viewport_y_ = 0;
        viewport_w_ = window_w_;
        viewport_h_ = window_h_;
    }

    // =============================================================================
    // Camera
    // =============================================================================

    void RendererGL::set_camera_2d(const Camera2D &camera)
    {
        camera_ = camera;
    }

    Camera2D RendererGL::get_camera_2d() const
    {
        return camera_;
    }

    // =============================================================================
    // Textures
    // =============================================================================

    TextureHandle RendererGL::load_texture(const std::string &path)
    {
        auto it = texture_cache_.find(path);
        if (it != texture_cache_.end())
            return it->second;

        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (!data)
        {
            log_error("Failed to load texture '%s': %s",
                      path.c_str(), stbi_failure_reason());
            return TextureHandle{};
        }

        GLuint gl_tex = 0;
        glGenTextures(1, &gl_tex);
        glBindTexture(GL_TEXTURE_2D, gl_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);

        TextureHandle handle;
        handle.id = next_texture_id_++;
        textures_[handle.id] = gl_tex;
        texture_cache_[path] = handle;

        log_info("Loaded texture '%s' (%dx%d, %d channels)",
                 path.c_str(), w, h, channels);
        return handle;
    }

    void RendererGL::free_texture(TextureHandle tex)
    {
        auto it = textures_.find(tex.id);
        if (it == textures_.end())
            return;

        // Drop any path that maps to this handle, so a later
        // load_texture(path) reloads instead of returning a stale
        // handle.
        for (auto path_it = texture_cache_.begin(); path_it != texture_cache_.end();)
        {
            if (path_it->second == tex)
                path_it = texture_cache_.erase(path_it);
            else
                ++path_it;
        }

        GLuint gl_tex = it->second;
        glDeleteTextures(1, &gl_tex);
        textures_.erase(it);
    }

    // =============================================================================
    // Draw
    // =============================================================================

    void RendererGL::draw_rect(float x, float y, float w, float h,
                               Color color, int layer)
    {
        (void)layer;
        batch_.submit_quad(white_gl_id_,
                           x, y, w, h,
                           0.0f,
                           0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 1.0f,
                           color);
    }

    void RendererGL::draw_rect_outline(float x, float y, float w, float h,
                                       float thickness, Color color,
                                       int layer)
    {
        draw_rect(x, y, w, thickness, color, layer);
        draw_rect(x, y + h - thickness, w, thickness, color, layer);
        draw_rect(x, y + thickness, thickness, h - 2 * thickness, color, layer);
        draw_rect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, color, layer);
    }

    void RendererGL::draw_circle(float cx, float cy, float radius,
                                 Color color, int layer,
                                 int segments)
    {
        (void)layer;
        if (segments < 3)
            segments = 3;

        const float tau = 6.28318530718f;
        for (int i = 0; i < segments; ++i)
        {
            const float a0 = (static_cast<float>(i) / segments) * tau;
            const float a1 = (static_cast<float>(i + 1) / segments) * tau;
            const float x0 = cx + std::cos(a0) * radius;
            const float y0 = cy + std::sin(a0) * radius;
            const float x1 = cx + std::cos(a1) * radius;
            const float y1 = cy + std::sin(a1) * radius;

            batch_.submit_triangle(white_gl_id_,
                                   cx, cy, 0.5f, 0.5f,
                                   x0, y0, 0.5f, 0.5f,
                                   x1, y1, 0.5f, 0.5f,
                                   color);
        }
    }

    void RendererGL::draw_line(float x1, float y1, float x2, float y2,
                               float thickness, Color color,
                               int layer)
    {
        (void)layer;
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f)
            return;

        const float mx = (x1 + x2) * 0.5f;
        const float my = (y1 + y2) * 0.5f;
        const float angle = std::atan2(dy, dx);

        batch_.submit_quad(white_gl_id_,
                           mx, my, len, thickness,
                           angle,
                           0.5f, 0.5f,
                           0.0f, 0.0f, 1.0f, 1.0f,
                           color);
    }

    void RendererGL::draw_sprite(TextureHandle tex,
                                 float x, float y, float w, float h,
                                 float rotation, Color tint,
                                 float anchor_x, float anchor_y,
                                 int layer)
    {
        (void)layer;
        auto it = textures_.find(tex.id);
        if (it == textures_.end())
        {
            // Unknown handle: substitute the white texture.
            it = textures_.find(white_texture_.id);
            if (it == textures_.end())
                return;
        }
        const GLuint gl_tex = it->second;

        batch_.submit_quad(gl_tex,
                           x, y, w, h,
                           rotation,
                           anchor_x, anchor_y,
                           0.0f, 0.0f, 1.0f, 1.0f,
                           tint);

        frame_stats_.sprites_drawn++;
    }

    // =============================================================================
    // Stats
    // =============================================================================

    RenderStats RendererGL::stats() const
    {
        return frame_stats_;
    }

    void RendererGL::reset_stats()
    {
        frame_stats_ = RenderStats{};
    }

    // =============================================================================
    // Factory
    // =============================================================================

    Renderer *create_renderer_gl()
    {
        return new RendererGL();
    }

} // namespace engine