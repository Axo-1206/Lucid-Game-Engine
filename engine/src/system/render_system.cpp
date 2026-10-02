#include "engine/systems.h"
#include "engine/component_tags.h"

namespace engine
{

    void render_system(SystemContext &ctx, float dt)
    {
        (void)dt;

        ctx.render_list.clear();

        auto *world_tbl = ctx.runtime.get_table(WorldTransformTag::name);
        auto *sprite_tbl = ctx.runtime.get_table(SpriteTag::name);

        if (world_tbl && sprite_tbl)
        {
            world_tbl->each([&](lucid::RowRef world_ref)
                            {
            lucid::RowRef entity = world_tbl->get_row_ref(world_ref, "entity");
            lucid::RowRef sprite_ref = sprite_tbl->find_by_primary(
                "entity", lucid::CellValue::from_row_ref(entity));
            if (sprite_ref.is_nil()) return;

            const float x   = world_tbl->get_float32(world_ref, "x");
            const float y   = world_tbl->get_float32(world_ref, "y");
            const float rot = world_tbl->get_float32(world_ref, "rot_z");

            const float w = sprite_tbl->get_float32(sprite_ref, "w");
            const float h = sprite_tbl->get_float32(sprite_ref, "h");
            Color tint {
                sprite_tbl->get_float32(sprite_ref, "tint_r"),
                sprite_tbl->get_float32(sprite_ref, "tint_g"),
                sprite_tbl->get_float32(sprite_ref, "tint_b"),
                sprite_tbl->get_float32(sprite_ref, "tint_a"),
            };
            const int layer = sprite_tbl->get_int32(sprite_ref, "layer");

            ctx.render_list.add_rect(x, y, w, h, tint, layer);
            (void)rot; });
        }

        ctx.render_list.sort();
        ctx.render_list.submit(ctx.renderer);
    }

    void physics_sync_system(SystemContext &ctx, float dt)
    {
        (void)ctx;
        (void)dt;
    }

    void collision_system(SystemContext &ctx, float dt)
    {
        (void)ctx;
        (void)dt;
    }

    void script_system(SystemContext &ctx, float dt)
    {
        (void)ctx;
        (void)dt;
    }

} // namespace engine
