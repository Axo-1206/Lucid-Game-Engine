#include "engine/scene/scene.h"
#include "engine/scene/scene_schema.h"
#include "engine/ecs/core/component_tags.h"
#include "engine/ecs/core/entity.h"
#include "engine/core/log.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <vector>

namespace engine
{

    namespace
    {

        using json = nlohmann::json;

        // ---- Component savers ----

        json save_transform(const engine::ecs::Table *tbl, engine::ecs::RowRef c)
        {
            json j;
            j["x"] = tbl->get_float32(c, "x");
            j["y"] = tbl->get_float32(c, "y");
            j["z"] = tbl->get_float32(c, "z");
            j["rot_z"] = tbl->get_float32(c, "rot_z");
            return j;
        }

        json save_world_transform(const engine::ecs::Table *tbl, engine::ecs::RowRef c)
        {
            json j;
            j["x"] = tbl->get_float32(c, "x");
            j["y"] = tbl->get_float32(c, "y");
            j["z"] = tbl->get_float32(c, "z");
            j["rot_z"] = tbl->get_float32(c, "rot_z");
            return j;
        }

        json save_velocity(const engine::ecs::Table *tbl, engine::ecs::RowRef c)
        {
            json j;
            j["dx"] = tbl->get_float32(c, "dx");
            j["dy"] = tbl->get_float32(c, "dy");
            j["dz"] = tbl->get_float32(c, "dz");
            return j;
        }

        json save_sprite(const engine::ecs::Table *tbl, engine::ecs::RowRef c)
        {
            json j;
            j["texture_path"] = tbl->get_string(c, "texture_path");
            j["w"] = tbl->get_float32(c, "w");
            j["h"] = tbl->get_float32(c, "h");
            j["tint_r"] = tbl->get_float32(c, "tint_r");
            j["tint_g"] = tbl->get_float32(c, "tint_g");
            j["tint_b"] = tbl->get_float32(c, "tint_b");
            j["tint_a"] = tbl->get_float32(c, "tint_a");
            j["layer"] = tbl->get_int32(c, "layer");
            return j;
        }

        json save_script(const engine::ecs::Table *tbl, engine::ecs::RowRef c)
        {
            json j;
            j["graph"] = tbl->get_string(c, "graph");
            return j;
        }

        json save_component_by_name(const engine::ecs::World &world,
                                    EntityRef e,
                                    const std::string &table_name)
        {
            const auto *tbl = world.get_table(table_name);
            if (!tbl)
                return json();

            auto comp = tbl->find_by_primary("entity", engine::ecs::CellValue::from_row_ref(e));
            if (comp.is_nil())
                return json();

            if (table_name == "Engine_Transform")
                return save_transform(tbl, comp);
            else if (table_name == "Engine_WorldTransform")
                return save_world_transform(tbl, comp);
            else if (table_name == "Engine_Velocity")
                return save_velocity(tbl, comp);
            else if (table_name == "Engine_Sprite")
                return save_sprite(tbl, comp);
            else if (table_name == "Engine_Script")
                return save_script(tbl, comp);
            else
            {
                log_warn("No saver for component table: %s", table_name.c_str());
                return json();
            }
        }

    } // namespace

    Scene SceneSaver_from_world(const engine::ecs::World &world)
    {
        Scene s;

        json root;
        root["version"] = kSceneSchemaVersion;

        // --- Name ---
        // A World has no name. Leave the scene name empty; the caller
        // can set it before saving.
        root["name"] = "";

        // --- Cameras ---
        {
            json cameras = json::array();
            const auto *cam_tbl = world.get_table(kCameraTableName);
            if (cam_tbl)
            {
                cam_tbl->each([&](engine::ecs::RowRef r)
                              {
                    const CameraDesc desc = read_camera_row(world, r);

                    json cj;
                    cj["name"]            = desc.name;
                    cj["pos"]             = { desc.pos_x, desc.pos_y, desc.pos_z };
                    cj["target"]          = { desc.target_x, desc.target_y, desc.target_z };
                    cj["fov"]             = desc.fov;
                    cj["near"]            = desc.near_plane;
                    cj["far"]             = desc.far_plane;
                    cj["is_orthographic"] = desc.is_orthographic;
                    cj["ortho_size"]      = desc.ortho_size;
                    cj["clear_color"]     = { desc.clear_r, desc.clear_g, desc.clear_b, desc.clear_a };
                    cameras.push_back(cj); });
            }
            root["cameras"] = cameras;
        }

        // --- Entities ---
        // Discover component tables: any table with an `entity` column
        // that is not the Entity table itself.
        std::vector<std::string> component_tables;
        world.each_table([&](const engine::ecs::Table &t)
                         {
            if (t.name() != EntityTag::name && t.has_column("entity"))
                component_tables.push_back(t.name()); });

        {
            json entities = json::array();
            const auto *entity_tbl = world.get_table(EntityTag::name);
            if (entity_tbl)
            {
                entity_tbl->each([&](engine::ecs::RowRef r)
                                 {
                    json ej;
                    ej["id"]   = entity_tbl->get_uint64(r, "id");
                    ej["name"] = entity_tbl->get_string(r, "name");

                    engine::ecs::RowRef parent = entity_tbl->get_row_ref(r, "parent");
                    if (!parent.is_nil() && entity_tbl->is_valid(parent))
                    {
                        ej["parent"] = entity_tbl->get_uint64(parent, "id");
                    }
                    else
                    {
                        ej["parent"] = 0;
                    }

                    json components = json::object();
                    for (const auto &table_name : component_tables)
                    {
                        json cj = save_component_by_name(world, r, table_name);
                        if (!cj.is_null())
                        {
                            components[table_name] = cj;
                        }
                    }
                    ej["components"] = components;

                    entities.push_back(ej); });
            }
            root["entities"] = entities;
        }

        // --- Engine state ---
        {
            json state = json::object();
            const auto *state_tbl = world.get_table("Engine_EngineState");
            if (state_tbl)
            {
                state_tbl->each([&](engine::ecs::RowRef r)
                                {
                    std::string key = state_tbl->get_string(r, "name");
                    if (key == "next_entity_id")
                    {
                        state["next_entity_id"] = state_tbl->get_uint64(r, "value_u64");
                    } });
            }
            root["engine_state"] = state;
        }

        s.set_json_text(root.dump(4));
        return s;
    }

    bool SceneSaver_save(const Scene &scene, const std::string &path)
    {
        if (scene.json_text().empty())
        {
            log_error("SceneSaver_save: scene has no JSON to save.");
            return false;
        }

        std::ofstream file(path);
        if (!file.is_open())
        {
            log_error("Failed to open scene file for writing: %s", path.c_str());
            return false;
        }

        file << scene.json_text();
        file.close();

        log_info("Saved scene to '%s'", path.c_str());
        return true;
    }

} // namespace engine