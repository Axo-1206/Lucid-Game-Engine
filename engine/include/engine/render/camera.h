#pragma once

#include "engine/ecs/world.h"
#include "engine/ecs/core/component_tags.h"

#include <string>

namespace engine
{

    // A camera description. The schema type: what is serialized, what
    // a Scene carries, what a camera table row mirrors. It is not the
    // renderer's view; that is Camera2D. See docs/Decision.md,
    // Decision 3 and Decision 4.
    //
    // The three camera types and their roles:
    //   - CameraDesc  — the schema. Serialized. Scene carries it.
    //   - Camera row  — runtime state. Columns mirror CameraDesc.
    //   - Camera2D    — the renderer's view. Produced by to_camera2d.
    struct CameraDesc
    {
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

        bool is_orthographic = true;
        float ortho_size = 1.0f;

        float clear_r = 0.0f;
        float clear_g = 0.0f;
        float clear_b = 0.0f;
        float clear_a = 1.0f;
    };

    // The renderer's 2D view. Populated from a CameraDesc by
    // to_camera2d, or from a camera row by read_camera_row + to_camera2d.
    struct Camera2D
    {
        float x = 0.0f;
        float y = 0.0f;
        float zoom = 1.0f;
        float viewport_w = 1280.0f;
        float viewport_h = 720.0f;
    };

    // Find the Engine_Camera row whose `name` column equals `name`.
    // Returns a nil RowRef if the table does not exist or the name is
    // not found. Safe to call on a world with no camera table.
    engine::ecs::RowRef find_camera(const engine::ecs::World &world,
                                    const std::string &name);

    // Read a camera row into a CameraDesc. The row must be valid for
    // the Engine_Camera table; a nil or stale ref returns a
    // default-constructed CameraDesc. Safe to call on a world with no
    // camera table.
    CameraDesc read_camera_row(const engine::ecs::World &world,
                               engine::ecs::RowRef ref);

    // Write a CameraDesc into the Engine_Camera table. If a row with
    // the same `name` exists, it is overwritten. Otherwise a new row
    // is created. Returns the row's ref, or a nil ref if the table
    // does not exist.
    engine::ecs::RowRef write_camera_row(engine::ecs::World &world,
                                         const CameraDesc &desc);

    // Convert a CameraDesc to a Camera2D for a viewport of
    // (viewport_w, viewport_h) pixels.
    Camera2D to_camera2d(const CameraDesc &desc,
                         int viewport_w,
                         int viewport_h);

} // namespace engine