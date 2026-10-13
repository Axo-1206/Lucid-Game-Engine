#pragma once

#include "engine/ecs/core/component_tags.h"
#include "engine/ecs/world.h"

#include <cstdint>
#include <string>

namespace engine
{

    // An EntityRef is a reference to a row in the Entity table.
    using EntityRef = engine::ecs::RowRef;

    // The nil entity.
    constexpr EntityRef kNullEntity{};

    // -----------------------------------------------------------------------------
    // Entity lifecycle
    // -----------------------------------------------------------------------------

    EntityRef create_entity(engine::ecs::World &world);
    EntityRef create_entity(engine::ecs::World &world,
                            std::uint64_t id,
                            const std::string &name);
    void destroy_entity(engine::ecs::World &world, EntityRef e);
    bool is_alive(const engine::ecs::World &world, EntityRef e);

    // -----------------------------------------------------------------------------
    // Entity accessors
    // -----------------------------------------------------------------------------

    std::uint64_t get_entity_id(const engine::ecs::World &world, EntityRef e);
    std::string get_entity_name(const engine::ecs::World &world, EntityRef e);
    void set_entity_name(engine::ecs::World &world, EntityRef e,
                         const std::string &name);

    // -----------------------------------------------------------------------------
    // Scene tree
    // -----------------------------------------------------------------------------

    EntityRef get_parent(const engine::ecs::World &world, EntityRef e);
    void set_parent(engine::ecs::World &world, EntityRef child, EntityRef parent);

    // -----------------------------------------------------------------------------
    // Components (template API)
    //
    // Every TableT here must name a table whose primary column is
    // `entity`. The camera table does not; it has no tag and no
    // add_component<...> instantiation.
    // -----------------------------------------------------------------------------

    template <typename TableT>
    engine::ecs::RowRef add_component(engine::ecs::World &world, EntityRef e)
    {
        auto *tbl = world.get_table(TableT::name);
        if (!tbl)
        {
            throw std::runtime_error(std::string("Table not found: ") + TableT::name);
        }
        if (!tbl->find_by_primary("entity",
                                  engine::ecs::CellValue::from_row_ref(e))
                 .is_nil())
        {
            throw std::runtime_error(std::string("Entity already has component: ") + TableT::name);
        }
        return tbl->add_partial("entity",
                                engine::ecs::CellValue::from_row_ref(e));
    }

    template <typename TableT>
    void remove_component(engine::ecs::World &world, EntityRef e)
    {
        auto *tbl = world.get_table(TableT::name);
        if (!tbl)
            return;
        engine::ecs::RowRef comp = tbl->find_by_primary(
            "entity", engine::ecs::CellValue::from_row_ref(e));
        if (!comp.is_nil())
        {
            tbl->remove(comp);
        }
    }

    template <typename TableT>
    engine::ecs::RowRef get_component(const engine::ecs::World &world, EntityRef e)
    {
        const auto *tbl = world.get_table(TableT::name);
        if (!tbl)
            return kNullEntity;
        return tbl->find_by_primary("entity",
                                    engine::ecs::CellValue::from_row_ref(e));
    }

    template <typename TableT>
    bool has_component(const engine::ecs::World &world, EntityRef e)
    {
        return !get_component<TableT>(world, e).is_nil();
    }

    // -----------------------------------------------------------------------------
    // Core table setup
    // -----------------------------------------------------------------------------

    void setup_core_tables(engine::ecs::World &world);
    std::uint64_t allocate_entity_id(engine::ecs::World &world);

} // namespace engine