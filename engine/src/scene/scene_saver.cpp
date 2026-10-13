#include "engine/scene/scene.h"
#include "engine/scene/scene_schema.h"
#include "engine/scene/component_registry.h"
#include "engine/ecs/core/component_tags.h"
#include "engine/ecs/core/entity.h"
#include "engine/render/camera.h"
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

    } // namespace

    Scene SceneSaver_from_world(const engine::ecs::World &world,
                                const ComponentRegistry &component_registry)
    {
        Scene s;

        json root;
        root["version"] = kSceneSchemaVersion;
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
        // Component tables: any table with an `entity` column, except
        // the Entity table itself. The codec determines whether the
        // component is serialized.
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
                        const ComponentCodec *codec = component_registry.codec(table_name);
                        if (codec == nullptr || !codec->save)
                        {
                            continue;
                        }
                        json cj = codec->save(world, r);
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