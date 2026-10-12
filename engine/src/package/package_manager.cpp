#include "engine/package/package_manager.h"
#include "engine/graph/tick_context.h"
#include "engine/core/log.h"

#include <cassert>

namespace engine
{

    PackageManager::PackageManager() = default;
    PackageManager::~PackageManager() = default;

    std::string PackageManager::provider_of(const std::string &package_name)
    {
        const auto pos = package_name.find('.');
        return pos == std::string::npos
                   ? package_name
                   : package_name.substr(0, pos);
    }

    std::string PackageManager::last_segment_of(const std::string &package_name)
    {
        const auto pos = package_name.rfind('.');
        return pos == std::string::npos
                   ? package_name
                   : package_name.substr(pos + 1);
    }

    bool PackageManager::load_package(std::unique_ptr<Package> pkg)
    {
        if (applied_)
        {
            engine::log_error(
                "PackageManager: load_package() after apply(); package '%s' ignored",
                pkg ? pkg->name() : "<null>");
            return false;
        }
        if (!pkg)
            return false;

        const std::string name = pkg->name();
        const std::string prefix = pkg->prefix();

        // Rule 1: package names are unique.
        for (const auto &existing : packages_)
        {
            if (existing->name() == name)
            {
                builder_.add_conflict(
                    "duplicate package: '" + name + "' (already loaded)");
                engine::log_error("PackageManager: duplicate package '%s'",
                                  name.c_str());
                return false;
            }
        }

        // Rule 2: last segments of package names are unique (the
        // format library uses the last segment as the import
        // qualifier).
        const std::string last = last_segment_of(name);
        for (const auto &existing : packages_)
        {
            if (last_segment_of(existing->name()) == last)
            {
                builder_.add_conflict(
                    "package name last segment '" + last +
                    "' collides with package '" + existing->name() + "'");
                engine::log_error(
                    "PackageManager: package last segment '%s' collides",
                    last.c_str());
                return false;
            }
        }

        // Rule 3: prefixes are unique across providers. Two packages
        // from the same provider may share a prefix.
        if (!prefix.empty())
        {
            const std::string provider = provider_of(name);
            for (const auto &existing : packages_)
            {
                if (existing->prefix() == prefix)
                {
                    if (provider_of(existing->name()) != provider)
                    {
                        builder_.add_conflict(
                            "prefix '" + prefix + "' already used by package '" +
                            existing->name() + "' (different provider)");
                        engine::log_error(
                            "PackageManager: prefix '%s' already used",
                            prefix.c_str());
                        return false;
                    }
                }
            }
        }

        engine::log_info("PackageManager: loading '%s' (prefix '%s')",
                         name.c_str(), prefix.c_str());

        builder_.begin_package(name, prefix);
        pkg->register_into(builder_);
        builder_.end_package();

        packages_.push_back(std::move(pkg));
        return true;
    }

    bool PackageManager::apply(engine::ecs::World &world)
    {
        if (applied_)
        {
            engine::log_warn("PackageManager: apply() called twice; ignored");
            return !builder_.has_conflicts();
        }

        if (builder_.has_conflicts())
        {
            for (const auto &c : builder_.conflicts())
            {
                engine::log_error("PackageManager: %s", c.c_str());
            }
            return false;
        }

        // 1. Create the packages' tables in the World.
        const auto &creators = builder_.table_creators();
        for (std::size_t i = 0; i < creators.size(); ++i)
        {
            creators[i](world);
        }
        engine::log_info("PackageManager: %zu tables created", creators.size());

        // 2. Declare the engine's phases. Phases are the engine's,
        //    not a package's.
        phases_ = {
            lucid::sema::PhaseInfo{"update"},
            lucid::sema::PhaseInfo{"physics"},
            lucid::sema::PhaseInfo{"render"},
        };

        // 3. Build the Registry from the builder's content.
        registry_.phases = ArenaSpan<lucid::sema::PhaseInfo>(
            phases_.data(), phases_.size());
        registry_.enums = ArenaSpan<lucid::sema::EnumTypeInfo>(
            builder_.enums().data(), builder_.enums().size());
        registry_.handles = ArenaSpan<lucid::sema::HandleTypeInfo>(
            builder_.handles().data(), builder_.handles().size());
        registry_.nodeTypes = ArenaSpan<lucid::sema::NodeTypeInfo>(
            builder_.node_types().data(), builder_.node_types().size());

        // 4. Verify executor and trigger-mode counts match node types.
        const std::size_t n = builder_.node_types().size();
        if (builder_.node_executors().size() != n)
        {
            engine::log_error(
                "PackageManager: node executor count (%zu) does not match "
                "node type count (%zu)",
                builder_.node_executors().size(), n);
            return false;
        }
        if (builder_.trigger_modes().size() != n)
        {
            engine::log_error(
                "PackageManager: trigger mode count (%zu) does not match "
                "node type count (%zu)",
                builder_.trigger_modes().size(), n);
            return false;
        }

        builder_.seal();
        applied_ = true;

        engine::log_info("PackageManager: %zu node types registered", n);
        return true;
    }

} // namespace engine