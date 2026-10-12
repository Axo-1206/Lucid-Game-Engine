#pragma once

#include "engine/ecs/world.h"
#include "engine/graph/tick_context.h"
#include "engine/registry/default_registry.h"
#include "engine/registry/editor_info.h"
#include "sema/Registry.hpp"

#include <functional>
#include <string>
#include <unordered_map>
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

        void begin_package(std::string package_name, std::string prefix);
        void end_package();

        // --- Registration ---

        void add_table(std::string name, TableCreator creator);
        void add_node_type(const lucid::sema::NodeTypeInfo &info);
        void add_node_executor(engine::graph::NodeExecutor exec);
        void add_trigger_mode(TriggerMode mode);
        void add_enum(const lucid::sema::EnumTypeInfo &info);
        void add_handle(const lucid::sema::HandleTypeInfo &info);

        // --- Editor metadata ---
        //
        // Optional. A node type without editor info gets defaults at
        // lookup time (see NodeEditorInfo). A table without table
        // editor info gets a default display name.

        void add_node_editor_info(const std::string &type_name,
                                  NodeEditorInfo info);

        void add_table_editor_info(const std::string &table_name,
                                   TableEditorInfo info);

        // --- Conflicts ---

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

        const std::unordered_map<std::string, NodeEditorInfo> &node_editor_infos() const
        {
            return node_editor_infos_;
        }
        const std::unordered_map<std::string, TableEditorInfo> &table_editor_infos() const
        {
            return table_editor_infos_;
        }

        // Per-package counts, filled by end_package().
        struct PackageCounts
        {
            std::size_t node_types = 0;
            std::size_t tables = 0;
            std::size_t enums = 0;
            std::size_t handles = 0;
        };
        const std::vector<PackageCounts> &package_counts() const { return package_counts_; }

        void seal();
        bool is_sealed() const { return sealed_; }

    private:
        std::string current_package_name_;
        std::string current_prefix_;

        // Counts at the moment begin_package() was called, so
        // end_package() can compute the delta.
        std::size_t begin_node_types_ = 0;
        std::size_t begin_tables_ = 0;
        std::size_t begin_enums_ = 0;
        std::size_t begin_handles_ = 0;

        std::vector<std::string> table_names_;
        std::vector<TableCreator> table_creators_;

        std::vector<lucid::sema::NodeTypeInfo> node_types_;
        std::vector<engine::graph::NodeExecutor> node_executors_;
        std::vector<NodeTypeRuntimeInfo> trigger_modes_;

        std::vector<lucid::sema::EnumTypeInfo> enums_;
        std::vector<lucid::sema::HandleTypeInfo> handles_;

        std::unordered_map<std::string, NodeEditorInfo> node_editor_infos_;
        std::unordered_map<std::string, TableEditorInfo> table_editor_infos_;

        std::vector<PackageCounts> package_counts_;

        std::vector<std::string> conflicts_;

        bool sealed_ = false;

        bool check_name(const std::string &name, const char *kind);
    };

} // namespace engine