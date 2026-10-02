#pragma once

#include "engine/camera2d.h"
#include "engine/render_types.h"
#include "engine/texture.h"

#include <string>

namespace engine {

// The abstract renderer interface.
//
// The API is immediate-mode: draw_* functions submit draws that are
// rendered during the current begin_frame() / end_frame() window. The
// backend batches internally, so drawing 1000 sprites with the same
// texture produces one or a few draw calls, not 1000.
//
// Coordinates are in screen space: (0, 0) is the top-left of the viewport,
// +x is right, +y is down.
class Renderer {
public:
    virtual ~Renderer() = default;

    // --- Lifecycle ---

    // Initialize with a native window handle (GLFWwindow* for the GL
    // backend). Returns false on failure.
    virtual bool init(void* native_window) = 0;
    virtual void shutdown() = 0;

    // --- Frame ---

    // Clears the screen with the given color and begins the frame.
    // Any pending draws are flushed first.
    virtual void begin_frame(Color clear_color) = 0;

    // Flushes any pending batches.
    virtual void end_frame() = 0;

    // Swaps front and back buffers. Called after end_frame().
    virtual void present() = 0;

    // --- Camera ---

    virtual void     set_camera_2d(const Camera2D& camera) = 0;
    virtual Camera2D get_camera_2d() const = 0;

    // --- Viewport ---

    virtual void set_viewport(int width, int height) = 0;
    virtual int  viewport_width()  const = 0;
    virtual int  viewport_height() const = 0;

    // --- Textures ---

    // Loads a PNG/JPG/TGA/BMP from disk. Returns an invalid handle on
    // failure. The renderer owns the texture.
    virtual TextureHandle load_texture(const std::string& path) = 0;
    virtual void          free_texture(TextureHandle tex) = 0;

    // A 1x1 white texture, created at init and always available. Used as
    // the default texture for untextured draws.
    virtual TextureHandle white_texture() const = 0;

    // --- Draw (immediate mode) ---

    // Filled axis-aligned rectangle. (x, y) is the top-left corner.
    virtual void draw_rect(float x, float y, float w, float h,
                           Color color, int layer = 0) = 0;

    // Rectangle outline. Thickness is in screen pixels.
    virtual void draw_rect_outline(float x, float y, float w, float h,
                                   float thickness, Color color,
                                   int layer = 0) = 0;

    // Filled circle. (cx, cy) is the center. Segments controls the
    // tessellation (higher = smoother, more triangles).
    virtual void draw_circle(float cx, float cy, float radius,
                             Color color, int layer = 0,
                             int segments = 32) = 0;

    // A line segment from (x1, y1) to (x2, y2). Thickness is in screen
    // pixels.
    virtual void draw_line(float x1, float y1, float x2, float y2,
                           float thickness, Color color,
                           int layer = 0) = 0;

    // A textured quad. (x, y) is where the anchor lands in screen space.
    // The quad extends from the anchor. Rotation is in radians around
    // the anchor point.
    //
    // Default anchor (0.5, 0.5) = center of the quad.
    // Anchor (0, 0)   = top-left; (x, y) is the quad's top-left corner.
    // Anchor (0.5, 1) = bottom-center.
    virtual void draw_sprite(TextureHandle tex,
                             float x, float y, float w, float h,
                             float rotation, Color tint,
                             float anchor_x = 0.5f,
                             float anchor_y = 0.5f,
                             int layer = 0) = 0;

    // --- Statistics ---

    virtual RenderStats stats() const = 0;
    virtual void        reset_stats() = 0;
};

// Factory: creates a new OpenGL renderer. The caller owns the pointer
// and must delete it (or call shutdown() and delete).
Renderer* create_renderer_gl();

} // namespace engine
