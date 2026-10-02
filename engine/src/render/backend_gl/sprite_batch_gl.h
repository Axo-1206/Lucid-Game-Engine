#pragma once

#include "engine/render_types.h"
#include "engine/texture.h"

#include <glad/glad.h>

#include <cstdint>
#include <vector>

namespace engine::gl {

class ShaderGL;

// A dynamic vertex buffer that batches textured quads.
//
// submit_quad() appends 6 vertices. If the batch is full or the texture
// changes, the batch is flushed and restarted. end() flushes any
// remaining vertices.
//
// The batch draws with a single shader and a single texture per flush.
class SpriteBatchGL {
public:
    explicit SpriteBatchGL(std::size_t max_quads = 8192);
    ~SpriteBatchGL();

    SpriteBatchGL(const SpriteBatchGL&) = delete;
    SpriteBatchGL& operator=(const SpriteBatchGL&) = delete;

    // Initialize GL resources. Must be called after the GL context exists.
    void init();

    // Free GL resources.
    void destroy();

    // Set the shader used for drawing. The shader must be valid.
    void set_shader(ShaderGL* shader);

    // Begin a batch. Resets counters.
    void begin();

    // Submit a textured quad.
    //   (x, y) is where the anchor lands in screen space.
    //   anchor_x / anchor_y are in [0, 1].
    //   rotation is in radians around the anchor.
    //   layer is currently ignored; sorting is the caller's job.
    void submit_quad(GLuint texture,
                     float x, float y, float w, float h,
                     float rotation,
                     float anchor_x, float anchor_y,
                     float u0, float v0, float u1, float v1,
                     Color color);

    // Submit a triangle (used by draw_circle).
    void submit_triangle(GLuint texture,
                         float x0, float y0, float u0, float v0,
                         float x1, float y1, float u1, float v1,
                         float x2, float y2, float u2, float v2,
                         Color color);

    // Flush any pending vertices and reset counters.
    void end();

    // Statistics for the current frame.
    std::uint32_t draw_calls()      const { return draw_calls_; }
    std::uint32_t quads_drawn()     const { return quads_drawn_; }
    std::uint32_t triangles_drawn() const { return triangles_drawn_; }

    // Reset per-frame stats. Called by begin().
    void reset_stats();

    std::size_t max_quads() const { return max_quads_; }

private:
    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    void flush();

    std::size_t max_quads_;
    std::size_t max_vertices_;

    std::vector<Vertex> vertices_;
    std::size_t         vertex_count_ = 0;
    GLuint              current_texture_ = 0;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    ShaderGL* shader_ = nullptr;

    std::uint32_t draw_calls_      = 0;
    std::uint32_t quads_drawn_     = 0;
    std::uint32_t triangles_drawn_ = 0;

    bool initialized_ = false;
};

} // namespace engine::gl
