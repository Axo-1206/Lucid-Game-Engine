#pragma once

#include "engine/render/camera2d.h"
#include "engine/render/render_types.h"
#include "engine/render/texture.h"

#include <string>

namespace engine
{

    // The abstract renderer interface.
    //
    // ─── The three-pass model ──────────────────────────────────────────────
    //
    // A frame has passes. A pass is a scope of draws that share a camera
    // and a viewport. Today there are two passes: world 2D and UI. A
    // world 3D pass will be added when 3D exists.
    //
    // The renderer itself is stateful: it owns GPU resources, a batch, a
    // current viewport, and a current pass. The goal is that its state
    // is *scoped*: a caller enters a pass, draws, and leaves the pass,
    // and the state between passes is what it was before.
    //
    // ─── World 2D pass ─────────────────────────────────────────────────────
    //
    // Draws the game's 2D world content. Uses the 2D world camera set
    // via set_camera_2d(). The projection is orthographic, centered on
    // the camera's position, scaled by the camera's zoom.
    //
    //   renderer.begin_world_2d();
    //   render_system(ctx, dt);   // fills a RenderList and submits
    //   renderer.end_world_2d();
    //
    // ─── UI pass ───────────────────────────────────────────────────────────
    //
    // Draws screen-space overlays: HUD elements, menus, the editor's
    // panels, minimaps. Uses a fixed orthographic projection from (0, 0)
    // to (viewport_width, viewport_height). The camera is not settable;
    // the pass is always screen-space.
    //
    //   renderer.begin_ui();
    //   ui_system(ctx, dt);
    //   renderer.end_ui();
    //
    // ─── Viewport ──────────────────────────────────────────────────────────
    //
    // The viewport is a rectangle in window pixels: (x, y, w, h), origin
    // top-left, +y down. The default is the full window. A caller sets
    // the viewport before entering a pass; the pass renders into it.
    // Setting the viewport affects subsequent passes, not the current one.
    //
    // ─── Layer semantics ───────────────────────────────────────────────────
    //
    // The immediate-mode draw_* functions draw in submission order. The
    // `layer` parameter is stored on the draw but does not change the
    // order in which the renderer issues draws; the backend batches in
    // submission order.
    //
    // To draw in layer order, use a RenderList:
    //
    //   render_list.add_sprite(tex, x, y, w, h, rot, tint, ax, ay, layer);
    //   render_list.add_rect(x, y, w, h, color, layer);
    //   render_list.sort();
    //   render_list.submit(renderer);
    //
    // RenderList::sort() orders by layer first, then by type, then by
    // texture. This is the canonical way to draw a frame.
    class Renderer
    {
    public:
        virtual ~Renderer() = default;

        // --- Lifecycle ---

        // Initialize with a native window handle (GLFWwindow* for the GL
        // backend). Returns false on failure.
        virtual bool init(void *native_window) = 0;
        virtual void shutdown() = 0;

        // --- Frame ---

        // Clears the screen with the given color and begins the frame.
        // Any pending draws are flushed first.
        //
        // begin_frame does not begin a pass. A caller who wants to draw
        // enters a pass after begin_frame.
        virtual void begin_frame(Color clear_color) = 0;

        // Flushes any pending batches and ends the frame. Called after
        // all passes have ended.
        virtual void end_frame() = 0;

        // Swaps front and back buffers. Called after end_frame().
        virtual void present() = 0;

        // --- Passes ---

        // World 2D pass. Uses the camera set via set_camera_2d().
        // Draws between begin_world_2d() and end_world_2d() land in
        // this pass.
        virtual void begin_world_2d() = 0;
        virtual void end_world_2d() = 0;

        // UI pass. Uses a screen-space orthographic projection from
        // (0, 0) to (viewport_width, viewport_height). The camera is
        // not settable.
        virtual void begin_ui() = 0;
        virtual void end_ui() = 0;

        // --- Viewport ---

        // Set the viewport rectangle. (x, y) is top-left in window
        // pixels; (w, h) is the size in pixels.
        //
        // Affects subsequent passes. Does not affect a pass that is
        // currently in progress.
        virtual void set_viewport(int x, int y, int w, int h) = 0;

        // Reset the viewport to the full window.
        // Equivalent to set_viewport(0, 0, window_w, window_h).
        virtual void set_full_viewport() = 0;

        virtual int viewport_x() const = 0;
        virtual int viewport_y() const = 0;
        virtual int viewport_width() const = 0;
        virtual int viewport_height() const = 0;

        // --- Camera ---

        // Set the camera used by the world 2D pass.
        virtual void set_camera_2d(const Camera2D &camera) = 0;
        virtual Camera2D get_camera_2d() const = 0;

        // --- Textures ---

        // Loads a PNG/JPG/TGA/BMP from disk. Returns an invalid handle on
        // failure. The renderer owns the texture.
        virtual TextureHandle load_texture(const std::string &path) = 0;
        virtual void free_texture(TextureHandle tex) = 0;

        // A 1x1 white texture, created at init and always available. Used
        // as the default texture for untextured draws.
        //
        // If a draw is issued with an unknown or invalid texture handle,
        // the renderer substitutes the white texture. The draw still
        // happens.
        virtual TextureHandle white_texture() const = 0;

        // --- Draw (immediate mode) ---

        // Filled axis-aligned rectangle. (x, y) is the top-left corner.
        // Coordinates are in the current pass's space: world space in the
        // world 2D pass, screen pixels in the UI pass.
        virtual void draw_rect(float x, float y, float w, float h,
                               Color color, int layer = 0) = 0;

        // Rectangle outline. Thickness is in the current pass's units.
        // Submits four rectangles; there is no single outline primitive.
        virtual void draw_rect_outline(float x, float y, float w, float h,
                                       float thickness, Color color,
                                       int layer = 0) = 0;

        // Filled circle. (cx, cy) is the center. Segments controls the
        // tessellation (higher = smoother, more triangles).
        virtual void draw_circle(float cx, float cy, float radius,
                                 Color color, int layer = 0,
                                 int segments = 32) = 0;

        // A line segment from (x1, y1) to (x2, y2). Thickness is in the
        // current pass's units.
        virtual void draw_line(float x1, float y1, float x2, float y2,
                               float thickness, Color color,
                               int layer = 0) = 0;

        // A textured quad. (x, y) is where the anchor lands. The quad
        // extends from the anchor. Rotation is in radians around the
        // anchor point.
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
        virtual void reset_stats() = 0;
    };

    // Factory: creates a new OpenGL renderer. The caller owns the pointer
    // and must delete it (or call shutdown() and delete).
    Renderer *create_renderer_gl();

} // namespace engine