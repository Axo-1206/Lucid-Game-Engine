#include "engine/registry/package_builder.h"
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

        // Snapshot counts so end_package() can compute the delta.
        begin_node_types_ = node_types_.size();
        begin_tables_ = table_names_.size();
        begin_enums_ = enums_.size();
        begin_handles_ = handles_.size();
        begin_structs_ = structs_.size();
    }

    void PackageBuilder::end_package()
    {
        PackageCounts counts;
        counts.node_types = node_types_.size() - begin_node_types_;
        counts.tables = table_names_.size() - begin_tables_;
        counts.enums = enums_.size() - begin_enums_;
        counts.handles = handles_.size() - begin_handles_;
        counts.structs = structs_.size() - begin_structs_;
        package_counts_.push_back(counts);

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
        if (kind_str == "struct" && is_duplicate(structs_))
        {
            conflicts_.push_back("duplicate struct: " + name);
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

    void PackageBuilder::add_struct(const lucid::sema::StructTypeInfo &info)
    {
        const std::string name(info.name);
        if (!check_name(name, "struct"))
            return;
        structs_.push_back(info);
    }

    void PackageBuilder::add_node_editor_info(const std::string &type_name,
                                              NodeEditorInfo info)
    {
        // No uniqueness check; two registrations for the same type
        // overwrite. A package that registers editor info twice for
        // the same node is a package authoring mistake, not a conflict
        // the builder should catch.
        node_editor_infos_[type_name] = std::move(info);
    }

    void PackageBuilder::add_table_editor_info(const std::string &table_name,
                                               TableEditorInfo info)
    {
        table_editor_infos_[table_name] = std::move(info);
    }

    void PackageBuilder::add_conflict(std::string message)
    {
        conflicts_.push_back(std::move(message));
    }

    void PackageBuilder::add_component(std::string table_name,
                                       TableCreator table_creator,
                                       ComponentCodec codec)
    {
        // The table name is checked the same way add_table checks it:
        // prefix rule and table-name uniqueness.
        if (!check_name(table_name, "table"))
            return;

        // The codec must not already exist for this table.
        for (const auto &entry : component_codecs_)
        {
            if (entry.table_name == table_name)
            {
                conflicts_.push_back(
                    "component codec for table '" + table_name +
                    "' already registered");
                return;
            }
        }

        table_names_.push_back(table_name);
        table_creators_.push_back(std::move(table_creator));

        component_codecs_.push_back({std::move(table_name), std::move(codec)});
    }

    void PackageBuilder::seal()
    {
        sealed_ = true;
    }

} // namespace engine