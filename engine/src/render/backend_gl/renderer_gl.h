#pragma once

#include "engine/renderer.h"
#include "render/backend_gl/shader_gl.h"
#include "render/backend_gl/sprite_batch_gl.h"

#include <cstdint>
#include <string>
#include <unordered_map>

struct GLFWwindow;

namespace engine {

class RendererGL : public Renderer {
public:
    RendererGL();
    ~RendererGL() override;

    bool init(void* native_window) override;
    void shutdown() override;

    void begin_frame(Color clear_color) override;
    void end_frame() override;
    void present() override;

    void     set_camera_2d(const Camera2D& camera) override;
    Camera2D get_camera_2d() const override;

    void set_viewport(int width, int height) override;
    int  viewport_width()  const override { return viewport_w_; }
    int  viewport_height() const override { return viewport_h_; }

    TextureHandle load_texture(const std::string& path) override;
    void          free_texture(TextureHandle tex) override;
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
    void        reset_stats() override;

private:
    GLFWwindow* window_ = nullptr;

    gl::ShaderGL    shader_;
    gl::SpriteBatchGL batch_;

    Camera2D camera_;

    int viewport_w_ = 1280;
    int viewport_h_ = 720;

    TextureHandle white_texture_;

    // Handle -> GL texture ID
    std::unordered_map<std::uint32_t, std::uint32_t> textures_;
    std::uint32_t next_texture_id_ = 1;

    // Stats accumulated across all batches in the current frame.
    RenderStats frame_stats_;

    bool initialized_ = false;

    // Compute the 2D orthographic projection matrix from camera_ and
    // viewport size.
    void compute_projection(float* out_mat4) const;
};

} // namespace engine
