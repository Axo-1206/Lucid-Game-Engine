#include "engine/systems/systems.h"
#include "engine/core/log.h"
#include "engine/render/render_list.h"
#include "engine/render/view.h"

#include <string>

namespace engine
{

    namespace
    {

        // The anchor point inside a view of (w, h). The nine-grid:
        //
        //   0 top-left      1 top-center      2 top-right
        //   3 middle-left   4 middle-center   5 middle-right
        //   6 bottom-left   7 bottom-center   8 bottom-right
        //
        // An out-of-range anchor is treated as top-left.
        void anchor_origin(int anchor, int vw, int vh, float &ox, float &oy)
        {
            switch (anchor)
            {
            case 0:
                ox = 0.0f;
                oy = 0.0f;
                break;
            case 1:
                ox = vw * 0.5f;
                oy = 0.0f;
                break;
            case 2:
                ox = static_cast<float>(vw);
                oy = 0.0f;
                break;
            case 3:
                ox = 0.0f;
                oy = vh * 0.5f;
                break;
            case 4:
                ox = vw * 0.5f;
                oy = vh * 0.5f;
                break;
            case 5:
                ox = static_cast<float>(vw);
                oy = vh * 0.5f;
                break;
            case 6:
                ox = 0.0f;
                oy = static_cast<float>(vh);
                break;
            case 7:
                ox = vw * 0.5f;
                oy = static_cast<float>(vh);
                break;
            case 8:
                ox = static_cast<float>(vw);
                oy = static_cast<float>(vh);
                break;
            default:
                ox = 0.0f;
                oy = 0.0f;
                break;
            }
        }

    } // namespace

    void ui_system(SystemContext &ctx, float dt)
    {
        (void)dt;

        ctx.render_list.clear();

        auto *ui_tbl = ctx.world.get_table("Engine_UIElement");
        if (!ui_tbl)
        {
            return;
        }

        // The view's size. The UI system needs it to resolve anchors.
        // A caller that runs ui_system outside render_view (a test)
        // sets ctx.current_view before calling; a caller that runs
        // it through render_view gets it set automatically.
        int vw = 0;
        int vh = 0;
        if (ctx.current_view != nullptr)
        {
            vw = ctx.current_view->w;
            vh = ctx.current_view->h;
        }

        ui_tbl->each([&](engine::ecs::RowRef r)
                     {
            if (!ui_tbl->get_bool(r, "visible"))
                return;

            const int   anchor = ui_tbl->get_int32(r, "anchor");
            const float x      = ui_tbl->get_float32(r, "x");
            const float y      = ui_tbl->get_float32(r, "y");
            const float w      = ui_tbl->get_float32(r, "w");
            const float h      = ui_tbl->get_float32(r, "h");
            const int   layer  = ui_tbl->get_int32(r, "layer");
            const std::string texture_path = ui_tbl->get_string(r, "texture_path");
            const Color tint {
                ui_tbl->get_float32(r, "tint_r"),
                ui_tbl->get_float32(r, "tint_g"),
                ui_tbl->get_float32(r, "tint_b"),
                ui_tbl->get_float32(r, "tint_a"),
            };

            float ox = 0.0f;
            float oy = 0.0f;
            anchor_origin(anchor, vw, vh, ox, oy);

            const float px = ox + x;
            const float py = oy + y;

            if (texture_path.empty())
            {
                ctx.render_list.add_rect(px, py, w, h, tint, layer);
            }
            else
            {
                TextureHandle tex = ctx.renderer.load_texture(texture_path);
                ctx.render_list.add_sprite(tex, px, py, w, h,
                                           /*rotation=*/0.0f, tint,
                                           /*anchor_x=*/0.0f,
                                           /*anchor_y=*/0.0f,
                                           layer);
            } });

        ctx.render_list.sort();
        ctx.render_list.submit(ctx.renderer);
    }

} // namespace engine