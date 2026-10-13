#include "engine/scene/scene.h"
#include "engine/scene/scene_schema.h"
#include "engine/scene/component_tags.h"
#include "engine/scene/entity.h"
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

        // ---- Component loaders ----
        //
        // Each loader creates the component row (via add_component) and
        // fills the columns present in the JSON. Missing columns keep
        // their default values.

        void load_transform(engine::ecs::World &world, EntityRef e, const json &j)
        {
            auto c = add_component<TransformTag>(world, e);
            auto *tbl = world.get_table(TransformTag::name);
            if (j.contains("x"))
                tbl->set_float32(c, "x", j["x"].get<float>());
            if (j.contains("y"))
                tbl->set_float32(c, "y", j["y"].get<float>());
            if (j.contains("z"))
                tbl->set_float32(c, "z", j["z"].get<float>());
            if (j.contains("rot_z"))
                tbl->set_float32(c, "rot_z", j["rot_z"].get<float>());
        }

        void load_world_transform(engine::ecs::World &world, EntityRef e, const json &j)
        {
            auto c = add_component<WorldTransformTag>(world, e);
            auto *tbl = world.get_table(WorldTransformTag::name);
            if (j.contains("x"))
                tbl->set_float32(c, "x", j["x"].get<float>());
            if (j.contains("y"))
                tbl->set_float32(c, "y", j["y"].get<float>());
            if (j.contains("z"))
                tbl->set_float32(c, "z", j["z"].get<float>());
            if (j.contains("rot_z"))
                tbl->set_float32(c, "rot_z", j["rot_z"].get<float>());
        }

        void load_velocity(engine::ecs::World &world, EntityRef e, const json &j)
        {
            auto c = add_component<VelocityTag>(world, e);
            auto *tbl = world.get_table(VelocityTag::name);
            if (j.contains("dx"))
                tbl->set_float32(c, "dx", j["dx"].get<float>());
            if (j.contains("dy"))
                tbl->set_float32(c, "dy", j["dy"].get<float>());
            if (j.contains("dz"))
                tbl->set_float32(c, "dz", j["dz"].get<float>());
        }

        void load_sprite(engine::ecs::World &world, EntityRef e, const json &j)
        {
            auto c = add_component<SpriteTag>(world, e);
            auto *tbl = world.get_table(SpriteTag::name);
            if (j.contains("texture_path"))
                tbl->set_string(c, "texture_path", j["texture_path"].get<std::string>());
            if (j.contains("w"))
                tbl->set_float32(c, "w", j["w"].get<float>());
            if (j.contains("h"))
                tbl->set_float32(c, "h", j["h"].get<float>());
            if (j.contains("tint_r"))
                tbl->set_float32(c, "tint_r", j["tint_r"].get<float>());
            if (j.contains("tint_g"))
                tbl->set_float32(c, "tint_g", j["tint_g"].get<float>());
            if (j.contains("tint_b"))
                tbl->set_float32(c, "tint_b", j["tint_b"].get<float>());
            if (j.contains("tint_a"))
                tbl->set_float32(c, "tint_a", j["tint_a"].get<float>());
            if (j.contains("layer"))
                tbl->set_int32(c, "layer", j["layer"].get<int>());
        }

        void load_script(engine::ecs::World &world, EntityRef e, const json &j)
        {
            auto c = add_component<ScriptTag>(world, e);
            auto *tbl = world.get_table(ScriptTag::name);
            if (!tbl)
                return;
            if (j.contains("graph"))
                tbl->set_string(c, "graph", j["graph"].get<std::string>());
        }

        void load_component(engine::ecs::World &world, EntityRef e,
                            const std::string &name, const json &j)
        {
            if (name == "Engine_Transform")
                load_transform(world, e, j);
            else if (name == "Engine_WorldTransform")
                load_world_transform(world, e, j);
            else if (name == "Engine_Velocity")
                load_velocity(world, e, j);
            else if (name == "Engine_Sprite")
                load_sprite(world, e, j);
            else if (name == "Engine_Script")
                load_script(world, e, j);
            else
                log_warn("Unknown component in scene file: %s", name.c_str());
        }

        // ---- Camera loader ----
        //
        // Populates a row in Engine_Camera. The JSON field names match
        // the columns; the defaults match CameraDesc.

        engine::ecs::RowRef load_camera(engine::ecs::World &world, const json &j)
        {
            auto *tbl = world.get_table(CameraTag::name);
            if (!tbl)
                return engine::ecs::RowRef{};

            const std::string name = j.value("name", std::string("camera"));
            if (!tbl->find_by_primary("name", engine::ecs::CellValue::from_string(name)).is_nil())
            {
                log_warn("Duplicate camera name in scene: %s", name.c_str());
                return engine::ecs::RowRef{};
            }

            float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
            if (j.contains("pos") && j["pos"].is_array() && j["pos"].size() >= 2)
            {
                pos_x = j["pos"][0].get<float>();
                pos_y = j["pos"][1].get<float>();
                if (j["pos"].size() >= 3)
                    pos_z = j["pos"][2].get<float>();
            }

            float target_x = 0.0f, target_y = 0.0f, target_z = 0.0f;
            if (j.contains("target") && j["target"].is_array() && j["target"].size() >= 2)
            {
                target_x = j["target"][0].get<float>();
                target_y = j["target"][1].get<float>();
                if (j["target"].size() >= 3)
                    target_z = j["target"][2].get<float>();
            }

            float clear_r = 0.0f, clear_g = 0.0f, clear_b = 0.0f, clear_a = 1.0f;
            if (j.contains("clear_color") && j["clear_color"].is_array() && j["clear_color"].size() >= 3)
            {
                clear_r = j["clear_color"][0].get<float>();
                clear_g = j["clear_color"][1].get<float>();
                clear_b = j["clear_color"][2].get<float>();
                if (j["clear_color"].size() >= 4)
                    clear_a = j["clear_color"][3].get<float>();
            }

            auto ref = tbl->add_partial("name", engine::ecs::CellValue::from_string(name));
            tbl->set_float32(ref, "pos_x", pos_x);
            tbl->set_float32(ref, "pos_y", pos_y);
            tbl->set_float32(ref, "pos_z", pos_z);
            tbl->set_float32(ref, "target_x", target_x);
            tbl->set_float32(ref, "target_y", target_y);
            tbl->set_float32(ref, "target_z", target_z);
            tbl->set_float32(ref, "fov", j.value("fov", 60.0f));
            tbl->set_float32(ref, "near", j.value("near", 0.1f));
            tbl->set_float32(ref, "far", j.value("far", 1000.0f));
            tbl->set_bool(ref, "is_orthographic", j.value("is_orthographic", true));
            tbl->set_float32(ref, "ortho_size", j.value("ortho_size", 1.0f));
            tbl->set_float32(ref, "clear_r", clear_r);
            tbl->set_float32(ref, "clear_g", clear_g);
            tbl->set_float32(ref, "clear_b", clear_b);
            tbl->set_float32(ref, "clear_a", clear_a);

            return ref;
        }

        // ---- Apply the parsed JSON to the world ----
        //
        // Shared by apply_to for both modes. Mode decides whether the
        // world's rows are cleared first.

        void apply_json_to_world(engine::ecs::World &world,
                                 const json &j,
                                 ApplyMode mode)
        {
            if (mode == ApplyMode::Clear)
            {
                // Clear every table's rows, keeping every table's schema.
                //
                // clear() removes the non-preserved tables, which includes
                // Engine_Entity, Engine_Transform, and the rest of the
                // core schema that this function needs to populate. That
                // is why clear_rows() is used: it empties the rows and
                // leaves the schema in place.
                //
                // One consequence: the Engine_EngineState row that holds
                // next_entity_id is cleared. The engine-state block below
                // re-creates it.
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
            // Pass 1: create entities and components. Remember id -> RowRef.
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
                        for (auto it = ej["components"].begin(); it != ej["components"].end(); ++it)
                        {
                            load_component(world, e, it.key(), it.value());
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
            //
            // The scene's next_entity_id. If the scene does not carry
            // one, compute it from the loaded entities.
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

            // Write it to the state table. If the row was cleared (by
            // clear_rows) or never existed, create it.
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

    void Scene::apply_to(engine::ecs::World &world, ApplyMode mode) const
    {
        if (json_text_.empty())
        {
            // A Scene built by hand or by extract_from() has no JSON.
            // Applying it with Clear mode empties the world's rows,
            // keeping the schema. This is the "load nothing on purpose"
            // behavior.
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

        apply_json_to_world(world, j, mode);
    }

    // -----------------------------------------------------------------------------
    // Scene::extract_from
    // -----------------------------------------------------------------------------
    //
    // The saver's counterpart. Builds a Scene from the world's current
    // state. The heavy lifting is in scene_saver.cpp's
    // SceneSaver_from_world; this method forwards to it.

    Scene Scene::extract_from(const engine::ecs::World &world)
    {
        return SceneSaver_from_world(world);
    }

} // namespace engine