#pragma once

#include "engine/ecs/world.h"
#include "engine/graph/tick_context.h"
#include "engine/nodes/default_registry.h"
#include "sema/Registry.hpp"

#include <functional>
#include <string>
#include <vector>

namespace engine
{

    // The accumulator for package registration. A Package calls the
    // add_* methods in register_into(). After all packages have
    // registered, the PackageManager reads the builder's content to
    // produce the Registry and populate the World.
    //
    // The builder validates the prefix convention and uniqueness as
    // it goes. A registration that violates either is recorded in
    // conflicts_; the manager refuses to produce a Registry until
    // conflicts are empty.
    class PackageBuilder
    {
    public:
        // A function that creates a table in a World. Called once, at
        // apply time.
        using TableCreator = std::function<void(engine::ecs::World &)>;

        PackageBuilder();
        ~PackageBuilder();

        PackageBuilder(const PackageBuilder &) = delete;
        PackageBuilder &operator=(const PackageBuilder &) = delete;

        // --- Package boundaries ---
        //
        // The manager calls begin_package() before register_into() and
        // end_package() after. Between the two calls, the builder
        // knows which package is registering, and can validate names
        // against the package's prefix.

        void begin_package(std::string package_name, std::string prefix);
        void end_package();

        // --- Registration ---

        void add_table(std::string name, TableCreator creator);
        void add_node_type(const lucid::sema::NodeTypeInfo &info);
        void add_node_executor(engine::graph::NodeExecutor exec);
        void add_trigger_mode(TriggerMode mode);
        void add_enum(const lucid::sema::EnumTypeInfo &info);
        void add_handle(const lucid::sema::HandleTypeInfo &info);

        // Record a conflict. Called by the manager for package-level
        // conflicts (duplicate package name, duplicate prefix) that
        // the builder does not see directly.
        void add_conflict(std::string message);

        // --- Queries ---

        bool has_conflicts() const { return !conflicts_.empty(); }
        const std::vector<std::string> &conflicts() const { return conflicts_; }

        const std::vector<std::string> &table_names() const { return table_names_; }
        const std::vector<TableCreator> &table_creators() const { return table_creators_; }

        const std::vector<lucid::sema::NodeTypeInfo> &node_types() const { return node_types_; }
        const std::vector<engine::graph::NodeExecutor> &node_executors() const { return node_executors_; }
        const std::vector<NodeTypeRuntimeInfo> &trigger_modes() const { return trigger_modes_; }

        const std::vector<lucid::sema::EnumTypeInfo> &enums() const { return enums_; }
        const std::vector<lucid::sema::HandleTypeInfo> &handles() const { return handles_; }

        // The backing arrays for the Registry's ArenaSpans. Valid
        // after all packages have registered.
        void seal();
        bool is_sealed() const { return sealed_; }

    private:
        // The package currently registering. Empty between packages.
        std::string current_package_name_;
        std::string current_prefix_;

        std::vector<std::string> table_names_;
        std::vector<TableCreator> table_creators_;

        std::vector<lucid::sema::NodeTypeInfo> node_types_;
        std::vector<engine::graph::NodeExecutor> node_executors_;
        std::vector<NodeTypeRuntimeInfo> trigger_modes_;

        std::vector<lucid::sema::EnumTypeInfo> enums_;
        std::vector<lucid::sema::HandleTypeInfo> handles_;

        std::vector<std::string> conflicts_;

        bool sealed_ = false;

        // Validate a name against the current package's prefix.
        // Returns true if the name is acceptable; otherwise records
        // a conflict and returns false.
        bool check_name(const std::string &name, const char *kind);
    };

} // namespace engine