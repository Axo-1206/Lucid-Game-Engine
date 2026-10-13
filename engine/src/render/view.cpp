#include "engine/render/view.h"
#include "engine/render/renderer.h"
#include "engine/systems/system_context.h"
#include "engine/systems/systems.h"

namespace engine
{

    View make_world_2d_view(engine::ecs::World &world,
                            const std::string &camera_name,
                            int x, int y, int w, int h)
    {
        View v;
        v.world = &world;
        v.camera_name = camera_name;
        v.x = x;
        v.y = y;
        v.w = w;
        v.h = h;
        v.pass = View::Pass::World2D;

        const auto ref = find_camera(world, camera_name);
        if (!ref.is_nil())
        {
            v.camera = read_camera_row(world, ref);
        }
        // If the lookup fails, the camera stays at CameraDesc's
        // defaults. The caller can check camera.name to see.
        return v;
    }

    View make_ui_view(engine::ecs::World *world,
                      int x, int y, int w, int h)
    {
        View v;
        v.world = world;
        v.x = x;
        v.y = y;
        v.w = w;
        v.h = h;
        v.pass = View::Pass::UI;
        return v;
    }

    void render_view(const View &view,
                     engine::SystemContext &ctx,
                     float dt,
                     engine::Renderer &renderer)
    {
        renderer.set_viewport(view.x, view.y, view.w, view.h);

        switch (view.pass)
        {
        case View::Pass::World2D:
            renderer.set_camera_2d(to_camera2d(view.camera, view.w, view.h));
            renderer.begin_world_2d();
            engine::render_system(ctx, dt);
            renderer.end_world_2d();
            break;

        case View::Pass::UI:
            renderer.begin_ui();
            // V2: ui_system(ctx, dt);
            renderer.end_ui();
            break;
        }

        renderer.set_full_viewport();
    }

} // namespace engine