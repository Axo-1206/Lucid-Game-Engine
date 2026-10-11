#include "engine/package/package_builder.h"
#include "engine/core/log.h"

#include <algorithm>

namespace engine
{

    PackageBuilder::PackageBuilder() = default;
    PackageBuilder::~PackageBuilder() = default;

    void PackageBuilder::begin_package(std::string package_name,
                                       std::string prefix)
    {
        current_package_name_ = std::move(package_name);
        current_prefix_ = std::move(prefix);
    }

    void PackageBuilder::end_package()
    {
        current_package_name_.clear();
        current_prefix_.clear();
    }

    bool PackageBuilder::check_name(const std::string &name, const char *kind)
    {
        // The name must start with `<current_prefix_>_`.
        //
        // If current_prefix_ is empty (a package did not declare a
        // prefix), any name is allowed — this is the logic-only
        // package case, where the package registers no node types.
        if (!current_prefix_.empty())
        {
            const std::string expected = current_prefix_ + "_";
            if (name.size() < expected.size() ||
                name.compare(0, expected.size(), expected) != 0)
            {
                conflicts_.push_back(
                    std::string("package '") + current_package_name_ +
                    "' attempted to register " + kind + " '" + name +
                    "'. Its prefix is '" + current_prefix_ + "', so its " +
                    kind + "s must start with '" + expected + "'.");
                return false;
            }
        }

        // The name must be unique across all previously registered
        // content.
        //
        // The check depends on the kind: node types, enums, and
        // handles each have their own namespace; tables have another.
        auto is_duplicate = [&](const auto &vec)
        {
            for (const auto &existing : vec)
            {
                if (existing.name == name)
                    return true;
            }
            return false;
        };

        const std::string kind_str(kind);
        if (kind_str == "node type" && is_duplicate(node_types_))
        {
            conflicts_.push_back("duplicate node type: " + name);
            return false;
        }
        if (kind_str == "enum" && is_duplicate(enums_))
        {
            conflicts_.push_back("duplicate enum: " + name);
            return false;
        }
        if (kind_str == "handle" && is_duplicate(handles_))
        {
            conflicts_.push_back("duplicate handle: " + name);
            return false;
        }
        if (kind_str == "table")
        {
            if (std::find(table_names_.begin(), table_names_.end(), name) != table_names_.end())
            {
                conflicts_.push_back("duplicate table: " + name);
                return false;
            }
        }

        return true;
    }

    void PackageBuilder::add_table(std::string name, TableCreator creator)
    {
        if (!check_name(name, "table"))
            return;
        table_names_.push_back(std::move(name));
        table_creators_.push_back(std::move(creator));
    }

    void PackageBuilder::add_node_type(const lucid::sema::NodeTypeInfo &info)
    {
        const std::string name(info.name);
        if (!check_name(name, "node type"))
            return;
        node_types_.push_back(info);
    }

    void PackageBuilder::add_node_executor(engine::graph::NodeExecutor exec)
    {
        node_executors_.push_back(exec);
    }

    void PackageBuilder::add_trigger_mode(TriggerMode mode)
    {
        trigger_modes_.push_back(NodeTypeRuntimeInfo{mode});
    }

    void PackageBuilder::add_enum(const lucid::sema::EnumTypeInfo &info)
    {
        const std::string name(info.name);
        if (!check_name(name, "enum"))
            return;
        enums_.push_back(info);
    }

    void PackageBuilder::add_handle(const lucid::sema::HandleTypeInfo &info)
    {
        const std::string name(info.name);
        if (!check_name(name, "handle"))
            return;
        handles_.push_back(info);
    }

    void PackageBuilder::add_conflict(std::string message)
    {
        conflicts_.push_back(std::move(message));
    }

    void PackageBuilder::seal()
    {
        sealed_ = true;
    }

} // namespace engine