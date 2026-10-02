#pragma once

#include "engine/component_tags.h"
#include "lucid/runtime.h"

#include <cstdint>
#include <string>

namespace engine {

// An EntityRef is a reference to a row in the Entity table.
using EntityRef = lucid::RowRef;

// The nil entity.
constexpr EntityRef kNullEntity {};

// -----------------------------------------------------------------------------
// Entity lifecycle
// -----------------------------------------------------------------------------

// Create a new entity with a generated id and an empty name.
EntityRef create_entity(lucid::Runtime& rt);

// Create with explicit id and name.
EntityRef create_entity(lucid::Runtime& rt,
                        std::uint64_t id,
                        const std::string& name);

// Destroy an entity and all its components.
void destroy_entity(lucid::Runtime& rt, EntityRef e);

// True if the entity row is alive.
bool is_alive(const lucid::Runtime& rt, EntityRef e);

// -----------------------------------------------------------------------------
// Entity accessors
// -----------------------------------------------------------------------------

std::uint64_t get_entity_id  (const lucid::Runtime& rt, EntityRef e);
std::string   get_entity_name(const lucid::Runtime& rt, EntityRef e);
void          set_entity_name(lucid::Runtime& rt, EntityRef e, const std::string& name);

// -----------------------------------------------------------------------------
// Scene tree
// -----------------------------------------------------------------------------

EntityRef get_parent(const lucid::Runtime& rt, EntityRef e);
void      set_parent(lucid::Runtime& rt, EntityRef child, EntityRef parent);

// -----------------------------------------------------------------------------
// Components (template API)
// -----------------------------------------------------------------------------
//
// add_component<Tag>(rt, e) creates a component row keyed by `e`. The
// caller sets the component's other columns afterward. Throws if the
// entity already has that component.
//
// remove_component<Tag>(rt, e) removes the component. No-op if absent.
//
// get_component<Tag>(rt, e) returns the component's RowRef, or a nil
// RowRef if absent.
//
// has_component<Tag>(rt, e) returns true if the entity has the component.

template<typename TableT>
lucid::RowRef add_component(lucid::Runtime& rt, EntityRef e) {
    auto* tbl = rt.get_table(TableT::name);
    if (!tbl) {
        throw std::runtime_error(std::string("Table not found: ") + TableT::name);
    }
    if (tbl->find_by_primary("entity", lucid::CellValue::from_row_ref(e)).is_nil() == false) {
        throw std::runtime_error(std::string("Entity already has component: ") + TableT::name);
    }
    return tbl->add_partial("entity", lucid::CellValue::from_row_ref(e));
}

template<typename TableT>
void remove_component(lucid::Runtime& rt, EntityRef e) {
    auto* tbl = rt.get_table(TableT::name);
    if (!tbl) return;
    lucid::RowRef comp = tbl->find_by_primary("entity", lucid::CellValue::from_row_ref(e));
    if (!comp.is_nil()) {
        tbl->remove(comp);
    }
}

template<typename TableT>
lucid::RowRef get_component(const lucid::Runtime& rt, EntityRef e) {
    const auto* tbl = rt.get_table(TableT::name);
    if (!tbl) return kNullEntity;
    return tbl->find_by_primary("entity", lucid::CellValue::from_row_ref(e));
}

template<typename TableT>
bool has_component(const lucid::Runtime& rt, EntityRef e) {
    return !get_component<TableT>(rt, e).is_nil();
}

// -----------------------------------------------------------------------------
// Core table setup
// -----------------------------------------------------------------------------

// Create the core tables: Entity, Transform, WorldTransform, Sprite, Script,
// and _EngineState. Called once at engine startup.
void setup_core_tables(lucid::Runtime& rt);

// Returns and increments the next entity id.
std::uint64_t allocate_entity_id(lucid::Runtime& rt);

} // namespace engine
