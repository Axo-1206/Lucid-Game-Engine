// engine/include/engine/render/view.h
#pragma once

#include "engine/ecs/world.h"
#include "engine/render/camera.h"
#include "engine/render/render_list.h"

#include <string>
#include <vector>

namespace engine
{
    struct SystemContext;

    // A view: a scope of draws that share a world, a camera, a
    // viewport, and a pass.
    //
    // The engine loop iterates a list of views. For each:
    //   - set_viewport(view.x, view.y, view.w, view.h)
    //   - if the view is World2D:
    //       set_camera_2d(to_camera2d(view.camera, view.w, view.h))
    //       begin_world_2d()
    //       <draw the view's world>
    //       end_world_2d()
    //   - if the view is UI:
    //       begin_ui()
    //       <draw the view's UI>
    //       end_ui()
    //
    // A view does not own its world. A view does not own its camera
    // row; it holds a CameraDesc (a copy). The camera name is
    // canonical; the desc is what the loop last read.
    //
    // A view with `world == nullptr` is legal. A UI view may draw
    // from engine state rather than from a world.
    struct View
    {
        // What kind of pass this view draws. World2D uses the
        // camera; UI is screen-space and ignores the camera.
        enum class Pass
        {
            World2D,
            UI,
        };

        // The world to draw. May be null.
        engine::ecs::World *world = nullptr;

        // The camera. For World2D, the name is looked up in the
        // world's Engine_Camera table when the view is built; the
        // camera field is a copy of the row. For UI, both are
        // ignored.
        std::string camera_name;
        CameraDesc camera;

        // The viewport rectangle, in window pixels. Origin is
        // top-left, +y down. The default is the full window; the
        // caller overrides x/y/w/h as needed.
        int x = 0;
        int y = 0;
        int w = 1280;
        int h = 720;

        // Which pass this view draws.
        Pass pass = Pass::World2D;

        // A draw order hint for the loop. Views with lower `order`
        // run first. Used to draw a minimap under a HUD, or the
        // world under the UI, without relying on the vector's order.
        // The default is 0.
        int order = 0;

        // A name, for the editor and for logs. Not used by the loop.
        std::string name;

        // If true, the view's w/h follow the window size on resize.
        // If false, the view keeps its rectangle.
        bool resize_with_window = true;
    };

    // Build a World2D view from a world and a camera name.
    //
    // Looks up the camera in the world's Engine_Camera table. If
    // the name is empty or not found, the view's camera stays at
    // CameraDesc's defaults and `camera_name` is set to the name
    // that was requested. A caller can check `camera.name` to see
    // whether the lookup succeeded.
    View make_world_2d_view(engine::ecs::World &world,
                            const std::string &camera_name,
                            int x, int y, int w, int h);

    // Build a UI view. The world is not used; pass the world if the
    // UI reads it (some overlays do), or nullptr if it does not.
    View make_ui_view(engine::ecs::World *world,
                      int x, int y, int w, int h);

    // Render one view. Sets the viewport, sets the camera (for
    // World2D), enters the pass, calls the systems that draw into
    // the pass, leaves the pass, and restores the full viewport.
    void render_view(const View &view,
                     engine::SystemContext &ctx,
                     float dt,
                     engine::Renderer &renderer);

} // namespace engine