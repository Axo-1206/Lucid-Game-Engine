#pragma once

#include "engine/render/renderer.h"
#include "engine/render/backend_gl/shader_gl.h"
#include "engine/render/backend_gl/sprite_batch_gl.h"

#include <cstdint>
#include <string>
#include <unordered_map>

struct GLFWwindow;

namespace engine
{

    class RendererGL : public Renderer
    {
    public:
        RendererGL();
        ~RendererGL() override;

        bool init(void *native_window) override;
        void shutdown() override;

        void begin_frame(Color clear_color) override;
        void end_frame() override;
        void present() override;

        void begin_world_2d() override;
        void end_world_2d() override;
        void begin_ui() override;
        void end_ui() override;

        void set_viewport(int x, int y, int w, int h) override;
        void set_full_viewport() override;

        int viewport_x() const override { return viewport_x_; }
        int viewport_y() const override { return viewport_y_; }
        int viewport_width() const override { return viewport_w_; }
        int viewport_height() const override { return viewport_h_; }

        void set_camera_2d(const Camera2D &camera) override;
        Camera2D get_camera_2d() const override;

        TextureHandle load_texture(const std::string &path) override;
        void free_texture(TextureHandle tex) override;
        TextureHandle white_texture() const override { return white_texture_; }

        void draw_rect(float x, float y, float w, float h,
                       Color color, int layer = 0) override;

        void draw_rect_outline(float x, float y, float w, float h,
                               float thickness, Color color,
                               int layer = 0) override;

        void draw_circle(float cx, float cy, float radius,
                         Color color, int layer = 0,
                         int segments = 32) override;

        void draw_line(float x1, float y1, float x2, float y2,
                       float thickness, Color color,
                       int layer = 0) override;

        void draw_sprite(TextureHandle tex,
                         float x, float y, float w, float h,
                         float rotation, Color tint,
                         float anchor_x = 0.5f,
                         float anchor_y = 0.5f,
                         int layer = 0) override;

        RenderStats stats() const override;
        void reset_stats() override;

    private:
        GLFWwindow *window_ = nullptr;

        gl::ShaderGL shader_;
        gl::SpriteBatchGL batch_;

        Camera2D camera_;

        // The current viewport rectangle, in window pixels.
        int viewport_x_ = 0;
        int viewport_y_ = 0;
        int viewport_w_ = 1280;
        int viewport_h_ = 720;

        // The window size, tracked so set_full_viewport() can restore it.
        int window_w_ = 1280;
        int window_h_ = 720;

        TextureHandle white_texture_;
        // Cached GL id of the white texture, so draw_rect and friends
        // don't look it up in textures_ on every call.
        std::uint32_t white_gl_id_ = 0;

        // Handle -> GL texture ID
        std::unordered_map<std::uint32_t, std::uint32_t> textures_;
        std::uint32_t next_texture_id_ = 1;

        // Path -> handle. The reverse of textures_, so load_texture
        // can return the existing handle for a path already loaded.
        std::unordered_map<std::string, TextureHandle> texture_cache_;

        // Stats accumulated across all batches in the current frame.
        RenderStats frame_stats_;

        bool initialized_ = false;

        // Compute the 2D orthographic projection for the world pass from
        // camera_ and the current viewport.
        void compute_world_projection(float *out_mat4) const;

        // Compute the screen-space projection for the UI pass:
        // orthographic from (0, 0) to (viewport_w, viewport_h), y down.
        void compute_ui_projection(float *out_mat4) const;

        // Bind the current viewport rectangle and set the projection to
        // the given matrix.
        void apply_projection(const float *mat4);
    };

} // namespace engine