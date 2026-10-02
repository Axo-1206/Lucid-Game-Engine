#include "engine/scene.h"
#include "engine/log.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <vector>

namespace engine {

namespace {

using json = nlohmann::json;

// ---- Component savers ----

json save_transform(const lucid::Table* tbl, lucid::RowRef c) {
    json j;
    j["x"]     = tbl->get_float32(c, "x");
    j["y"]     = tbl->get_float32(c, "y");
    j["z"]     = tbl->get_float32(c, "z");
    j["rot_z"] = tbl->get_float32(c, "rot_z");
    return j;
}

json save_world_transform(const lucid::Table* tbl, lucid::RowRef c) {
    json j;
    j["x"]     = tbl->get_float32(c, "x");
    j["y"]     = tbl->get_float32(c, "y");
    j["z"]     = tbl->get_float32(c, "z");
    j["rot_z"] = tbl->get_float32(c, "rot_z");
    return j;
}

json save_sprite(const lucid::Table* tbl, lucid::RowRef c) {
    json j;
    j["texture_path"] = tbl->get_string(c, "texture_path");
    j["w"]            = tbl->get_float32(c, "w");
    j["h"]            = tbl->get_float32(c, "h");
    j["tint_r"]       = tbl->get_float32(c, "tint_r");
    j["tint_g"]       = tbl->get_float32(c, "tint_g");
    j["tint_b"]       = tbl->get_float32(c, "tint_b");
    j["tint_a"]       = tbl->get_float32(c, "tint_a");
    j["layer"]        = tbl->get_int32(c, "layer");
    return j;
}

json save_script(const lucid::Table* tbl, lucid::RowRef c) {
    json j;
    j["module_path"] = tbl->get_string(c, "module_path");
    return j;
}

json save_component_by_name(lucid::Runtime& rt, EntityRef e,
                            const std::string& table_name)
{
    auto* tbl = rt.get_table(table_name);
    if (!tbl) return json();

    auto comp = tbl->find_by_primary("entity", lucid::CellValue::from_row_ref(e));
    if (comp.is_nil()) return json();

    if (table_name == "Transform")           return save_transform(tbl, comp);
    else if (table_name == "WorldTransform") return save_world_transform(tbl, comp);
    else if (table_name == "Sprite")         return save_sprite(tbl, comp);
    else if (table_name == "Script")         return save_script(tbl, comp);
    else {
        log_warn("No saver for component table: %s", table_name.c_str());
        return json();
    }
}

} // namespace

bool Scene::save_to_file(const std::string& path) const {
    json root;
    root["version"] = 1;
    root["name"]    = name_;

    // --- Cameras ---
    {
        json cameras = json::array();
        auto* cam_tbl = runtime_.get_table(CameraTag::name);
        if (cam_tbl) {
            cam_tbl->each([&](lucid::RowRef r) {
                json cj;
                cj["name"] = cam_tbl->get_string(r, "name");
                cj["pos"]  = { cam_tbl->get_float32(r, "pos_x"),
                               cam_tbl->get_float32(r, "pos_y"),
                               cam_tbl->get_float32(r, "pos_z") };
                cj["target"] = { cam_tbl->get_float32(r, "target_x"),
                                 cam_tbl->get_float32(r, "target_y"),
                                 cam_tbl->get_float32(r, "target_z") };
                cj["fov"]             = cam_tbl->get_float32(r, "fov");
                cj["near"]            = cam_tbl->get_float32(r, "near");
                cj["far"]             = cam_tbl->get_float32(r, "far");
                cj["is_orthographic"] = cam_tbl->get_bool   (r, "is_orthographic");
                cj["ortho_size"]      = cam_tbl->get_float32(r, "ortho_size");
                cj["clear_color"]     = { cam_tbl->get_float32(r, "clear_r"),
                                          cam_tbl->get_float32(r, "clear_g"),
                                          cam_tbl->get_float32(r, "clear_b"),
                                          cam_tbl->get_float32(r, "clear_a") };
                cameras.push_back(cj);
            });
        }
        root["cameras"] = cameras;
    }

    // --- Entities ---
    // Discover the component tables: any table with an `entity` column
    // that is not the `Entity` table itself.
    std::vector<std::string> component_tables;
    runtime_.each_table([&](lucid::Table& t) {
        if (t.has_column("entity")) {
            component_tables.push_back(t.name());
        }
    });

    {
        json entities = json::array();
        auto* entity_tbl = runtime_.get_table(EntityTag::name);
        if (entity_tbl) {
            entity_tbl->each([&](lucid::RowRef r) {
                json ej;
                ej["id"]   = entity_tbl->get_uint64(r, "id");
                ej["name"] = entity_tbl->get_string(r, "name");

                // Parent: emit the parent's id, or 0.
                lucid::RowRef parent = entity_tbl->get_row_ref(r, "parent");
                if (!parent.is_nil() && entity_tbl->is_valid(parent)) {
                    ej["parent"] = entity_tbl->get_uint64(parent, "id");
                } else {
                    ej["parent"] = 0;
                }

                // Components.
                json components = json::object();
                for (const auto& table_name : component_tables) {
                    json cj = save_component_by_name(const_cast<lucid::Runtime&>(runtime_), r, table_name);
                    if (!cj.is_null()) {
                        components[table_name] = cj;
                    }
                }
                ej["components"] = components;

                entities.push_back(ej);
            });
        }
        root["entities"] = entities;
    }

    // --- Engine state ---
    {
        json state = json::object();
        auto* state_tbl = runtime_.get_table("_EngineState");
        if (state_tbl) {
            state_tbl->each([&](lucid::RowRef r) {
                std::string key = state_tbl->get_string(r, "name");
                if (key == "next_entity_id") {
                    state["next_entity_id"] = state_tbl->get_uint64(r, "value_u64");
                }
            });
        }
        root["engine_state"] = state;
    }

    // --- Write ---
    std::ofstream file(path);
    if (!file.is_open()) {
        log_error("Failed to open scene file for writing: %s", path.c_str());
        return false;
    }

    file << root.dump(4);
    file.close();

    log_info("Saved scene '%s' to '%s'", name_.c_str(), path.c_str());
    return true;
}

} // namespace engine
