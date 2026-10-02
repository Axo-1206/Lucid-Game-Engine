#include "engine/scene.h"
#include "engine/log.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace engine
{

    namespace
    {

        using json = nlohmann::json;

        // ---- Component loaders ----
        //
        // Each loader creates the component row (via add_component) and fills
        // the columns present in the JSON. Missing columns keep their default
        // values.

        void load_transform(lucid::Runtime &rt, EntityRef e, const json &j)
        {
            auto c = add_component<TransformTag>(rt, e);
            auto *tbl = rt.get_table(TransformTag::name);
            if (j.contains("x"))
                tbl->set_float32(c, "x", j["x"].get<float>());
            if (j.contains("y"))
                tbl->set_float32(c, "y", j["y"].get<float>());
            if (j.contains("z"))
                tbl->set_float32(c, "z", j["z"].get<float>());
            if (j.contains("rot_z"))
                tbl->set_float32(c, "rot_z", j["rot_z"].get<float>());
        }

        void load_world_transform(lucid::Runtime &rt, EntityRef e, const json &j)
        {
            auto c = add_component<WorldTransformTag>(rt, e);
            auto *tbl = rt.get_table(WorldTransformTag::name);
            if (j.contains("x"))
                tbl->set_float32(c, "x", j["x"].get<float>());
            if (j.contains("y"))
                tbl->set_float32(c, "y", j["y"].get<float>());
            if (j.contains("z"))
                tbl->set_float32(c, "z", j["z"].get<float>());
            if (j.contains("rot_z"))
                tbl->set_float32(c, "rot_z", j["rot_z"].get<float>());
        }

        void load_sprite(lucid::Runtime &rt, EntityRef e, const json &j)
        {
            auto c = add_component<SpriteTag>(rt, e);
            auto *tbl = rt.get_table(SpriteTag::name);
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

        void load_script(lucid::Runtime &rt, EntityRef e, const json &j)
        {
            auto c = add_component<ScriptTag>(rt, e);
            auto *tbl = rt.get_table(ScriptTag::name);
            if (j.contains("module_path"))
                tbl->set_string(c, "module_path", j["module_path"].get<std::string>());
        }

        void load_component(lucid::Runtime &rt, EntityRef e,
                            const std::string &name, const json &j)
        {
            if (name == "Transform")
                load_transform(rt, e, j);
            else if (name == "WorldTransform")
                load_world_transform(rt, e, j);
            else if (name == "Sprite")
                load_sprite(rt, e, j);
            else if (name == "Script")
                load_script(rt, e, j);
            else
                log_warn("Unknown component in scene file: %s", name.c_str());
        }

        // ---- Camera loader ----

        lucid::RowRef load_camera(lucid::Runtime &rt, const json &j)
        {
            auto *tbl = rt.get_table(CameraTag::name);
            if (!tbl)
                return lucid::RowRef{};

            std::string name = j.value("name", "camera");
            if (tbl->find_by_primary("name", lucid::CellValue::from_string(name)).is_nil() == false)
            {
                log_warn("Duplicate camera name in scene: %s", name.c_str());
                return lucid::RowRef{};
            }

            // Build the row with all columns.
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

            // Add the row with the primary key first, then set the rest.
            auto ref = tbl->add_partial("name", lucid::CellValue::from_string(name));

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

    } // namespace

    // ---- The loader ----

    bool Scene::load_from_file(const std::string &path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            log_error("Failed to open scene file: %s", path.c_str());
            return false;
        }

        json j;
        try
        {
            file >> j;
        }
        catch (const std::exception &ex)
        {
            log_error("Failed to parse scene file '%s': %s", path.c_str(), ex.what());
            return false;
        }

        // Version check.
        int version = j.value("version", 1);
        if (version > 1)
        {
            log_warn("Scene file '%s' is version %d, engine supports version 1. Loading best-effort.",
                     path.c_str(), version);
        }

        // Clear existing content.
        clear();

        name_ = j.value("name", "");

        // --- Cameras ---
        if (j.contains("cameras") && j["cameras"].is_array())
        {
            for (const auto &cj : j["cameras"])
            {
                load_camera(runtime_, cj);
            }
        }

        // --- Entities ---
        // Pass 1: create entities and components, remember id -> RowRef.
        std::unordered_map<std::uint64_t, EntityRef> id_to_entity;

        if (j.contains("entities") && j["entities"].is_array())
        {
            for (const auto &ej : j["entities"])
            {
                std::uint64_t id = ej.value("id", std::uint64_t(0));
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

                std::string ename = ej.value("name", "");

                EntityRef e = create_entity(runtime_, id, ename);
                id_to_entity[id] = e;

                // Components.
                if (ej.contains("components") && ej["components"].is_object())
                {
                    for (auto it = ej["components"].begin(); it != ej["components"].end(); ++it)
                    {
                        load_component(runtime_, e, it.key(), it.value());
                    }
                }
            }

            // Pass 2: resolve parents.
            for (const auto &ej : j["entities"])
            {
                std::uint64_t id = ej.value("id", std::uint64_t(0));
                std::uint64_t parent_id = ej.value("parent", std::uint64_t(0));

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

                set_parent(runtime_, child_it->second, parent_it->second);
            }
        }

        // --- Engine state ---
        std::uint64_t next_id = 1;
        if (j.contains("engine_state") && j["engine_state"].is_object())
        {
            next_id = j["engine_state"].value("next_entity_id", std::uint64_t(1));
        }
        else
        {
            // Compute from the loaded entities.
            for (auto &[id, ref] : id_to_entity)
            {
                if (id + 1 > next_id)
                    next_id = id + 1;
            }
        }

        {
            auto *state_tbl = runtime_.get_table("_EngineState");
            if (state_tbl)
            {
                auto row = state_tbl->find_by_primary("name", lucid::CellValue::from_string("next_entity_id"));
                if (!row.is_nil())
                {
                    state_tbl->set_uint64(row, "value_u64", next_id);
                }
            }
        }

        // --- Active camera ---
        // The first camera becomes active.
        {
            auto *cam_tbl = runtime_.get_table(CameraTag::name);
            if (cam_tbl && cam_tbl->count() > 0)
            {
                cam_tbl->each([&](lucid::RowRef r)
                              {
                if (active_camera_.is_nil()) {
                    active_camera_ = r;
                } });
            }
        }

        log_info("Loaded scene '%s' from '%s' (%zu entities)",
                 name_.c_str(), path.c_str(), id_to_entity.size());
        return true;
    }

} // namespace engine
