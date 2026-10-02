#include "engine/entity.h"
#include "engine/log.h"

#include <stdexcept>

namespace engine {

namespace {

constexpr const char* kEngineStateTable = "_EngineState";
constexpr const char* kNextEntityIdKey  = "next_entity_id";

// Reads the value_u64 of the _EngineState row with the given name.
// Throws if the row doesn't exist.
std::uint64_t read_engine_state_u64(lucid::Runtime& rt, const std::string& name) {
    auto* tbl = rt.get_table(kEngineStateTable);
    if (!tbl) throw std::runtime_error("_EngineState table not set up");

    lucid::RowRef row = tbl->find_by_primary("name", lucid::CellValue::from_string(name));
    if (row.is_nil()) throw std::runtime_error("_EngineState key not found: " + name);

    return tbl->get_uint64(row, "value_u64");
}

void write_engine_state_u64(lucid::Runtime& rt, const std::string& name, std::uint64_t value) {
    auto* tbl = rt.get_table(kEngineStateTable);
    if (!tbl) throw std::runtime_error("_EngineState table not set up");

    lucid::RowRef row = tbl->find_by_primary("name", lucid::CellValue::from_string(name));
    if (row.is_nil()) throw std::runtime_error("_EngineState key not found: " + name);

    tbl->set_uint64(row, "value_u64", value);
}

} // namespace

// -----------------------------------------------------------------------------
// Entity id allocation
// -----------------------------------------------------------------------------

std::uint64_t allocate_entity_id(lucid::Runtime& rt) {
    const std::uint64_t current = read_engine_state_u64(rt, kNextEntityIdKey);

    if (current == UINT64_MAX) {
        log_fatal("Entity id counter exhausted");
    }

    write_engine_state_u64(rt, kNextEntityIdKey, current + 1);
    return current;
}

// -----------------------------------------------------------------------------
// Entity lifecycle
// -----------------------------------------------------------------------------

EntityRef create_entity(lucid::Runtime& rt) {
    return create_entity(rt, allocate_entity_id(rt), "");
}

EntityRef create_entity(lucid::Runtime& rt,
                        std::uint64_t id,
                        const std::string& name)
{
    auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl) throw std::runtime_error("Entity table not set up");

    return tbl->add({
        lucid::CellValue::from_uint64(id),
        lucid::CellValue::from_string(name),
        lucid::CellValue::from_row_ref(kNullEntity),
    });
}

void destroy_entity(lucid::Runtime& rt, EntityRef e) {
    if (!is_alive(rt, e)) return;

    // Remove components in reverse dependency order.
    // (Nothing depends on WorldTransform or Sprite, but be tidy.)
    remove_component<ScriptTag>(rt, e);
    remove_component<SpriteTag>(rt, e);
    remove_component<WorldTransformTag>(rt, e);
    remove_component<TransformTag>(rt, e);

    // Remove the entity row itself.
    auto* tbl = rt.get_table(EntityTag::name);
    if (tbl) tbl->remove(e);
}

bool is_alive(const lucid::Runtime& rt, EntityRef e) {
    const auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl) return false;
    return tbl->is_valid(e);
}

// -----------------------------------------------------------------------------
// Entity accessors
// -----------------------------------------------------------------------------

std::uint64_t get_entity_id(const lucid::Runtime& rt, EntityRef e) {
    const auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl || !tbl->is_valid(e)) return 0;
    return tbl->get_uint64(e, "id");
}

std::string get_entity_name(const lucid::Runtime& rt, EntityRef e) {
    const auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl || !tbl->is_valid(e)) return "";
    return tbl->get_string(e, "name");
}

void set_entity_name(lucid::Runtime& rt, EntityRef e, const std::string& name) {
    auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl || !tbl->is_valid(e)) return;
    tbl->set_string(e, "name", name);
}

// -----------------------------------------------------------------------------
// Scene tree
// -----------------------------------------------------------------------------

EntityRef get_parent(const lucid::Runtime& rt, EntityRef e) {
    const auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl || !tbl->is_valid(e)) return kNullEntity;
    return tbl->get_row_ref(e, "parent");
}

void set_parent(lucid::Runtime& rt, EntityRef child, EntityRef parent) {
    auto* tbl = rt.get_table(EntityTag::name);
    if (!tbl || !tbl->is_valid(child)) return;
    tbl->set_row_ref(child, "parent", parent);
}

// -----------------------------------------------------------------------------
// Core table setup
// -----------------------------------------------------------------------------

void setup_core_tables(lucid::Runtime& rt) {
    // _EngineState
    {
        auto* t = rt.create_table(kEngineStateTable);
        t->add_column("name",      lucid::ColumnType::String, /*primary=*/true);
        t->add_column("value_u64", lucid::ColumnType::Uint64);
        t->add({
            lucid::CellValue::from_string(kNextEntityIdKey),
            lucid::CellValue::from_uint64(1),
        });
    }

    // Entity
    {
        auto* t = rt.create_table(EntityTag::name);
        t->add_column("id",     lucid::ColumnType::Uint64, /*primary=*/true);
        t->add_column("name",   lucid::ColumnType::String);
        t->add_column("parent", lucid::ColumnType::RowRef);
    }

    // Transform
    {
        auto* t = rt.create_table(TransformTag::name);
        t->add_column("entity", lucid::ColumnType::RowRef, /*primary=*/true);
        t->add_column("x",      lucid::ColumnType::Float32);
        t->add_column("y",      lucid::ColumnType::Float32);
        t->add_column("z",      lucid::ColumnType::Float32);
        t->add_column("rot_z",  lucid::ColumnType::Float32);
    }

    // WorldTransform
    {
        auto* t = rt.create_table(WorldTransformTag::name);
        t->add_column("entity", lucid::ColumnType::RowRef, /*primary=*/true);
        t->add_column("x",      lucid::ColumnType::Float32);
        t->add_column("y",      lucid::ColumnType::Float32);
        t->add_column("z",      lucid::ColumnType::Float32);
        t->add_column("rot_z",  lucid::ColumnType::Float32);
    }

    // Sprite
    {
        auto* t = rt.create_table(SpriteTag::name);
        t->add_column("entity",       lucid::ColumnType::RowRef, /*primary=*/true);
        t->add_column("texture_path", lucid::ColumnType::String);
        t->add_column("w",            lucid::ColumnType::Float32);
        t->add_column("h",            lucid::ColumnType::Float32);
        t->add_column("tint_r",       lucid::ColumnType::Float32);
        t->add_column("tint_g",       lucid::ColumnType::Float32);
        t->add_column("tint_b",       lucid::ColumnType::Float32);
        t->add_column("tint_a",       lucid::ColumnType::Float32);
        t->add_column("layer",        lucid::ColumnType::Int32);
    }

    // Script
    {
        auto* t = rt.create_table(ScriptTag::name);
        t->add_column("entity",      lucid::ColumnType::RowRef, /*primary=*/true);
        t->add_column("module_path", lucid::ColumnType::String);
    }

    log_info("Core tables created");
}

} // namespace engine
