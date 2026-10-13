#include "engine/render/camera.h"
#include "engine/core/log.h"

#include <string>

namespace engine
{

    engine::ecs::RowRef find_camera(const engine::ecs::World &world,
                                    const std::string &name)
    {
        const auto *tbl = world.get_table(kCameraTable);
        if (tbl == nullptr || name.empty())
        {
            return engine::ecs::RowRef{};
        }
        return tbl->find_by_primary(
            "name", engine::ecs::CellValue::from_string(name));
    }

    CameraDesc read_camera_row(const engine::ecs::World &world,
                               engine::ecs::RowRef ref)
    {
        CameraDesc desc;

        const auto *tbl = world.get_table(kCameraTable);
        if (tbl == nullptr || !tbl->is_valid(ref))
        {
            return desc;
        }

        desc.name = tbl->get_string(ref, "name");
        desc.pos_x = tbl->get_float32(ref, "pos_x");
        desc.pos_y = tbl->get_float32(ref, "pos_y");
        desc.pos_z = tbl->get_float32(ref, "pos_z");
        desc.target_x = tbl->get_float32(ref, "target_x");
        desc.target_y = tbl->get_float32(ref, "target_y");
        desc.target_z = tbl->get_float32(ref, "target_z");
        desc.fov = tbl->get_float32(ref, "fov");
        desc.near_plane = tbl->get_float32(ref, "near");
        desc.far_plane = tbl->get_float32(ref, "far");
        desc.is_orthographic = tbl->get_bool(ref, "is_orthographic");
        desc.ortho_size = tbl->get_float32(ref, "ortho_size");
        desc.clear_r = tbl->get_float32(ref, "clear_r");
        desc.clear_g = tbl->get_float32(ref, "clear_g");
        desc.clear_b = tbl->get_float32(ref, "clear_b");
        desc.clear_a = tbl->get_float32(ref, "clear_a");

        return desc;
    }

    engine::ecs::RowRef write_camera_row(engine::ecs::World &world,
                                         const CameraDesc &desc)
    {
        auto *tbl = world.get_table(kCameraTable);
        if (tbl == nullptr)
        {
            return engine::ecs::RowRef{};
        }

        auto ref = tbl->find_by_primary(
            "name", engine::ecs::CellValue::from_string(desc.name));
        if (ref.is_nil())
        {
            ref = tbl->add_partial(
                "name", engine::ecs::CellValue::from_string(desc.name));
        }

        tbl->set_float32(ref, "pos_x", desc.pos_x);
        tbl->set_float32(ref, "pos_y", desc.pos_y);
        tbl->set_float32(ref, "pos_z", desc.pos_z);
        tbl->set_float32(ref, "target_x", desc.target_x);
        tbl->set_float32(ref, "target_y", desc.target_y);
        tbl->set_float32(ref, "target_z", desc.target_z);
        tbl->set_float32(ref, "fov", desc.fov);
        tbl->set_float32(ref, "near", desc.near_plane);
        tbl->set_float32(ref, "far", desc.far_plane);
        tbl->set_bool(ref, "is_orthographic", desc.is_orthographic);
        tbl->set_float32(ref, "ortho_size", desc.ortho_size);
        tbl->set_float32(ref, "clear_r", desc.clear_r);
        tbl->set_float32(ref, "clear_g", desc.clear_g);
        tbl->set_float32(ref, "clear_b", desc.clear_b);
        tbl->set_float32(ref, "clear_a", desc.clear_a);

        return ref;
    }

    Camera2D to_camera2d(const CameraDesc &desc,
                         int viewport_w,
                         int viewport_h)
    {
        Camera2D cam;
        cam.x = desc.pos_x;
        cam.y = desc.pos_y;
        cam.zoom = desc.ortho_size > 0.0f ? 1.0f / desc.ortho_size : 1.0f;
        cam.viewport_w = static_cast<float>(viewport_w);
        cam.viewport_h = static_cast<float>(viewport_h);
        return cam;
    }

} // namespace engine