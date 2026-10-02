#pragma once

namespace engine {

// A 2D orthographic camera.
//
// (x, y) is the point in world space at the center of the view.
// zoom = 1.0 means 1 world unit == 1 pixel. zoom = 2.0 doubles everything.
//
// viewport_w and viewport_h are in pixels. The renderer syncs them to the
// actual window size each frame, or the caller can set them explicitly.
struct Camera2D {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
    float viewport_w = 1280.0f;
    float viewport_h = 720.0f;
};

} // namespace engine
