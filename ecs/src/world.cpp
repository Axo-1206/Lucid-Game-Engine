#include "engine/ecs/world.h"

#include <stdexcept>

namespace engine::ecs
{

    World::World() = default;
    World::~World() = default;

    Table *World::create_table(const std::string &name)
    {
        if (tables_.count(name))
        {
            throw std::runtime_error("Table already exists: " + name);
        }
        auto table = std::make_unique<Table>(name);
        Table *raw = table.get();
        tables_.emplace(name, std::move(table));
        table_order_.push_back(name);
        return raw;
    }

    Table *World::get_table(const std::string &name)
    {
        auto it = tables_.find(name);
        return it == tables_.end() ? nullptr : it->second.get();
    }

    const Table *World::get_table(const std::string &name) const
    {
        auto it = tables_.find(name);
        return it == tables_.end() ? nullptr : it->second.get();
    }

    bool World::has_table(const std::string &name) const
    {
        return tables_.count(name) != 0;
    }

    void World::clear()
    {
        tables_.clear();
        table_order_.clear();
    }

    void World::clear_rows()
    {
        for (auto &name : table_order_)
        {
            tables_.at(name)->clear();
        }
    }

} // namespace engine::ecs