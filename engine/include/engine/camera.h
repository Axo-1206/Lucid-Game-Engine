#pragma once

#include "engine/render_types.h"
#include "lucid/row_ref.h"

#include <string>

namespace engine {

// A camera is a row in the Camera table. Cameras are NOT entities; they're
// a separate concept, selected by name.
//
// For Step 6, only the 2D orthographic fields are used by the renderer:
//   - pos_x, pos_y: the center of the 2D view
//   - ortho_size:   the half-height of the view in world units
//   - clear_*:      the frame's clear color
//
// The other fields (target, fov, near, far) are stored for future 3D
// camera support.

struct CameraData {
    std::string name;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;

    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;

    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;

    bool  is_orthographic = true;
    float ortho_size = 1.0f;

    Color clear_color = Color::black();
};

} // namespace engine
