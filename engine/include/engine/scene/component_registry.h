#pragma once

#include "engine/ecs/world.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine
{

    // A component's codec: how the component's JSON becomes a row in
    // its table, and how a row becomes its JSON.
    //
    // A component that is not serialized leaves both null. The loader
    // logs a warning and ignores the component in a .lscene file; the
    // saver skips it.
    //
    //   load(world, entity, json) — the component's JSON object.
    //   save(world, entity)       — the component's JSON object, or
    //                               null if the entity has no row.
    //
    // See docs/notes/PackageComposition.md §5 and
    // docs/notes/SceneFormat.md for the component's shape.
    struct ComponentCodec
    {
        using LoadFn = std::function<void(engine::ecs::World &,
                                          engine::ecs::RowRef entity,
                                          const nlohmann::json &)>;

        using SaveFn = std::function<nlohmann::json(const engine::ecs::World &,
                                                    engine::ecs::RowRef entity)>;

        LoadFn load;
        SaveFn save;
    };

    // The set of component types the scene loader and saver know
    // about. Populated at startup by register_core_components (the
    // engine's five core components) and by PackageBuilder::add_component
    // (each package's components, in package load order).
    //
    // Registration order is preserved. A codec's table name is unique;
    // registering the same name twice is a conflict caught at
    // registration time.
    class ComponentRegistry
    {
    public:
        ComponentRegistry() = default;

        // Register a codec. Throws if the table name is already
        // registered. The caller (PackageBuilder) is responsible for
        // catching the throw and recording it as a conflict.
        void register_component(const std::string &table_name,
                                ComponentCodec codec);

        // True if a codec is registered for this table name.
        bool has(const std::string &table_name) const;

        // The codec for this table name, or nullptr if none.
        const ComponentCodec *codec(const std::string &table_name) const;

        // Iterate registered codecs in registration order.
        template <typename Fn>
        void each(Fn &&fn) const
        {
            for (const auto &name : order_)
            {
                fn(name, codecs_.at(name));
            }
        }

        std::size_t size() const { return codecs_.size(); }

    private:
        std::vector<std::string> order_;
        std::unordered_map<std::string, ComponentCodec> codecs_;
    };

    // Register the engine's core component codecs:
    //   Engine_Transform, Engine_WorldTransform, Engine_Velocity,
    //   Engine_Sprite, Engine_Script.
    //
    // Idempotent within one registry (second call is a no-op); not
    // idempotent across registries (a second registry needs its own
    // call).
    void register_core_components(ComponentRegistry &registry);

} // namespace engine