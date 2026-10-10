#include "engine/entity.h"
#include "engine/log.h"

#include <stdexcept>

namespace engine
{

    namespace
    {

        constexpr const char *kEngineStateTable = "_EngineState";
        constexpr const char *kNextEntityIdKey = "next_entity_id";

        // Reads the value_u64 of the _EngineState row with the given name.
        // Throws if the row doesn't exist.
        std::uint64_t read_engine_state_u64(engine::ecs::World &world, const std::string &name)
        {
            auto *tbl = world.get_table(kEngineStateTable);
            if (!tbl)
                throw std::runtime_error("_EngineState table not set up");

            engine::ecs::RowRef row = tbl->find_by_primary("name", engine::ecs::CellValue::from_string(name));
            if (row.is_nil())
                throw std::runtime_error("_EngineState key not found: " + name);

            return tbl->get_uint64(row, "value_u64");
        }

        void write_engine_state_u64(engine::ecs::World &world, const std::string &name, std::uint64_t value)
        {
            auto *tbl = world.get_table(kEngineStateTable);
            if (!tbl)
                throw std::runtime_error("_EngineState table not set up");

            engine::ecs::RowRef row = tbl->find_by_primary("name", engine::ecs::CellValue::from_string(name));
            if (row.is_nil())
                throw std::runtime_error("_EngineState key not found: " + name);

            tbl->set_uint64(row, "value_u64", value);
        }

    } // namespace

    // -----------------------------------------------------------------------------
    // Entity id allocation
    // -----------------------------------------------------------------------------

    std::uint64_t allocate_entity_id(engine::ecs::World &world)
    {
        const std::uint64_t current = read_engine_state_u64(world, kNextEntityIdKey);

        if (current == UINT64_MAX)
        {
            log_fatal("Entity id counter exhausted");
        }

        write_engine_state_u64(world, kNextEntityIdKey, current + 1);
        return current;
    }

    // -----------------------------------------------------------------------------
    // Entity lifecycle
    // -----------------------------------------------------------------------------

    EntityRef create_entity(engine::ecs::World &world)
    {
        return create_entity(world, allocate_entity_id(world), "");
    }

    EntityRef create_entity(engine::ecs::World &world,
                            std::uint64_t id,
                            const std::string &name)
    {
        auto *tbl = world.get_table(EntityTag::name);
        if (!tbl)
            throw std::runtime_error("Entity table not set up");

        return tbl->add({
            engine::ecs::CellValue::from_uint64(id),
            engine::ecs::CellValue::from_string(name),
            engine::ecs::CellValue::from_row_ref(kNullEntity),
        });
    }

    void destroy_entity(engine::ecs::World &world, EntityRef e)
    {
        if (!is_alive(world, e))
            return;

        // Remove components in reverse dependency order.
        // (Nothing depends on WorldTransform or Sprite, but be tidy.)
        remove_component<VelocityTag>(world, e);
        remove_component<SpriteTag>(world, e);
        remove_component<WorldTransformTag>(world, e);
        remove_component<TransformTag>(world, e);

        // Remove the entity row itself.
        auto *tbl = world.get_table(EntityTag::name);
        if (tbl)
            tbl->remove(e);
    }

    bool is_alive(const engine::ecs::World &world, EntityRef e)
    {
        const auto *tbl = world.get_table(EntityTag::name);
        if (!tbl)
            return false;
        return tbl->is_valid(e);
    }

    // -----------------------------------------------------------------------------
    // Entity accessors
    // -----------------------------------------------------------------------------

    std::uint64_t get_entity_id(const engine::ecs::World &world, EntityRef e)
    {
        const auto *tbl = world.get_table(EntityTag::name);
        if (!tbl || !tbl->is_valid(e))
            return 0;
        return tbl->get_uint64(e, "id");
    }

    std::string get_entity_name(const engine::ecs::World &world, EntityRef e)
    {
        const auto *tbl = world.get_table(EntityTag::name);
        if (!tbl || !tbl->is_valid(e))
            return "";
        return tbl->get_string(e, "name");
    }

    void set_entity_name(engine::ecs::World &world, EntityRef e, const std::string &name)
    {
        auto *tbl = world.get_table(EntityTag::name);
        if (!tbl || !tbl->is_valid(e))
            return;
        tbl->set_string(e, "name", name);
    }

    // -----------------------------------------------------------------------------
    // Scene tree
    // -----------------------------------------------------------------------------

    EntityRef get_parent(const engine::ecs::World &world, EntityRef e)
    {
        const auto *tbl = world.get_table(EntityTag::name);
        if (!tbl || !tbl->is_valid(e))
            return kNullEntity;
        return tbl->get_row_ref(e, "parent");
    }

    void set_parent(engine::ecs::World &world, EntityRef child, EntityRef parent)
    {
        auto *tbl = world.get_table(EntityTag::name);
        if (!tbl || !tbl->is_valid(child))
            return;
        tbl->set_row_ref(child, "parent", parent);
    }

    // -----------------------------------------------------------------------------
    // Core table setup
    // -----------------------------------------------------------------------------

    void setup_core_tables(engine::ecs::World &world)
    {
        // _EngineState
        {
            auto *t = world.create_table(kEngineStateTable);
            t->add_column("name", engine::ecs::ColumnType::String, /*primary=*/true);
            t->add_column("value_u64", engine::ecs::ColumnType::Uint64);
            t->add({
                engine::ecs::CellValue::from_string(kNextEntityIdKey),
                engine::ecs::CellValue::from_uint64(1),
            });
        }

        // Entity
        {
            auto *t = world.create_table(EntityTag::name);
            t->add_column("id", engine::ecs::ColumnType::Uint64, /*primary=*/true);
            t->add_column("name", engine::ecs::ColumnType::String);
            t->add_column("parent", engine::ecs::ColumnType::RowRef);
        }

        // Transform
        {
            auto *t = world.create_table(TransformTag::name);
            t->add_column("entity", engine::ecs::ColumnType::RowRef, /*primary=*/true);
            t->add_column("x", engine::ecs::ColumnType::Float32);
            t->add_column("y", engine::ecs::ColumnType::Float32);
            t->add_column("z", engine::ecs::ColumnType::Float32);
            t->add_column("rot_z", engine::ecs::ColumnType::Float32);
        }

        // WorldTransform
        {
            auto *t = world.create_table(WorldTransformTag::name);
            t->add_column("entity", engine::ecs::ColumnType::RowRef, /*primary=*/true);
            t->add_column("x", engine::ecs::ColumnType::Float32);
            t->add_column("y", engine::ecs::ColumnType::Float32);
            t->add_column("z", engine::ecs::ColumnType::Float32);
            t->add_column("rot_z", engine::ecs::ColumnType::Float32);
        }

        // Velocity
        {
            auto *t = world.create_table(VelocityTag::name);
            t->add_column("entity", engine::ecs::ColumnType::RowRef, /*primary=*/true);
            t->add_column("dx", engine::ecs::ColumnType::Float32);
            t->add_column("dy", engine::ecs::ColumnType::Float32);
            t->add_column("dz", engine::ecs::ColumnType::Float32);
        }

        // Sprite
        {
            auto *t = world.create_table(SpriteTag::name);
            t->add_column("entity", engine::ecs::ColumnType::RowRef, /*primary=*/true);
            t->add_column("texture_path", engine::ecs::ColumnType::String);
            t->add_column("w", engine::ecs::ColumnType::Float32);
            t->add_column("h", engine::ecs::ColumnType::Float32);
            t->add_column("tint_r", engine::ecs::ColumnType::Float32);
            t->add_column("tint_g", engine::ecs::ColumnType::Float32);
            t->add_column("tint_b", engine::ecs::ColumnType::Float32);
            t->add_column("tint_a", engine::ecs::ColumnType::Float32);
            t->add_column("layer", engine::ecs::ColumnType::Int32);
        }

        // Camera
        {
            auto *t = world.create_table(CameraTag::name);
            t->add_column("name", engine::ecs::ColumnType::String, /*primary=*/true);
            t->add_column("pos_x", engine::ecs::ColumnType::Float32);
            t->add_column("pos_y", engine::ecs::ColumnType::Float32);
            t->add_column("pos_z", engine::ecs::ColumnType::Float32);
            t->add_column("target_x", engine::ecs::ColumnType::Float32);
            t->add_column("target_y", engine::ecs::ColumnType::Float32);
            t->add_column("target_z", engine::ecs::ColumnType::Float32);
            t->add_column("fov", engine::ecs::ColumnType::Float32);
            t->add_column("near", engine::ecs::ColumnType::Float32);
            t->add_column("far", engine::ecs::ColumnType::Float32);
            t->add_column("is_orthographic", engine::ecs::ColumnType::Bool);
            t->add_column("ortho_size", engine::ecs::ColumnType::Float32);
            t->add_column("clear_r", engine::ecs::ColumnType::Float32);
            t->add_column("clear_g", engine::ecs::ColumnType::Float32);
            t->add_column("clear_b", engine::ecs::ColumnType::Float32);
            t->add_column("clear_a", engine::ecs::ColumnType::Float32);
        }

        log_info("Core tables created");
    }

} // namespace engine
