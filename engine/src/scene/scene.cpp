#include "engine/scene/scene.h"
#include "engine/scene/scene_schema.h"
#include "engine/scene/component_registry.h"
#include "engine/ecs/core/component_tags.h"
#include "engine/ecs/core/entity.h"
#include "engine/render/camera.h"
#include "engine/core/log.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace engine
{

    namespace
    {

        using json = nlohmann::json;

        // ---- Camera loader ----
        //
        // Populates a row in Engine_Camera. The JSON field names match
        // the columns; the defaults match CameraDesc. The camera is
        // not a component and has no codec; it is loaded here.

        engine::ecs::RowRef load_camera(engine::ecs::World &world, const json &j)
        {
            const std::string name = j.value("name", std::string("camera"));
            auto *tbl = world.get_table(kCameraTableName);
            if (tbl && !tbl->find_by_primary(
                               "name", engine::ecs::CellValue::from_string(name))
                            .is_nil())
            {
                log_warn("Duplicate camera name in scene: %s", name.c_str());
                return engine::ecs::RowRef{};
            }
            CameraDesc desc;
            desc.name = name;

            if (j.contains("pos") && j["pos"].is_array() && j["pos"].size() >= 2)
            {
                desc.pos_x = j["pos"][0].get<float>();
                desc.pos_y = j["pos"][1].get<float>();
                if (j["pos"].size() >= 3)
                    desc.pos_z = j["pos"][2].get<float>();
            }

            if (j.contains("target") && j["target"].is_array() && j["target"].size() >= 2)
            {
                desc.target_x = j["target"][0].get<float>();
                desc.target_y = j["target"][1].get<float>();
                if (j["target"].size() >= 3)
                    desc.target_z = j["target"][2].get<float>();
            }

            if (j.contains("clear_color") && j["clear_color"].is_array() && j["clear_color"].size() >= 3)
            {
                desc.clear_r = j["clear_color"][0].get<float>();
                desc.clear_g = j["clear_color"][1].get<float>();
                desc.clear_b = j["clear_color"][2].get<float>();
                if (j["clear_color"].size() >= 4)
                    desc.clear_a = j["clear_color"][3].get<float>();
            }

            desc.fov = j.value("fov", 60.0f);
            desc.near_plane = j.value("near", 0.1f);
            desc.far_plane = j.value("far", 1000.0f);
            desc.is_orthographic = j.value("is_orthographic", true);
            desc.ortho_size = j.value("ortho_size", 1.0f);

            return write_camera_row(world, desc);
        }

        // ---- Apply the parsed JSON to the world ----

        void apply_json_to_world(engine::ecs::World &world,
                                 const ComponentRegistry &components,
                                 const json &j,
                                 ApplyMode mode)
        {
            if (mode == ApplyMode::Clear)
            {
                world.clear_rows();
            }

            // --- Cameras ---
            if (j.contains("cameras") && j["cameras"].is_array())
            {
                for (const auto &cj : j["cameras"])
                {
                    load_camera(world, cj);
                }
            }

            // --- Entities ---
            std::unordered_map<std::uint64_t, EntityRef> id_to_entity;

            if (j.contains("entities") && j["entities"].is_array())
            {
                for (const auto &ej : j["entities"])
                {
                    const std::uint64_t id = ej.value("id", std::uint64_t(0));
                    if (id == 0)
                    {
                        log_warn("Entity with id 0 in scene file; skipping.");
                        continue;
                    }
                    if (id_to_entity.count(id))
                    {
                        log_warn("Duplicate entity id %llu in scene file; skipping.",
                                 static_cast<unsigned long long>(id));
                        continue;
                    }

                    const std::string ename = ej.value("name", std::string(""));
                    EntityRef e = create_entity(world, id, ename);
                    id_to_entity[id] = e;

                    if (ej.contains("components") && ej["components"].is_object())
                    {
                        for (auto it = ej["components"].begin();
                             it != ej["components"].end(); ++it)
                        {
                            const std::string &name = it.key();
                            const ComponentCodec *codec = components.codec(name);
                            if (codec == nullptr || !codec->load)
                            {
                                log_warn("Unknown component in scene file: %s",
                                         name.c_str());
                                continue;
                            }
                            codec->load(world, e, it.value());
                        }
                    }
                }

                // Pass 2: resolve parents.
                for (const auto &ej : j["entities"])
                {
                    const std::uint64_t id = ej.value("id", std::uint64_t(0));
                    const std::uint64_t parent_id = ej.value("parent", std::uint64_t(0));
                    if (parent_id == 0)
                        continue;

                    auto child_it = id_to_entity.find(id);
                    auto parent_it = id_to_entity.find(parent_id);
                    if (child_it == id_to_entity.end())
                        continue;
                    if (parent_it == id_to_entity.end())
                    {
                        log_warn("Entity %llu references missing parent %llu.",
                                 static_cast<unsigned long long>(id),
                                 static_cast<unsigned long long>(parent_id));
                        continue;
                    }
                    set_parent(world, child_it->second, parent_it->second);
                }
            }

            // --- Engine state ---
            std::uint64_t next_id = 0;
            if (j.contains("engine_state") && j["engine_state"].is_object())
            {
                next_id = j["engine_state"].value("next_entity_id", std::uint64_t(0));
            }
            if (next_id == 0)
            {
                next_id = 1;
                for (auto &[id, ref] : id_to_entity)
                {
                    if (id + 1 > next_id)
                        next_id = id + 1;
                }
            }

            if (auto *state_tbl = world.get_table("Engine_EngineState"))
            {
                auto row = state_tbl->find_by_primary(
                    "name", engine::ecs::CellValue::from_string("next_entity_id"));
                if (row.is_nil())
                {
                    state_tbl->add({
                        engine::ecs::CellValue::from_string("next_entity_id"),
                        engine::ecs::CellValue::from_uint64(next_id),
                    });
                }
                else
                {
                    state_tbl->set_uint64(row, "value_u64", next_id);
                }
            }
        }

    } // namespace

    // -----------------------------------------------------------------------------
    // Scene::apply_to
    // -----------------------------------------------------------------------------

    void Scene::apply_to(engine::ecs::World &world,
                         const ComponentRegistry &components,
                         ApplyMode mode) const
    {
        if (json_text_.empty())
        {
            if (mode == ApplyMode::Clear)
            {
                world.clear_rows();
            }
            return;
        }

        json j;
        try
        {
            j = json::parse(json_text_);
        }
        catch (const std::exception &ex)
        {
            log_error("Scene::apply_to: parse error: %s", ex.what());
            return;
        }

        apply_json_to_world(world, components, j, mode);
    }

    // -----------------------------------------------------------------------------
    // Scene::extract_from
    // -----------------------------------------------------------------------------

    Scene Scene::extract_from(const engine::ecs::World &world,
                              const ComponentRegistry &components)
    {
        return SceneSaver_from_world(world, components);
    }

} // namespace engine