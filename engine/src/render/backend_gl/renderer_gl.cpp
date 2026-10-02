#include "render/backend_gl/renderer_gl.h"
#include "engine/log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cmath>
#include <cstring>

namespace engine {

namespace {

const char* kSpriteVertexShader = R"(
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

const char* kSpriteFragmentShader = R"(
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
{}

RendererGL::~RendererGL() {
    shutdown();
}

bool RendererGL::init(void* native_window) {
    window_ = static_cast<GLFWwindow*>(native_window);
    if (window_ == nullptr) {
        log_error("RendererGL: null window");
        return false;
    }

    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        log_error("RendererGL: failed to load OpenGL functions");
        return false;
    }

    log_info("OpenGL version: %s", glGetString(GL_VERSION));
    log_info("GPU renderer:   %s", glGetString(GL_RENDERER));

    // Enable blending for alpha.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable depth testing (harmless for 2D; needed later for 3D).
    // glEnable(GL_DEPTH_TEST);

    // Compile the sprite shader.
    if (!shader_.compile(kSpriteVertexShader, kSpriteFragmentShader)) {
        log_error("RendererGL: failed to compile sprite shader");
        return false;
    }

    // Init the batch.
    batch_.init();
    batch_.set_shader(&shader_);

    // Create the white texture.
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
        textures_[white_texture_.id] = gl_tex;
    }

    initialized_ = true;
    return true;
}

void RendererGL::shutdown() {
    if (!initialized_) return;

    for (auto& [handle, gl_tex] : textures_) {
        GLuint t = gl_tex;
        glDeleteTextures(1, &t);
    }
    textures_.clear();

    batch_.destroy();
    shader_.destroy();

    initialized_ = false;
    window_ = nullptr;
}

void RendererGL::begin_frame(Color clear_color) {
    glViewport(0, 0, viewport_w_, viewport_h_);
    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    reset_stats();

    // Compute and upload the projection matrix.
    float proj[16];
    compute_projection(proj);
    shader_.bind();
    shader_.set_mat4("u_projection", proj);

    batch_.begin();
}

void RendererGL::end_frame() {
    batch_.end();

    // Accumulate the batch's stats into the frame stats.
    frame_stats_.draw_calls      += batch_.draw_calls();
    frame_stats_.quads_drawn     += batch_.quads_drawn();
    frame_stats_.triangles_drawn += batch_.triangles_drawn();
}

void RendererGL::present() {
    if (window_) glfwSwapBuffers(window_);
}

void RendererGL::set_camera_2d(const Camera2D& camera) {
    camera_ = camera;
    // Sync viewport to camera's viewport if the caller set it.
    if (camera_.viewport_w > 0.0f) viewport_w_ = static_cast<int>(camera_.viewport_w);
    if (camera_.viewport_h > 0.0f) viewport_h_ = static_cast<int>(camera_.viewport_h);
}

Camera2D RendererGL::get_camera_2d() const {
    return camera_;
}

void RendererGL::set_viewport(int width, int height) {
    viewport_w_ = width;
    viewport_h_ = height;
    camera_.viewport_w = static_cast<float>(width);
    camera_.viewport_h = static_cast<float>(height);
}

void RendererGL::compute_projection(float* out_mat4) const {
    // Orthographic projection in screen space:
    //   (0, 0) is top-left, +y down.
    //
    // The camera's (x, y) is the center of the view.
    // zoom scales the view.
    const float half_w = camera_.viewport_w * 0.5f / camera_.zoom;
    const float half_h = camera_.viewport_h * 0.5f / camera_.zoom;

    const float left   = camera_.x - half_w;
    const float right  = camera_.x + half_w;
    const float bottom = camera_.y + half_h;  // +y down: bottom > top
    const float top    = camera_.y - half_h;

    glm::mat4 proj = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    std::memcpy(out_mat4, &proj[0][0], sizeof(float) * 16);
}

// -----------------------------------------------------------------------------
// Textures
// -----------------------------------------------------------------------------

TextureHandle RendererGL::load_texture(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        log_error("Failed to load texture '%s': %s", path.c_str(), stbi_failure_reason());
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

    log_info("Loaded texture '%s' (%dx%d, %d channels)", path.c_str(), w, h, channels);
    return handle;
}

void RendererGL::free_texture(TextureHandle tex) {
    auto it = textures_.find(tex.id);
    if (it == textures_.end()) return;

    GLuint gl_tex = it->second;
    glDeleteTextures(1, &gl_tex);
    textures_.erase(it);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void RendererGL::draw_rect(float x, float y, float w, float h,
                           Color color, int layer)
{
    (void)layer;
    GLuint white = textures_[white_texture_.id];

    // For draw_rect, (x, y) is the top-left corner. Use anchor (0, 0).
    batch_.submit_quad(white,
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
    // Four thin rectangles.
    draw_rect(x, y, w, thickness, color, layer);                     // top
    draw_rect(x, y + h - thickness, w, thickness, color, layer);     // bottom
    draw_rect(x, y + thickness, thickness, h - 2 * thickness, color, layer);  // left
    draw_rect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, color, layer);  // right
}

void RendererGL::draw_circle(float cx, float cy, float radius,
                             Color color, int layer,
                             int segments)
{
    (void)layer;
    if (segments < 3) segments = 3;
    GLuint white = textures_[white_texture_.id];

    // Center vertex at (cx, cy). Triangle fan around it.
    // UVs are irrelevant (white texture); use (0.5, 0.5).
    const float tau = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        float a0 = (static_cast<float>(i)     / segments) * tau;
        float a1 = (static_cast<float>(i + 1) / segments) * tau;
        float x0 = cx + std::cos(a0) * radius;
        float y0 = cy + std::sin(a0) * radius;
        float x1 = cx + std::cos(a1) * radius;
        float y1 = cy + std::sin(a1) * radius;

        batch_.submit_triangle(white,
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
    if (len < 1e-6f) return;

    // Midpoint and angle.
    const float mx = (x1 + x2) * 0.5f;
    const float my = (y1 + y2) * 0.5f;
    const float angle = std::atan2(dy, dx);

    // A quad of size (len, thickness), anchored at center, rotated.
    GLuint white = textures_[white_texture_.id];
    batch_.submit_quad(white,
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
    if (it == textures_.end()) {
        // Unknown handle: use white.
        it = textures_.find(white_texture_.id);
        if (it == textures_.end()) return;
    }
    GLuint gl_tex = it->second;

    batch_.submit_quad(gl_tex,
                       x, y, w, h,
                       rotation,
                       anchor_x, anchor_y,
                       0.0f, 0.0f, 1.0f, 1.0f,
                       tint);

    frame_stats_.sprites_drawn++;
}

// -----------------------------------------------------------------------------
// Stats
// -----------------------------------------------------------------------------

RenderStats RendererGL::stats() const {
    return frame_stats_;
}

void RendererGL::reset_stats() {
    frame_stats_ = RenderStats{};
}

// -----------------------------------------------------------------------------
// Factory
// -----------------------------------------------------------------------------

Renderer* create_renderer_gl() {
    return new RendererGL();
}

} // namespace engine
