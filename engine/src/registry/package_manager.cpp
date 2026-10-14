#include "engine/registry/package_manager.h"
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

        // Record metadata.
        PackageInfo info;
        info.name = name;
        info.prefix = prefix;
        info.version = pkg->version();
        info.description = pkg->description();
        info.author = pkg->author();
        package_infos_.push_back(info);

        engine::log_info("PackageManager: loading '%s' (prefix '%s')",
                         name.c_str(), prefix.c_str());

        builder_.begin_package(name, prefix);
        pkg->register_into(builder_);
        builder_.end_package();

        packages_.push_back(std::move(pkg));
        return true;
    }

    namespace
    {

        // Build the Registry, the ComponentRegistry, and the package
        // counts from the builder's content. Shared by apply() and
        // apply_registration_only(). Does not seal the builder; the
        // caller does that.
        //
        // Returns false if the executor or trigger-mode counts do not
        // match the node-type count.
        bool build_registries(
            PackageBuilder &builder,
            std::vector<lucid::sema::PhaseInfo> &phases,
            lucid::sema::Registry &registry,
            engine::ComponentRegistry &component_registry,
            std::vector<engine::PackageInfo> &package_infos)
        {
            // Phases. The engine's, not a package's.
            phases = {
                lucid::sema::PhaseInfo{"update"},
                lucid::sema::PhaseInfo{"physics"},
                lucid::sema::PhaseInfo{"render"},
            };

            registry.phases = ArenaSpan<lucid::sema::PhaseInfo>(
                phases.data(), phases.size());
            registry.enums = ArenaSpan<lucid::sema::EnumTypeInfo>(
                builder.enums().data(), builder.enums().size());
            registry.handles = ArenaSpan<lucid::sema::HandleTypeInfo>(
                builder.handles().data(), builder.handles().size());
            registry.nodeTypes = ArenaSpan<lucid::sema::NodeTypeInfo>(
                builder.node_types().data(), builder.node_types().size());
            registry.structs = ArenaSpan<lucid::sema::StructTypeInfo>(
                builder.structs().data(), builder.structs().size());

            // Executor and trigger-mode counts must match node types.
            const std::size_t n = builder.node_types().size();
            if (builder.node_executors().size() != n)
            {
                engine::log_error(
                    "PackageManager: node executor count (%zu) does not match "
                    "node type count (%zu)",
                    builder.node_executors().size(), n);
                return false;
            }
            if (builder.trigger_modes().size() != n)
            {
                engine::log_error(
                    "PackageManager: trigger mode count (%zu) does not match "
                    "node type count (%zu)",
                    builder.trigger_modes().size(), n);
                return false;
            }

            // Component registry: core first, then each package's.
            engine::register_core_components(component_registry);
            for (const auto &entry : builder.component_codecs())
            {
                try
                {
                    component_registry.register_component(entry.table_name, entry.codec);
                }
                catch (const std::exception &ex)
                {
                    engine::log_error("PackageManager: %s", ex.what());
                    return false;
                }
            }

            // Package counts.
            const auto &counts = builder.package_counts();
            for (std::size_t i = 0; i < package_infos.size() && i < counts.size(); ++i)
            {
                package_infos[i].node_type_count = counts[i].node_types;
                package_infos[i].table_count = counts[i].tables;
                package_infos[i].enum_count = counts[i].enums;
                package_infos[i].handle_count = counts[i].handles;
                package_infos[i].struct_count = counts[i].structs;
            }

            engine::log_info("PackageManager: %zu node types registered, "
                             "%zu component codecs registered",
                             n, component_registry.size());
            return true;
        }

    } // namespace

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

        // 2. Build the registries.
        if (!build_registries(builder_, phases_, registry_,
                              component_registry_, package_infos_))
        {
            return false;
        }

        builder_.seal();
        applied_ = true;
        return true;
    }

    bool PackageManager::apply_registration_only()
    {
        if (applied_)
        {
            engine::log_warn(
                "PackageManager: apply_registration_only() called twice; ignored");
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

        // No world; no tables. Build the registries directly.
        if (!build_registries(builder_, phases_, registry_,
                              component_registry_, package_infos_))
        {
            return false;
        }

        builder_.seal();
        applied_ = true;
        return true;
    }

    std::string PackageManager::base_name_of(const std::string &type_name,
                                             const std::string &prefix)
    {
        if (prefix.empty())
            return type_name;
        const std::string prefix_underscore = prefix + "_";
        if (type_name.size() > prefix_underscore.size() &&
            type_name.compare(0, prefix_underscore.size(), prefix_underscore) == 0)
        {
            return type_name.substr(prefix_underscore.size());
        }
        return type_name;
    }

    const NodeEditorInfo *PackageManager::node_editor_info(
        const std::string &type_name) const
    {

        // Explicit info?
        const auto &explicit_infos = builder_.node_editor_infos();
        auto it = explicit_infos.find(type_name);
        if (it != explicit_infos.end())
        {
            return &it->second;
        }

        // Verify the type exists in the registry. A node type that
        // isn't registered has no editor info at all.
        bool exists = false;
        for (const auto &nt : registry_.nodeTypes)
        {
            if (std::string(nt.name) == type_name)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            return nullptr;

        // Construct a default and cache it.
        auto &slot = default_node_infos_[type_name];
        if (slot.display_name.empty())
        {
            // Find the package's prefix by matching the node type's
            // name against each package's prefix.
            std::string prefix;
            for (const auto &info : package_infos_)
            {
                const std::string prefix_underscore = info.prefix + "_";
                if (type_name.size() > prefix_underscore.size() &&
                    type_name.compare(0, prefix_underscore.size(),
                                      prefix_underscore) == 0)
                {
                    prefix = info.prefix;
                    break;
                }
            }
            slot.display_name = base_name_of(type_name, prefix);
            // doc_summary, icon_id, example left empty.
        }
        return &slot;
    }

    const TableEditorInfo *PackageManager::table_editor_info(
        const std::string &table_name) const
    {

        const auto &explicit_infos = builder_.table_editor_infos();
        auto it = explicit_infos.find(table_name);
        if (it != explicit_infos.end())
        {
            return &it->second;
        }

        // Verify the table exists in the world? The manager doesn't
        // hold a World. Skip the check; the caller knows what tables
        // exist.
        auto &slot = default_table_infos_[table_name];
        if (slot.display_name.empty())
        {
            std::string prefix;
            for (const auto &info : package_infos_)
            {
                const std::string prefix_underscore = info.prefix + "_";
                if (table_name.size() > prefix_underscore.size() &&
                    table_name.compare(0, prefix_underscore.size(),
                                       prefix_underscore) == 0)
                {
                    prefix = info.prefix;
                    break;
                }
            }
            slot.display_name = base_name_of(table_name, prefix);
        }
        return &slot;
    }

} // namespace engine