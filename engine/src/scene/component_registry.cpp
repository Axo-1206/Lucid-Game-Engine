#include "engine/scene/component_registry.h"
#include "engine/ecs/core/component_tags.h"
#include "engine/ecs/core/entity.h"
#include "engine/core/log.h"

#include <stdexcept>
#include <string>

namespace engine
{

    namespace
    {

        using json = nlohmann::json;

        // ---- Core component loaders ----
        //
        // Each creates the component row (via add_component) and fills
        // the columns present in the JSON. Missing columns keep their
        // default values.

        void load_transform(engine::ecs::World &world,
                            engine::ecs::RowRef entity,
                            const json &j)
        {
            auto c = add_component<TransformTag>(world, entity);
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

        void load_world_transform(engine::ecs::World &world,
                                  engine::ecs::RowRef entity,
                                  const json &j)
        {
            auto c = add_component<WorldTransformTag>(world, entity);
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

        void load_velocity(engine::ecs::World &world,
                           engine::ecs::RowRef entity,
                           const json &j)
        {
            auto c = add_component<VelocityTag>(world, entity);
            auto *tbl = world.get_table(VelocityTag::name);
            if (j.contains("dx"))
                tbl->set_float32(c, "dx", j["dx"].get<float>());
            if (j.contains("dy"))
                tbl->set_float32(c, "dy", j["dy"].get<float>());
            if (j.contains("dz"))
                tbl->set_float32(c, "dz", j["dz"].get<float>());
        }

        void load_sprite(engine::ecs::World &world,
                         engine::ecs::RowRef entity,
                         const json &j)
        {
            auto c = add_component<SpriteTag>(world, entity);
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

        void load_script(engine::ecs::World &world,
                         engine::ecs::RowRef entity,
                         const json &j)
        {
            auto c = add_component<ScriptTag>(world, entity);
            auto *tbl = world.get_table(ScriptTag::name);
            if (!tbl)
                return;
            if (j.contains("graph"))
                tbl->set_string(c, "graph", j["graph"].get<std::string>());
        }

        // ---- Core component savers ----

        json save_transform(const engine::ecs::World &world,
                            engine::ecs::RowRef entity)
        {
            const auto *tbl = world.get_table(TransformTag::name);
            if (!tbl)
                return json();
            auto c = tbl->find_by_primary("entity", engine::ecs::CellValue::from_row_ref(entity));
            if (c.is_nil())
                return json();

            json j;
            j["x"] = tbl->get_float32(c, "x");
            j["y"] = tbl->get_float32(c, "y");
            j["z"] = tbl->get_float32(c, "z");
            j["rot_z"] = tbl->get_float32(c, "rot_z");
            return j;
        }

        json save_world_transform(const engine::ecs::World &world,
                                  engine::ecs::RowRef entity)
        {
            const auto *tbl = world.get_table(WorldTransformTag::name);
            if (!tbl)
                return json();
            auto c = tbl->find_by_primary("entity", engine::ecs::CellValue::from_row_ref(entity));
            if (c.is_nil())
                return json();

            json j;
            j["x"] = tbl->get_float32(c, "x");
            j["y"] = tbl->get_float32(c, "y");
            j["z"] = tbl->get_float32(c, "z");
            j["rot_z"] = tbl->get_float32(c, "rot_z");
            return j;
        }

        json save_velocity(const engine::ecs::World &world,
                           engine::ecs::RowRef entity)
        {
            const auto *tbl = world.get_table(VelocityTag::name);
            if (!tbl)
                return json();
            auto c = tbl->find_by_primary("entity", engine::ecs::CellValue::from_row_ref(entity));
            if (c.is_nil())
                return json();

            json j;
            j["dx"] = tbl->get_float32(c, "dx");
            j["dy"] = tbl->get_float32(c, "dy");
            j["dz"] = tbl->get_float32(c, "dz");
            return j;
        }

        json save_sprite(const engine::ecs::World &world,
                         engine::ecs::RowRef entity)
        {
            const auto *tbl = world.get_table(SpriteTag::name);
            if (!tbl)
                return json();
            auto c = tbl->find_by_primary("entity", engine::ecs::CellValue::from_row_ref(entity));
            if (c.is_nil())
                return json();

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

        json save_script(const engine::ecs::World &world,
                         engine::ecs::RowRef entity)
        {
            const auto *tbl = world.get_table(ScriptTag::name);
            if (!tbl)
                return json();
            auto c = tbl->find_by_primary("entity", engine::ecs::CellValue::from_row_ref(entity));
            if (c.is_nil())
                return json();

            json j;
            j["graph"] = tbl->get_string(c, "graph");
            return j;
        }

    } // namespace

    // -----------------------------------------------------------------------------
    // ComponentRegistry
    // -----------------------------------------------------------------------------

    void ComponentRegistry::register_component(const std::string &table_name,
                                               ComponentCodec codec)
    {
        if (codecs_.count(table_name))
        {
            throw std::runtime_error(
                "ComponentRegistry: duplicate component codec for table '" +
                table_name + "'");
        }
        order_.push_back(table_name);
        codecs_.emplace(table_name, std::move(codec));
    }

    bool ComponentRegistry::has(const std::string &table_name) const
    {
        return codecs_.count(table_name) != 0;
    }

    const ComponentCodec *ComponentRegistry::codec(const std::string &table_name) const
    {
        auto it = codecs_.find(table_name);
        return it == codecs_.end() ? nullptr : &it->second;
    }

    // -----------------------------------------------------------------------------
    // register_core_components
    // -----------------------------------------------------------------------------

    void register_core_components(ComponentRegistry &registry)
    {
        // Idempotent within a registry: if a core component is already
        // registered, skip it. This makes a caller who does not know
        // whether the core is registered safe to call.
        auto reg = [&](const std::string &name, ComponentCodec codec)
        {
            if (!registry.has(name))
            {
                registry.register_component(name, std::move(codec));
            }
        };

        reg(TransformTag::name, ComponentCodec{load_transform, save_transform});
        reg(WorldTransformTag::name, ComponentCodec{load_world_transform, save_world_transform});
        reg(VelocityTag::name, ComponentCodec{load_velocity, save_velocity});
        reg(SpriteTag::name, ComponentCodec{load_sprite, save_sprite});
        reg(ScriptTag::name, ComponentCodec{load_script, save_script});
    }

} // namespace engine