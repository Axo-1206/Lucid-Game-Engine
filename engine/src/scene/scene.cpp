#include "engine/scene.h"
#include "engine/log.h"

namespace engine {

Scene::Scene(lucid::Runtime& rt)
    : runtime_(rt)
    , active_camera_()
{}

void Scene::clear() {
    // Remove all entity rows. This triggers destroy_entity, which cascades
    // to component rows.
    auto* entity_tbl = runtime_.get_table(EntityTag::name);
    if (entity_tbl) {
        // Collect all entity refs first, then destroy them.
        // (We can't iterate and remove in the same pass safely.)
        std::vector<lucid::RowRef> refs;
        entity_tbl->each([&](lucid::RowRef r) { refs.push_back(r); });
        for (auto r : refs) {
            destroy_entity(runtime_, r);
        }
    }

    // Clear the Camera table.
    auto* camera_tbl = runtime_.get_table(CameraTag::name);
    if (camera_tbl) camera_tbl->clear();

    active_camera_ = lucid::RowRef{};
    name_.clear();
}

void Scene::set_active_camera_by_name(const std::string& name) {
    active_camera_ = find_camera(name);
}

lucid::RowRef Scene::find_camera(const std::string& name) const {
    const auto* tbl = runtime_.get_table(CameraTag::name);
    if (!tbl) return lucid::RowRef{};
    return tbl->find_by_primary("name", lucid::CellValue::from_string(name));
}

CameraData Scene::get_active_camera_data() const {
    CameraData cam;  // defaults

    const auto* tbl = runtime_.get_table(CameraTag::name);
    if (!tbl || active_camera_.is_nil() || !tbl->is_valid(active_camera_)) {
        return cam;
    }

    cam.name             = tbl->get_string(active_camera_, "name");
    cam.pos_x            = tbl->get_float32(active_camera_, "pos_x");
    cam.pos_y            = tbl->get_float32(active_camera_, "pos_y");
    cam.pos_z            = tbl->get_float32(active_camera_, "pos_z");
    cam.target_x         = tbl->get_float32(active_camera_, "target_x");
    cam.target_y         = tbl->get_float32(active_camera_, "target_y");
    cam.target_z         = tbl->get_float32(active_camera_, "target_z");
    cam.fov              = tbl->get_float32(active_camera_, "fov");
    cam.near_plane       = tbl->get_float32(active_camera_, "near");
    cam.far_plane        = tbl->get_float32(active_camera_, "far");
    cam.is_orthographic  = tbl->get_bool(active_camera_, "is_orthographic");
    cam.ortho_size       = tbl->get_float32(active_camera_, "ortho_size");
    cam.clear_color.r    = tbl->get_float32(active_camera_, "clear_r");
    cam.clear_color.g    = tbl->get_float32(active_camera_, "clear_g");
    cam.clear_color.b    = tbl->get_float32(active_camera_, "clear_b");
    cam.clear_color.a    = tbl->get_float32(active_camera_, "clear_a");

    return cam;
}

} // namespace engine
