#include "render/backend_gl/sprite_batch_gl.h"
#include "render/backend_gl/shader_gl.h"
#include "engine/log.h"

#include <cmath>

namespace engine::gl {

SpriteBatchGL::SpriteBatchGL(std::size_t max_quads)
    : max_quads_(max_quads)
    , max_vertices_(max_quads * 6)
{
    vertices_.resize(max_vertices_);
}

SpriteBatchGL::~SpriteBatchGL() {
    destroy();
}

void SpriteBatchGL::init() {
    if (initialized_) return;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // Allocate storage for max_vertices_ vertices.
    glBufferData(GL_ARRAY_BUFFER,
                 max_vertices_ * sizeof(Vertex),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));

    // UV
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, u)));

    // Color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    initialized_ = true;
    log_info("SpriteBatchGL initialized: %zu quads max", max_quads_);
}

void SpriteBatchGL::destroy() {
    if (!initialized_) return;

    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    initialized_ = false;
}

void SpriteBatchGL::set_shader(ShaderGL* shader) {
    shader_ = shader;
}

void SpriteBatchGL::begin() {
    vertex_count_ = 0;
    current_texture_ = 0;
    reset_stats();
}

void SpriteBatchGL::reset_stats() {
    draw_calls_ = 0;
    quads_drawn_ = 0;
    triangles_drawn_ = 0;
}

void SpriteBatchGL::flush() {
    if (vertex_count_ == 0 || current_texture_ == 0) {
        vertex_count_ = 0;
        return;
    }
    if (shader_ == nullptr || !shader_->is_valid()) {
        vertex_count_ = 0;
        return;
    }

    // Upload only the vertices we have.
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    vertex_count_ * sizeof(Vertex),
                    vertices_.data());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_texture_);

    shader_->bind();
    shader_->set_int("u_texture", 0);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex_count_));

    glBindVertexArray(0);

    draw_calls_++;
    vertex_count_ = 0;
}

void SpriteBatchGL::submit_quad(GLuint texture,
                                float x, float y, float w, float h,
                                float rotation,
                                float anchor_x, float anchor_y,
                                float u0, float v0, float u1, float v1,
                                Color color)
{
    // If the texture differs from the current batch, flush first.
    if (current_texture_ != 0 && current_texture_ != texture) {
        flush();
    }
    current_texture_ = texture;

    // If we're at capacity, flush.
    if (vertex_count_ + 6 > max_vertices_) {
        flush();
        current_texture_ = texture;
    }

    // Compute the four corners in local space, relative to the anchor.
    // Local space: (0, 0) is top-left of the quad, (w, h) is bottom-right.
    // The anchor is at (anchor_x * w, anchor_y * h).
    const float ax = anchor_x * w;
    const float ay = anchor_y * h;

    // Corner offsets from the anchor (before rotation).
    const float corners[4][2] = {
        {-ax,         -ay        },  // top-left
        { w - ax,     -ay        },  // top-right
        { w - ax,      h - ay    },  // bottom-right
        {-ax,          h - ay    },  // bottom-left
    };

    // UVs for the four corners.
    const float uvs[4][2] = {
        {u0, v0},
        {u1, v0},
        {u1, v1},
        {u0, v1},
    };

    // Rotation.
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);

    // Compute rotated positions.
    float px[4], py[4];
    for (int i = 0; i < 4; ++i) {
        float lx = corners[i][0];
        float ly = corners[i][1];
        px[i] = x + lx * c - ly * s;
        py[i] = y + lx * s + ly * c;
    }

    // Emit two triangles: (0, 1, 2) and (0, 2, 3).
    auto emit = [&](int i) {
        Vertex& v = vertices_[vertex_count_++];
        v.x = px[i];
        v.y = py[i];
        v.u = uvs[i][0];
        v.v = uvs[i][1];
        v.r = color.r;
        v.g = color.g;
        v.b = color.b;
        v.a = color.a;
    };

    emit(0); emit(1); emit(2);
    emit(0); emit(2); emit(3);

    quads_drawn_++;
}

void SpriteBatchGL::submit_triangle(GLuint texture,
                                    float x0, float y0, float u0, float v0,
                                    float x1, float y1, float u1, float v1,
                                    float x2, float y2, float u2, float v2,
                                    Color color)
{
    if (current_texture_ != 0 && current_texture_ != texture) {
        flush();
    }
    current_texture_ = texture;

    if (vertex_count_ + 3 > max_vertices_) {
        flush();
        current_texture_ = texture;
    }

    auto emit = [&](float px, float py, float pu, float pv) {
        Vertex& v = vertices_[vertex_count_++];
        v.x = px; v.y = py;
        v.u = pu; v.v = pv;
        v.r = color.r; v.g = color.g; v.b = color.b; v.a = color.a;
    };

    emit(x0, y0, u0, v0);
    emit(x1, y1, u1, v1);
    emit(x2, y2, u2, v2);

    triangles_drawn_++;
}

void SpriteBatchGL::end() {
    flush();
}

} // namespace engine::gl
