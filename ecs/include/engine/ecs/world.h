#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/table.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::ecs
{

    // The engine's live table store. Holds every table in the running game:
    // Entity, Transform, WorldTransform, Velocity, Sprite, Camera, _EngineState,
    // and any table a future system adds.
    //
    // A World owns its tables. It does not reference a scene, a renderer, or
    // the format library. It is the data half of the engine; systems read and
    // write it; the scene loader populates it.
    //
    // World is not copyable. It is movable.
    //
    // The engine builds a lucid::sema::Registry from its own node definitions
    // elsewhere; that struct is a declaration of node vocabulary, not a
    // container of rows, and is never stored in a World. See docs/Decision.md.
    class World
    {
    public:
        World();
        ~World();

        World(const World &) = delete;
        World &operator=(const World &) = delete;
        World(World &&) noexcept = default;
        World &operator=(World &&) noexcept = default;

        // Create a table. Returns the new table. Throws if a table with the
        // same name already exists.
        Table *create_table(const std::string &name);

        // Look up a table by name. Returns nullptr if absent.
        Table *get_table(const std::string &name);
        const Table *get_table(const std::string &name) const;

        bool has_table(const std::string &name) const;

        // Remove every table. After clear(), has_table() is false for every
        // name, and the world is as it was before the first create_table().
        void clear();

        // Clear every table's rows, but keep the tables themselves. After
        // clear_rows(), each table's count() is 0 and its columns remain.
        void clear_rows();

        // Iterate tables in creation order.
        template <typename Fn>
        void each_table(Fn &&fn)
        {
            for (auto &name : table_order_)
            {
                fn(*tables_.at(name));
            }
        }

        template <typename Fn>
        void each_table(Fn &&fn) const
        {
            for (auto &name : table_order_)
            {
                fn(*tables_.at(name));
            }
        }

        std::size_t table_count() const { return tables_.size(); }

    private:
        std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
        std::vector<std::string> table_order_;
    };

} // namespace engine::ecs