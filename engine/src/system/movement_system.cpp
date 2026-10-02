#include "engine/systems.h"
#include "engine/component_tags.h"

namespace engine
{

    void movement_system(SystemContext &ctx, float dt)
    {
        auto *vel_tbl = ctx.runtime.get_table(VelocityTag::name);
        auto *trans_tbl = ctx.runtime.get_table(TransformTag::name);
        if (!vel_tbl || !trans_tbl)
            return;

        vel_tbl->each([&](lucid::RowRef vel_ref)
                      {
        lucid::RowRef entity = vel_tbl->get_row_ref(vel_ref, "entity");
        lucid::RowRef trans_ref = trans_tbl->find_by_primary(
            "entity", lucid::CellValue::from_row_ref(entity));
        if (trans_ref.is_nil()) return;

        const float dx = vel_tbl->get_float32(vel_ref, "dx");
        const float dy = vel_tbl->get_float32(vel_ref, "dy");
        const float dz = vel_tbl->get_float32(vel_ref, "dz");

        if (dx == 0.0f && dy == 0.0f && dz == 0.0f) return;

        const float x = trans_tbl->get_float32(trans_ref, "x");
        const float y = trans_tbl->get_float32(trans_ref, "y");
        const float z = trans_tbl->get_float32(trans_ref, "z");

        trans_tbl->set_float32(trans_ref, "x", x + dx * dt);
        trans_tbl->set_float32(trans_ref, "y", y + dy * dt);
        trans_tbl->set_float32(trans_ref, "z", z + dz * dt); });
    }

} // namespace engine
