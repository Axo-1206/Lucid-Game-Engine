#pragma once

#include "engine/ecs/world.h"
#include "engine/registry/package.h"
#include "engine/registry/package_builder.h"
#include "engine/registry/default_registry.h"
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

        // Add a package. Must be called before apply(). The manager
        // takes ownership. Returns false if the package's name or
        // prefix conflicts with an already-loaded package; the
        // conflict is recorded.
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

        bool has_conflicts() const { return builder_.has_conflicts(); }
        const std::vector<std::string> &conflicts() const { return builder_.conflicts(); }

        bool applied() const { return applied_; }

    private:
        PackageBuilder builder_;
        std::vector<std::unique_ptr<Package>> packages_;

        bool applied_ = false;

        // The backing storage for the Registry's ArenaSpans. The
        // manager owns these; the Registry holds views into them.
        std::vector<lucid::sema::PhaseInfo> phases_;
        lucid::sema::Registry registry_{};

        static std::string provider_of(const std::string &package_name);
        static std::string last_segment_of(const std::string &package_name);
    };

} // namespace engine