#include "engine/systems.h"
#include "engine/scene/component_tags.h"

#include <cstddef>

namespace engine
{

    void hierarchy_system(SystemContext &ctx, float dt)
    {
        (void)dt;

        auto *entity_tbl = ctx.world.get_table(EntityTag::name);
        auto *local_tbl = ctx.world.get_table(TransformTag::name);
        auto *world_tbl = ctx.world.get_table(WorldTransformTag::name);
        if (!entity_tbl || !local_tbl || !world_tbl)
            return;

        local_tbl->each([&](engine::ecs::RowRef local_ref)
                        {
        engine::ecs::RowRef entity = local_tbl->get_row_ref(local_ref, "entity");
        if (world_tbl->find_by_primary("entity",
                engine::ecs::CellValue::from_row_ref(entity)).is_nil())
        {
            world_tbl->add_partial("entity", engine::ecs::CellValue::from_row_ref(entity));
        } });

        constexpr std::size_t kMaxPasses = 32;
        for (std::size_t pass = 0; pass < kMaxPasses; ++pass)
        {
            bool changed = false;

            local_tbl->each([&](engine::ecs::RowRef local_ref)
                            {
            engine::ecs::RowRef entity = local_tbl->get_row_ref(local_ref, "entity");
            engine::ecs::RowRef world_ref = world_tbl->find_by_primary(
                "entity", engine::ecs::CellValue::from_row_ref(entity));
            if (world_ref.is_nil()) return;

            const float lx     = local_tbl->get_float32(local_ref, "x");
            const float ly     = local_tbl->get_float32(local_ref, "y");
            const float lz     = local_tbl->get_float32(local_ref, "z");
            const float lrot_z = local_tbl->get_float32(local_ref, "rot_z");

            const engine::ecs::RowRef parent = entity_tbl->get_row_ref(entity, "parent");

            float wx, wy, wz, wrot_z;
            if (parent.is_nil()) {
                wx = lx; wy = ly; wz = lz; wrot_z = lrot_z;
            } else {
                const engine::ecs::RowRef pworld = world_tbl->find_by_primary(
                    "entity", engine::ecs::CellValue::from_row_ref(parent));
                if (pworld.is_nil()) {
                    return;
                }
                wx = world_tbl->get_float32(pworld, "x") + lx;
                wy = world_tbl->get_float32(pworld, "y") + ly;
                wz = world_tbl->get_float32(pworld, "z") + lz;
                wrot_z = world_tbl->get_float32(pworld, "rot_z") + lrot_z;
            }

            if (world_tbl->get_float32(world_ref, "x") != wx ||
                world_tbl->get_float32(world_ref, "y") != wy ||
                world_tbl->get_float32(world_ref, "z") != wz ||
                world_tbl->get_float32(world_ref, "rot_z") != wrot_z)
            {
                world_tbl->set_float32(world_ref, "x", wx);
                world_tbl->set_float32(world_ref, "y", wy);
                world_tbl->set_float32(world_ref, "z", wz);
                world_tbl->set_float32(world_ref, "rot_z", wrot_z);
                changed = true;
            } });

            if (!changed)
                break;
        }
    }

} // namespace engine
