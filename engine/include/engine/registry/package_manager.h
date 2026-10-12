#pragma once

#include "engine/ecs/world.h"
#include "engine/registry/default_registry.h"
#include "engine/registry/editor_info.h"
#include "engine/registry/package.h"
#include "engine/registry/package_builder.h"
#include "sema/Registry.hpp"

#include <memory>
#include <string>
#include <vector>

namespace engine
{

    // The package manager: loads packages, produces one Registry,
    // and populates one World.
    //
    // Lifetime:
    //   - Construct an empty manager.
    //   - load_package(...) for each package, in order.
    //   - apply(World&) once, after all packages are loaded and the
    //     World's core tables have been created.
    //   - registry() and trigger_modes() are valid after apply().
    class PackageManager
    {
    public:
        PackageManager();
        ~PackageManager();

        PackageManager(const PackageManager &) = delete;
        PackageManager &operator=(const PackageManager &) = delete;

        bool load_package(std::unique_ptr<Package> pkg);

        // Apply the registered content: create the packages' tables in
        // the World, seal the builder, build the Registry.
        //
        // Must be called exactly once, after all load_package() calls
        // and after the World's core tables have been created.
        //
        // Returns false if any conflict was recorded.
        bool apply(engine::ecs::World &world);

        // The merged registry. Valid only after apply().
        const lucid::sema::Registry &registry() const { return registry_; }

        // The per-node-type runtime metadata, parallel to
        // registry().nodeTypes. Valid only after apply().
        const std::vector<NodeTypeRuntimeInfo> &trigger_modes() const
        {
            return builder_.trigger_modes();
        }

        // The node executors, parallel to registry().nodeTypes. Valid
        // only after apply().
        const std::vector<engine::graph::NodeExecutor> &node_executors() const
        {
            return builder_.node_executors();
        }

        // --- Editor metadata ---

        // One PackageInfo per loaded package. The list is in load
        // order. Valid after load_package() calls; the counts are
        // populated after apply().
        const std::vector<PackageInfo> &package_infos() const
        {
            return package_infos_;
        }

        // The editor info for one node type. If the type has no
        // explicit editor info, returns a default: display_name is
        // the base name, everything else empty. Returns nullptr if
        // the type name is not in the registry.
        const NodeEditorInfo *node_editor_info(const std::string &type_name) const;

        // The editor info for one table. Same fallback semantics.
        const TableEditorInfo *table_editor_info(const std::string &table_name) const;

        bool has_conflicts() const { return builder_.has_conflicts(); }
        const std::vector<std::string> &conflicts() const { return builder_.conflicts(); }

        bool applied() const { return applied_; }

    private:
        PackageBuilder builder_;
        std::vector<std::unique_ptr<Package>> packages_;
        std::vector<PackageInfo> package_infos_;

        // Storage for default editor infos. When a caller asks for
        // editor info on a node without explicit info, we construct a
        // default and store it here so we can return a stable pointer.
        // Mutable because node_editor_info() is const.
        mutable std::unordered_map<std::string, NodeEditorInfo> default_node_infos_;
        mutable std::unordered_map<std::string, TableEditorInfo> default_table_infos_;

        bool applied_ = false;

        // The backing storage for the Registry's ArenaSpans. The
        // manager owns these; the Registry holds views into them.
        std::vector<lucid::sema::PhaseInfo> phases_;
        lucid::sema::Registry registry_{};

        static std::string provider_of(const std::string &package_name);
        static std::string last_segment_of(const std::string &package_name);

        static std::string base_name_of(const std::string &type_name,
                                        const std::string &prefix);
    };

} // namespace engine